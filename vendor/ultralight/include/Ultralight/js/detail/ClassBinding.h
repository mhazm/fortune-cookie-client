///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// Implementation detail of `<Ultralight/js/Class.h>`; include that header instead.
/// The class-binding machinery: the per-type class registry, the type-erased ownership
/// stakes behind the waist's holder channel, instance wrapping/unwrapping, and the
/// trampolines behind ClassBuilder registrations.
///
/// \cond INTERNAL
#pragma once
#include <Ultralight/CAPI/CAPI_JSAPI.h>
#include <Ultralight/CAPI/CAPI_JSClass.h>
#include <Ultralight/js/Error.h>
#include <Ultralight/js/Holder.h>
#include <Ultralight/js/Resolver.h>
#include <Ultralight/js/Value.h>
#include <Ultralight/js/detail/Binding.h>
#include <Ultralight/js/detail/Reflection.h>

#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

namespace ultralight {
namespace js {

template <typename T, typename>
struct ClassHolder;

namespace detail {

// One bound engine class per native type per program. DefineClass creates it on first use;
// it lives for the remainder of the program (like the registry the bindings live in).
template <typename T>
struct ClassRegistry {
  static_assert(std::is_same_v<T, std::remove_cv_t<T>>);
  static inline ULJSClass cls = nullptr;
};

template <typename T>
ULJSClass BoundClass() {
  return ClassRegistry<std::remove_cv_t<T>>::cls;
}

// The per-wrapper ownership stake, type-erased behind the waist's holder channel. The
// destroy callback is minted here (in the embedder's binary), so the library never learns
// the holder type; it runs on the Renderer's thread when the wrapper is collected or its
// context is torn down. The magic word marks boxes minted by this layer: a holder attached
// through the C API is an arbitrary opaque token, and the readers below must never
// reinterpret one as a StakeBox.
inline constexpr uint32_t kStakeBoxMagic = 0x534B4231;  // "SKB1"

template <typename H>
struct StakeBox {
  uint32_t magic = kStakeBoxMagic;
  H holder;
};

template <typename H>
void DestroyStakeBox(void* user_data) {
  delete static_cast<StakeBox<H>*>(user_data);
}

template <typename P>
void DestroyPayload(void* user_data) {
  delete static_cast<P*>(user_data);
}

template <typename T>
constexpr const char* ClassSchemaName() {
#if ULTRALIGHT_REFLECTION
  return kTypeNameZ<std::remove_cv_t<T>>.data;
#else
  return "object";
#endif
}

// Whether H is the canonical holder of its own element type (see js::ClassHolder). Only the
// canonical holder participates in marshaling, so ownership vocabulary cannot mix.
template <typename H, typename = void>
struct IsCanonicalHolderImpl : std::false_type {};
template <typename H>
struct IsCanonicalHolderImpl<H, std::void_t<typename HolderTraits<H>::element_type>>
    : std::bool_constant<std::is_same_v<
          H, typename ClassHolder<
                 std::remove_cv_t<typename HolderTraits<H>::element_type>, void>::type>> {
  static_assert(!IsCanonicalHolderImpl::value || Holder<H>,
                "a weak holder cannot be a class's canonical holder (every wrapper needs a "
                "strong ownership stake): select a shared holder in js::ClassHolder and "
                "bind weakly per call with js::Bind(weak, &T::Method)");
};

template <typename H>
inline constexpr bool kIsCanonicalHolder = IsCanonicalHolderImpl<H>::value;

// Unwraps a bound-class instance argument. JavaScript `null` converts to a NULL borrow;
// `undefined` (and a missing argument) does not convert, matching every other non-optional
// parameter type. A detached instance throws a TypeError (code ULJS_DETACHED) rather than
// reporting a type mismatch it does not have.
template <typename T>
bool UnwrapInstance(ULJSContext ctx, ULJSValue value, T** out, ULJSValue* exception) {
  *out = nullptr;
  ULJSType type = ulJSValueGetType(value);
  if (type == kULJSType_Null)
    return true;
  if (type != kULJSType_Object)
    return false;
  ULJSClass cls = BoundClass<T>();
  if (!cls)
    return false;
  switch (ulJSObjectGetInstanceState(value, cls)) {
    case kULJSInstanceState_Attached:
      *out = static_cast<T*>(ulJSObjectGetInstance(value, cls));
      return true;
    case kULJSInstanceState_Detached:
      if (exception) {
        std::string message = std::string("this ") + ClassSchemaName<T>() + " was detached";
        *exception = Error::TypeError(message.c_str()).WithCode("ULJS_DETACHED").ToJS(ctx);
      }
      return false;
    case kULJSInstanceState_NotAnInstance:
      break;
  }
  return false;
}

// An instance reached JavaScript before DefineClass<T>() bound its class (a native-side setup
// error): the page gets `undefined`, and at Warn the log and page console say why.
template <typename T>
void WarnUnboundClass(ULJSContext ctx) {
  if (ulJSAPIGetCallDiagnostics() < kULJSDiagnostics_Warn)
    return;
  std::string message = std::string("An instance of ") + ClassSchemaName<T>()
      + " reached JavaScript before DefineClass<T>() bound it, so the page got undefined. "
        "Call DefineClass<T>() before exposing instances.";
  ulJSAPIEmitDiagnostic(ctx, message.c_str());
}

// Wraps a non-owning view of an instance (identity-preserving). A NULL pointer converts to
// JavaScript `null`.
template <typename T>
ULJSValue WrapView(ULJSContext ctx, T* instance) {
  if (!instance)
    return ulCreateJSValueNull(ctx);
  ULJSClass cls = BoundClass<T>();
  if (!cls) {
    WarnUnboundClass<T>(ctx);
    return nullptr;
  }
  return ulCreateJSObjectWithClass(
      ctx, cls, const_cast<std::remove_cv_t<T>*>(instance), false);
}

// Wraps an owning stake (identity-preserving; a view wrapper upgrades to owned). When the
// wrapper already owns its instance the redundant stake is released for shared holders; a
// second exclusive stake is a caller logic error and is deliberately leaked (destroying it
// would free an instance the live wrapper still owns).
template <typename H>
ULJSValue WrapOwned(ULJSContext ctx, H holder) {
  using Traits = HolderTraits<H>;
  using T = typename Traits::element_type;
  T* instance = Traits::Get(holder);
  if (!instance)
    return ulCreateJSValueNull(ctx);
  ULJSClass cls = BoundClass<T>();
  if (!cls) {
    WarnUnboundClass<T>(ctx);
    return nullptr;
  }
  auto* box = new StakeBox<H> { .holder = std::move(holder) };
  bool taken = false;
  ULJSValue result = ulCreateJSObjectWithClassHolder(
      ctx, cls, const_cast<std::remove_cv_t<T>*>(instance), box, &DestroyStakeBox<H>, &taken);
  if (!taken) {
    if (!result || Traits::kind == HolderKind::Shared)
      delete box;
  }
  return result;
}

// -----------------------------------------------------------------------------------------
// Async instance methods: the instance stays protected (ulJSObjectProtectInstance) until the
// Task finishes or the Resolver settles or is destroyed, so a wrapper collected, severed, or
// detached mid-call cannot free the instance under a suspended Task or a pending Resolver.
// -----------------------------------------------------------------------------------------

class InstanceProtection {
 public:
  explicit InstanceProtection(ULJSProtectedInstance token) : token_(token) {}
  InstanceProtection(const InstanceProtection&) = delete;
  InstanceProtection& operator=(const InstanceProtection&) = delete;
  ~InstanceProtection() { ulJSObjectUnprotectInstance(token_); }

 private:
  ULJSProtectedInstance token_ = nullptr;
};

template <typename Storage>
struct ClassCallFrame : AsyncCallFrame<Storage> {
  InstanceProtection protection;

  ClassCallFrame(ULJSValue borrowed_this, const ULJSValue* borrowed_args, size_t argc,
                 bool keep_this, ULJSClass protect_cls)
      : AsyncCallFrame<Storage>(borrowed_this, borrowed_args, argc, keep_this),
        protection(protect_cls ? ulJSObjectProtectInstance(borrowed_this, protect_cls)
                               : nullptr) {}
};

// A trailing-Resolver method's Resolver owns the protection, so it lasts until the method
// settles or drops that Resolver (on any thread), whatever the page's state.
struct ResolverAccess {
  static void Protect(Resolver& resolver, ULJSProtectedInstance token) {
    ulJSObjectUnprotectInstance(resolver.protected_.exchange(token));
  }
};

// The async half of InvokeClassCallable below: converts, then hands the callable the Resolver
// or starts the Task it returns. A named template rather than a lambda body, since MSVC
// mishandles an enclosing template's local type aliases inside a lambda.
template <typename Fn, typename Invoke, typename Convert>
void StartClassAsync(Invoke& invoke, Convert& convert, ULJSValue this_value,
                     const ULJSValue* args, size_t argc, ULJSClass protect_cls,
                     Resolver& resolver) {
  using Traits = CallableTraits<Fn>;
  using Shape = ParamShape<typename Traits::Params>;
  using R = typename Traits::Return;
  using Storage = typename StorageOf<typename Shape::Converted>::Type;
  if constexpr (Shape::kHasResolver) {
    static_assert(std::is_void_v<R>,
                  "a callable taking a trailing js::Resolver must return void (deliver the "
                  "result through the resolver)");
    Storage storage;
    if (convert(storage)) {
      if (protect_cls)
        ResolverAccess::Protect(resolver, ulJSObjectProtectInstance(this_value, protect_cls));
      invoke(storage, this_value, args, std::move(resolver));
    }
  } else {
    auto frame = std::make_unique<ClassCallFrame<Storage>>(
        this_value, args, argc, Shape::kHasCallInfo || Shape::kHasTrailingCallInfo,
        protect_cls);
    if (!convert(frame->storage))
      return;
    R returned = invoke(frame->storage, frame->this_value, frame->args.data());
    StartBoundTask(std::move(returned), std::move(frame), std::move(resolver));
  }
}

// -----------------------------------------------------------------------------------------
// The shared call driver for class methods and static methods: converts arguments, invokes,
// and marshals the result, with the same sync/async dispatch as a top-level binding (a
// js::Task<T> return or a trailing js::Resolver makes the JavaScript call return a Promise).
// `protect_cls` is the instance's class for an instance method (NULL for a static method).
// -----------------------------------------------------------------------------------------

template <typename Fn>
ULJSValue InvokeClassCallable(Fn& fn, const CallSite& site, ULJSContext ctx,
                              ULJSValue this_value, const ULJSValue* args, size_t argc,
                              ULJSValue* exception, ULJSClass protect_cls) {
  using Traits = CallableTraits<Fn>;
  using Shape = ParamShape<typename Traits::Params>;
  using R = typename Traits::Return;
  using Storage = typename StorageOf<typename Shape::Converted>::Type;
  int diag = static_cast<int>(ulJSAPIGetCallDiagnostics());

  MarshalFailureScope scope;

  auto invoke = [&](Storage& storage, ULJSValue call_this, const ULJSValue* call_args,
                    auto&&... extra) -> decltype(auto) {
    LossyMuteScope mute;
    return [&]<size_t... Is>(std::index_sequence<Is...>) -> decltype(auto) {
      if constexpr (Shape::kHasCallInfo) {
        return fn(CallInfo(ctx, call_this, call_args, argc),
                  AsParam<std::tuple_element_t<Is, typename Shape::Converted>>(
                      std::get<Is>(storage))...,
                  std::forward<decltype(extra)>(extra)...);
      } else if constexpr (Shape::kHasTrailingCallInfo) {
        return fn(AsParam<std::tuple_element_t<Is, typename Shape::Converted>>(
                      std::get<Is>(storage))...,
                  CallInfo(ctx, call_this, call_args, argc),
                  std::forward<decltype(extra)>(extra)...);
      } else {
        return fn(AsParam<std::tuple_element_t<Is, typename Shape::Converted>>(
                      std::get<Is>(storage))...,
                  std::forward<decltype(extra)>(extra)...);
      }
    }(std::make_index_sequence<Shape::kCount> {});
  };

  if constexpr (Shape::kHasResolver || kIsAsyncReturn<R>) {
    ULJSPromiseResolver raw_resolver = nullptr;
    ULJSValue promise = ulCreateJSPromise(ctx, &raw_resolver);
    if (!promise)
      return nullptr;
    Resolver resolver = Resolver::Adopt(raw_resolver);
    if (diag >= kULJSDiagnostics_Strict && !Shape::kHasCallInfo
        && !Shape::kHasTrailingCallInfo && argc > Shape::kCount) {
      ULJSValue error = MakeExtraArgsError(ctx, site.display, Shape::kCount, argc);
      ulJSPromiseResolverReject(resolver.raw(), error);
      ulDestroyJSValue(error);
      return promise;
    }
    auto convert = [&](Storage& storage) {
      ULJSValue reject_reason = nullptr;
      if (ConvertArgs<typename Shape::Converted>(ctx, args, argc, site, storage,
                                                 &reject_reason, diag,
                                                 std::make_index_sequence<Shape::kCount> {}))
        return true;
      // The caller already holds a Promise: a conversion failure rejects it with the same
      // error a sync binding would have thrown.
      ulJSPromiseResolverReject(resolver.raw(), reject_reason);
      ulDestroyJSValue(reject_reason);
      return false;
    };
    // The caller gets the Promise even when the callable throws: the error rejects it (once the
    // callable took the Resolver, the unwind destroying it rejects it instead).
    ultralight::detail::CallCatchingExceptions(
        [&] {
          StartClassAsync<Fn>(invoke, convert, this_value, args, argc, protect_cls, resolver);
        },
        [&](const char* what) {
          Error error = NativeExceptionError(site.path, what);
          if (resolver)
            resolver.Reject(std::move(error));
        });
    return promise;
  } else {
    if (diag >= kULJSDiagnostics_Strict && !Shape::kHasCallInfo
        && !Shape::kHasTrailingCallInfo && argc > Shape::kCount) {
      if (exception)
        *exception = MakeExtraArgsError(ctx, site.display, Shape::kCount, argc);
      return nullptr;
    }
    Storage storage;
    if (!ConvertArgs<typename Shape::Converted>(ctx, args, argc, site, storage, exception,
                                                diag,
                                                std::make_index_sequence<Shape::kCount> {}))
      return nullptr;
    if constexpr (std::is_void_v<R>) {
      invoke(storage, this_value, args);
      return nullptr;
    } else if constexpr (IsResultType<R>::value) {
      using T = typename IsResultType<R>::value_type;
      R result = invoke(storage, this_value, args);
      if (!result) {
        if (exception)
          *exception = std::move(result).error().ToJS(ctx);
        return nullptr;
      }
      if constexpr (std::is_void_v<T>)
        return nullptr;
      else
        return MarshalReturn(ctx, site.path, diag, [&] {
          return TypeTraits<std::remove_cvref_t<T>>::ToJS(ctx, result.value());
        });
    } else {
      CheckMarshalableReturn<R>();
      R result = invoke(storage, this_value, args);
      return MarshalReturn(ctx, site.path, diag, [&] {
        return TypeTraits<std::remove_cvref_t<R>>::ToJS(ctx, std::move(result));
      });
    }
  }
}

template <typename T, typename M>
struct BoundClassMethod {
  M method;
  std::string path;
  std::string display;              // path + declared signature (mis-call error prefix).
  std::vector<std::string> names;   // Declared payload-parameter names (empty when none).

  CallSite Site() const { return CallSite(path, display, &names); }
};

template <typename T, typename M>
ULJSValue ClassMethodTrampoline(void* user_data, ULJSContext ctx, void* instance,
                                ULJSValue this_value, const ULJSValue* args, size_t argc,
                                ULJSValue* exception) {
  auto* bound = static_cast<BoundClassMethod<T, M>*>(user_data);
  auto callable
      = MakeBoundMethod(static_cast<T*>(instance), bound->method, MemberFnTraits<M> {});
  return CallGuarded<ULJSValue>(ctx, bound->path.c_str(), exception, nullptr, [&] {
    return InvokeClassCallable(callable, bound->Site(), ctx, this_value, args, argc, exception,
                               BoundClass<T>());
  });
}

template <typename F>
ULJSValue ClassStaticTrampoline(void* user_data, ULJSContext ctx, ULJSValue this_value,
                                const ULJSValue* args, size_t argc, ULJSValue* exception) {
  auto* bound = static_cast<BoundCallable<F>*>(user_data);
  return CallGuarded<ULJSValue>(ctx, bound->path.c_str(), exception, nullptr, [&] {
    return InvokeClassCallable(bound->fn, bound->Site(), ctx, this_value, args, argc,
                               exception, nullptr);
  });
}

// -----------------------------------------------------------------------------------------
// Constructor: converts the declared argument types, constructs into the canonical holder,
// and hands the stake to the waist through the holder channel.
// -----------------------------------------------------------------------------------------

template <typename T, typename H, typename... Args>
H MakeCanonicalHolder(Args&&... args) {
  if constexpr (std::is_same_v<H, RefPtr<T>>) {
    return MakeRefPtr<T>(std::forward<Args>(args)...);
  } else if constexpr (std::is_same_v<H, std::unique_ptr<T>>) {
    return std::make_unique<T>(std::forward<Args>(args)...);
  } else if constexpr (std::is_same_v<H, std::shared_ptr<T>>) {
    return std::make_shared<T>(std::forward<Args>(args)...);
  } else {
    static_assert(kAlwaysFalse<H>,
                  "Constructor<...> can construct into the built-in canonical holders only "
                  "(RefPtr<T>, std::unique_ptr<T>, std::shared_ptr<T>). For a custom holder "
                  "or js::Eternal, bind a StaticMethod factory that returns the holder");
  }
}

template <typename T, typename... Args>
struct ClassCtor {
  std::string path;

  static void* Invoke(void* user_data, ULJSContext ctx, const ULJSValue* args, size_t argc,
                      void** out_holder, ULUserDataDestroyCallback* out_destroy_holder,
                      ULJSValue* exception) {
    return CallGuarded<void*>(
        ctx, static_cast<ClassCtor*>(user_data)->path.c_str(), exception, nullptr, [&] {
          return InvokeBody(user_data, ctx, args, argc, out_holder, out_destroy_holder,
                            exception);
        });
  }

  static void* InvokeBody(void* user_data, ULJSContext ctx, const ULJSValue* args, size_t argc,
                          void** out_holder, ULUserDataDestroyCallback* out_destroy_holder,
                          ULJSValue* exception) {
    auto* bound = static_cast<ClassCtor*>(user_data);
    using H = typename ClassHolder<std::remove_cv_t<T>, void>::type;
    using Converted = std::tuple<Args...>;
    int diag = static_cast<int>(ulJSAPIGetCallDiagnostics());

    CallSite site(bound->path);
    if (diag >= kULJSDiagnostics_Strict && argc > sizeof...(Args)) {
      if (exception)
        *exception = MakeExtraArgsError(ctx, site.display, sizeof...(Args), argc);
      return nullptr;
    }

    MarshalFailureScope scope;
    typename StorageOf<Converted>::Type storage;
    if (!ConvertArgs<Converted>(ctx, args, argc, site, storage, exception, diag,
                                std::index_sequence_for<Args...> {}))
      return nullptr;

    H holder = [&]<size_t... Is>(std::index_sequence<Is...>) {
      LossyMuteScope mute;
      return MakeCanonicalHolder<std::remove_cv_t<T>, H>(
          AsParam<Args>(std::get<Is>(storage))...);
    }(std::index_sequence_for<Args...> {});

    T* instance = HolderTraits<H>::Get(holder);
    auto* box = new StakeBox<H> { .holder = std::move(holder) };
    *out_holder = box;
    *out_destroy_holder = &DestroyStakeBox<H>;
    return const_cast<std::remove_cv_t<T>*>(instance);
  }
};

// -----------------------------------------------------------------------------------------
// Fields and accessor properties.
// -----------------------------------------------------------------------------------------

template <typename T, typename F>
struct BoundField {
  F T::* member;
  std::string path;
};

template <typename T, typename F>
ULJSValue FieldGetterTrampoline(void* user_data, ULJSContext ctx, void* instance,
                                ULJSValue* exception) {
  auto* bound = static_cast<BoundField<T, F>*>(user_data);
  return CallGuarded<ULJSValue>(ctx, bound->path.c_str(), exception, nullptr, [&] {
    return MarshalReturn(ctx, bound->path.c_str(),
                         static_cast<int>(ulJSAPIGetCallDiagnostics()), [&] {
                           return TypeTraits<std::remove_cv_t<F>>::ToJS(
                               ctx, static_cast<T*>(instance)->*(bound->member));
                         });
  });
}

template <typename T, typename F>
bool FieldSetterTrampoline(void* user_data, ULJSContext ctx, void* instance, ULJSValue value,
                           ULJSValue* exception) {
  auto* bound = static_cast<BoundField<T, F>*>(user_data);
  return CallGuarded<bool>(ctx, bound->path.c_str(), exception, false, [&] {
    MarshalFailureScope scope;
    ParamStorage<F> storage {};
    ULJSValue args[1] = { value };
    // Argument number 0: a property assignment's messages carry no argument clause.
    if (!ConvertArg<F>(ctx, args, 1, 0, 0, CallSite(bound->path), storage, exception,
                       static_cast<int>(ulJSAPIGetCallDiagnostics())))
      return false;
    static_cast<T*>(instance)->*(bound->member) = AsParam<F>(storage);
    return true;
  });
}

template <typename T, typename GM, typename SM>
struct BoundAccessor {
  GM getter;
  SM setter;
  std::string path;
};

template <typename T, typename Bound>
ULJSValue AccessorGetterBody(void* user_data, ULJSContext ctx, void* instance,
                             ULJSValue* exception) {
  auto* bound = static_cast<Bound*>(user_data);
  T* self = static_cast<T*>(instance);
  using R = typename MemberFnTraits<decltype(bound->getter)>::Return;
  int diag = static_cast<int>(ulJSAPIGetCallDiagnostics());
  if constexpr (IsResultType<R>::value) {
    using V = typename IsResultType<R>::value_type;
    R result = [&]() -> R {
      LossyMuteScope mute;
      return (self->*(bound->getter))();
    }();
    if (!result) {
      if (exception)
        *exception = std::move(result).error().ToJS(ctx);
      return nullptr;
    }
    return MarshalReturn(ctx, bound->path.c_str(), diag, [&] {
      return TypeTraits<std::remove_cvref_t<V>>::ToJS(ctx, result.value());
    });
  } else {
    R result = [&]() -> R {
      LossyMuteScope mute;
      return (self->*(bound->getter))();
    }();
    return MarshalReturn(ctx, bound->path.c_str(), diag, [&] {
      return TypeTraits<std::remove_cvref_t<R>>::ToJS(ctx, std::move(result));
    });
  }
}

template <typename T, typename Bound>
ULJSValue AccessorGetterTrampoline(void* user_data, ULJSContext ctx, void* instance,
                                   ULJSValue* exception) {
  return CallGuarded<ULJSValue>(
      ctx, static_cast<Bound*>(user_data)->path.c_str(), exception, nullptr,
      [&] { return AccessorGetterBody<T, Bound>(user_data, ctx, instance, exception); });
}

template <typename T, typename Bound>
bool AccessorSetterBody(void* user_data, ULJSContext ctx, void* instance, ULJSValue value,
                        ULJSValue* exception) {
  auto* bound = static_cast<Bound*>(user_data);
  using Params = typename MemberFnTraits<decltype(bound->setter)>::Params;
  using P = std::tuple_element_t<0, Params>;
  using R = typename MemberFnTraits<decltype(bound->setter)>::Return;
  static_assert(IsSetterReturn<R>(),
                "a property setter returns void, or js::Result<void> to refuse the "
                "assignment (its error is thrown into the page)");
  MarshalFailureScope scope;
  ParamStorage<P> storage {};
  ULJSValue args[1] = { value };
  // Argument number 0: a property assignment's messages carry no argument clause.
  if (!ConvertArg<P>(ctx, args, 1, 0, 0, CallSite(bound->path), storage, exception,
                     static_cast<int>(ulJSAPIGetCallDiagnostics())))
    return false;
  LossyMuteScope mute;
  if constexpr (IsResultType<R>::value) {
    R result = (static_cast<T*>(instance)->*(bound->setter))(AsParam<P>(storage));
    if (!result) {
      if (exception)
        *exception = std::move(result).error().ToJS(ctx);
      return false;
    }
  } else {
    (static_cast<T*>(instance)->*(bound->setter))(AsParam<P>(storage));
  }
  return true;
}

template <typename T, typename Bound>
bool AccessorSetterTrampoline(void* user_data, ULJSContext ctx, void* instance, ULJSValue value,
                              ULJSValue* exception) {
  return CallGuarded<bool>(
      ctx, static_cast<Bound*>(user_data)->path.c_str(), exception, false,
      [&] { return AccessorSetterBody<T, Bound>(user_data, ctx, instance, value, exception); });
}

}  // namespace detail
}  // namespace js
}  // namespace ultralight
/// \endcond
