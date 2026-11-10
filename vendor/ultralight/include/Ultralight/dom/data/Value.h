///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/CAPI/CAPI_DOMData.h>
#include <Ultralight/dom/Error.h>
#include <Ultralight/dom/data/ValueTraits.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace ultralight {
namespace dom {
namespace data {

/// \cond INTERNAL
namespace detail {
template <typename T>
inline constexpr bool kNoValueConversion = false;
}
/// \endcond

///
/// Get a Result's value or a default-constructed T if it holds an error (see dom::OrEmpty()).
///
using ultralight::dom::OrEmpty;

///
/// A generic value passed to data-binding formatters and handlers.
///
/// A Value represents incoming data in formatters and in action and change handlers that take a
/// generic parameter instead of a concrete C++ type.
///
/// DefineFormat() passes the bound field to your formatter as a Value:
///
/// ```
/// // Formats 12500 as "12,500".
/// ctx.DefineFormat("comma", [](dd::Value v) {
///   std::string digits = std::to_string(v.Or(int64_t(0)));
///   for (int i = int(digits.size()) - 3; i > 0; i -= 3)
///     digits.insert(i, ",");
///   return digits;
/// });
/// ```
///
/// ## What a Value Holds
///
/// An instance represents one of several data shapes:
///
/// - **An empty state holds no data.** Actions dispatched without a payload arrive empty, including
///   every action fired from markup.
/// - **A leaf holds a single value.** Formatters, change handlers, and scalar action payloads
///   provide one boolean, numeric, or string value.
/// - **An object holds named members.** Actions dispatched with a payload struct provide fields
///   that you look up by name.
///
/// Brackets look up an object's members by name:
///
/// ```
/// binding.OnAction<"moveItem">([&](dd::Value request) {
///   int from = request["from"].Or(0);
///   int to = request["to"].Or(0);
///   bag.MoveItem(from, to);
/// });
/// ```
///
/// ## Converting Values
///
/// Choose the extraction method that matches your error-handling style:
///
/// - **To() returns a Result.** Failure yields a TypeError explaining the mismatch.
/// - **Maybe() returns an optional.** Failure yields `std::nullopt` without an error.
/// - **Or() returns a fallback.** Failure returns the default value you supplied.
///
/// Conversions target bool, integer types, floating-point types, String, std::string, and reflected
/// enums. Integer values convert to floating-point types, but floating-point values don't convert
/// to integers. Reflected enums convert from string values using their enumerator names.
///
/// Indexing a missing member or an empty Value returns an empty Value. Because conversions fail
/// safely on empty instances, you don't need to check Contains() or IsEmpty() before reading a
/// member with Or() or Maybe().
///
/// @note  dom::data::Value and js::Value are unrelated. A dom::data::Value holds an owned copy of
///        native data rather than a handle into a page's JavaScript context.
///
/// @see  dom::data::Binding::OnAction(), dom::data::Binding::OnChange(),
///       dom::data::Context::DefineFormat(), dom::data::ActionInfo
///
class Value {
 public:
  ///
  /// Create an empty Value.
  ///
  Value() = default;

  ///
  /// Whether or not this Value holds nothing.
  ///
  bool IsEmpty() const { return state_ == State::kEmpty; }

  ///
  /// Whether or not this Value holds something (the opposite of IsEmpty()).
  ///
  explicit operator bool() const { return !IsEmpty(); }

  ///
  /// Whether or not this Value is a boolean leaf.
  ///
  bool IsBoolean() const { return state_ == State::kLeaf && kind_ == ValueKind::Bool; }

  ///
  /// Whether or not this Value is an integer leaf.
  ///
  bool IsInt64() const { return state_ == State::kLeaf && kind_ == ValueKind::Int64; }

  ///
  /// Whether or not this Value is a floating-point leaf.
  ///
  bool IsDouble() const { return state_ == State::kLeaf && kind_ == ValueKind::Double; }

  ///
  /// Whether or not this Value is a string leaf.
  ///
  bool IsString() const { return state_ == State::kLeaf && kind_ == ValueKind::String; }

  ///
  /// Whether or not this Value is an object (the members of an action's payload struct).
  ///
  bool IsObject() const { return state_ == State::kObject; }

  ///
  /// Get a member of an object.
  ///
  /// @param  name  The member's name.
  ///
  /// @return  Returns a copy of the member's value or an empty Value if this Value isn't an
  ///          object or has no such member.
  ///
  Value operator[](const char* name) const {
    if (state_ == State::kObject && name) {
      for (const auto& member : members_) {
        if (member.first == name)
          return member.second;
      }
    }
    return Value();
  }

  ///
  /// Whether or not this Value is an object with a member of the given name.
  ///
  /// @param  name  The member's name.
  ///
  bool Contains(const char* name) const {
    if (state_ != State::kObject || !name)
      return false;
    for (const auto& member : members_) {
      if (member.first == name)
        return true;
    }
    return false;
  }

  ///
  /// Get the number of members (0 unless this Value is an object).
  ///
  size_t member_count() const { return members_.size(); }

  ///
  /// Convert to a C++ type (see "Converting Values" in the class description).
  ///
  /// @return  Returns the converted value. Fails with a TypeError if this Value is empty or
  ///          isn't a leaf of the requested kind.
  ///
  template <typename T>
  [[nodiscard]] Result<T> To() const {
    std::optional<T> value = Maybe<T>();
    if (value)
      return std::move(*value);
    return Unexpected<Error>(Error::Make(ErrorType::TypeError, MismatchMessage<T>()));
  }

  ///
  /// Convert to a C++ type like To() but without the failure reason.
  ///
  /// @return  Returns the converted value or nullopt on any failure.
  ///
  template <typename T>
  [[nodiscard]] std::optional<T> Maybe() const {
    static_assert(!std::is_same_v<std::remove_cvref_t<T>, std::string_view>
                      && !std::is_same_v<std::remove_cvref_t<T>, const char*>,
                  "a Value conversion must produce an OWNING type; read strings as "
                  "String or std::string");
    using Plain = std::remove_cvref_t<T>;
    if (state_ != State::kLeaf)
      return std::nullopt;
    if constexpr (std::is_same_v<Plain, bool>) {
      if (kind_ == ValueKind::Bool)
        return bool_;
    } else if constexpr (std::is_integral_v<Plain>) {
      if (kind_ == ValueKind::Int64)
        return static_cast<Plain>(int_);
    } else if constexpr (std::is_floating_point_v<Plain>) {
      if (kind_ == ValueKind::Double)
        return static_cast<Plain>(double_);
      if (kind_ == ValueKind::Int64)
        return static_cast<Plain>(int_);
    } else if constexpr (std::is_same_v<Plain, ultralight::String>) {
      if (kind_ == ValueKind::String)
        return string_;
    } else if constexpr (std::is_same_v<Plain, std::string>) {
      if (kind_ == ValueKind::String) {
        const ultralight::String8& utf8 = const_cast<ultralight::String&>(string_).utf8();
        return std::string(utf8.data(), utf8.length());
      }
#if ULTRALIGHT_REFLECTION
    } else if constexpr (std::is_enum_v<Plain>) {
      if (kind_ == ValueKind::String) {
        const ultralight::String8& utf8 = const_cast<ultralight::String&>(string_).utf8();
        const std::string_view name(utf8.data(), utf8.length());
        for (const auto& entry : detail::EnumInfo<Plain>::kEntries) {
          if (entry.name == name)
            return static_cast<Plain>(entry.value);
        }
      }
#endif
    } else {
      static_assert(detail::kNoValueConversion<Plain>,
                    "unsupported Value conversion target: expected bool, an integral or "
                    "floating-point type, String, std::string, or a reflected enum");
    }
    return std::nullopt;
  }

  ///
  /// Convert to a C++ type like To() but with a fallback.
  ///
  /// @param  fallback  The value to return if the conversion fails.
  ///
  /// @return  Returns the converted value or `fallback` on any failure.
  ///
  template <typename T>
  [[nodiscard]] T Or(T fallback) const {
    std::optional<T> value = Maybe<T>();
    return value ? std::move(*value) : std::move(fallback);
  }

  ///
  /// Convert to a std::string like To() but with a string-literal fallback.
  ///
  /// @param  fallback  The text to return if the conversion fails (nullptr returns an empty
  ///                   string).
  ///
  /// @return  Returns the converted string or `fallback` on any failure.
  ///
  [[nodiscard]] std::string Or(const char* fallback) const {
    std::optional<std::string> value = Maybe<std::string>();
    return value ? std::move(*value) : std::string(fallback ? fallback : "");
  }

  // --- Interop with the C API (most embedders never touch raw handles) -------------------

  ///
  /// Create a leaf Value from a C leaf value.
  ///
  /// You only need this when you receive values through the C API (eg, in a change callback
  /// you set with ulDOMDataBindingSetChangeCallback()).
  ///
  /// @param  value  The C value. Its string is copied (exactly `string_length` bytes).
  ///
  /// @return  Returns the leaf Value. A color, a StyleValue, or a kind that isn't a leaf gives
  ///          an empty Value.
  ///
  static Value FromLeaf(const ULDOMDataLeafValue& value) {
    Value result;
    switch (static_cast<ValueKind>(value.kind)) {
      case ValueKind::Bool:
        result.state_ = State::kLeaf;
        result.kind_ = ValueKind::Bool;
        result.bool_ = value.bool_value;
        break;
      case ValueKind::Int64:
        result.state_ = State::kLeaf;
        result.kind_ = ValueKind::Int64;
        result.int_ = value.int64_value;
        break;
      case ValueKind::Double:
        result.state_ = State::kLeaf;
        result.kind_ = ValueKind::Double;
        result.double_ = value.double_value;
        break;
      case ValueKind::String: {
        result.state_ = State::kLeaf;
        result.kind_ = ValueKind::String;
        result.string_ = value.string_value
            ? ultralight::String(value.string_value, value.string_length)
            : ultralight::String();
        break;
      }
      default:
        // Non-leaf kinds, and the StyleValue/Color leaves, have no Value form; the
        // result stays empty (typed delivery hands those kinds over in their own types).
        break;
    }
    return result;
  }

  ///
  /// Create an object Value from a C action payload.
  ///
  /// @param  payload        The payload's members (members with a null name are skipped).
  ///
  /// @param  payload_count  The number of members in `payload`.
  ///
  /// @return  Returns the object Value or an empty Value if `payload` is nullptr or
  ///          `payload_count` is 0.
  ///
  /// @note  A scalar payload arrives as one member named `value`, so this gives an object with
  ///        that one member (the typed handlers deliver a leaf instead).
  ///
  static Value FromPayload(const ULDOMDataPayloadEntry* payload, size_t payload_count) {
    Value result;
    if (!payload || payload_count == 0)
      return result;
    result.state_ = State::kObject;
    result.members_.reserve(payload_count);
    for (size_t i = 0; i < payload_count; i++) {
      if (!payload[i].name)
        continue;
      result.members_.emplace_back(payload[i].name, FromLeaf(payload[i].value));
    }
    return result;
  }

 private:
  enum class State : uint8_t { kEmpty, kLeaf, kObject };

  template <typename T>
  const char* MismatchMessage() const {
    if (IsEmpty())
      return "cannot convert an empty Value";
    if constexpr (std::is_same_v<std::remove_cvref_t<T>, bool>)
      return "expected a boolean Value";
    else if constexpr (std::is_integral_v<std::remove_cvref_t<T>>)
      return "expected an integer Value";
    else if constexpr (std::is_floating_point_v<std::remove_cvref_t<T>>)
      return "expected a numeric Value";
    else
      return "expected a string Value";
  }

  State state_ = State::kEmpty;
  ValueKind kind_ = ValueKind::Bool;
  bool bool_ = false;
  int64_t int_ = 0;
  double double_ = 0;
  ultralight::String string_;
  std::vector<std::pair<std::string, Value>> members_;
};

}  // namespace data
}  // namespace dom
}  // namespace ultralight
