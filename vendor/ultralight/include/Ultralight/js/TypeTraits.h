///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/CAPI/CAPI_JSValue.h>
#include <Ultralight/js/Holder.h>
#include <Ultralight/js/Value.h>
#include <Ultralight/js/detail/Marshal.h>
#include <Ultralight/js/detail/Reflection.h>

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

///
/// @def ULTRALIGHT_REFLECTION
///
/// Whether or not enums and aggregate structs convert automatically (1 on Clang, GCC, and MSVC
/// 19.40+, ie. Visual Studio 2022 17.10 or later).
///
/// The automatic conversions read enumerator and field names from the compiler. When this is 0,
/// enums and aggregate structs have no built-in conversion, so you'll need to specialize
/// js::TypeTraits for the ones you use (and dom::data::ValueTraits or dom::data::TypeTraits for
/// the ones you bind with the data-binding API). Every other built-in conversion is unaffected.
///
/// To turn it off, define `ULTRALIGHT_REFLECTION=0` before including any Ultralight header.
///
#ifndef ULTRALIGHT_REFLECTION
#error "<Ultralight/detail/Reflection.h> defines ULTRALIGHT_REFLECTION"
#endif

namespace ultralight {

class Buffer;
class Color;
class String;
class URL;

namespace dom {
class Element;
}

namespace js {

/// \cond INTERNAL
namespace detail {
template <typename T, typename = void>
struct BuiltinTypeTraits;
}  // namespace detail
/// \endcond

///
/// Type conversions between C++ and JavaScript across the bridge.
///
/// The library uses js::TypeTraits to convert values whenever data crosses between C++ and
/// JavaScript. The bridge converts common standard types and reflected data structures
/// automatically without manual conversion code.
///
/// Bind functions that pass structs and enums:
///
/// ```
/// enum class Rarity { Common, Rare, Legendary };
///
/// struct Monster {
///   std::string species;
///   double damage;
///   Rarity rarity;
///   std::vector<std::string> loot;
///   std::optional<std::string> nickname;
/// };
///
/// app["spawn"] = [](Monster monster) { Log(monster.species); };
/// app["boss"] = [] {
///   return Monster { "Vampire Lord", 30, Rarity::Legendary, { "gold" } };
/// };
/// ```
///
/// Call the bound functions and pass matching objects from page script:
///
/// ```js
/// app.spawn({ species: "Vampire", damage: 12, rarity: "Rare", loot: ["fang"] });
/// const boss = app.boss();  // { species: "Vampire Lord", damage: 30, ... }
/// ```
///
/// ## Built-In Conversions
///
/// Standard C++ types map to JavaScript types:
///
/// | C++ Type                    | JavaScript Type       |
/// |-----------------------------|-----------------------|
/// | `bool`                      | boolean               |
/// | `int`, `double`             | number                |
/// | `std::string`               | string                |
/// | `std::string_view`          | string                |
/// | `js::Value`                 | any value             |
/// | `js::null`, `js::undefined` | `null` or `undefined` |
/// | `std::optional<T>`          | `T` or `undefined`    |
/// | `std::vector<T>`            | Array                 |
/// | `std::span<T>`              | typed array           |
/// | `std::map<std::string, T>`  | object                |
/// | `std::variant<Ts...>`       | any of `Ts`           |
/// | `std::function`             | function              |
///
/// The library's own types convert anywhere standard types do, once their header is included:
///
/// - **Most library types require `<Ultralight/JS.h>`.** It provides conversions for String, URL,
///   Color, and `RefPtr<Buffer>` (as an ArrayBuffer without copying).
/// - **DOM elements require `<Ultralight/dom/JSInterop.h>`.** A dom::Element handle crosses as the
///   element's page object.
///
/// Bound class instances cross the bridge as references to native objects rather than copies (see
/// js::ClassBuilder).
///
/// ## Strict Conversions
///
/// Conversions from JavaScript are strict and never coerce types. If page script passes an argument
/// that doesn't match the parameter's type (such as a string for a number parameter), the call
/// throws a TypeError with code `ULJS_BAD_ARG`, and the bound function never runs.
///
/// Integer conversions handle fractions and out-of-range numbers based on the diagnostics level and
/// call site:
///
/// - **At `Off` and `Warn`, integer parameters truncate fractions and wrap into range.** `Warn`
///   also logs a warning to the console and native log.
/// - **At `Strict`, lossy numbers throw a TypeError.** The error code is `ULJS_LOSSY`, and the
///   bound function never runs.
/// - **Value::To() always refuses lossy numbers.** Conversions fail with code `ULJS_BAD_ARG` at
///   every diagnostics level.
///
/// ## Structs and Enums
///
/// Under `ULTRALIGHT_REFLECTION` (enabled by default on Clang, GCC, and MSVC 19.40+, ie. Visual
/// Studio 2022 17.10 or later), the library reflects struct fields and enum enumerators
/// automatically. When `ULTRALIGHT_REFLECTION` is 0, automatic reflection is disabled, and structs
/// and enums require explicit TypeTraits specializations.
///
/// A struct converts to a JavaScript object with one property per field when it satisfies these
/// conditions:
///
/// - **Declare the struct at namespace scope.** It can't be inside a function or an anonymous
///   namespace.
/// - **Keep the field count to 24 or fewer.**
/// - **Avoid C arrays.** Use `std::vector` instead.
/// - **Do not use base classes.**
/// - **Ensure every field converts.** Fields wrapped in `std::optional` become optional properties
///   on the page.
///
/// An enum converts to a JavaScript string matching its enumerator name. Enumerator values from 0
/// to 63 convert by default. For values outside that range, specialize EnumRange with the minimum
/// and maximum values (spanning at most 1024 values).
///
/// ## Custom Type Conversions
///
/// You can bridge custom types using either value conversions or bound classes, depending on what
/// the type represents:
///
/// - **Use TypeTraits for small value types.** Values cross as copies each time (such as
///   identifiers or units).
/// - **Use bound classes for objects with identity.** The page receives a reference to your native
///   instance without copying (see js::ClassBuilder).
///
/// Declare specializations in namespace `ultralight::js`. The type must be default-constructible
/// because FromJS() receives a pointer to a default-constructed instance.
///
/// A specialization defines these static members:
///
/// | Member            | Required | Purpose                              |
/// |-------------------|----------|--------------------------------------|
/// | SchemaType()      | Yes      | Type name for errors and schemas     |
/// | FromJS()          | Yes      | Converts borrowed values into `*out` |
/// | ToJS()            | Yes      | Returns new JavaScript values        |
/// | AcceptsJSType()   | Optional | Checks types for variant selection   |
/// | WriteSchemaExpr() | Optional | Emits custom schema expressions      |
///
/// If a type crosses the bridge in only one direction, leave out FromJS() or ToJS().
///
/// Specialize TypeTraits to convert a 64-bit identifier as a string:
///
/// ```
/// struct EntityId {
///   uint64_t value = 0;
/// };
///
/// namespace ultralight::js {
///
/// template <>
/// struct TypeTraits<EntityId> {
///   static bool FromJS(ULJSContext ctx, ULJSValue value, EntityId* out,
///                      ULJSValue* exception) {
///     std::string text;
///     if (!TypeTraits<std::string>::FromJS(ctx, value, &text, exception))
///       return false;
///     const char* end = text.data() + text.size();
///     auto parsed = std::from_chars(text.data(), end, out->value);
///     return parsed.ec == std::errc() && parsed.ptr == end;
///   }
///
///   static ULJSValue ToJS(ULJSContext ctx, const EntityId& id) {
///     return TypeTraits<std::string>::ToJS(ctx, std::to_string(id.value));
///   }
///
///   static constexpr const char* SchemaType() { return "EntityId"; }
///
///   static bool AcceptsJSType(js::Type type, bool is_array, bool is_function) {
///     return type == js::Type::String;
///   }
///
///   static void WriteSchemaExpr(std::string& out) { out += "\"string\""; }
/// };
///
/// }  // namespace ultralight::js
/// ```
///
/// A custom specialization works wherever built-in conversions do, containers like `std::vector`
/// and `std::optional` included, and takes priority over any built-in conversion.
///
/// @warning  Declaring a specialization after code that already converts the type instantiates the
///           primary template first, which is ill-formed in C++.
///
/// \parblock
/// @note  Constants bound with API::SetConstant() (or direct property assignment) are copied as
///        JSON when registered and don't use TypeTraits specializations.
/// \endparblock
///
/// \parblock
/// @note  The js::TypeTraits template belongs to the JavaScript bridge and is unrelated to
///        dom::data::TypeTraits in the data-binding system.
/// \endparblock
///
/// @see  js::Value::To(), js::ClassBuilder, js::EnumRange, ULTRALIGHT_REFLECTION
///
template <typename T, typename>
struct TypeTraits : detail::BuiltinTypeTraits<T> {};

// Buffer is binary data, not a bound class: RefPtr<Buffer> marshals as an ArrayBuffer via
// the full specialization in <Ultralight/js/Buffer.h>. Declared here so a translation unit
// that marshals RefPtr<Buffer> without that header fails to compile (incomplete type)
// instead of silently selecting the canonical-holder class conversion.
template <>
struct TypeTraits<RefPtr<ultralight::Buffer>>;

// The cross-family and core value types marshal once their defining header is included:
// dom::Element via <Ultralight/dom/JSInterop.h>; String, URL, and Color via
// <Ultralight/JS.h>. Declared here, like RefPtr<Buffer> above, so a translation unit that
// marshals one without its header fails to compile (detail::AssertInteropHeader names the
// include that fixes it) instead of drifting:
// declaring a specialization after a marshaling query has already instantiated the
// primary template is ill-formed, and an Element pointer would otherwise select the
// bound-class pointer conversion and fail at runtime. The pointer forms stay permanently
// undefined by design: pass dom::Element by value (an owning page-lifetime handle).
template <>
struct TypeTraits<dom::Element>;
template <>
struct TypeTraits<dom::Element*>;
template <>
struct TypeTraits<const dom::Element*>;
template <>
struct TypeTraits<ultralight::String>;
template <>
struct TypeTraits<ultralight::URL>;
template <>
struct TypeTraits<ultralight::Color>;

///
/// Whether or not js::TypeTraits can convert T (ignoring const, volatile, and references).
///
template <typename T>
concept Marshalable = requires { TypeTraits<std::remove_cvref_t<T>>::SchemaType(); };

///
/// The range of values searched for an enum's enumerator names.
///
/// Only enumerators whose values fall in this range (0 to 63 by default) convert by name.
/// Specialize it for enums with values outside that range:
///
/// ```
/// template <> struct js::EnumRange<MyFlags> {
///   static constexpr long long min = 0, max = 255;
/// };
/// ```
///
/// The range can span at most 1024 values. An enum with no enumerator in its range fails to
/// compile.
///
template <typename E>
struct EnumRange {
  static constexpr long long min = 0;   ///< Lowest underlying value scanned, inclusive.
  static constexpr long long max = 63;  ///< Highest underlying value scanned, inclusive.
};

/// \cond INTERNAL
namespace detail {

// The library's built-in conversions. TypeTraits inherits this tier, so a user specialization
// of TypeTraits always takes priority, and a release can add built-ins here without colliding
// with specializations users already wrote. Nested types convert through the public
// TypeTraits, so a user specialization also applies inside containers.
template <typename T, typename>
struct BuiltinTypeTraits {};

// -----------------------------------------------------------------------------------------
// Scalar built-ins.
// -----------------------------------------------------------------------------------------

template <>
struct BuiltinTypeTraits<bool> {
  static bool FromJS(ULJSContext, ULJSValue borrowed, bool* out, ULJSValue*) {
    if (ulJSValueGetType(borrowed) != kULJSType_Boolean)
      return false;
    return ulJSValueToBoolean(borrowed, out);
  }
  static ULJSValue ToJS(ULJSContext ctx, bool value) {
    return ulCreateJSValueBoolean(ctx, value);
  }
  static constexpr const char* SchemaType() { return "boolean"; }
};

template <typename T>
struct BuiltinTypeTraits<T,
                         std::enable_if_t<std::is_arithmetic_v<T> && !std::is_same_v<T, bool>>> {
  static bool FromJS(ULJSContext, ULJSValue borrowed, T* out, ULJSValue* exception) {
    if (ulJSValueGetType(borrowed) != kULJSType_Number)
      return false;
    double number = 0;
    if (!ulJSValueToNumber(borrowed, &number, exception))
      return false;
    *out = detail::DoubleToArithmetic<T>(number);
    if constexpr (std::is_integral_v<T>)
      detail::ReportLossyFromDouble<T>(number, *out);
    return true;
  }
  static ULJSValue ToJS(ULJSContext ctx, T value) {
    // Only 64-bit integrals can exceed a JavaScript number's integer precision.
    if constexpr (std::is_integral_v<T> && sizeof(T) == 8)
      detail::ReportLossyToDouble<T>(value);
    return ulCreateJSValueNumber(ctx, static_cast<double>(value));
  }
  static constexpr const char* SchemaType() { return "number"; }
};

template <>
struct BuiltinTypeTraits<std::string> {
  static bool FromJS(ULJSContext, ULJSValue borrowed, std::string* out, ULJSValue* exception) {
    if (ulJSValueGetType(borrowed) != kULJSType_String)
      return false;
    // Stack buffer first: typical short strings cross with no intermediate heap allocation.
    char stack_buffer[256];
    size_t length = 0;
    if (ulJSValueGetUTF8(borrowed, stack_buffer, sizeof(stack_buffer), &length, exception)) {
      out->assign(stack_buffer, length);
      return true;
    }
    if (length <= sizeof(stack_buffer))
      return false;  // Conversion failed (exception stored if any), not a capacity miss.
    out->resize(length);
    return ulJSValueGetUTF8(borrowed, out->data(), out->size(), &length, exception);
  }
  static ULJSValue ToJS(ULJSContext ctx, const std::string& value) {
    return ulCreateJSValueStringUTF8(ctx, value.data(), value.size());
  }
  static constexpr const char* SchemaType() { return "string"; }
};

template <>
struct BuiltinTypeTraits<std::string_view> {
  static ULJSValue ToJS(ULJSContext ctx, std::string_view value) {
    return ulCreateJSValueStringUTF8(ctx, value.data(), value.size());
  }
  static constexpr const char* SchemaType() { return "string"; }
};

template <>
struct BuiltinTypeTraits<const char*> {
  static ULJSValue ToJS(ULJSContext ctx, const char* value) {
    return value ? ulCreateJSValueStringFromCString(ctx, value)
                 : ulCreateJSValueStringUTF8(ctx, "", 0);
  }
  static constexpr const char* SchemaType() { return "string"; }
};

template <>
struct BuiltinTypeTraits<char*> : BuiltinTypeTraits<const char*> {};

template <>
struct BuiltinTypeTraits<Value> {
  static bool FromJS(ULJSContext, ULJSValue borrowed, Value* out, ULJSValue*) {
    *out = Value::FromBorrowed(borrowed);
    return true;
  }
  static ULJSValue ToJS(ULJSContext, const Value& value) {
    return value.raw() ? ulCreateJSValueRef(value.raw()) : nullptr;
  }
  static constexpr const char* SchemaType() { return "any"; }
};

template <>
struct BuiltinTypeTraits<NullType> {
  static bool FromJS(ULJSContext, ULJSValue borrowed, NullType*, ULJSValue*) {
    return ulJSValueGetType(borrowed) == kULJSType_Null;
  }
  static ULJSValue ToJS(ULJSContext ctx, NullType) { return ulCreateJSValueNull(ctx); }
  static constexpr const char* SchemaType() { return "null"; }
};

template <>
struct BuiltinTypeTraits<UndefinedType> {
  static bool FromJS(ULJSContext, ULJSValue borrowed, UndefinedType*, ULJSValue*) {
    return ulJSValueGetType(borrowed) == kULJSType_Undefined;
  }
  static ULJSValue ToJS(ULJSContext ctx, UndefinedType) {
    return ulCreateJSValueUndefined(ctx);
  }
  static constexpr const char* SchemaType() { return "undefined"; }
};

// -----------------------------------------------------------------------------------------
// std::optional<T>
// -----------------------------------------------------------------------------------------

template <typename T>
struct BuiltinTypeTraits<std::optional<T>> {
  static bool FromJS(ULJSContext ctx, ULJSValue borrowed, std::optional<T>* out,
                     ULJSValue* exception) {
    ULJSType type = ulJSValueGetType(borrowed);
    if (type == kULJSType_Undefined || type == kULJSType_Null) {
      out->reset();
      return true;
    }
    T inner;
    if (!TypeTraits<T>::FromJS(ctx, borrowed, &inner, exception))
      return false;
    *out = std::move(inner);
    return true;
  }
  static ULJSValue ToJS(ULJSContext ctx, const std::optional<T>& value) {
    return value ? TypeTraits<T>::ToJS(ctx, *value) : ulCreateJSValueUndefined(ctx);
  }
  static constexpr const char* SchemaType() { return TypeTraits<T>::SchemaType(); }
};

// -----------------------------------------------------------------------------------------
// std::vector<T>  <->  Array
// -----------------------------------------------------------------------------------------

template <typename T>
struct BuiltinTypeTraits<std::vector<T>> {
  static_assert(!detail::IsCallScopedView<T>::value,
                "a container element cannot be a std::span (the views die with the bound "
                "call); use std::vector<std::vector<E>> to copy or RefPtr<Buffer> to share");

  static bool FromJS(ULJSContext ctx, ULJSValue borrowed, std::vector<T>* out,
                     ULJSValue* exception) {
    if (!ulJSValueIsArray(borrowed))
      return false;
    ULJSValue length_value = ulJSObjectGetProperty(borrowed, "length", exception);
    if (!length_value)
      return false;
    double length_number = 0;
    bool have_length = ulJSValueToNumber(length_value, &length_number, exception);
    ulDestroyJSValue(length_value);
    if (!have_length || length_number < 0)
      return false;
    size_t length = static_cast<size_t>(length_number);
    out->clear();
    // The length is script-controlled (`a.length = 4294967295` is a legal Array length),
    // so reserving it wholesale would let a hostile page force a giant allocation before
    // the first element ever converts. Reserve a small bound and let push_back grow.
    constexpr size_t kMaxUpFrontReserve = 256;
    out->reserve(length < kMaxUpFrontReserve ? length : kMaxUpFrontReserve);
    for (size_t i = 0; i < length; i++) {
      ULJSValue element = ulJSObjectGetPropertyAtIndex(borrowed, i, exception);
      if (!element)
        return false;
      T converted;
      if (!TypeTraits<T>::FromJS(ctx, element, &converted, exception)) {
        if (exception && *exception) {
          ulDestroyJSValue(element);
          return false;
        }
        if (!detail::ReportChildFailure(TypeTraits<T>::SchemaType(), element))
          ulDestroyJSValue(element);
        detail::ReportPathIndex(i);
        return false;
      }
      ulDestroyJSValue(element);
      out->push_back(std::move(converted));
    }
    return true;
  }
  static ULJSValue ToJS(ULJSContext ctx, const std::vector<T>& value) {
    std::vector<ULJSValue> elements(value.size());
    for (size_t i = 0; i < value.size(); i++)
      elements[i] = TypeTraits<T>::ToJS(ctx, value[i]);
    ULJSValue array = ulCreateJSArray(ctx, elements.data(), elements.size());
    for (ULJSValue element : elements)
      ulDestroyJSValue(element);
    return array;
  }
  static constexpr const char* SchemaType() { return detail::kArraySchema<T>.data; }
};

// -----------------------------------------------------------------------------------------
// std::map<std::string, T>  <->  plain object
// -----------------------------------------------------------------------------------------

template <typename T>
struct BuiltinTypeTraits<std::map<std::string, T>> {
  static_assert(!detail::IsCallScopedView<T>::value,
                "a container element cannot be a std::span (the views die with the bound "
                "call); use std::vector<E> values to copy or RefPtr<Buffer> to share");

  static bool FromJS(ULJSContext ctx, ULJSValue borrowed, std::map<std::string, T>* out,
                     ULJSValue* exception) {
    if (ulJSValueGetType(borrowed) != kULJSType_Object || ulJSValueIsArray(borrowed)
        || ulJSValueIsCallable(borrowed))
      return false;
    // Two-call pattern; a getter may add properties between the calls, so grow and retry
    // (bounded, in case a hostile getter grows the object every read).
    std::vector<ULString> names;
    std::vector<ULJSValue> values;
    size_t count = 0;
    if (!ulJSObjectGetOwnEntries(borrowed, nullptr, nullptr, 0, &count, exception))
      return false;
    for (int attempt = 0; attempt < 8; attempt++) {
      names.resize(count);
      values.resize(count);
      size_t total = 0;
      if (!ulJSObjectGetOwnEntries(borrowed, names.data(), values.data(), count, &total,
                                   exception))
        return false;
      if (total <= count) {
        count = total;
        break;
      }
      // Grew: nothing above `count` was stored; release what was and retry larger.
      for (size_t i = 0; i < count; i++) {
        ulDestroyString(names[i]);
        ulDestroyJSValue(values[i]);
      }
      count = total;
      if (attempt == 7)
        return false;
    }
    out->clear();
    bool ok = true;
    for (size_t i = 0; i < count; i++) {
      if (!ok)
        continue;
      std::string key(ulStringGetData(names[i]), ulStringGetLength(names[i]));
      T converted;
      if (!TypeTraits<T>::FromJS(ctx, values[i], &converted, exception)) {
        ok = false;
        if (!(exception && *exception)) {
          if (detail::ReportChildFailure(TypeTraits<T>::SchemaType(), values[i]))
            values[i] = nullptr;  // Ownership moved to the failure record.
          detail::ReportPathName(key.data(), key.size());
        }
        continue;
      }
      out->emplace(std::move(key), std::move(converted));
    }
    for (size_t i = 0; i < count; i++) {
      ulDestroyString(names[i]);
      ulDestroyJSValue(values[i]);
    }
    return ok;
  }
  static ULJSValue ToJS(ULJSContext ctx, const std::map<std::string, T>& value) {
    std::vector<const char*> names;
    std::vector<ULJSValue> values;
    names.reserve(value.size());
    values.reserve(value.size());
    bool valid_keys = true;
    for (const auto& [key, entry] : value) {
      if (key.find('\0') != std::string::npos) {
        valid_keys = false;  // JavaScript property definition takes null-terminated names.
        break;
      }
      names.push_back(key.c_str());
      values.push_back(TypeTraits<T>::ToJS(ctx, entry));
    }
    ULJSValue object = valid_keys
        ? ulCreateJSObjectWithProperties(ctx, names.data(), values.data(), values.size(),
                                         nullptr)
        : nullptr;
    for (ULJSValue entry : values)
      ulDestroyJSValue(entry);
    return object;
  }
  static constexpr const char* SchemaType() { return detail::kMapSchema<T>.data; }
};

// -----------------------------------------------------------------------------------------
// std::variant<Ts...>
// -----------------------------------------------------------------------------------------

template <typename... Ts>
struct BuiltinTypeTraits<std::variant<Ts...>> {
  static_assert((!detail::IsCallScopedView<Ts>::value && ...),
                "a variant alternative cannot be a std::span (the variant outlives the "
                "bound call the view is scoped to); use std::vector<E> or RefPtr<Buffer>");

  static bool FromJS(ULJSContext ctx, ULJSValue borrowed, std::variant<Ts...>* out,
                     ULJSValue* exception) {
    Type type = static_cast<Type>(ulJSValueGetType(borrowed));
    bool is_array = ulJSValueIsArray(borrowed);
    bool is_function = ulJSValueIsCallable(borrowed);
    // Alternatives record no partial failures; either one converts or the variant as a
    // whole reports its joined type expectation.
    detail::MarshalFailure* saved = detail::g_marshal_failure;
    detail::g_marshal_failure = nullptr;
    bool converted = TryAlternatives<Ts...>(ctx, borrowed, out, exception, type, is_array,
                                            is_function);
    detail::g_marshal_failure = saved;
    return converted;
  }
  static ULJSValue ToJS(ULJSContext ctx, const std::variant<Ts...>& value) {
    return std::visit(
        [ctx](const auto& alternative) {
          using U = std::remove_cvref_t<decltype(alternative)>;
          return TypeTraits<U>::ToJS(ctx, alternative);
        },
        value);
  }
  static constexpr const char* SchemaType() { return detail::kVariantSchema<Ts...>.data; }

 private:
  template <typename First, typename... Rest>
  static bool TryAlternatives(ULJSContext ctx, ULJSValue borrowed, std::variant<Ts...>* out,
                              ULJSValue* exception, Type type, bool is_array,
                              bool is_function) {
    if (detail::AcceptsJSValue<First>(type, is_array, is_function)) {
      First converted;
      if (TypeTraits<First>::FromJS(ctx, borrowed, &converted, exception)) {
        *out = std::move(converted);
        return true;
      }
      if (exception && *exception)
        return false;
    }
    if constexpr (sizeof...(Rest) > 0)
      return TryAlternatives<Rest...>(ctx, borrowed, out, exception, type, is_array,
                                      is_function);
    return false;
  }
};

// -----------------------------------------------------------------------------------------
// std::function<R(Args...)>
// -----------------------------------------------------------------------------------------

template <typename R, typename... A>
struct BuiltinTypeTraits<std::function<R(A...)>> {
  static_assert((Marshalable<A> && ...),
                "every parameter of a std::function binding must have TypeTraits");
  static_assert(std::is_void_v<R> || detail::IsResultType<R>::value,
                "a std::function converted from JavaScript must return void (failures are "
                "dropped) or js::Result<T> (failures are observable)");
  static_assert(
      [] {
        if constexpr (detail::IsResultType<R>::value)
          return !detail::IsCallScopedView<typename detail::IsResultType<R>::value_type>::value;
        else
          return true;
      }(),
      "a JavaScript function's converted return value cannot be a std::span (the view would "
      "dangle once the underlying value is released); use std::vector<E> or RefPtr<Buffer>");

  static bool FromJS(ULJSContext ctx, ULJSValue borrowed, std::function<R(A...)>* out,
                     ULJSValue*) {
    if (!ulJSValueIsCallable(borrowed))
      return false;
    detail::ContextRef context(ctx);
    Value function = Value::FromBorrowed(borrowed);
    *out = [context, function](A... args) -> R {
      Value arg_values[sizeof...(A) > 0 ? sizeof...(A) : 1];
      size_t index = 0;
      ((arg_values[index++]
        = Value::Adopt(TypeTraits<std::remove_cvref_t<A>>::ToJS(context.ctx, args))),
       ...);
      Result<Value> result = function.Call(Value(), arg_values, sizeof...(A));
      if constexpr (std::is_void_v<R>) {
        (void)result;  // Fire-and-forget: failures (including a page-gone error) are dropped.
      } else {
        using T = typename detail::IsResultType<R>::value_type;
        if (!result)
          return Unexpected<Error>(std::move(result).error());
        if constexpr (std::is_void_v<T>) {
          return R();
        } else {
          // Same diagnosis as Value::To(), in frames of its own, so a binding's ambient
          // argument-conversion frame is never touched.
          return detail::ConvertBorrowed<std::remove_cvref_t<T>>(result.value().raw());
        }
      }
    };
    return true;
  }
  static constexpr const char* SchemaType() { return "function"; }
};

#if ULTRALIGHT_REFLECTION

// -----------------------------------------------------------------------------------------
// Enums
// -----------------------------------------------------------------------------------------

template <typename E>
struct BuiltinTypeTraits<E, std::enable_if_t<std::is_enum_v<E>>> {
  static bool FromJS(ULJSContext, ULJSValue borrowed, E* out, ULJSValue* exception) {
    if (ulJSValueGetType(borrowed) != kULJSType_String)
      return false;
    char buffer[128];
    size_t length = 0;
    if (!ulJSValueGetUTF8(borrowed, buffer, sizeof(buffer), &length, exception))
      return false;
    std::string_view name(buffer, length);
    for (const auto& entry : detail::EnumInfo<E>::kEntries) {
      if (entry.name == name) {
        *out = static_cast<E>(entry.value);
        return true;
      }
    }
    return false;
  }
  static ULJSValue ToJS(ULJSContext ctx, E value) {
    long long raw = static_cast<long long>(value);
    for (const auto& entry : detail::EnumInfo<E>::kEntries) {
      if (entry.value == raw)
        return ulCreateJSValueStringUTF8(ctx, entry.name.data(), entry.name.size());
    }
    // A value with no reflected name (out of range or not an enumerator) is a native-side
    // bug; surface it as the raw number rather than silently inventing a name.
    return ulCreateJSValueNumber(ctx, static_cast<double>(raw));
  }
  static constexpr const char* SchemaType() { return detail::EnumInfo<E>::kSchema.data; }
};

// -----------------------------------------------------------------------------------------
// Aggregate structs
// -----------------------------------------------------------------------------------------

template <typename T>
struct BuiltinTypeTraits<T, std::enable_if_t<detail::kIsReflectableAggregate<T>>> {
 private:
  static constexpr size_t kFieldCount = detail::CountFields<T>();

  template <size_t I>
  static bool ConvertField(ULJSContext ctx, T* out, ULJSValue* values, ULJSValue* exception) {
    using Field = detail::FieldType<T, I>;
    static_assert(Marshalable<Field>,
                  "aggregate field type has no js::TypeTraits; specialize it (or the whole "
                  "aggregate) explicitly");
    static_assert(detail::ConvertibleFromJS<Field>,
                  "aggregate field type cannot be converted FROM JavaScript (const char* "
                  "and std::string_view fields cannot own converted text; use std::string)");
    static_assert(!detail::IsCallScopedView<Field>::value,
                  "an aggregate field cannot be a std::span (the view dies with the bound "
                  "call while the aggregate outlives it); use std::vector<T> to copy or "
                  "RefPtr<Buffer> to share the bytes");
    if (TypeTraits<Field>::FromJS(ctx, values[I], detail::MemberPointer<I>(*out), exception))
      return true;
    if (!(exception && *exception)) {
      if (detail::ReportChildFailure(TypeTraits<Field>::SchemaType(), values[I]))
        values[I] = nullptr;  // Ownership moved to the failure record.
      detail::ReportPathName(detail::kFieldNameZ<T, I>.data,
                             detail::kFieldNameZ<T, I>.len);
    }
    return false;
  }

 public:
  static bool FromJS(ULJSContext ctx, ULJSValue borrowed, T* out, ULJSValue* exception) {
    if (ulJSValueGetType(borrowed) != kULJSType_Object || ulJSValueIsArray(borrowed)
        || ulJSValueIsCallable(borrowed))
      return false;
    if constexpr (kFieldCount == 0) {
      return true;
    } else {
      ULJSValue values[kFieldCount] = {};
      if (!ulJSObjectGetProperties(borrowed, detail::kFieldNames<T>.data(), kFieldCount,
                                   values, exception))
        return false;
      bool ok = [&]<size_t... Is>(std::index_sequence<Is...>) {
        return (ConvertField<Is>(ctx, out, values, exception) && ...);
      }(std::make_index_sequence<kFieldCount> {});
      for (size_t i = 0; i < kFieldCount; i++)
        ulDestroyJSValue(values[i]);
      return ok;
    }
  }

  static ULJSValue ToJS(ULJSContext ctx, const T& value) {
    if constexpr (kFieldCount == 0) {
      return ulCreateJSObjectWithProperties(ctx, nullptr, nullptr, 0, nullptr);
    } else {
      ULJSValue values[kFieldCount];
      T& mutable_value = const_cast<T&>(value);  // MemberPointer reads only.
      [&]<size_t... Is>(std::index_sequence<Is...>) {
        ((values[Is] = TypeTraits<detail::FieldType<T, Is>>::ToJS(
              ctx, *detail::MemberPointer<Is>(mutable_value))),
         ...);
      }(std::make_index_sequence<kFieldCount> {});
      ULJSValue object = ulCreateJSObjectWithProperties(
          ctx, detail::kFieldNames<T>.data(), values, kFieldCount, nullptr);
      for (size_t i = 0; i < kFieldCount; i++)
        ulDestroyJSValue(values[i]);
      return object;
    }
  }

  static constexpr const char* SchemaType() { return detail::kTypeNameZ<T>.data; }
};

#endif  // ULTRALIGHT_REFLECTION

}  // namespace detail
/// \endcond

}  // namespace js
}  // namespace ultralight

// The typed-conversion members js::Value, js::Value::Ref, and js::Arg declare in
// <Ultralight/js/Value.h> convert through the traits above, so their definitions live
// behind this header.
#include <Ultralight/js/detail/ValueImpl.h>
