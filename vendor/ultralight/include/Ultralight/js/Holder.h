///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/HolderTraits.h>
#include <Ultralight/RefPtr.h>

#include <concepts>
#include <memory>
#include <type_traits>

namespace ultralight {
namespace js {

///
/// Non-owning holder for bound-class singletons and program-lifetime objects.
///
/// An Eternal wraps a pointer to a native object that lives for the entire run of the application
/// and is never destroyed, such as an engine subsystem or a global singleton. It lets page scripts
/// interact with the native instance through a standard JavaScript wrapper without reference
/// counting or ownership transfers.
///
/// You'll typically specialize js::ClassHolder with Eternal beside the class declaration so every
/// file that binds or converts the class includes it. This makes Eternal the class's canonical
/// holder, allowing you to assign the singleton directly to an API path.
///
/// This example exposes a game's world singleton to the page as `app.world`:
///
/// ```
/// class GameWorld {
///  public:
///   static GameWorld& Current();
/// };
///
/// template <>
/// struct ultralight::js::ClassHolder<GameWorld> {
///   using type = js::Eternal<GameWorld>;
/// };
///
/// void RegisterWorld(js::API& api) {
///   api.DefineClass<GameWorld>("GameWorld");  // no Constructor: `new` throws
///   api["world"] = js::Eternal(&GameWorld::Current());
/// }
/// ```
///
/// ## Differences from Raw Pointers
///
/// Both a `T*` and an Eternal leave ownership with you, but they differ in how they can cross the
/// bridge:
///
/// - **Raw pointers cannot be assigned to an API path directly.** A bound function or property
///   getter can return a `T*` to give the page a wrapper that lasts, but raw pointers can't be
///   passed to API::Emit() or Resolver::Resolve().
/// - **Eternal holders support direct assignment and deferred deliveries.** An Eternal can be
///   assigned directly to an API path (as shown above) or passed to API::Emit() and
///   Resolver::Resolve(), which deliver the instance on a later frame during Renderer::Update().
///
/// @note  ClassBuilder::Constructor() doesn't support classes whose canonical holder is an Eternal.
///        Calling `new` on the page throws a TypeError with code `ULJS_NO_CTOR` (use a property
///        binding or a static method returning the Eternal instead).
///
/// @see  js::ClassHolder, js::ClassBuilder, ultralight::HolderTraits
///
template <typename T>
class Eternal {
 public:
  ///
  /// Create an empty Eternal.
  ///
  constexpr Eternal() = default;

  ///
  /// Create an Eternal for an instance.
  ///
  /// @param  instance  The instance, which must never be destroyed.
  ///
  constexpr explicit Eternal(T* instance) : ptr_(instance) {}

  ///
  /// Get the instance (NULL when empty).
  ///
  T* get() const { return ptr_; }

  ///
  /// Whether or not this holds an instance.
  ///
  explicit operator bool() const { return ptr_ != nullptr; }

 private:
  T* ptr_ = nullptr;
};

template <typename T>
Eternal(T*) -> Eternal<T>;

///
/// Whether or not H (ignoring const and references) is an owning holder: its
/// ultralight::HolderTraits declares element_type, kind, and Get().
///
template <typename H>
concept Holder = requires(const std::remove_cvref_t<H>& h) {
  typename HolderTraits<std::remove_cvref_t<H>>::element_type;
  { HolderTraits<std::remove_cvref_t<H>>::kind } -> std::convertible_to<HolderKind>;
  HolderTraits<std::remove_cvref_t<H>>::Get(h);
};

///
/// Whether or not H (ignoring const and references) is a weak holder: its
/// ultralight::HolderTraits declares element_type and a Lock() that returns an owning holder.
///
template <typename H>
concept LockableHolder = requires(const std::remove_cvref_t<H>& h) {
  typename HolderTraits<std::remove_cvref_t<H>>::element_type;
  { HolderTraits<std::remove_cvref_t<H>>::Lock(h) } -> Holder;
};

///
/// Type trait that selects the smart pointer type for owned instances of a bound class.
///
/// When an owned instance of a bound C++ class crosses the bridge to JavaScript, it travels in its
/// canonical holder-- the smart pointer that manages native ownership for that class.
///
/// This specialization sets `std::shared_ptr` as the holder for a bound `Database` class:
///
/// ```
/// template <>
/// struct ultralight::js::ClassHolder<Database> {
///   using type = std::shared_ptr<Database>;
/// };
/// ```
///
/// ## Where the Holder Is Used
///
/// The library requires the canonical holder when transferring ownership across the bridge:
///
/// - **Bound functions must return owned instances in the canonical holder.** Returning an owned
///   instance in any other smart pointer is a compile error.
/// - **API::Emit() and Resolver::Resolve() require the canonical holder.** Delivering an instance
///   later during Renderer::Update() requires an owning smart pointer rather than a borrowed raw
///   pointer.
///
/// ## Supported Holder Types
///
/// By default, `type` is RefPtr for a class that derives from RefCounted, or std::unique_ptr for
/// any other class. You can specialize ClassHolder to select any smart pointer that matches your
/// ownership model:
///
/// - **Use std::shared_ptr for shared ownership.** Native code and the JavaScript wrapper share
///   ownership of non-RefCounted objects.
/// - **Use js::Eternal for program-lifetime objects.** Singletons and subsystems wrapped in
///   js::Eternal are never destroyed by the library or the page.
/// - **Use a custom smart pointer with HolderTraits.** Specialize ultralight::HolderTraits for your
///   smart pointer type before specializing ClassHolder.
///
/// \parblock
/// @note  Declare the specialization in the header that defines the class, so every file that binds
///        or converts the class uses the same holder type.
/// \endparblock
///
/// \parblock
/// @note  ClassBuilder::Constructor() works only when the canonical holder is RefPtr,
///        std::unique_ptr, or std::shared_ptr. For another holder (such as js::Eternal), bind a
///        static factory method with ClassBuilder::StaticMethod() that returns the holder instead.
/// \endparblock
///
/// @see  js::ClassBuilder, ultralight::HolderTraits, js::Eternal, js::Detach()
///
template <typename T, typename = void>
struct ClassHolder {
  using type = std::conditional_t<std::is_base_of_v<RefCounted, T>, RefPtr<T>,
                                  std::unique_ptr<T>>;
};

}  // namespace js

/// \cond INTERNAL
namespace detail {

template <typename T>
struct BuiltinHolderTraits<js::Eternal<T>> {
  using element_type = T;
  static constexpr HolderKind kind = HolderKind::Shared;
  static T* Get(const js::Eternal<T>& holder) { return holder.get(); }
};

}  // namespace detail
/// \endcond

}  // namespace ultralight
