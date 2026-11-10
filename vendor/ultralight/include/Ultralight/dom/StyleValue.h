///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
// X11's headers define None as a macro, which would break CssUnit::None below.
#pragma push_macro("None")
#undef None

#include <Ultralight/CAPI/CAPI_DOMElement.h>
#include <Ultralight/detail/Css.h>

#include <limits>
#include <string_view>

namespace ultralight {
namespace dom {

///
/// The unit of a StyleValue (the same values as the C API's ULDOMStyleUnit).
///
enum class StyleUnit : unsigned {
  Number = 0,  ///< A unitless number (CSS `<number>`).
  Percent,     ///< `%`
  Px,          ///< `px`
  Em,          ///< `em`
  Rem,         ///< `rem`
  Vw,          ///< `vw`
  Vh,          ///< `vh`
  Deg,         ///< `deg`
  Ms,          ///< `ms`
  S,           ///< `s`
  Empty,       ///< No number (an Empty StyleValue; writing it removes the property).
};

static_assert(static_cast<unsigned>(StyleUnit::Number) == kULDOMStyleUnit_Number
                  && static_cast<unsigned>(StyleUnit::Percent) == kULDOMStyleUnit_Percent
                  && static_cast<unsigned>(StyleUnit::Px) == kULDOMStyleUnit_Px
                  && static_cast<unsigned>(StyleUnit::Em) == kULDOMStyleUnit_Em
                  && static_cast<unsigned>(StyleUnit::Rem) == kULDOMStyleUnit_Rem
                  && static_cast<unsigned>(StyleUnit::Vw) == kULDOMStyleUnit_Vw
                  && static_cast<unsigned>(StyleUnit::Vh) == kULDOMStyleUnit_Vh
                  && static_cast<unsigned>(StyleUnit::Deg) == kULDOMStyleUnit_Deg
                  && static_cast<unsigned>(StyleUnit::Ms) == kULDOMStyleUnit_Ms
                  && static_cast<unsigned>(StyleUnit::S) == kULDOMStyleUnit_S
                  && static_cast<unsigned>(StyleUnit::Empty) == kULDOMStyleUnit_Empty,
              "dom::StyleUnit must mirror ULDOMStyleUnit");

/// \cond INTERNAL
namespace detail {

// The StyleUnit for a parsed CSS unit. Returns false for a real CSS unit that has no
// StyleUnit (pt, cm, fr, ...).
constexpr bool StyleUnitOf(ultralight::detail::CssUnit css_unit, StyleUnit* out) {
  using ultralight::detail::CssUnit;
  switch (css_unit) {
    case CssUnit::None: *out = StyleUnit::Number; return true;
    case CssUnit::Percent: *out = StyleUnit::Percent; return true;
    case CssUnit::Px: *out = StyleUnit::Px; return true;
    case CssUnit::Em: *out = StyleUnit::Em; return true;
    case CssUnit::Rem: *out = StyleUnit::Rem; return true;
    case CssUnit::Vw: *out = StyleUnit::Vw; return true;
    case CssUnit::Vh: *out = StyleUnit::Vh; return true;
    case CssUnit::Deg: *out = StyleUnit::Deg; return true;
    case CssUnit::Ms: *out = StyleUnit::Ms; return true;
    case CssUnit::S: *out = StyleUnit::S; return true;
    default: return false;
  }
}

// The CSS text of a unit ("" for a unitless number and for Empty).
constexpr const char* StyleUnitText(StyleUnit unit) {
  switch (unit) {
    case StyleUnit::Percent: return "%";
    case StyleUnit::Px: return "px";
    case StyleUnit::Em: return "em";
    case StyleUnit::Rem: return "rem";
    case StyleUnit::Vw: return "vw";
    case StyleUnit::Vh: return "vh";
    case StyleUnit::Deg: return "deg";
    case StyleUnit::Ms: return "ms";
    case StyleUnit::S: return "s";
    case StyleUnit::Number:
    case StyleUnit::Empty: return "";
  }
  return "";
}

}  // namespace detail
/// \endcond

///
/// A numeric CSS value and its unit.
///
/// dom::StyleValue pairs a number with a CSS unit for styling page elements from native code. It
/// lets you write numeric values directly to an element's inline style without formatting strings,
/// and read numeric properties back from inline or computed styles.
///
/// You can update a health bar with computed values:
///
/// ```
/// dom::Element bar = document.getElementById("health-bar");
/// bar.style.width = dom::StyleValue::Pct(health * 100);
/// bar.style.left = dom::StyleValue::Px(x);
/// bar.style.opacity = 0.8;  // a bare number has no unit
/// ```
///
/// The library still parses the value as CSS when applying it to the element.
///
/// ## Reading Styles and Value States
///
/// Calling AsStyleValue() on an inline or computed style parses the property into a
/// dom::StyleValue:
///
/// ```
/// dom::StyleValue width = dom::getComputedStyle(bar).width.AsStyleValue();
/// if (width)
///   PlaceMinimap(width.value);  // 300 when the width is "300px"
/// ```
///
/// A dom::StyleValue is always in one of these states:
///
/// - **A number holds a finite value and a StyleUnit.** Writing it sets the property.
/// - **Empty holds no value.** Writing it removes the inline property.
/// - **Invalid represents text that doesn't convert to a number (eg, `auto`) or a number that isn't
///   finite.** Writing it does nothing and keeps the existing style.
///
/// Checking a dom::StyleValue with `if (value)` evaluates to true only when it holds a number.
///
/// You can copy an inline style directly between elements by assigning what you read:
///
/// ```
/// icon.style.width = badge.style.width.AsStyleValue();  // removes icon's width
///                                                       // if badge has none
/// ```
///
/// ## Unsupported Units and Custom Properties
///
/// - **Properties ignore unsupported units.** Assigning a unit that a property doesn't accept (such
///   as pixels for `opacity` or a unitless number for `width`) does nothing and keeps the previous
///   value. When DOM diagnostics are on, the library logs a warning for each ignored write.
/// - **Custom properties require text.** Assigning a dom::StyleValue to a custom property
///   (`--name`) does nothing, so assign text like `"8px"` instead.
///
/// @see  dom::Element::style, dom::getComputedStyle(), dom::StyleUnit, dom::CssValue
///
struct StyleValue {
  double value = 0;                  ///< The number (only meaningful when this holds one).
  StyleUnit unit = StyleUnit::Empty; ///< The number's unit (StyleUnit::Empty when it's Empty).

  ///
  /// Create an Empty StyleValue (it holds nothing, and writing it removes the property).
  ///
  constexpr StyleValue() = default;

  ///
  /// Create a unitless number (CSS `<number>`). A bare number converts to this implicitly.
  ///
  /// @param  value  The number.
  ///
  constexpr StyleValue(double value) : value(value), unit(StyleUnit::Number) {}

  ///
  /// Create a number with a unit. The unit factories (eg, Px()) are shorter.
  ///
  /// @param  value  The number.
  ///
  /// @param  unit   The number's unit.
  ///
  constexpr StyleValue(double value, StyleUnit unit) : value(value), unit(unit) {}

  ///
  /// Parse CSS text into a StyleValue.
  ///
  /// The text must be a single number with no unit or a StyleUnit unit. Style members and reads
  /// by name have AsStyleValue(), which does the same.
  ///
  /// ```
  /// StyleValue::Parse("300px");   // 300, StyleUnit::Px
  /// StyleValue::Parse("0.5");     // 0.5, StyleUnit::Number
  /// StyleValue::Parse("");        // Empty
  /// StyleValue::Parse("auto");    // Invalid (a keyword)
  /// StyleValue::Parse("12pt");    // Invalid (pt isn't a StyleUnit)
  /// ```
  ///
  /// @param  text  The CSS text.
  ///
  /// @return  Returns the number with its unit, an Empty StyleValue when `text` is empty, or an
  ///          Invalid one when `text` doesn't convert.
  ///
  static constexpr StyleValue Parse(std::string_view text) {
    if (text.empty())
      return StyleValue();
    StyleUnit parsed_unit = StyleUnit::Number;
    if (ultralight::detail::IsLoneNumericToken(text.data(), text.size())) {
      const ultralight::detail::CssNumber num
          = ultralight::detail::ParseCssNumber(text.data(), text.size());
      if (num.status == ultralight::detail::CssStatus::Ok
          && detail::StyleUnitOf(num.unit, &parsed_unit))
        return StyleValue(num.value, parsed_unit);
    }
    return StyleValue(std::numeric_limits<double>::quiet_NaN());
  }

  ///
  /// Whether or not this StyleValue is Empty (it holds nothing).
  ///
  constexpr bool IsEmpty() const { return unit == StyleUnit::Empty; }

  ///
  /// Whether or not this StyleValue is Invalid (it came from text that doesn't convert, or its
  /// value isn't finite).
  ///
  constexpr bool IsInvalid() const { return !IsEmpty() && !IsFinite(value); }

  ///
  /// Whether or not this StyleValue holds a number (it's neither Empty nor Invalid).
  ///
  constexpr explicit operator bool() const { return !IsEmpty() && IsFinite(value); }

  ///
  /// Create a unitless number (CSS `<number>`).
  ///
  /// @param  v  The number.
  ///
  static constexpr StyleValue Number(double v) { return { v, StyleUnit::Number }; }

  ///
  /// Create a length in CSS pixels (`px`).
  ///
  /// @param  v  The number of pixels.
  ///
  static constexpr StyleValue Px(double v) { return { v, StyleUnit::Px }; }

  ///
  /// Create a percentage (`%`).
  ///
  /// @param  v  The percentage (50 means 50%).
  ///
  static constexpr StyleValue Pct(double v) { return { v, StyleUnit::Percent }; }

  ///
  /// Create a multiple of the element's font size (`em`).
  ///
  /// @param  v  The multiple.
  ///
  static constexpr StyleValue Em(double v) { return { v, StyleUnit::Em }; }

  ///
  /// Create a multiple of the root element's font size (`rem`).
  ///
  /// @param  v  The multiple.
  ///
  static constexpr StyleValue Rem(double v) { return { v, StyleUnit::Rem }; }

  ///
  /// Create a percentage of the viewport width (`vw`).
  ///
  /// @param  v  The percentage (100 is the full width).
  ///
  static constexpr StyleValue Vw(double v) { return { v, StyleUnit::Vw }; }

  ///
  /// Create a percentage of the viewport height (`vh`).
  ///
  /// @param  v  The percentage (100 is the full height).
  ///
  static constexpr StyleValue Vh(double v) { return { v, StyleUnit::Vh }; }

  ///
  /// Create an angle in degrees (`deg`).
  ///
  /// @param  v  The number of degrees.
  ///
  static constexpr StyleValue Deg(double v) { return { v, StyleUnit::Deg }; }

  ///
  /// Create a time in milliseconds (`ms`).
  ///
  /// @param  v  The number of milliseconds.
  ///
  static constexpr StyleValue Ms(double v) { return { v, StyleUnit::Ms }; }

  ///
  /// Create a time in seconds (`s`).
  ///
  /// @param  v  The number of seconds.
  ///
  static constexpr StyleValue S(double v) { return { v, StyleUnit::S }; }

 private:
  static constexpr bool IsFinite(double v) {
    return v >= -std::numeric_limits<double>::max() && v <= std::numeric_limits<double>::max();
  }
};

///
/// Compile-time wrapper for CSS string literals assigned to element styles.
///
/// CssValue wraps CSS string literals assigned to inline styles, which the library parses when
/// written. It checks the literal at compile time to catch mistyped numbers and units before your
/// code runs.
///
/// String literals convert to CssValue on their own when assigned to style properties:
///
/// ```
/// hud.style.width = "50%";       // a number: checked when you compile
/// hud.style.margin = "0 auto";   // other CSS: parsed when it's written
/// hud.style.fontSize = "12pt";   // a unit StyleValue doesn't have: parsed when it's written
/// // hud.style.width = "50pxx";  // a typo: compile error
/// ```
///
/// ## Strings Built at Run Time
///
/// Style properties provide a separate overload for dynamic values, parsing the CSS text when it's
/// written.
///
/// Wrap a char array filled at run time in std::string_view so the compiler doesn't treat the
/// buffer as a string literal and fail to compile:
///
/// ```
/// char text[16];
/// std::snprintf(text, sizeof(text), "%dpx", bar_width);
/// hud.style.width = std::string_view(text);
/// ```
///
/// @note  On older MSVC toolsets (before Visual Studio 2026), a mistyped literal causes a compile
///        error only in a constant expression. In ordinary style writes, it's ignored when written.
///
/// @see  dom::StyleValue, dom::Element::style
///
class CssValue {
 public:
  ///
  /// How the literal is written-- either parsed as a number at compile time (Numeric) or passed
  /// to the library's CSS parser as text (Passthrough).
  ///
  enum class Kind : uint8_t { Passthrough = 0, Numeric };

  ///
  /// Capture a string literal and parse it at compile time.
  ///
  /// @param  s  The string literal.
  ///
  template <size_t N>
  UL_CSS_CONSTEVAL CssValue(const char (&s)[N]) : text_(s), length_(N - 1) {
    if (!ultralight::detail::IsLoneNumericToken(s, N - 1))
      return;
    const ultralight::detail::CssNumber num = ultralight::detail::ParseCssNumber(s, N - 1);
    if (num.status != ultralight::detail::CssStatus::Ok) {
      // A lone numeric token that fails strict parsing is a typo, not a compound value.
      // On the UL_CSS_CONSTEVAL fallback arm it degrades to passthrough at runtime,
      // where the engine's CSS parser rejects it with the web's own silence.
      if (std::is_constant_evaluated())
        ultralight::detail::RaiseCssError(num.status);
      return;
    }
    // A real CSS unit with no StyleUnit (pt, cm, fr, ...) is valid CSS, carried as
    // passthrough text.
    StyleUnit style_unit = StyleUnit::Number;
    if (detail::StyleUnitOf(num.unit, &style_unit))
      SetNumeric(num.value, style_unit);
  }

  ///
  /// Get how the literal is written (see Kind).
  ///
  constexpr Kind kind() const { return kind_; }

  ///
  /// Get the parsed number (only meaningful when kind() is Kind::Numeric).
  ///
  constexpr double value() const { return value_; }

  ///
  /// Get the parsed unit (only meaningful when kind() is Kind::Numeric).
  ///
  constexpr StyleUnit unit() const { return unit_; }

  ///
  /// Get the literal's text as written (null-terminated).
  ///
  constexpr const char* text() const { return text_; }

  ///
  /// Get the literal's length in bytes.
  ///
  constexpr size_t length() const { return length_; }

 private:
  constexpr void SetNumeric(double value, StyleUnit unit) {
    kind_ = Kind::Numeric;
    value_ = value;
    unit_ = unit;
  }

  const char* text_ = nullptr;
  size_t length_ = 0;
  double value_ = 0.0;
  StyleUnit unit_ = StyleUnit::Number;
  Kind kind_ = Kind::Passthrough;
};

}  // namespace dom
}  // namespace ultralight

#pragma pop_macro("None")
