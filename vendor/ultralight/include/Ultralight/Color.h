///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/CAPI/CAPI_ColorTypes.h>
#include <Ultralight/Defines.h>
#include <Ultralight/String.h>
#include <Ultralight/detail/Css.h>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace ultralight {

/// \cond INTERNAL
namespace detail {

// A string type that holds UTF-8 bytes it exposes through data() and size() (std::string,
// std::string_view, String8, ...), so a header-only overload can hand the bytes to an exported
// function without an STL type crossing the ABI. String is excluded: it has its own overloads.
template <class T>
concept Utf8StringLike = !std::is_same_v<std::remove_cvref_t<T>, String>
                         && requires(const T& text) {
                              { text.data() } -> std::convertible_to<const char*>;
                              { text.size() } -> std::convertible_to<size_t>;
                            };

}  // namespace detail
/// \endcond

///
/// An RGBA color value in a certain color space (with CSS parsing helpers).
///
/// Colors are stored as floats [0..1] in the sRGB color space with straight alpha.
///
/// ## Creating Colors
///
/// You can create Colors from float RGBA (0..1) channels:
///
/// ```
/// Color green = Color(0.0f, 1.0f, 0.0f);  // opaque green
/// Color semi  = Color(1.0f, 1.0f, 1.0f, 0.1f);  // semi-transparent white
/// ```
///
/// CSS string literals (parsed and validated at compile-time, see below for limitations):
///
/// ```
/// Color ultramarine = "#3f00ff"; // compile-time literal
/// Color alpaca      = "rgb(255, 240, 220)"; // compile-time literal
/// ```
///
/// Runtime CSS strings (supports the full CSS color grammar, including named colors):
///
/// ```
/// Color themed = Color::Parse(user_text); // a String, std::string, std::string_view, ...
/// ```
///
/// ## Limitations of Compile-Time CSS Literals
///
/// String literals accept hex, `rgb()` / `rgba()`, and `transparent`; anything else (for example,
/// `hsl()` or named colors like `purple`) is not supported. Use Parse() if you need the full CSS
/// color grammar.
///
/// @note  Default-constructed Colors are unset (test with is_set(), or `if (color)`).
///
class Color {
 public:
  ///
  /// The color space of the channels.
  ///
  enum Space : uint8_t {
    kSRGB = 0,  ///< sRGB (the only value currently produced or accepted).
  };

  ///
  /// State bits carried in `flags`.
  ///
  enum Flags : uint8_t {
    kSet = 1 << 0,      ///< Explicitly set (unset means the receiving field's default applies).
    kInvalid = 1 << 1,  ///< Came from a string that failed to parse; ignored with a warning.
  };

  float r = 0.0f;         ///< Red, 0..1, in the space named by `space`.
  float g = 0.0f;         ///< Green, 0..1.
  float b = 0.0f;         ///< Blue, 0..1.
  float a = 0.0f;         ///< Alpha, 0..1, linear (1 is fully opaque).
  uint8_t space = kSRGB;  ///< A Space value.
  uint8_t flags = 0;      ///< A Flags combination (see is_set() and invalid()).

  ///
  /// Create an unset Color (the receiving field's default applies).
  ///
  constexpr Color() = default;

  ///
  /// Create a set sRGB color from float channels (0..1; alpha 1 is fully opaque).
  ///
  constexpr Color(float red, float green, float blue, float alpha = 1.0f)
      : r(red), g(green), b(blue), a(alpha), flags(kSet) {}

  ///
  /// Create a Color from a CSS string literal, parsed and validated at compile time.
  ///
  /// Accepted forms are hex colors (eg, `#ff00ff`), rgb()/rgba(), and `transparent`. Anything else
  /// is a compile-time error naming the reason. Only string literals bind here, runtime strings go
  /// through Parse().
  ///
  template <size_t N>
  UL_CSS_CONSTEVAL Color(const char (&s)[N]) {
    const detail::CssColor c = detail::ParseCssColor(s, N - 1);
    if (c.status != detail::CssStatus::Ok) {
      if (std::is_constant_evaluated())
        detail::RaiseCssError(c.status);
      flags = kInvalid;
      return;
    }
    r = c.r;
    g = c.g;
    b = c.b;
    a = c.a;
    flags = kSet;
  }

  ///
  /// Create a Color from its C mirror (implicit; the two carry identical state).
  ///
  constexpr Color(const ULColor& c)
      : r(c.r), g(c.g), b(c.b), a(c.a), space(c.space), flags(c.flags) {}

  ///
  /// Convert to the C mirror (implicit; the two carry identical state).
  ///
  constexpr operator ULColor() const { return ULColor { r, g, b, a, space, flags }; }

  ///
  /// Parse a CSS color string at runtime: the full CSS color grammar (named colors, hex,
  /// rgb()/hsl()/hwb()/lab()/oklch()/color(), `transparent`). Wide-gamut input narrows to
  /// sRGB.
  ///
  /// @param  css  The CSS color text to parse.
  ///
  /// @return  Returns the parsed color (set) on success, or an invalid Color when the
  ///          string does not parse (see invalid()).
  ///
  static UExport Color Parse(const String& css);

  ///
  /// Parse CSS color text held in any UTF-8 string type with `data()` and `size()` (eg,
  /// std::string or std::string_view), the same as Parse(const String&).
  ///
  /// @param  css  The CSS color text to parse, as UTF-8.
  ///
  /// @return  Returns the parsed color (set) on success, or an invalid Color when the
  ///          string does not parse (see invalid()).
  ///
  template <detail::Utf8StringLike S>
  static Color Parse(const S& css) {
    return Parse(String(css.data(), css.size()));
  }

  ///
  /// Serialize this color as CSS hex text: lowercase `#rrggbb`, or `#rrggbbaa` when the
  /// alpha channel is not fully opaque. Float channels quantize to 8-bit here (the hex
  /// wire form is 8-bit), so Parse(ToHexString()) round-trips to the quantized channels.
  ///
  /// @return  Returns the hex text, or an empty string when this color is unset or
  ///          invalid (see is_set() and invalid()).
  ///
  /// @see  Parse()
  ///
  UExport String ToHexString() const;

  ///
  /// Whether or not this color was explicitly set. An unset color in any options struct or
  /// setter means the built-in default (writing one to a style property removes the property).
  ///
  constexpr bool is_set() const { return (flags & kSet) != 0; }

  ///
  /// Whether or not this color came from a string that failed to parse (Parse() at runtime,
  /// or a malformed literal on a compiler where literal validation degrades to a runtime
  /// flag). Invalid colors are inert-- whatever you pass one to ignores it and logs a warning.
  ///
  constexpr bool invalid() const { return (flags & kInvalid) != 0; }

  ///
  /// Whether or not this color is set and valid.
  ///
  constexpr explicit operator bool() const { return is_set() && !invalid(); }

  friend constexpr bool operator==(const Color&, const Color&) = default;
};

static_assert(sizeof(Color) == sizeof(ULColor), "Color must mirror ULColor");

}  // namespace ultralight
