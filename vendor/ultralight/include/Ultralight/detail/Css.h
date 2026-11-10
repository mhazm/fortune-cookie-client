///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// Implementation detail shared by the typed C++ header families (`<Ultralight/Color.h>`,
/// `<Ultralight/dom/*.h>`, `<AppCore/layout/Length.h>`); include those instead. Everything here
/// lives in ultralight::detail (diagnostic anchors in ultralight::css_error) and may change
/// without notice.
///
/// \cond INTERNAL
#pragma once
// X11's headers define None as a macro, which would break the None enumerators.
#pragma push_macro("None")
#undef None

#include <cstddef>
#include <cstdint>
#include <type_traits>

// UL_CSS_CONSTEVAL: consteval everywhere except MSVC toolsets whose handling of consteval
// constructors inside block-scope aggregate initialization is defective (valid code such as
// `{ .size = "240px" }` is rejected with a bare C7595; fixed in the VS 2026 toolset, 19.51).
// On the defective range the literal constructors compile as constexpr instead: constant
// contexts still hard-fail on malformed literals (the css_error anchors fire under
// is_constant_evaluated), and a malformed literal outside a constant context sets the value's
// invalid flag, which every ingestion site checks and Warning-logs.
#if defined(_MSC_VER) && !defined(__clang__) && _MSC_VER < 1951
#define UL_CSS_CONSTEVAL constexpr
#define UL_CSS_CONSTEVAL_IS_FALLBACK 1
#else
#define UL_CSS_CONSTEVAL consteval
#define UL_CSS_CONSTEVAL_IS_FALLBACK 0
#endif

namespace ultralight {

// Compile-time diagnostic anchors. Deliberately NOT constexpr: a constant evaluation that
// reaches one cannot be a constant expression, and every compiler prints the offending
// function's qualified name, so the name IS the diagnostic. Keep the names reading as
// sentences; they are the entire actionable content of the error on MSVC.
namespace css_error {
inline void expected_a_css_number() {}
inline void unknown_css_unit() {}
inline void unit_not_supported_in_layout() {}
inline void viewport_units_not_supported_yet() {}
inline void fr_is_only_valid_for_a_track_size() {}
inline void percent_is_not_valid_here() {}
inline void expected_an_fr_flex_factor() {}
inline void expected_a_color_literal() {}
inline void named_colors_are_runtime_only_use_Color_Parse() {}
inline void invalid_hex_color_literal() {}
inline void invalid_rgb_color_literal() {}
}  // namespace css_error

namespace detail {

enum class CssStatus : uint8_t {
  Ok,
  ExpectedNumber,
  UnknownUnit,
  UnitNotSupportedInLayout,
  ViewportUnitDeferred,
  FrNotValidHere,
  PercentNotValidHere,
  ExpectedFr,
  ExpectedColor,
  NamedColorRuntimeOnly,
  InvalidHexColor,
  InvalidRgbColor,
};

enum class CssUnit : uint8_t {
  None,  // bare number
  Px,
  Percent,
  Fr,
  Em,
  Rem,
  Vw,
  Vh,
  Deg,
  Ms,
  S,
  // Recognized so a parser can reject or pass them through by name rather than as garbage.
  Ex,
  Ch,
  Vmin,
  Vmax,
  Pt,
  Pc,
  In,
  Cm,
  Mm,
  Q,
  Rad,
  Turn,
  Unknown,
};

constexpr bool IsDigit(char c) { return c >= '0' && c <= '9'; }

constexpr char ToAsciiLower(char c) { return (c >= 'A' && c <= 'Z') ? char(c + 32) : c; }

// CSS keywords, function names, and units are ASCII case-insensitive.
constexpr bool TokenIsCI(const char* tok, size_t len, const char* lit) {
  size_t i = 0;
  for (; i < len; ++i) {
    if (lit[i] == '\0' || ToAsciiLower(tok[i]) != lit[i])
      return false;
  }
  return lit[i] == '\0';
}

constexpr CssUnit ClassifyUnit(const char* tok, size_t len) {
  if (len == 0)
    return CssUnit::None;
  if (TokenIsCI(tok, len, "px"))
    return CssUnit::Px;
  if (TokenIsCI(tok, len, "%"))
    return CssUnit::Percent;
  if (TokenIsCI(tok, len, "fr"))
    return CssUnit::Fr;
  if (TokenIsCI(tok, len, "em"))
    return CssUnit::Em;
  if (TokenIsCI(tok, len, "rem"))
    return CssUnit::Rem;
  if (TokenIsCI(tok, len, "ex"))
    return CssUnit::Ex;
  if (TokenIsCI(tok, len, "ch"))
    return CssUnit::Ch;
  if (TokenIsCI(tok, len, "vw"))
    return CssUnit::Vw;
  if (TokenIsCI(tok, len, "vh"))
    return CssUnit::Vh;
  if (TokenIsCI(tok, len, "vmin"))
    return CssUnit::Vmin;
  if (TokenIsCI(tok, len, "vmax"))
    return CssUnit::Vmax;
  if (TokenIsCI(tok, len, "pt"))
    return CssUnit::Pt;
  if (TokenIsCI(tok, len, "pc"))
    return CssUnit::Pc;
  if (TokenIsCI(tok, len, "in"))
    return CssUnit::In;
  if (TokenIsCI(tok, len, "cm"))
    return CssUnit::Cm;
  if (TokenIsCI(tok, len, "mm"))
    return CssUnit::Mm;
  if (TokenIsCI(tok, len, "q"))
    return CssUnit::Q;
  if (TokenIsCI(tok, len, "deg"))
    return CssUnit::Deg;
  if (TokenIsCI(tok, len, "rad"))
    return CssUnit::Rad;
  if (TokenIsCI(tok, len, "turn"))
    return CssUnit::Turn;
  if (TokenIsCI(tok, len, "s"))
    return CssUnit::S;
  if (TokenIsCI(tok, len, "ms"))
    return CssUnit::Ms;
  return CssUnit::Unknown;
}

// CSS's number-start rule: a digit, a dot followed by a digit, or a sign followed by a
// digit or dot-digit. A bare leading sign on an identifier is NOT a number start, so
// vendor keywords ("-webkit-fill-available") fall through to passthrough.
constexpr bool StartsNumberToken(const char* s, size_t n) {
  size_t i = 0;
  if (i < n && (s[i] == '+' || s[i] == '-'))
    ++i;
  if (i < n && s[i] == '.')
    ++i;
  return i < n && IsDigit(s[i]);
}

// A "lone numeric token" is what a zero-runtime-parse fast path can digest: it starts like a
// number and carries no separator that would make it a compound value ("1px solid red",
// "rgb(0,0,0)", "1 / 2"). The lone-token test is what keeps a passthrough escape hatch from
// swallowing typos: a lone token parses STRICTLY (unknown unit = error), anything else is
// someone else's grammar.
constexpr bool IsLoneNumericToken(const char* s, size_t n) {
  if (n == 0 || !StartsNumberToken(s, n))
    return false;
  for (size_t i = 0; i < n; ++i) {
    const char c = s[i];
    if (c == ' ' || c == '\t' || c == ',' || c == '(' || c == ')' || c == '/')
      return false;
  }
  return true;
}

struct CssNumber {
  double value = 0.0;
  CssUnit unit = CssUnit::None;
  CssStatus status = CssStatus::Ok;
};

// Pure: no diagnostics, no side effects. The same code backs the consteval literal paths AND
// the runtime TryParse entry points, so the two can never diverge. RaiseCssError() is what
// turns a bad status into a compile error; a runtime caller branches on the status instead.
constexpr CssNumber ParseCssNumber(const char* s, size_t n) {
  size_t i = 0;
  bool negative = false;
  if (i < n && (s[i] == '+' || s[i] == '-')) {
    negative = (s[i] == '-');
    ++i;
  }
  double whole = 0.0;
  size_t digits = 0;
  while (i < n && IsDigit(s[i])) {
    whole = whole * 10.0 + static_cast<double>(s[i] - '0');
    ++i;
    ++digits;
  }
  double frac = 0.0;
  double place = 1.0;
  if (i < n && s[i] == '.') {
    ++i;
    size_t frac_digits = 0;
    while (i < n && IsDigit(s[i])) {
      place /= 10.0;
      frac += static_cast<double>(s[i] - '0') * place;
      ++i;
      ++frac_digits;
    }
    // CSS's number token requires a digit after the decimal point ("1." is not a number).
    if (frac_digits == 0)
      return CssNumber { 0.0, CssUnit::None, CssStatus::ExpectedNumber };
    digits += frac_digits;
  }
  if (digits == 0)
    return CssNumber { 0.0, CssUnit::None, CssStatus::ExpectedNumber };

  double value = whole + frac;

  // Optional scientific-notation exponent ('e' or 'E', optional sign, digits), part of
  // CSS's number token. Consumed only when a digit follows, so "1em" still classifies
  // its unit below.
  if (i < n && (s[i] == 'e' || s[i] == 'E')) {
    size_t j = i + 1;
    bool exp_negative = false;
    if (j < n && (s[j] == '+' || s[j] == '-')) {
      exp_negative = (s[j] == '-');
      ++j;
    }
    if (j < n && IsDigit(s[j])) {
      int exponent = 0;
      while (j < n && IsDigit(s[j])) {
        // Anything past double's range saturates either way; cap so the loop stays small.
        if (exponent < 400)
          exponent = exponent * 10 + (s[j] - '0');
        ++j;
      }
      if (exponent > 400)
        exponent = 400;
      for (int k = 0; k < exponent; ++k)
        value = exp_negative ? value / 10.0 : value * 10.0;
      i = j;
    }
  }

  const CssUnit unit = ClassifyUnit(s + i, n - i);
  if (unit == CssUnit::Unknown)
    return CssNumber { 0.0, CssUnit::None, CssStatus::UnknownUnit };

  return CssNumber { negative ? -value : value, unit, CssStatus::Ok };
}

// unit_code: 0 = px, 1 = percent, 2 = fr.
struct TrackValue {
  double value = 0.0;
  uint8_t unit_code = 0;
  CssStatus status = CssStatus::Ok;
};

constexpr TrackValue ResolveTrack(const char* s, size_t n, bool allow_fr) {
  const CssNumber num = ParseCssNumber(s, n);
  if (num.status != CssStatus::Ok)
    return TrackValue { 0.0, 0, num.status };

  switch (num.unit) {
    case CssUnit::None:
    case CssUnit::Px:
      return TrackValue { num.value, 0, CssStatus::Ok };
    case CssUnit::Percent:
      return TrackValue { num.value, 1, CssStatus::Ok };
    case CssUnit::Fr:
      return allow_fr ? TrackValue { num.value, 2, CssStatus::Ok }
                      : TrackValue { 0.0, 0, CssStatus::FrNotValidHere };
    case CssUnit::Vw:
    case CssUnit::Vh:
    case CssUnit::Vmin:
    case CssUnit::Vmax:
      return TrackValue { 0.0, 0, CssStatus::ViewportUnitDeferred };
    default:
      return TrackValue { 0.0, 0, CssStatus::UnitNotSupportedInLayout };
  }
}

struct CssColor {
  float r = 0.0f;
  float g = 0.0f;
  float b = 0.0f;
  float a = 0.0f;
  CssStatus status = CssStatus::Ok;
};

constexpr int HexDigit(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return 10 + (c - 'a');
  if (c >= 'A' && c <= 'F')
    return 10 + (c - 'A');
  return -1;
}

constexpr CssColor ParseHexColor(const char* s, size_t n) {
  // s points past the '#'. Accepted digit counts are CSS's: 3, 4, 6, 8.
  int d[8] = {};
  if (n != 3 && n != 4 && n != 6 && n != 8)
    return CssColor { 0, 0, 0, 0, CssStatus::InvalidHexColor };
  for (size_t i = 0; i < n; ++i) {
    d[i] = HexDigit(s[i]);
    if (d[i] < 0)
      return CssColor { 0, 0, 0, 0, CssStatus::InvalidHexColor };
  }
  int r = 0, g = 0, b = 0, a = 255;
  if (n <= 4) {
    r = d[0] * 17;
    g = d[1] * 17;
    b = d[2] * 17;
    if (n == 4)
      a = d[3] * 17;
  } else {
    r = d[0] * 16 + d[1];
    g = d[2] * 16 + d[3];
    b = d[4] * 16 + d[5];
    if (n == 8)
      a = d[6] * 16 + d[7];
  }
  return CssColor { static_cast<float>(r) / 255.0f, static_cast<float>(g) / 255.0f,
                    static_cast<float>(b) / 255.0f, static_cast<float>(a) / 255.0f,
                    CssStatus::Ok };
}

constexpr double ClampUnit(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }

struct RgbComponent {
  double value = 0.0;
  bool is_percent = false;
  bool ok = false;
};

// One rgb() argument: a trimmed token that is a bare number or a percentage. Internal
// whitespace (two tokens where one belongs) fails ParseCssNumber's unit classification.
constexpr RgbComponent ParseRgbComponent(const char* s, size_t b, size_t e) {
  while (b < e && (s[b] == ' ' || s[b] == '\t'))
    ++b;
  while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t'))
    --e;
  if (b >= e)
    return RgbComponent {};
  const CssNumber num = ParseCssNumber(s + b, e - b);
  if (num.status != CssStatus::Ok
      || (num.unit != CssUnit::None && num.unit != CssUnit::Percent))
    return RgbComponent {};
  return RgbComponent { num.value, num.unit == CssUnit::Percent, true };
}

constexpr CssColor ParseRgbFunction(const char* s, size_t n) {
  // s/n span the argument list between the parentheses. Exactly CSS's two argument
  // grammars, never a mix: comma syntax "R, G, B[, A]" or space syntax "R G B[ / A]".
  // Channels are numbers (0-255) or percentages, all three agreeing; alpha is a number
  // (0-1) or a percentage. Out-of-range values clamp, as in CSS.
  constexpr CssColor kBad { 0, 0, 0, 0, CssStatus::InvalidRgbColor };
  RgbComponent parts[4];
  size_t count = 0;
  bool has_alpha = false;

  bool saw_comma = false;
  for (size_t i = 0; i < n; ++i)
    if (s[i] == ',')
      saw_comma = true;

  if (saw_comma) {
    size_t start = 0;
    for (size_t i = 0; i <= n; ++i) {
      if (i == n || s[i] == ',') {
        if (count >= 4)
          return kBad;
        parts[count] = ParseRgbComponent(s, start, i);
        if (!parts[count].ok)
          return kBad;
        ++count;
        start = i + 1;
      } else if (s[i] == '/') {
        return kBad;
      }
    }
    if (count != 3 && count != 4)
      return kBad;
    has_alpha = (count == 4);
  } else {
    size_t slash = n;
    for (size_t i = 0; i < n; ++i) {
      if (s[i] == '/') {
        if (slash != n)
          return kBad;
        slash = i;
      }
    }
    size_t i = 0;
    while (i < slash && count < 4) {
      while (i < slash && (s[i] == ' ' || s[i] == '\t'))
        ++i;
      if (i >= slash)
        break;
      size_t start = i;
      while (i < slash && s[i] != ' ' && s[i] != '\t')
        ++i;
      parts[count] = ParseRgbComponent(s, start, i);
      if (!parts[count].ok)
        return kBad;
      ++count;
    }
    if (count != 3)
      return kBad;
    if (slash != n) {
      parts[3] = ParseRgbComponent(s, slash + 1, n);
      if (!parts[3].ok)
        return kBad;
      has_alpha = true;
      count = 4;
    }
  }

  if (parts[0].is_percent != parts[1].is_percent || parts[1].is_percent != parts[2].is_percent)
    return kBad;
  double rgb[3] = {};
  for (size_t c = 0; c < 3; ++c)
    rgb[c] = parts[c].is_percent ? parts[c].value / 100.0 : parts[c].value / 255.0;
  const double alpha
      = has_alpha ? (parts[3].is_percent ? parts[3].value / 100.0 : parts[3].value) : 1.0;
  return CssColor { static_cast<float>(ClampUnit(rgb[0])), static_cast<float>(ClampUnit(rgb[1])),
                    static_cast<float>(ClampUnit(rgb[2])), static_cast<float>(ClampUnit(alpha)),
                    CssStatus::Ok };
}

constexpr bool IsAsciiAlpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }

// The compile-time color subset: hex forms, rgb()/rgba(), and the keyword `transparent`.
// Anything alphabetic beyond that classifies as a named color / keyword and reports the
// runtime route (Color::Parse handles the full CSS grammar). Pure, like ParseCssNumber.
constexpr CssColor ParseCssColor(const char* s, size_t n) {
  // Trim surrounding whitespace.
  size_t b = 0, e = n;
  while (b < e && (s[b] == ' ' || s[b] == '\t'))
    ++b;
  while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t'))
    --e;
  if (b >= e)
    return CssColor { 0, 0, 0, 0, CssStatus::ExpectedColor };

  if (s[b] == '#')
    return ParseHexColor(s + b + 1, e - b - 1);

  if (TokenIsCI(s + b, e - b, "transparent"))
    return CssColor { 0, 0, 0, 0, CssStatus::Ok };

  // rgb( ... ) / rgba( ... ).
  const bool rgb = (e - b) > 4 && ToAsciiLower(s[b]) == 'r' && ToAsciiLower(s[b + 1]) == 'g'
                   && ToAsciiLower(s[b + 2]) == 'b';
  if (rgb) {
    size_t open = b + 3;
    if (open < e && ToAsciiLower(s[open]) == 'a')
      ++open;
    if (open < e && s[open] == '(' && s[e - 1] == ')')
      return ParseRgbFunction(s + open + 1, e - open - 2);
    return CssColor { 0, 0, 0, 0, CssStatus::InvalidRgbColor };
  }

  if (IsAsciiAlpha(s[b]))
    return CssColor { 0, 0, 0, 0, CssStatus::NamedColorRuntimeOnly };
  return CssColor { 0, 0, 0, 0, CssStatus::ExpectedColor };
}

// Reaching a css_error:: anchor during constant evaluation is the compile error; the
// anchor's name carries the explanation. At runtime (the UL_CSS_CONSTEVAL fallback arm)
// the anchors are ordinary empty functions, so callers must ALSO record the failure in
// their invalid flag; ingestion sites check that flag and Warning-log.
constexpr void RaiseCssError(CssStatus status) {
  switch (status) {
    case CssStatus::Ok:
      return;
    case CssStatus::ExpectedNumber:
      css_error::expected_a_css_number();
      return;
    case CssStatus::UnknownUnit:
      css_error::unknown_css_unit();
      return;
    case CssStatus::UnitNotSupportedInLayout:
      css_error::unit_not_supported_in_layout();
      return;
    case CssStatus::ViewportUnitDeferred:
      css_error::viewport_units_not_supported_yet();
      return;
    case CssStatus::FrNotValidHere:
      css_error::fr_is_only_valid_for_a_track_size();
      return;
    case CssStatus::PercentNotValidHere:
      css_error::percent_is_not_valid_here();
      return;
    case CssStatus::ExpectedFr:
      css_error::expected_an_fr_flex_factor();
      return;
    case CssStatus::ExpectedColor:
      css_error::expected_a_color_literal();
      return;
    case CssStatus::NamedColorRuntimeOnly:
      css_error::named_colors_are_runtime_only_use_Color_Parse();
      return;
    case CssStatus::InvalidHexColor:
      css_error::invalid_hex_color_literal();
      return;
    case CssStatus::InvalidRgbColor:
      css_error::invalid_rgb_color_literal();
      return;
  }
}

}  // namespace detail
}  // namespace ultralight

#pragma pop_macro("None")
/// \endcond
