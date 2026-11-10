///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/Color.h>
#include <Ultralight/String.h>
#include <Ultralight/dom/StyleValue.h>
#include <Ultralight/dom/data/detail/EnumReflection.h>

#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <type_traits>

namespace ultralight {
namespace dom {
namespace data {

///
/// The kinds of value a schema entry holds.
///
/// A leaf field (one that holds a value, not a nested object or a list) has one of the first six
/// kinds (see ValueTraits). Object, List, and Action entries hold no value of their own.
///
enum class ValueKind : uint8_t {
  Bool = 0,     ///< A boolean.
  Int64,        ///< A signed 64-bit integer (from any integer type).
  Double,       ///< A double-precision float (from any floating-point type).
  String,       ///< UTF-8 text (from strings and enums).
  StyleValue,   ///< A CSS number with a unit (see dom::StyleValue).
  Color,        ///< A color (see ultralight::Color).
  Object,       ///< A nested object (no value of its own).
  List,         ///< A list of rows (no value of its own).
  Action,       ///< An action (no value of its own).
};

///
/// The range of values searched for an enum's enumerator names.
///
/// Only enumerators whose values fall in this range (0 to 63 by default) bind by name. Specialize
/// it for enums with values outside that range:
///
/// ```
/// template <> struct ultralight::dom::data::EnumRange<MyFlags> {
///   static constexpr long long min = 0, max = 255;
/// };
/// ```
///
/// The range can span at most 1024 values. An enum with no enumerator in its range fails to
/// compile.
///
/// @note  This is separate from js::EnumRange. An enum you use with both APIs needs a
///        specialization of each.
///
template <typename E>
struct EnumRange {
  static constexpr long long min = 0;   ///< The lowest value searched (inclusive).
  static constexpr long long max = 63;  ///< The highest value searched (inclusive).
};

/// \cond INTERNAL
namespace detail {

// The library's built-in leaf conversions. ValueTraits inherits this tier, so a user
// specialization of ValueTraits always takes priority, and a release can add built-ins here
// without colliding with specializations users already wrote.
template <typename T, typename = void>
struct BuiltinValueTraits {};

template <>
struct BuiltinValueTraits<bool> {
  static constexpr ValueKind kind = ValueKind::Bool;
  static constexpr bool ToSlot(bool value) { return value; }
};

// Integral types (bool excluded) lower to the signed 64-bit slot.
template <typename T>
struct BuiltinValueTraits<T,
                          std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>>> {
  static constexpr ValueKind kind = ValueKind::Int64;
  static constexpr int64_t ToSlot(T value) { return static_cast<int64_t>(value); }
};

// Floating-point types lower to the double slot.
template <typename T>
struct BuiltinValueTraits<T, std::enable_if_t<std::is_floating_point_v<T>>> {
  static constexpr ValueKind kind = ValueKind::Double;
  static constexpr double ToSlot(T value) { return static_cast<double>(value); }
};

template <>
struct BuiltinValueTraits<String> {
  static constexpr ValueKind kind = ValueKind::String;
  static const String& ToSlot(const String& value) { return value; }
};

template <>
struct BuiltinValueTraits<std::string> {
  static constexpr ValueKind kind = ValueKind::String;
  static String ToSlot(const std::string& value) {
    return String(value.data(), value.size());
  }
};

template <>
struct BuiltinValueTraits<std::string_view> {
  static constexpr ValueKind kind = ValueKind::String;
  static String ToSlot(std::string_view value) { return String(value.data(), value.size()); }
};

template <>
struct BuiltinValueTraits<const char*> {
  static constexpr ValueKind kind = ValueKind::String;
  static String ToSlot(const char* value) { return String(value ? value : ""); }
};

// Non-const char* fields are strings too; without this they would classify as nullable
// pointers to an undescribed char and fail with a misleading diagnostic.
template <>
struct BuiltinValueTraits<char*> : BuiltinValueTraits<const char*> {};

template <>
struct BuiltinValueTraits<StyleValue> {
  static constexpr ValueKind kind = ValueKind::StyleValue;
  static const StyleValue& ToSlot(const StyleValue& value) { return value; }
};

template <>
struct BuiltinValueTraits<Color> {
  static constexpr ValueKind kind = ValueKind::Color;
  static const Color& ToSlot(const Color& value) { return value; }
};

#if ULTRALIGHT_REFLECTION

// Enums lower to the string slot as their enumerator name ("Small"), reflected from the
// type. A value with no reflected name lowers as its decimal number rather than an
// invented name (a native-side bug made visible, matching the js family's behavior).
template <typename E>
struct BuiltinValueTraits<E, std::enable_if_t<std::is_enum_v<E>>> {
  static constexpr ValueKind kind = ValueKind::String;
  static String ToSlot(E value) {
    const long long raw = static_cast<long long>(value);
    const std::string_view name = detail::EnumInfo<E>::NameOf(raw);
    if (!name.empty())
      return String(name.data(), name.size());
    char buffer[24];
    const int length = snprintf(buffer, sizeof(buffer), "%lld", raw);
    return String(buffer, length > 0 ? static_cast<size_t>(length) : 0);
  }
};

#endif  // ULTRALIGHT_REFLECTION

}  // namespace detail
/// \endcond

///
/// Conversion rules that expose a C++ type as a primitive value in data bindings.
///
/// ValueTraits defines how data bindings convert a custom C++ scalar type into a primitive value on
/// the page. Specializing it lets your application bind custom scalar types, such as a fixed-point
/// coordinate, directly as values without schemas.
///
/// This specialization binds a fixed-point engine type as a floating-point number:
///
/// ```
/// template <> struct dd::ValueTraits<Fixed> {
///   static constexpr dd::ValueKind kind = dd::ValueKind::Double;
///   static double ToSlot(const Fixed& value) { return value.ToDouble(); }
/// };
///
/// struct Ship {
///   Fixed speed;
///   void Boost(Fixed amount);
///
///   static constexpr auto schema = dd::Schema(
///       dd::Field("speed", &Ship::speed, dd::Editable),  // ship.speed
///       dd::Action<Fixed>("boost"));
/// };
/// ```
///
/// ## Built-In Value Types
///
/// The library provides built-in traits for primitive C++ types:
///
/// | Type                 | Kind                  |
/// |----------------------|-----------------------|
/// | `bool`               | ValueKind::Bool       |
/// | Integer types        | ValueKind::Int64      |
/// | Floating-point types | ValueKind::Double     |
/// | String types         | ValueKind::String     |
/// | Enums                | ValueKind::String     |
/// | dom::StyleValue      | ValueKind::StyleValue |
/// | Color                | ValueKind::Color      |
///
/// An enum converts to text using its enumerator name. If an enum value doesn't have a matching
/// name in the reflected range (0 to 63 by default), it displays as its decimal number instead (see
/// dom::data::EnumRange).
///
/// ## Custom Value Types
///
/// To bind a custom scalar type, specialize ValueTraits in the `ultralight::dom::data` namespace
/// with two members:
///
/// - **Set `kind` to the matching ValueKind constant.** A specialization can use any of the six
///   value kinds to represent the type on the page.
/// - **Define a static ToSlot() function.** It accepts your custom type by value or const reference
///   and returns the corresponding C++ representation.
///
/// Specializations follow two priority rules:
///
/// - **Your specialization takes priority over built-in traits.**
/// - **A ValueTraits specialization takes priority over a schema.** The library binds the type as a
///   leaf value even if it defines a schema or qualifies for aggregate reflection.
///
/// ## Receiving Values from the Page
///
/// Conversion works in one direction only, from native code to the page. When receiving values
/// back, handlers accept primitive representations:
///
/// - **Change handlers take the primitive C++ type or a dom::data::Value.** A handler registered
///   with Binding::OnChange() receives the type corresponding to your `kind` (such as `double` for
///   `ValueKind::Double`).
/// - **Action payloads arrive as a dom::data::Value.**
///
/// Convert the primitive value back to your custom type inside the handler:
///
/// ```
/// binding.OnChange<"speed">([&](double speed) {
///   ship.speed = Fixed::FromDouble(speed);
/// });
/// binding.OnAction<"boost">([&](dd::Value amount) {
///   ship.Boost(Fixed::FromDouble(amount.Or(0.0)));
/// });
/// ```
///
/// @note  Don't register a change handler that takes your own type. If your type converts from a
///        number, it can compile and receive the wrong value (see the warning on
///        dom::data::Binding::OnChange()).
///
/// @see  dom::data::ValueKind, dom::data::EnumRange, dom::data::TypeTraits,
///       dom::data::Binding::OnChange()
///
template <typename T, typename = void>
struct ValueTraits : detail::BuiltinValueTraits<T> {};

///
/// Whether or not T binds as a leaf value (ignoring const, volatile, and references), ie.
/// ValueTraits is specialized for it.
///
template <typename T>
concept Bindable = requires { ValueTraits<std::remove_cvref_t<T>>::kind; };

///
/// Whether or not P can be an action's payload without a payload struct (ignoring const,
/// volatile, and references).
///
/// It can when it binds as a Bool, Int64, Double, or String value (eg, `Action<double>` or an
/// enum). dom::StyleValue and Color can't be payloads.
///
/// @note  A handler for a payload of your own ValueTraits type must take a dd::Value (see
///        ValueTraits).
///
template <typename P>
concept ScalarPayload = Bindable<P>
    && (ValueTraits<std::remove_cvref_t<P>>::kind == ValueKind::Bool
        || ValueTraits<std::remove_cvref_t<P>>::kind == ValueKind::Int64
        || ValueTraits<std::remove_cvref_t<P>>::kind == ValueKind::Double
        || ValueTraits<std::remove_cvref_t<P>>::kind == ValueKind::String);

}  // namespace data
}  // namespace dom
}  // namespace ultralight
