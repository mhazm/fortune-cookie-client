///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/CAPI/CAPI_JSRuntime.h>
#include <Ultralight/detail/Exceptions.h>
#include <Ultralight/js/Context.h>
#include <Ultralight/js/Value.h>

#include <type_traits>
#include <utility>

namespace ultralight {
namespace js {

///
/// A weak reference to a JavaScript object.
///
/// Unlike a js::Value, a WeakValue doesn't keep its object alive, so the garbage collector can
/// reclaim the object. Lock() gives you a Value for the object, or an empty Value once the
/// object is collected or its page is gone:
///
/// ```
/// js::WeakValue cached(heavy_object);
/// // later:
/// if (js::Value strong = cached.Lock())
///   UseCached(strong);
/// else
///   RebuildCache();
/// ```
///
/// Use a WeakValue for objects the page keeps alive by itself (eg, a cached page object or a
/// bound instance's own JavaScript object).
///
/// @note  WeakValue is move-only. For the thread rule, see the ultralight::js namespace
///        overview (`<Ultralight/JS.h>`).
///
/// @warning  Don't use a WeakValue as the only reference to a callback. If nothing in the page
///           holds the callback, it's collected and disappears. See "Ownership Cycles" in
///           `<Ultralight/js/Class.h>` for how to break a reference cycle instead.
///
class WeakValue {
 public:
  ///
  /// Create an empty WeakValue (Lock() returns an empty Value).
  ///
  WeakValue() = default;

  ///
  /// Create a weak reference to `target`.
  ///
  /// @param  target  The object to observe. Anything else (a primitive, an empty Value, or a
  ///                 Value whose page is gone) gives an empty WeakValue.
  ///
  explicit WeakValue(const Value& target) : weak_(ulCreateJSWeakRef(target.raw())) {}

  ///
  /// Destructor (releases the weak reference).
  ///
  ~WeakValue() { ulDestroyJSWeakRef(weak_); }

  ///
  /// Move constructor (`other` becomes empty).
  ///
  WeakValue(WeakValue&& other) noexcept : weak_(other.weak_) { other.weak_ = nullptr; }

  ///
  /// Move assignment (releases the current weak reference first).
  ///
  WeakValue& operator=(WeakValue&& other) noexcept {
    if (this != &other) {
      ulDestroyJSWeakRef(weak_);
      weak_ = other.weak_;
      other.weak_ = nullptr;
    }
    return *this;
  }
  WeakValue(const WeakValue&) = delete;
  WeakValue& operator=(const WeakValue&) = delete;

  ///
  /// Get a Value for the object.
  ///
  /// @return  Returns a valid Value for the object (an empty Value if the object was
  ///          collected, its page is gone, or this WeakValue is empty).
  ///
  [[nodiscard]] Value Lock() const { return Value::Adopt(ulJSWeakRefLock(weak_)); }

  ///
  /// Set a callback to run after the object is garbage collected.
  ///
  /// The callback runs once on the Renderer's thread during a later Renderer::Update(). Setting a
  /// new callback replaces the previous one. You can destroy this WeakValue from inside the
  /// callback.
  ///
  /// @param  on_collected  A callable that takes a js::Context& or nothing.
  ///
  /// @note  The callback never runs if the object's page goes away first or this WeakValue is
  ///        destroyed first. On an empty WeakValue this does nothing.
  ///
  template <typename F>
  void OnCollected(F&& on_collected) {
    using Fn = std::decay_t<F>;
    static_assert(std::is_invocable_v<Fn&> || std::is_invocable_v<Fn&, Context&>,
                  "js::WeakValue::OnCollected takes a callable invocable with nothing or "
                  "with js::Context&");
    if (!weak_)
      return;
    ulJSWeakRefSetCollectedCallback(weak_, &CollectedThunk<Fn>,
                                    new Fn(std::forward<F>(on_collected)),
                                    &DeleteCollected<Fn>);
  }

  ///
  /// Whether or not this WeakValue holds nothing (it was default-constructed, moved from,
  /// released with LeakRef(), or created from something that isn't an object in a living page).
  ///
  /// This doesn't tell you whether the object still exists. Use Lock() for that.
  ///
  bool IsEmpty() const { return weak_ == nullptr; }

  // --- Interop with the C API (most embedders never touch raw handles) -------------------

  ///
  /// Take ownership of a handle from the C API.
  ///
  /// @param  handle  The handle to take ownership of.
  ///
  /// @return  Returns a WeakValue that owns `handle`.
  ///
  static WeakValue Adopt(ULJSWeakRef handle) {
    WeakValue weak;
    weak.weak_ = handle;
    return weak;
  }

  ///
  /// Get the C API handle without transferring ownership.
  ///
  /// @return  Returns the handle for use with the `<Ultralight/CAPI/CAPI_JSRuntime.h>` functions.
  ///
  ULJSWeakRef raw() const { return weak_; }

  ///
  /// Give up ownership of the C API handle and return it.
  ///
  /// @return  Returns the handle. You must destroy it with ulDestroyJSWeakRef() when finished.
  ///
  ULJSWeakRef LeakRef() {
    ULJSWeakRef weak = weak_;
    weak_ = nullptr;
    return weak;
  }

 private:
  template <typename Fn>
  static void CollectedThunk(void* user_data, ULJSContext ctx) {
    Fn& fn = *static_cast<Fn*>(user_data);
    // The callback runs inside the library's update: an exception must not unwind into it.
    ultralight::detail::CallCatchingExceptions(
        [&] {
          if constexpr (std::is_invocable_v<Fn&, Context&>) {
            Context borrowed = Context::FromBorrowed(ctx);
            fn(borrowed);
          } else {
            fn();
          }
        },
        [](const char* what) { detail::LogNativeException("js::WeakValue::OnCollected", what); });
  }

  template <typename Fn>
  static void DeleteCollected(void* user_data) {
    delete static_cast<Fn*>(user_data);
  }

  ULJSWeakRef weak_ = nullptr;
};

}  // namespace js
}  // namespace ultralight
