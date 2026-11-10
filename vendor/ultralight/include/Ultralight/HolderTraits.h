///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/RefPtr.h>

#include <memory>
#include <type_traits>

namespace ultralight {

///
/// Whether a holder shares ownership of a bound-class instance or owns it alone.
///
enum class HolderKind {
  ///
  /// Copies share ownership (eg, RefPtr), or the instance is never destroyed (js::Eternal).
  ///
  Shared,

  ///
  /// One holder owns the instance and ownership moves (eg, std::unique_ptr). Returning one to
  /// JavaScript leaves it empty, and it can't be a bound parameter.
  ///
  Exclusive,
};

/// \cond INTERNAL
namespace detail {

// The library's built-in holders. HolderTraits inherits this tier, so a user specialization of
// HolderTraits always takes priority, and a release can add built-ins here without colliding
// with specializations users already wrote.
template <typename H, typename = void>
struct BuiltinHolderTraits {};

template <typename T>
struct BuiltinHolderTraits<RefPtr<T>> {
  using element_type = T;
  static constexpr HolderKind kind = HolderKind::Shared;
  static T* Get(const RefPtr<T>& holder) { return holder.get(); }
};

template <typename T>
struct BuiltinHolderTraits<std::shared_ptr<T>> {
  using element_type = T;
  static constexpr HolderKind kind = HolderKind::Shared;
  static T* Get(const std::shared_ptr<T>& holder) { return holder.get(); }
};

template <typename T, typename D>
struct BuiltinHolderTraits<std::unique_ptr<T, D>> {
  using element_type = T;
  static constexpr HolderKind kind = HolderKind::Exclusive;
  static T* Get(const std::unique_ptr<T, D>& holder) { return holder.get(); }
};

template <typename T>
struct BuiltinHolderTraits<WeakPtr<T>> {
  using element_type = T;
  static RefPtr<T> Lock(const WeakPtr<T>& holder) { return holder.Lock(); }
};

template <typename T>
struct BuiltinHolderTraits<std::weak_ptr<T>> {
  using element_type = T;
  static std::shared_ptr<T> Lock(const std::weak_ptr<T>& holder) { return holder.lock(); }
};

}  // namespace detail
/// \endcond

///
/// Traits template that lets the library hold objects through custom smart pointers.
///
/// Specializing HolderTraits teaches the library how to inspect and unwrap your engine's smart
/// pointers.
///
/// Standard smart pointers like `std::shared_ptr` and `std::unique_ptr` work automatically without
/// extra setup. You only need to specialize this template when integrating custom smart pointers
/// from your application or engine.
///
/// This example specializes HolderTraits for an engine reference-counted pointer and binds a member
/// function:
///
/// ```
/// template <typename T>
/// struct ultralight::HolderTraits<MyRef<T>> {
///   using element_type = T;
///   static constexpr HolderKind kind = HolderKind::Shared;
///   static T* Get(const MyRef<T>& holder) { return holder.get(); }
/// };
///
/// MyRef<Player> player = SpawnPlayer();
/// app["jump"] = js::Bind(player, &Player::Jump);
/// ```
///
/// ## Built-In Holders
///
/// The library provides built-in traits for standard and library smart pointers. Built-in owning
/// holders declare a HolderKind:
///
/// | Holder            | Kind      |
/// |-------------------|-----------|
/// | RefPtr            | Shared    |
/// | `std::shared_ptr` | Shared    |
/// | `std::unique_ptr` | Exclusive |
/// | js::Eternal       | Shared    |
///
/// The holder's kind determines how the library manages your object's lifetime:
///
/// - **Shared holders share ownership between native code and the library.** Copies keep the object
///   alive until native code and any JavaScript wrapper, DOM listener, or data binding release it.
/// - **Exclusive holders transfer unique ownership to the library.** Moving the pointer leaves your
///   native holder empty, and the library keeps the object alive for as long as the wrapper, DOM
///   listener, or data binding exists.
///
/// WeakPtr and `std::weak_ptr` serve as built-in weak holders by defining Lock() instead of a kind.
/// A weak holder doesn't keep your object alive-- the library temporarily locks it for each call,
/// skipping the operation if the object has been destroyed, or throwing an error when called from
/// JavaScript.
///
/// ## Custom Smart Pointers
///
/// A single specialization works across the JavaScript, DOM, and data-binding APIs. Specialize the
/// template as an owning holder or a weak holder, depending on how your pointer manages lifetime:
///
/// - **An owning holder defines `element_type`, `kind`, and Get().** Set `kind` to
///   HolderKind::Shared or HolderKind::Exclusive, and return a raw pointer from Get().
/// - **A weak holder defines `element_type` and Lock().** Return an owning holder from Lock() to
///   provide temporary access to the instance during a call.
///
/// This example specializes HolderTraits for an engine weak pointer:
///
/// ```
/// template <typename T>
/// struct ultralight::HolderTraits<MyWeakRef<T>> {
///   using element_type = T;
///   static MyRef<T> Lock(const MyWeakRef<T>& holder) { return holder.lock(); }
/// };
/// ```
///
/// To use your custom owning holder as the canonical holder for a bound class, specialize
/// js::ClassHolder for that class.
///
/// Custom specializations take priority over built-in traits.
///
/// @warning  Declare your specialization in namespace ultralight before any code passes the pointer
///           to the library. If the compiler encounters code using the holder before seeing your
///           specialization, it either falls back silently to built-in traits or fails to build.
///
/// @see  js::ClassHolder, js::Bind(), dom::LockableHolder, js::Eternal
///
template <typename H, typename = void>
struct HolderTraits : detail::BuiltinHolderTraits<H> {};

/// \cond INTERNAL
namespace detail {

template <typename H>
concept HolderHasLock = requires(const H& holder) { HolderTraits<H>::Lock(holder); };

template <typename H>
concept HolderHasGet = requires(const H& holder) { HolderTraits<H>::Get(holder); };

// Strong access to a holder's object for one use: Lock()'s result for a weak holder, or the
// pointee itself for an owning holder (which keeps the object alive on its own). The caller
// tests the result, then dereferences it.
template <typename H>
decltype(auto) LockHolder(const H& holder) {
  if constexpr (HolderHasLock<H>)
    return HolderTraits<H>::Lock(holder);
  else
    return HolderTraits<H>::Get(holder);
}

}  // namespace detail
/// \endcond

}  // namespace ultralight
