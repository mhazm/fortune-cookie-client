///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/dom/detail/StyleNames.h>

namespace ultralight {
namespace dom {

///
/// A CSS property name checked when your code compiles (the type of dom::css).
///
/// An unknown name is a compile error. You'd normally write one as dom::css.
///
template <detail::FixedString Name>
struct CSSPropertyName {
  static_assert(detail::IsKnownCSSProperty(detail::kCSSName<Name>),
                "unknown CSS property name; check the spelling (custom properties start "
                "with --)");

  ///
  /// Get the property's CSS name (null-terminated). A camelCase name comes back in CSS form
  /// (eg, `background-color` for `backgroundColor`).
  ///
  static constexpr const char* name() { return detail::kCSSName<Name>.data(); }
};

///
/// A CSS property name checked when your code compiles.
///
/// Use it where style access takes a property name, so a typo is a compile error instead of a
/// write that silently does nothing:
///
/// ```
/// el.style[dom::css<"background-color">] = "purple";
/// el.style[dom::css<"backgroundColor">] = "purple";  // the same property
/// el.style[dom::css<"--accent">] = "#ff3366";
/// std::string size = dom::getComputedStyle(el)[dom::css<"font-size">];
/// ```
///
/// Write a name in lowercase CSS form (`background-color`) or in the camelCase form JavaScript
/// uses for it (`backgroundColor`, `cssFloat` for `float`). Prefixed legacy names (eg,
/// `-webkit-border-radius` or `webkitBorderRadius`) work too, and any custom property name (`--`
/// followed by at least one character) is accepted.
///
/// @note  For a name you only know at run time, pass a string instead (eg,
///        `el.style["background-color"]`). An unknown name then does nothing.
///
template <detail::FixedString Name>
inline constexpr CSSPropertyName<Name> css {};

}  // namespace dom
}  // namespace ultralight
