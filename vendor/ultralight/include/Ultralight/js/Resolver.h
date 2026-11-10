///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/CAPI/CAPI_JSAPI.h>
#include <Ultralight/CAPI/CAPI_JSClass.h>
#include <Ultralight/detail/Exceptions.h>
#include <Ultralight/js/Error.h>
#include <Ultralight/js/TypeTraits.h>

#include <atomic>
#include <type_traits>
#include <utility>

namespace ultralight {
namespace js {

/// \cond INTERNAL
namespace detail {
struct ResolverAccess;
}  // namespace detail
/// \endcond

///
/// A handle that settles a JavaScript Promise.
///
/// A Resolver lets native code settle a JavaScript Promise from any thread. Taking a Resolver as
/// the trailing parameter of a bound function turns it into an asynchronous call that returns a
/// Promise to the page immediately.
///
/// Pass the resolver to a background thread and settle it when the work finishes:
///
/// ```
/// app["save"] = [](std::string path, js::Resolver done) {
///   RunOnSaveThread([path, done = std::move(done)] {
///     if (WriteSave(path))
///       done.Resolve(true);
///     else
///       done.Reject(js::Error::Make(js::ErrorType::Error, "disk full"));
///   });
/// };
/// ```
///
/// Page script awaits the returned Promise normally:
///
/// ```js
/// const saved = await app.save("slot1.dat");
/// ```
///
/// ## Settling the Promise
///
/// Resolvers follow these rules:
///
/// - **Resolve() and Reject() are safe from any thread.**
/// - **Values convert on the Renderer's thread during a later Renderer::Update().** Passing an
///   argument to Resolve() copies or moves the C++ value immediately.
/// - **Only the first call to Resolve() or Reject() takes effect.**
/// - **Destroying an unsettled Resolver rejects the Promise.** This ensures the page never waits
///   forever.
///
/// ## Sharing a Resolver
///
/// Resolver is move-only and can't be captured directly by copyable types such as `std::function`.
/// Wrap the resolver in `std::shared_ptr` to pass it into a copyable callback or job queue:
///
/// ```
/// auto shared = std::make_shared<js::Resolver>(std::move(done));
/// jobs.push_back([shared] { shared->Resolve(42); });
/// ```
///
/// ## Promises Outside Bound Functions
///
/// Call js::Context::MakePromise() on the page's context to create a pending Promise directly
/// without a bound function:
///
/// ```
/// auto [promise, resolver] = ctx.MakePromise();
/// ctx["ready"] = promise;
///
/// // Later, from any thread:
/// resolver.Resolve(std::string("ok"));
/// ```
///
/// \parblock
/// @note  If the page navigates away or closes before the Promise settles, calling Resolve() or
///        Reject() is safe and the library discards the result.
/// \endparblock
///
/// \parblock
/// @note  Use a Resolver when an outside system or background thread signals completion of an
///        operation. For sequential multi-step async operations, use js::Task instead.
/// \endparblock
///
/// @see  js::Task, js::Context::MakePromise(), js::Error
///
class Resolver {
 public:
  ///
  /// Create an empty Resolver (Resolve() and Reject() do nothing).
  ///
  Resolver() = default;

  // --- Interop with the C API (most embedders never touch raw handles) -------------------

  ///
  /// Take ownership of a resolver handle from the C API.
  ///
  /// You only need this at the C boundary (eg, for the handle a ULJSAsyncFunctionCallback
  /// receives, or to call Task::Start()). A bound function that takes a js::Resolver gets one
  /// ready to use.
  ///
  /// @param  handle  The handle to take ownership of.
  ///
  /// @return  Returns a Resolver that owns `handle`.
  ///
  /// @note  There's no FromBorrowed(): a Resolver settles its Promise once. To share one, see
  ///        the class notes.
  ///
  static Resolver Adopt(ULJSPromiseResolver handle) { return Resolver(handle); }

  ///
  /// Move constructor (`other` becomes empty).
  ///
  Resolver(Resolver&& other) noexcept
      : resolver_(std::exchange(other.resolver_, nullptr)),
        protected_(other.protected_.exchange(nullptr)) {}

  ///
  /// Move assignment (`other` becomes empty). If this Resolver held a Promise it hasn't settled,
  /// that Promise is rejected now.
  ///
  Resolver& operator=(Resolver&& other) noexcept {
    if (this != &other) {
      ulDestroyJSPromiseResolver(resolver_);
      ReleaseProtection();
      resolver_ = std::exchange(other.resolver_, nullptr);
      protected_.store(other.protected_.exchange(nullptr));
    }
    return *this;
  }

  Resolver(const Resolver&) = delete;
  Resolver& operator=(const Resolver&) = delete;

  ///
  /// Destructor (rejects the Promise if it hasn't been settled).
  ///
  ~Resolver() {
    ulDestroyJSPromiseResolver(resolver_);
    ReleaseProtection();
  }

  ///
  /// Whether or not this Resolver holds a Promise (it stays true after Resolve() or Reject()).
  ///
  explicit operator bool() const { return resolver_ != nullptr; }

  ///
  /// Resolve the Promise with `undefined`.
  ///
  /// @note  Safe to call from any thread.
  ///
  void Resolve() const {
    ulJSPromiseResolverComplete(resolver_, &UndefinedThunk, nullptr, nullptr);
    ReleaseProtection();
  }

  ///
  /// Resolve the Promise with a value.
  ///
  /// @param  value  The value, copied or moved now and converted through js::TypeTraits later
  ///                on the Renderer's thread. A string literal or `char*` is copied into a
  ///                std::string.
  ///
  /// @note  Safe to call from any thread.
  ///
  template <typename T>
    requires Marshalable<detail::CapturedArg<T>>
  void Resolve(T&& value) const {
    static_assert(!detail::kIsClassInstancePointer<T>,
                  "a raw instance pointer cannot cross a deferred boundary (delivery "
                  "happens later, on the Renderer's thread): capture an owning holder (the "
                  "class's canonical holder) or js::Eternal<T> instead");
    static_assert(!detail::IsCallScopedView<std::decay_t<T>>::value,
                  "a std::span cannot cross a deferred boundary (delivery happens later; "
                  "the view would dangle): copy into a std::vector<E> or RefPtr<Buffer>");
    using Stored = detail::CapturedArg<T>;
    auto* payload = new Stored(detail::CaptureArg(std::forward<T>(value)));
    ulJSPromiseResolverComplete(resolver_, &ResolveThunk<Stored>, payload,
                                &DeletePayload<Stored>);
    ReleaseProtection();
  }

  ///
  /// Reject the Promise with an error.
  ///
  /// @param  error  The rejection reason.
  ///
  /// @note  Safe to call from any thread.
  ///
  void Reject(Error error) const {
    auto* payload = new Error(std::move(error));
    ulJSPromiseResolverComplete(resolver_, &RejectThunk, payload, &DeletePayload<Error>);
    ReleaseProtection();
  }

  ///
  /// Get the C API handle without transferring ownership (this Resolver still destroys it).
  ///
  /// @return  Returns the handle (NULL if this Resolver is empty).
  ///
  /// @warning  Destroying the handle must be its last use. If another thread settles the Promise
  ///           through this handle, make sure that happens before this Resolver is destroyed.
  ///
  ULJSPromiseResolver raw() const { return resolver_; }

  ///
  /// Give up ownership of the C API handle and return it.
  ///
  /// @return  Returns the handle (this Resolver becomes empty). You must call
  ///          ulDestroyJSPromiseResolver() when finished.
  ///
  /// @warning  Destroying the handle must be its last use (see ulDestroyJSPromiseResolver()).
  ///
  ULJSPromiseResolver LeakRef() {
    ULJSPromiseResolver handle = resolver_;
    resolver_ = nullptr;
    return handle;
  }

 private:
  explicit Resolver(ULJSPromiseResolver handle) : resolver_(handle) {}

  static ULJSValue UndefinedThunk(void*, ULJSContext, ULJSValue*) { return nullptr; }

  // The conversion runs during a later update, inside the library: a converter that throws
  // rejects the Promise with a generic Error instead of unwinding into it.
  template <typename Stored>
  static ULJSValue ResolveThunk(void* user_data, ULJSContext ctx, ULJSValue* exception) {
    ULJSValue result = nullptr;
    ultralight::detail::CallCatchingExceptions(
        [&] { result = TypeTraits<Stored>::ToJS(ctx, *static_cast<Stored*>(user_data)); },
        [&](const char* what) {
          result = nullptr;
          *exception = detail::NativeExceptionError(nullptr, what).ToJS(ctx);
        });
    return result;
  }

  static ULJSValue RejectThunk(void* user_data, ULJSContext ctx, ULJSValue* exception) {
    ultralight::detail::CallCatchingExceptions(
        [&] { *exception = static_cast<Error*>(user_data)->ToJS(ctx); },
        [&](const char* what) {
          *exception = detail::NativeExceptionError(nullptr, what).ToJS(ctx);
        });
    return nullptr;
  }

  template <typename Stored>
  static void DeletePayload(void* user_data) {
    delete static_cast<Stored*>(user_data);
  }

  friend struct detail::ResolverAccess;

  // The instance an async class method was called on stays protected until this Resolver
  // settles or is destroyed (see "Async Methods" in js::ClassBuilder). Any thread: the
  // release itself is handed to the Renderer's thread. Atomic because a shared Resolver can
  // settle from two threads at once, and only one may release the protection.
  void ReleaseProtection() const { ulJSObjectUnprotectInstance(protected_.exchange(nullptr)); }

  ULJSPromiseResolver resolver_ = nullptr;
  mutable std::atomic<ULJSProtectedInstance> protected_ { nullptr };
};

}  // namespace js
}  // namespace ultralight
