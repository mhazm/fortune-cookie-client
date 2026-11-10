///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// Implementation detail of `<Ultralight/js/TypeTraits.h>`; include that header instead.
/// Defines the typed-conversion members js::Value, js::Value::Ref, and js::Arg declare in
/// `<Ultralight/js/Value.h>` (they convert through TypeTraits, so they live behind it).
///
/// \cond INTERNAL
#pragma once
#include <Ultralight/CAPI/CAPI_JSValue.h>
#include <Ultralight/js/Error.h>
#include <Ultralight/js/TypeTraits.h>
#include <Ultralight/js/Value.h>
#include <Ultralight/js/detail/Marshal.h>

#include <optional>
#include <string>
#include <type_traits>
#include <utility>

namespace ultralight {
namespace js {

namespace detail {

// The message a failed conversion reports, in the form a binding's TypeError has ("volume:
// expected number, got 'loud'"). The waist formats it so every language layer describes the
// value the same way; `path` is null when the whole value failed.
inline std::string DescribeConversionFailure(ULJSContext ctx, const char* path,
                                             const char* expected, ULJSValue got) {
  ULJSValue error = ulCreateJSMarshalError(ctx, path ? path : "", 0, nullptr, expected, got);
  std::string message = ExceptionDetails::Text(ExceptionDetails(error).raw().message);
  ulDestroyJSValue(error);
  if (!path && message.rfind(": ", 0) == 0)
    message.erase(0, 2);
  return message.empty() ? std::string("expected ") + expected : message;
}

// Shared body of the user-invoked strict conversions (Value::To, Arg::To, js::Await). A local
// marshal-failure frame records where the value failed (so the message matches a binding's),
// and a local lossy frame makes an integer target refuse a fractional or out-of-range number
// the way Strict bindings do. Both frames replace any ambient ones (a binding may call this
// while its own argument conversion is running), so neither side pollutes the other.
template <typename U>
Result<U> ConvertBorrowed(ULJSValue handle) {
  if (!handle)
    return EmptyHandleError();
  if (!ulJSValueIsAlive(handle))
    return Unexpected<Error>(Error::PageGone());
  ULJSContext ctx = ulJSValueGetContext(handle);
  U out {};
  ULJSValue exception = nullptr;
  MarshalFailureScope failure;
  LossyConversionScope lossy(true);
  bool converted = TypeTraits<U>::FromJS(ctx, handle, &out, &exception);
  Result<U> result = [&]() -> Result<U> {
    if (converted && !lossy.frame.active)
      return std::move(out);
    if (exception)
      return Unexpected<Error>(Error::AdoptException(exception));
    if (!ulJSValueIsAlive(handle))
      return Unexpected<Error>(Error::PageGone());
    std::string message;
    if (converted) {
      message = std::string("expected integer in range, got ") + lossy.frame.from;
    } else {
      bool recorded = failure.frame.active;
      message = DescribeConversionFailure(
          ctx, failure.frame.path_or_null(),
          recorded ? failure.frame.expected : TypeTraits<U>::SchemaType(),
          recorded ? failure.frame.got : handle);
    }
    return Unexpected<Error>(Error::TypeError(std::move(message)).WithCode("ULJS_BAD_ARG"));
  }();
  ulDestroyJSContext(ctx);
  return result;
}

// ultralight::String, URL, and Color convert once <Ultralight/JS.h> is included (it defines
// their conversions), and dom::Element once <Ultralight/dom/JSInterop.h> is. Without the header
// they're declared but undefined, so name the header instead of the generic error.
template <typename U>
constexpr void AssertInteropHeader() {
  static_assert(Marshalable<U>
                    || !(std::is_same_v<U, ultralight::String> || std::is_same_v<U, URL>
                         || std::is_same_v<U, Color>),
                "js::TypeTraits for ultralight::String, URL, and Color are defined in "
                "<Ultralight/JS.h>: include it in this file");
  static_assert(Marshalable<U> || !std::is_same_v<U, dom::Element>,
                "js::TypeTraits for dom::Element are defined in <Ultralight/dom/JSInterop.h>: "
                "include it in this file");
}

}  // namespace detail

template <typename T>
Result<T> Value::To() const {
  using U = std::remove_cvref_t<T>;
  detail::AssertInteropHeader<U>();
  static_assert(Marshalable<U>,
                "no js::TypeTraits for T: use a supported built-in, make it a plain "
                "aggregate struct, or specialize js::TypeTraits for it");
  static_assert(detail::ConvertibleFromJS<U>,
                "T cannot be converted FROM JavaScript (std::string_view and const char* "
                "cannot own the converted bytes; use std::string)");
  static_assert(!detail::IsCallScopedView<U>::value,
                "std::span converts only as a direct bound-function parameter (the view is "
                "call-scoped); use To<std::vector<E>>() to copy, or js::TypedArray<E> and "
                "its view() for wrapper-bound access");
  return detail::ConvertBorrowed<U>(handle_);
}

template <typename T>
std::optional<T> Value::Maybe() const {
  Result<T> result = To<T>();
  if (result)
    return std::move(result).value();
  return std::nullopt;
}

template <typename T>
T Value::Or(T fallback) const {
  return To<T>().value_or(std::move(fallback));
}

inline std::string Value::Or(const char* fallback) const {
  return To<std::string>().value_or(std::string(fallback ? fallback : ""));
}

template <typename R, typename... A>
Result<R> Value::Invoke(A&&... args) const {
  return InvokeOn<R>(Value(), std::forward<A>(args)...);
}

template <typename R, typename... A>
Result<R> Value::InvokeOn(const Value& this_value, A&&... args) const {
  static_assert((Marshalable<std::decay_t<A>> && ...),
                "every Invoke argument must have js::TypeTraits");
  if (!handle_)
    return detail::EmptyHandleError();
  Value converted[sizeof...(A) > 0 ? sizeof...(A) : 1];
  if constexpr (sizeof...(A) > 0) {
    ULJSContext ctx = ulJSValueGetContext(handle_);
    size_t index = 0;
    ((converted[index++] = Value::Adopt(TypeTraits<std::decay_t<A>>::ToJS(ctx, args))),
     ...);
    ulDestroyJSContext(ctx);
  }
  Result<Value> result = Call(this_value, converted, sizeof...(A));
  if (!result)
    return Unexpected<Error>(std::move(result).error());
  if constexpr (std::is_same_v<R, Value>)
    return std::move(result).value();
  else
    return result.value().template To<R>();
}

template <typename T>
Value::Ref& Value::Ref::operator=(T&& value) {
  using U = std::decay_t<T>;
  if (!object_.raw())
    return *this;
  ULJSValue handle = nullptr;
  bool owned = false;
  if constexpr (std::is_base_of_v<Value, U>) {
    handle = value.raw();
  } else if constexpr (std::is_same_v<U, Ref>) {
    return (*this) = value.Get();
  } else if constexpr (std::is_convertible_v<U, std::string_view>) {
    ULJSContext ctx = ulJSValueGetContext(object_.raw());
    std::string_view text(value);
    handle = ulCreateJSValueStringUTF8(ctx, text.data(), text.size());
    ulDestroyJSContext(ctx);
    owned = true;
  } else {
    detail::AssertInteropHeader<U>();
    static_assert(Marshalable<U>,
                  "the assigned value has no js::TypeTraits: use a supported built-in, a "
                  "plain aggregate struct, js::null / js::undefined, or a js::Value");
    ULJSContext ctx = ulJSValueGetContext(object_.raw());
    handle = TypeTraits<U>::ToJS(ctx, value);
    ulDestroyJSContext(ctx);
    owned = true;
  }
  detail::SwallowedException swallow;
  if (by_index_)
    ulJSObjectSetPropertyAtIndex(object_.raw(), index_, handle, swallow.out());
  else
    ulJSObjectSetProperty(object_.raw(), name_.c_str(), handle,
                          kULJSPropertyAttributes_None, swallow.out());
  if (owned)
    ulDestroyJSValue(handle);
  return *this;
}

template <typename T>
Result<T> Value::Ref::To() const {
  Value value = Get();
  // A read from a page that's gone yields an empty Value; report the page, not the empty read.
  if (value.IsEmpty() && SourceGone())
    return Unexpected<Error>(Error::PageGone());
  return value.To<T>();
}

template <typename T>
std::optional<T> Value::Ref::Maybe() const {
  return Get().Maybe<T>();
}

template <typename T>
T Value::Ref::Or(T fallback) const {
  return Get().Or<T>(std::move(fallback));
}

template <typename R, typename... A>
Result<R> Value::Ref::Invoke(A&&... args) const {
  Value method = Get();
  if (method.IsEmpty() && SourceGone())
    return Unexpected<Error>(Error::PageGone());
  return method.InvokeOn<R>(object_, std::forward<A>(args)...);
}

template <typename T>
Result<T> Arg::To() const {
  using U = std::remove_cvref_t<T>;
  detail::AssertInteropHeader<U>();
  static_assert(Marshalable<U>,
                "no js::TypeTraits for T: use a supported built-in, make it a plain "
                "aggregate struct, or specialize js::TypeTraits for it");
  static_assert(detail::ConvertibleFromJS<U>,
                "T cannot be converted FROM JavaScript (std::string_view and const char* "
                "cannot own the converted bytes; use std::string)");
  static_assert(!detail::IsCallScopedView<U>::value,
                "std::span converts only as a direct bound-function parameter (the view is "
                "call-scoped); use To<std::vector<E>>() to copy, or js::TypedArray<E> and "
                "its view() for wrapper-bound access");
  if (!borrowed_) {
    // An absent argument is JavaScript `undefined`: absent optionals succeed.
    if constexpr (detail::IsOptional<U>::value)
      return U {};
    return Unexpected<Error>(
        Error::TypeError(std::string("expected ") + TypeTraits<U>::SchemaType()
                         + ", got undefined")
            .WithCode("ULJS_BAD_ARG"));
  }
  return detail::ConvertBorrowed<U>(borrowed_);
}

template <typename T>
std::optional<T> Arg::Maybe() const {
  Result<T> result = To<T>();
  if (result)
    return std::move(result).value();
  return std::nullopt;
}

template <typename T>
T Arg::Or(T fallback) const {
  return To<T>().value_or(std::move(fallback));
}

inline std::string Arg::Or(const char* fallback) const {
  return To<std::string>().value_or(std::string(fallback ? fallback : ""));
}

}  // namespace js
}  // namespace ultralight
/// \endcond
