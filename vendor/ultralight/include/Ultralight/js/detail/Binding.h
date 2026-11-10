///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// Implementation detail of `<Ultralight/js/API.h>`; include that header instead.
/// The typed binding machinery: callable introspection, parameter conversion, and the
/// trampolines minted per bound signature (compiled into the embedder's binary; the
/// callable lives in a heap holder passed as user_data, and no STL type ever crosses the
/// DLL boundary).
///
/// \cond INTERNAL
#pragma once
#include <Ultralight/CAPI/CAPI_JSAPI.h>
#include <Ultralight/CAPI/CAPI_JSValue.h>
#include <Ultralight/RefPtr.h>
#include <Ultralight/detail/Exceptions.h>
#include <Ultralight/js/CallInfo.h>
#include <Ultralight/js/Error.h>
#include <Ultralight/js/Holder.h>
#include <Ultralight/js/Resolver.h>
#include <Ultralight/js/Value.h>
#include <Ultralight/js/detail/Coroutine.h>
#include <Ultralight/js/detail/Marshal.h>

#include <atomic>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace ultralight {
namespace js {

template <typename T>
class Task;

namespace detail {

// ---------------------------------------------------------------------------------------
// Callable introspection: the return type and parameter list of a bound callable, taken
// from its (single, non-template) call signature.
// ---------------------------------------------------------------------------------------

template <typename R, typename... A>
struct Signature {
  using Return = R;
  using Params = std::tuple<A...>;
};

template <typename M>
struct MemberFnTraits;
template <typename C, typename R, typename... A>
struct MemberFnTraits<R (C::*)(A...)> : Signature<R, A...> {
  using Class = C;
};
template <typename C, typename R, typename... A>
struct MemberFnTraits<R (C::*)(A...) const> : Signature<R, A...> {
  using Class = C;
};
template <typename C, typename R, typename... A>
struct MemberFnTraits<R (C::*)(A...) noexcept> : Signature<R, A...> {
  using Class = C;
};
template <typename C, typename R, typename... A>
struct MemberFnTraits<R (C::*)(A...) const noexcept> : Signature<R, A...> {
  using Class = C;
};

template <typename F, typename = void>
struct CallableTraitsImpl;
template <typename R, typename... A>
struct CallableTraitsImpl<R (*)(A...)> : Signature<R, A...> {};
template <typename R, typename... A>
struct CallableTraitsImpl<R (*)(A...) noexcept> : Signature<R, A...> {};
template <typename F>
struct CallableTraitsImpl<F, std::void_t<decltype(&F::operator())>>
    : MemberFnTraits<decltype(&F::operator())> {};

template <typename F>
using CallableTraits = CallableTraitsImpl<std::decay_t<F>>;

template <typename F, typename = void>
struct IsIntrospectableCallable : std::false_type {};
template <typename F>
struct IsIntrospectableCallable<F, std::void_t<typename CallableTraits<F>::Return>>
    : std::true_type {};

template <typename T>
struct IsTaskType : std::false_type {};
template <typename T>
struct IsTaskType<Task<T>> : std::true_type {};

template <typename R>
inline constexpr bool kIsAsyncReturn = [] {
  if constexpr (IsTaskType<R>::value) {
    return true;
  } else if constexpr (IsResultType<R>::value) {
    return IsTaskType<typename IsResultType<R>::value_type>::value;
  } else {
    return false;
  }
}();

// ---------------------------------------------------------------------------------------
// Parameter-list shape: [CallInfo,] converted..., [Resolver].
// ---------------------------------------------------------------------------------------

template <typename Params, size_t Offset, size_t... Is>
auto SubTupleImpl(std::index_sequence<Is...>)
    -> std::tuple<std::tuple_element_t<Offset + Is, Params>...>;

template <typename Params, size_t Offset, size_t Count>
using SubTuple = decltype(SubTupleImpl<Params, Offset>(std::make_index_sequence<Count> {}));

template <typename Params>
struct ParamShape {
  static constexpr size_t kTotal = std::tuple_size_v<Params>;

  static constexpr bool kHasCallInfo = [] {
    if constexpr (kTotal > 0)
      return std::is_same_v<std::remove_cvref_t<std::tuple_element_t<0, Params>>, CallInfo>;
    else
      return false;
  }();

  static constexpr bool kHasResolver = [] {
    if constexpr (kTotal > 0)
      return std::is_same_v<std::remove_cvref_t<std::tuple_element_t<kTotal - 1, Params>>,
                            Resolver>;
    else
      return false;
  }();

  // CallInfo may also trail the payload (before a trailing Resolver), matching
  // dd::ActionInfo's position; a lone CallInfo parameter counts as the leading form.
  static constexpr bool kHasTrailingCallInfo = [] {
    constexpr size_t kEnd = kTotal - (kHasResolver ? size_t { 1 } : size_t { 0 });
    if constexpr (kEnd > (kHasCallInfo ? size_t { 1 } : size_t { 0 }))
      return std::is_same_v<std::remove_cvref_t<std::tuple_element_t<kEnd - 1, Params>>,
                            CallInfo>;
    else
      return false;
  }();
  static_assert(!(kHasCallInfo && kHasTrailingCallInfo),
                "declare js::CallInfo once: leading or trailing, not both");

  static constexpr size_t kFirst = kHasCallInfo ? 1 : 0;
  static constexpr size_t kCount
      = kTotal - kFirst - (kHasResolver ? 1 : 0) - (kHasTrailingCallInfo ? 1 : 0);

  using Converted = SubTuple<Params, kFirst, kCount>;
};

// Parameters convert into local storage; std::string_view views a call-scoped std::string.
template <typename P>
using ParamStorage
    = std::conditional_t<std::is_same_v<std::remove_cvref_t<P>, std::string_view>,
                         std::string, std::remove_cvref_t<P>>;

template <typename Converted>
struct StorageOf;
template <typename... Ps>
struct StorageOf<std::tuple<Ps...>> {
  using Type = std::tuple<ParamStorage<Ps>...>;
};

template <typename P, typename S>
decltype(auto) AsParam(S& storage) {
  if constexpr (std::is_same_v<std::remove_cvref_t<P>, std::string_view>)
    return std::string_view(storage);
  else
    return std::move(storage);
}

template <typename P>
constexpr void ValidateParam() {
  using U = std::remove_cvref_t<P>;
  static_assert(!std::is_same_v<U, CallInfo>,
                "CallInfo may only appear leading (the first parameter) or trailing "
                "(after the payload parameters, before a trailing js::Resolver)");
  static_assert(!std::is_same_v<U, Resolver>,
                "js::Resolver may only appear as the last parameter of a bound callable");
  static_assert(!std::is_same_v<U, const char*> && !std::is_same_v<U, char*>,
                "declare string parameters as std::string (owning) or std::string_view "
                "(call-scoped view); a char pointer cannot own converted text");
  static_assert(!(std::is_lvalue_reference_v<P> && !std::is_const_v<std::remove_reference_t<P>>),
                "bound-callable parameters must be by value or const reference (arguments "
                "are call-scoped conversions; there is nothing durable to mutate)");
  AssertInteropHeader<U>();
  static_assert(Marshalable<U>,
                "a parameter type has no js::TypeTraits: use a supported built-in, make it "
                "a plain aggregate struct, or specialize js::TypeTraits for it");
}

// ---------------------------------------------------------------------------------------
// Diagnostics helpers: lossy-conversion warnings (Warn) and the Strict promotions. The
// governing level comes from ulJSAPIGetCallDiagnostics(), installed by the waist around
// every binding dispatch; messages emit through ulJSAPIEmitDiagnostic() so both sinks and
// the call-site capture match every other bridge diagnostic.
// ---------------------------------------------------------------------------------------

inline std::string LossyMessagePrefix(const char* path, size_t arg_number) {
  std::string message(path);
  if (arg_number) {
    message += ": argument ";
    message += std::to_string(arg_number);
  }
  return message;
}

inline void EmitLossyWarning(ULJSContext ctx, const char* path, size_t arg_number,
                             const char* detail) {
  std::string message = LossyMessagePrefix(path, arg_number);
  message += ": lossy conversion (";
  message += detail;
  message += ").";
  ulJSAPIEmitDiagnostic(ctx, message.c_str());
}

inline ULJSValue MakeLossyError(ULJSContext ctx, const char* path, size_t arg_number,
                                const char* detail) {
  std::string message = LossyMessagePrefix(path, arg_number);
  message += ": lossy conversion refused under Strict diagnostics (";
  message += detail;
  message += ')';
  return Error::TypeError(std::move(message)).WithCode("ULJS_LOSSY").ToJS(ctx);
}

inline ULJSValue MakeExtraArgsError(ULJSContext ctx, const char* path, size_t expected,
                                    size_t got) {
  std::string message(path);
  message += ": expected at most ";
  message += std::to_string(expected);
  message += expected == 1 ? " argument, got " : " arguments, got ";
  message += std::to_string(got);
  message += " (extra arguments are refused under Strict diagnostics)";
  return Error::TypeError(std::move(message)).WithCode("ULJS_EXTRA_ARGS").ToJS(ctx);
}

// A return type the marshaler doesn't take although it holds a class instance: a holder other
// than the class's canonical one (js::ClassHolder), such as std::shared_ptr<T> for a class
// whose canonical holder is std::unique_ptr<T>, or a std::weak_ptr<T>.
template <typename R>
concept NonCanonicalHolderReturn
    = !Marshalable<R>
      && requires { typename HolderTraits<std::remove_cvref_t<R>>::element_type; };

// The compile errors for a plain (not Result or Task) return type the marshaler can't take.
template <typename R>
constexpr void CheckMarshalableReturn() {
  AssertInteropHeader<std::remove_cvref_t<R>>();
  static_assert(!NonCanonicalHolderReturn<R>,
                "a bound callable returns a class instance in a holder that isn't the class's "
                "canonical holder: return js::ClassHolder<T>::type (RefPtr<T> for a RefCounted "
                "class, otherwise std::unique_ptr<T>), or specialize js::ClassHolder<T> to make "
                "this holder canonical");
  static_assert(Marshalable<R> || NonCanonicalHolderReturn<R>,
                "a bound callable's return type has no js::TypeTraits: return a supported "
                "type, js::Result<T>, or js::Task<T>");
}

// Runs a synchronous trampoline's body and returns its result; a C++ exception escaping it
// returns `failed` and throws NativeExceptionError() into the page instead.
template <typename R, typename Body>
R CallGuarded(ULJSContext ctx, const char* path, ULJSValue* exception, R failed, Body&& body) {
  R result = failed;
  ultralight::detail::CallCatchingExceptions([&] { result = body(); },
                                             [&](const char* what) {
                                               result = failed;
                                               Error error = NativeExceptionError(path, what);
                                               if (exception)
                                                 *exception = error.ToJS(ctx);
                                             });
  return result;
}

// Marshals a bound callable's return value, reporting a lossy numeric conversion (a 64-bit
// integer that cannot round-trip through a JavaScript number) through the warning sink.
// Never promotes, even under Strict: a lossy return is a native-side type choice the page
// caller cannot fix.
template <typename F>
ULJSValue MarshalReturn(ULJSContext ctx, const char* path, int diag, F&& to_js) {
  if (diag < kULJSDiagnostics_Warn)
    return to_js();
  LossyConversionScope lossy(true);
  ULJSValue result = to_js();
  if (lossy.frame.active) {
    std::string message(path);
    message += ": return value: lossy conversion (";
    message += lossy.frame.detail;
    message += ").";
    ulJSAPIEmitDiagnostic(ctx, message.c_str());
  }
  return result;
}

// ---------------------------------------------------------------------------------------
// Argument conversion. On failure, *exception receives either the exception script threw
// during conversion or a path-qualified TypeError built through the waist so the message
// shape matches every other language layer. `arg_number` is 1-based; 0 means the property
// form (assignment messages carry no argument clause).
// ---------------------------------------------------------------------------------------

// The message identity of one dispatch site. Lossy diagnostics carry the plain dot-path;
// mis-call TypeErrors carry `display` (the path with its declared signature appended) and
// name the failing parameter when the registration declared names.
struct CallSite {
  const char* path = "";
  const char* display = "";
  const std::vector<std::string>* names = nullptr;

  explicit CallSite(const std::string& p) : path(p.c_str()), display(p.c_str()) {}
  CallSite(const std::string& p, const std::string& d, const std::vector<std::string>* n)
      : path(p.c_str()), display(d.empty() ? p.c_str() : d.c_str()), names(n) {}
};

template <typename P>
bool ConvertArg(ULJSContext ctx, const ULJSValue* args, size_t argc, size_t index,
                size_t arg_number, const CallSite& site, ParamStorage<P>& out,
                ULJSValue* exception, int diag) {
  using U = std::remove_cvref_t<P>;
  using Stored = ParamStorage<P>;
  ULJSValue borrowed = index < argc ? args[index] : nullptr;
  MarshalFailure* failure = g_marshal_failure;
  if (!borrowed) {
    // A missing argument is `undefined`: absent optionals succeed, everything else fails.
    if constexpr (IsOptional<Stored>::value) {
      out.reset();
      return true;
    }
    if (failure && !failure->active) {
      failure->active = true;
      failure->expected = TypeTraits<Stored>::SchemaType();
      failure->got = nullptr;
    }
  } else {
    LossyConversionScope lossy(diag >= kULJSDiagnostics_Warn);
    bool converted;
    if constexpr (std::is_same_v<U, std::string_view>)
      converted = TypeTraits<std::string>::FromJS(ctx, borrowed, &out, exception);
    else
      converted = TypeTraits<Stored>::FromJS(ctx, borrowed, &out, exception);
    if (converted) {
      if (lossy.installed && lossy.frame.active) {
        if (diag >= kULJSDiagnostics_Strict) {
          if (exception)
            *exception = MakeLossyError(ctx, site.path, arg_number, lossy.frame.detail);
          return false;
        }
        EmitLossyWarning(ctx, site.path, arg_number, lossy.frame.detail);
      }
      return true;
    }
    if (exception && *exception)
      return false;  // Script threw during conversion; propagate its exception unchanged.
    if (failure && !failure->active) {
      failure->active = true;
      failure->expected = std::is_same_v<U, std::string_view>
          ? TypeTraits<std::string>::SchemaType()
          : TypeTraits<Stored>::SchemaType();
      failure->got = borrowed;
    }
  }
  if (failure && exception) {
    // The argument clause names the failing parameter when the registration declared
    // names; a nested aggregate-field path from the marshaler takes precedence.
    const char* clause = failure->path_or_null();
    if (!clause && site.names && index < site.names->size())
      clause = (*site.names)[index].c_str();
    *exception = ulCreateJSMarshalError(ctx, site.display, arg_number, clause,
                                        failure->expected, failure->got);
  }
  return false;
}

template <typename Converted, typename Storage, size_t... Is>
bool ConvertArgs(ULJSContext ctx, const ULJSValue* args, size_t argc, const CallSite& site,
                 Storage& storage, ULJSValue* exception, int diag,
                 std::index_sequence<Is...>) {
  return (ConvertArg<std::tuple_element_t<Is, Converted>>(ctx, args, argc, Is, Is + 1, site,
                                                          std::get<Is>(storage), exception,
                                                          diag)
          && ...);
}

// ---------------------------------------------------------------------------------------
// The minted trampolines.
// ---------------------------------------------------------------------------------------

template <typename Fn>
struct BoundCallable {
  Fn fn;
  std::string path;
  std::string display;              // path + declared signature (mis-call error prefix).
  std::vector<std::string> names;   // Declared payload-parameter names (empty when none).
  // The registration's reference plus one per Task the callable started: a lambda coroutine
  // reads its captures through the callable, so the callable must outlive every such Task.
  std::atomic<uint32_t> refs { 1 };

  CallSite Site() const { return CallSite(path, display, &names); }
};

template <typename Fn>
void DestroyBoundCallable(void* user_data) {
  auto* bound = static_cast<BoundCallable<Fn>*>(user_data);
  if (bound->refs.fetch_sub(1, std::memory_order_acq_rel) == 1)
    delete bound;
}

// A started Task's reference to the callable that returned it.
template <typename Fn>
struct BoundCallableRef : TaskKeepAlive {
  explicit BoundCallableRef(BoundCallable<Fn>* b) : bound(b) {
    bound->refs.fetch_add(1, std::memory_order_relaxed);
  }
  ~BoundCallableRef() override { DestroyBoundCallable<Fn>(bound); }
  BoundCallableRef(const BoundCallableRef&) = delete;
  BoundCallableRef& operator=(const BoundCallableRef&) = delete;

  BoundCallable<Fn>* bound;
};

template <typename Fn, typename Extra, typename... ExtraArgs>
decltype(auto) InvokeBound(BoundCallable<Fn>* bound, ULJSContext ctx, ULJSValue this_value,
                           const ULJSValue* args, size_t argc,
                           typename StorageOf<typename ParamShape<
                               typename CallableTraits<Fn>::Params>::Converted>::Type& storage,
                           ExtraArgs&&... extra) {
  using Shape = ParamShape<typename CallableTraits<Fn>::Params>;
  LossyMuteScope mute;
  return [&]<size_t... Is>(std::index_sequence<Is...>) -> decltype(auto) {
    if constexpr (Shape::kHasCallInfo) {
      return bound->fn(CallInfo(ctx, this_value, args, argc),
                       AsParam<std::tuple_element_t<Is, typename Shape::Converted>>(
                           std::get<Is>(storage))...,
                       std::forward<ExtraArgs>(extra)...);
    } else if constexpr (Shape::kHasTrailingCallInfo) {
      // Combined trailing order is (payload..., CallInfo, Resolver): the Resolver rides
      // in as the trailing extra argument.
      return bound->fn(AsParam<std::tuple_element_t<Is, typename Shape::Converted>>(
                           std::get<Is>(storage))...,
                       CallInfo(ctx, this_value, args, argc),
                       std::forward<ExtraArgs>(extra)...);
    } else {
      return bound->fn(AsParam<std::tuple_element_t<Is, typename Shape::Converted>>(
                           std::get<Is>(storage))...,
                       std::forward<ExtraArgs>(extra)...);
    }
  }(std::make_index_sequence<Shape::kCount> {});
}

template <typename Fn>
ULJSValue SyncTrampolineBody(void* user_data, ULJSContext ctx, ULJSValue this_value,
                             const ULJSValue* args, size_t argc, ULJSValue* exception) {
  auto* bound = static_cast<BoundCallable<Fn>*>(user_data);
  using Traits = CallableTraits<Fn>;
  using Shape = ParamShape<typename Traits::Params>;
  using R = typename Traits::Return;
  int diag = static_cast<int>(ulJSAPIGetCallDiagnostics());

  // Strict refuses arguments beyond the declared arity; a CallInfo callable receives the
  // raw argument list, so its extras are observable rather than silently unknown.
  CallSite site = bound->Site();
  if (diag >= kULJSDiagnostics_Strict && !Shape::kHasCallInfo && !Shape::kHasTrailingCallInfo
      && argc > Shape::kCount) {
    if (exception)
      *exception = MakeExtraArgsError(ctx, site.display, Shape::kCount, argc);
    return nullptr;
  }

  MarshalFailureScope scope;
  typename StorageOf<typename Shape::Converted>::Type storage;
  if (!ConvertArgs<typename Shape::Converted>(ctx, args, argc, site, storage, exception,
                                              diag,
                                              std::make_index_sequence<Shape::kCount> {}))
    return nullptr;

  if constexpr (std::is_void_v<R>) {
    InvokeBound<Fn, void>(bound, ctx, this_value, args, argc, storage);
    return nullptr;
  } else if constexpr (IsResultType<R>::value) {
    using T = typename IsResultType<R>::value_type;
    R result = InvokeBound<Fn, void>(bound, ctx, this_value, args, argc, storage);
    if (!result) {
      if (exception)
        *exception = std::move(result).error().ToJS(ctx);
      return nullptr;
    }
    if constexpr (std::is_void_v<T>)
      return nullptr;
    else
      return MarshalReturn(ctx, bound->path.c_str(), diag, [&] {
        return TypeTraits<std::remove_cvref_t<T>>::ToJS(ctx, result.value());
      });
  } else {
    CheckMarshalableReturn<R>();
    R result = InvokeBound<Fn, void>(bound, ctx, this_value, args, argc, storage);
    return MarshalReturn(ctx, bound->path.c_str(), diag, [&] {
      return TypeTraits<std::remove_cvref_t<R>>::ToJS(ctx, std::move(result));
    });
  }
}

template <typename Fn>
ULJSValue SyncTrampoline(void* user_data, ULJSContext ctx, ULJSValue this_value,
                         const ULJSValue* args, size_t argc, ULJSValue* exception) {
  return CallGuarded<ULJSValue>(
      ctx, static_cast<BoundCallable<Fn>*>(user_data)->path.c_str(), exception, nullptr,
      [&] { return SyncTrampolineBody<Fn>(user_data, ctx, this_value, args, argc, exception); });
}

// A Task binding's call state, attached to the Task so it lives as long as the coroutine: the
// converted arguments its reference and view parameters refer to, and owned copies of the
// argument handles (they keep a std::span parameter's bytes alive and back a js::CallInfo
// parameter). `this` is copied only for a js::CallInfo, so a class method's wrapper can still
// be collected while its Task runs. A sync call keeps the same state on the stack instead.
template <typename Storage>
struct AsyncCallFrame : TaskKeepAlive {
  Storage storage;
  ULJSValue this_value = nullptr;
  std::vector<ULJSValue> args;

  AsyncCallFrame(ULJSValue borrowed_this, const ULJSValue* borrowed_args, size_t argc,
                 bool keep_this)
      : this_value(keep_this ? ulCreateJSValueRef(borrowed_this) : nullptr), args(argc) {
    for (size_t i = 0; i < argc; i++)
      args[i] = ulCreateJSValueRef(borrowed_args[i]);
  }
  ~AsyncCallFrame() override {
    ulDestroyJSValue(this_value);
    for (ULJSValue arg : args)
      ulDestroyJSValue(arg);
  }
};

// Starts a Task a bound callable returned, or rejects with the error of a js::Result<Task<T>>
// (the js::Bind holder forms report a gone instance that way). `frame` lives as long as the
// Task. `path` (the bound path, or null) must stay valid while the Task runs.
template <typename R>
void StartBoundTask(R returned, std::unique_ptr<TaskKeepAlive> frame, Resolver resolver,
                    const char* path = nullptr) {
  if constexpr (IsTaskType<R>::value) {
    TaskAccess::KeepAlive(returned, std::move(frame));
    TaskAccess::SetCallPath(returned, path);
    LossyMuteScope mute;  // The coroutine body runs (until its first suspend) inside Start.
    std::move(returned).Start(std::move(resolver));
  } else {
    static_assert(IsTaskType<typename IsResultType<R>::value_type>::value);
    if (!returned) {
      resolver.Reject(std::move(returned).error());
      return;
    }
    StartBoundTask(std::move(returned).value(), std::move(frame), std::move(resolver), path);
  }
}

// `resolver` is moved out once the callable takes it (a trailing Resolver, or a started Task);
// until then the caller still owns it.
template <typename Fn>
void AsyncTrampolineBody(void* user_data, ULJSContext ctx, ULJSValue this_value,
                         const ULJSValue* args, size_t argc, Resolver& resolver) {
  auto* bound = static_cast<BoundCallable<Fn>*>(user_data);
  using Traits = CallableTraits<Fn>;
  using Shape = ParamShape<typename Traits::Params>;
  using R = typename Traits::Return;
  using Storage = typename StorageOf<typename Shape::Converted>::Type;
  int diag = static_cast<int>(ulJSAPIGetCallDiagnostics());

  CallSite site = bound->Site();
  if (diag >= kULJSDiagnostics_Strict && !Shape::kHasCallInfo && !Shape::kHasTrailingCallInfo
      && argc > Shape::kCount) {
    ULJSValue error = MakeExtraArgsError(ctx, site.display, Shape::kCount, argc);
    ulJSPromiseResolverReject(resolver.raw(), error);
    ulDestroyJSValue(error);
    return;
  }

  MarshalFailureScope scope;
  auto convert = [&](Storage& storage) {
    ULJSValue exception = nullptr;
    if (ConvertArgs<typename Shape::Converted>(ctx, args, argc, site, storage, &exception, diag,
                                               std::make_index_sequence<Shape::kCount> {}))
      return true;
    // An async call cannot throw synchronously (the caller already holds a Promise); a
    // conversion failure rejects with the same error a sync binding would have thrown.
    ulJSPromiseResolverReject(resolver.raw(), exception);
    ulDestroyJSValue(exception);
    return false;
  };

  if constexpr (Shape::kHasResolver) {
    static_assert(std::is_void_v<R>,
                  "a callable taking a trailing js::Resolver must return void (deliver the "
                  "result through the resolver)");
    Storage storage;
    if (convert(storage))
      InvokeBound<Fn, void>(bound, ctx, this_value, args, argc, storage, std::move(resolver));
  } else {
    auto frame = std::make_unique<AsyncCallFrame<Storage>>(
        this_value, args, argc, Shape::kHasCallInfo || Shape::kHasTrailingCallInfo);
    if (!convert(frame->storage))
      return;
    // Destroyed after the argument storage (it sits behind the frame in the chain).
    frame->next = std::make_unique<BoundCallableRef<Fn>>(bound);
    R returned = InvokeBound<Fn, void>(bound, ctx, frame->this_value, frame->args.data(), argc,
                                       frame->storage);
    StartBoundTask(std::move(returned), std::move(frame), std::move(resolver),
                   bound->path.c_str());
  }
}

template <typename Fn>
void AsyncTrampoline(void* user_data, ULJSContext ctx, ULJSValue this_value,
                     const ULJSValue* args, size_t argc, ULJSPromiseResolver raw_resolver) {
  Resolver resolver = Resolver::Adopt(raw_resolver);
  ultralight::detail::CallCatchingExceptions(
      [&] { AsyncTrampolineBody<Fn>(user_data, ctx, this_value, args, argc, resolver); },
      [&](const char* what) {
        Error error = NativeExceptionError(
            static_cast<BoundCallable<Fn>*>(user_data)->path.c_str(), what);
        // A Resolver the callable already took rejects when the unwind destroys it.
        if (resolver)
          resolver.Reject(std::move(error));
      });
}

// ---------------------------------------------------------------------------------------
// Property trampolines.
// ---------------------------------------------------------------------------------------

template <typename Getter, typename Setter>
struct BoundProperty {
  Getter get;
  Setter set;
  std::string path;
};

template <typename Bound>
void DestroyBoundProperty(void* user_data) {
  delete static_cast<Bound*>(user_data);
}

template <typename Bound>
ULJSValue PropertyGetterBody(void* user_data, ULJSContext ctx, ULJSValue* exception) {
  auto* bound = static_cast<Bound*>(user_data);
  using R = typename CallableTraits<decltype(bound->get)>::Return;
  int diag = static_cast<int>(ulJSAPIGetCallDiagnostics());
  if constexpr (IsResultType<R>::value) {
    using T = typename IsResultType<R>::value_type;
    R result = [&]() -> R {
      LossyMuteScope mute;
      return bound->get();
    }();
    if (!result) {
      if (exception)
        *exception = std::move(result).error().ToJS(ctx);
      return nullptr;
    }
    return MarshalReturn(ctx, bound->path.c_str(), diag, [&] {
      return TypeTraits<std::remove_cvref_t<T>>::ToJS(ctx, result.value());
    });
  } else {
    R result = [&]() -> R {
      LossyMuteScope mute;
      return bound->get();
    }();
    return MarshalReturn(ctx, bound->path.c_str(), diag, [&] {
      return TypeTraits<std::remove_cvref_t<R>>::ToJS(ctx, std::move(result));
    });
  }
}

template <typename Bound>
ULJSValue PropertyGetterThunk(void* user_data, ULJSContext ctx, ULJSValue* exception) {
  return CallGuarded<ULJSValue>(
      ctx, static_cast<Bound*>(user_data)->path.c_str(), exception, nullptr,
      [&] { return PropertyGetterBody<Bound>(user_data, ctx, exception); });
}

// A property setter returns void, or js::Result<void> to refuse an assignment: its error is
// thrown into the page like a getter's.
template <typename R>
consteval bool IsSetterReturn() {
  if constexpr (IsResultType<R>::value)
    return std::is_void_v<typename IsResultType<R>::value_type>;
  else
    return std::is_void_v<R>;
}

template <typename Bound>
bool PropertySetterBody(void* user_data, ULJSContext ctx, ULJSValue value,
                        ULJSValue* exception) {
  auto* bound = static_cast<Bound*>(user_data);
  using Params = typename CallableTraits<decltype(bound->set)>::Params;
  static_assert(std::tuple_size_v<Params> == 1,
                "a property setter takes exactly one parameter");
  using P = std::tuple_element_t<0, Params>;
  static_assert(IsSetterReturn<typename CallableTraits<decltype(bound->set)>::Return>(),
                "a property setter returns void, or js::Result<void> to refuse the "
                "assignment (its error is thrown into the page)");

  MarshalFailureScope scope;
  ParamStorage<P> storage;
  ULJSValue args[1] = { value };
  // Argument number 0: a property assignment's messages carry no argument clause.
  if (!ConvertArg<P>(ctx, args, 1, 0, 0, CallSite(bound->path), storage, exception,
                     static_cast<int>(ulJSAPIGetCallDiagnostics())))
    return false;
  LossyMuteScope mute;
  // A holder-bound setter reports expiry through its Result; swallowing it would let the
  // page's assignment appear to land on a gone instance (the getter already throws).
  using SetR = typename CallableTraits<decltype(bound->set)>::Return;
  if constexpr (IsResultType<SetR>::value) {
    SetR result = bound->set(AsParam<P>(storage));
    if (!result) {
      if (exception)
        *exception = std::move(result).error().ToJS(ctx);
      return false;
    }
  } else {
    bound->set(AsParam<P>(storage));
  }
  return true;
}

template <typename Bound>
bool PropertySetterThunk(void* user_data, ULJSContext ctx, ULJSValue value,
                         ULJSValue* exception) {
  return CallGuarded<bool>(
      ctx, static_cast<Bound*>(user_data)->path.c_str(), exception, false,
      [&] { return PropertySetterBody<Bound>(user_data, ctx, value, exception); });
}

// ---------------------------------------------------------------------------------------
// Event-argument thunk.
// ---------------------------------------------------------------------------------------

template <typename Payload>
size_t EmitArgsThunk(void* user_data, ULJSContext ctx, ULJSValue* out, size_t max_args) {
  auto* payload = static_cast<Payload*>(user_data);
  constexpr size_t kCount = std::tuple_size_v<Payload>;
  if (kCount > max_args)
    return 0;
  size_t count = kCount;
  // A conversion that throws delivers the event with no arguments (and logs what()).
  ultralight::detail::CallCatchingExceptions(
      [&] {
        [&]<size_t... Is>(std::index_sequence<Is...>) {
          ((out[Is] = TypeTraits<std::remove_cvref_t<std::tuple_element_t<Is, Payload>>>::ToJS(
                ctx, std::get<Is>(*payload))),
           ...);
        }(std::make_index_sequence<kCount> {});
      },
      [&](const char* what) {
        for (size_t i = 0; i < kCount; i++) {
          ulDestroyJSValue(out[i]);
          out[i] = nullptr;
        }
        count = 0;
        std::string log = "Converting an event argument threw a C++ exception, so the event "
                          "was delivered with no arguments";
        if (what) {
          log += ": ";
          log += what;
        }
        ulJSAPIEmitDiagnostic(nullptr, log.c_str());
      });
  return count;
}

template <typename Payload>
void DeleteEmitPayload(void* user_data) {
  delete static_cast<Payload*>(user_data);
}

// ---------------------------------------------------------------------------------------
// Member-function binding functors (the js::Bind forms).
// ---------------------------------------------------------------------------------------

// The raw-pointer form: unfallible by contract (the caller guarantees the lifetime), and
// the hottest path, so it carries no check and no error channel.
template <typename T, typename M, typename R, typename... A>
struct BoundMethod {
  T* instance;
  M method;
  R operator()(A... args) const {
    return (instance->*method)(std::forward<A>(args)...);
  }
};

// A binding functor that accepts the bound dot-path for its own diagnostics; the
// registration sites apply it after they compute the full path.
template <typename F>
concept HasSettableBoundPath
    = requires(F& f, std::string p) { f.SetBoundPath(std::move(p)); };

// Keeps a strong holder for a Task's whole life.
template <typename S>
struct HeldHolder : TaskKeepAlive {
  S holder;
  explicit HeldHolder(S h) : holder(std::move(h)) {}
};

// One fallible functor for every traits-declared holder: a strong holder is checked
// through HolderTraits::Get per call; a weak holder locks into a strong holder held for
// the whole invocation (an expiry pre-check alone would race destruction on another
// thread). A Task method holds that strong holder (a copy of it, for a shared holder the
// binding may drop mid-flight) until the Task finishes, not just until its first
// suspension. A gone referent reports the path-qualified ULJS_DETACHED TypeError through
// the Result channel (sync calls throw, async calls reject) instead of touching a dangling
// pointer. A method with a trailing js::Resolver keeps its void return (the binding requires
// it) and reports a gone referent by rejecting that Resolver.
template <typename H, typename T, typename M, typename R, typename... A>
struct HolderBoundMethod {
  static constexpr bool kTrailingResolver = [] {
    if constexpr (sizeof...(A) > 0)
      return std::is_same_v<std::remove_cvref_t<std::tuple_element_t<sizeof...(A) - 1,
                                                                      std::tuple<A...>>>,
                            Resolver>;
    else
      return false;
  }();

  // The wrapped return adds the expiry error channel unless the method already returns a
  // Result (whose error channel is reused). Reference returns decay to a value: Expected
  // cannot hold a reference, and the marshaler copies the result to JavaScript either way.
  using Wrapped = std::conditional_t<
      kTrailingResolver && std::is_void_v<R>, void,
      std::conditional_t<IsResultType<R>::value, R,
                         std::conditional_t<std::is_void_v<R>, Expected<void, Error>,
                                            Expected<std::remove_cvref_t<R>, Error>>>>;

  H holder;
  M method;
  std::string bound_path;

  void SetBoundPath(std::string path) { bound_path = std::move(path); }

  Wrapped operator()(A... args) const {
    if constexpr (LockableHolder<H>) {
      auto locked = HolderTraits<H>::Lock(holder);
      using Strong = std::remove_cvref_t<decltype(locked)>;
      if (T* instance = HolderTraits<Strong>::Get(locked))
        return Invoke(locked, instance, std::forward<A>(args)...);
    } else {
      if (T* instance = HolderTraits<H>::Get(holder))
        return Invoke(holder, instance, std::forward<A>(args)...);
    }
    std::string message = bound_path;
    if (!message.empty())
      message += ": ";
    message += "the native object behind this binding was destroyed";
    Error error = Error::TypeError(std::move(message)).WithCode("ULJS_DETACHED");
    if constexpr (std::is_void_v<Wrapped>)
      std::get<sizeof...(A) - 1>(std::forward_as_tuple(args...)).Reject(std::move(error));
    else
      return Unexpected<Error>(std::move(error));
  }

 private:
  template <typename S>
  Wrapped Invoke(const S& strong, T* instance, A... args) const {
    if constexpr (std::is_void_v<Wrapped>) {
      (instance->*method)(std::forward<A>(args)...);
    } else if constexpr (std::is_void_v<R>) {
      (instance->*method)(std::forward<A>(args)...);
      return Wrapped();
    } else if constexpr (IsTaskType<R>::value) {
      R task = (instance->*method)(std::forward<A>(args)...);
      TaskAccess::KeepAlive(task, std::make_unique<HeldHolder<S>>(strong));
      return task;
    } else if constexpr (kIsAsyncReturn<R>) {
      R result = (instance->*method)(std::forward<A>(args)...);
      if (result)
        TaskAccess::KeepAlive(*result, std::make_unique<HeldHolder<S>>(strong));
      return result;
    } else {
      return (instance->*method)(std::forward<A>(args)...);
    }
  }
};

// MemberFnTraits<M> derives from Signature<R, A...>, so passing a traits value deduces the
// return and parameter packs here.
template <typename T, typename M, typename R, typename... A>
auto MakeBoundMethod(T* instance, M method, Signature<R, A...>) {
  return BoundMethod<T, M, R, A...> { instance, method };
}

template <typename H, typename T, typename M, typename R, typename... A>
auto MakeHolderBoundMethod(H holder, M method, Signature<R, A...>) {
  return HolderBoundMethod<H, T, M, R, A...> { std::move(holder), method, std::string() };
}

}  // namespace detail
}  // namespace js
}  // namespace ultralight
/// \endcond
