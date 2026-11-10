///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/Color.h>
#include <Ultralight/dom/Element.h>
#include <Ultralight/dom/StyleValue.h>

#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace ultralight {
namespace dom {

/// \cond INTERNAL
namespace detail {

// One named computed-style member (computed.fontSize). Reads recover the owning
// ComputedStyle from the member's own address (the StyleProp pattern, over ComputedStyle's
// standard layout). Distinct type per property name, so members may share one collapsed
// address. Non-copyable for the same reason as StyleProp: a copy would carry a dangling owner.
template <FixedString Name>
struct ComputedStyleProp {
  ComputedStyleProp() = default;
  ComputedStyleProp(const ComputedStyleProp&) = delete;
  ComputedStyleProp& operator=(const ComputedStyleProp&) = delete;

  operator std::string() const;

  StyleValue AsStyleValue() const { return StyleValue::Parse(std::string(*this)); }

  ultralight::Color AsColor() const { return ColorFromStyleText(std::string(*this)); }

  friend bool operator==(const ComputedStyleProp& p, std::string_view v) {
    return std::string(p) == v;
  }
};

// ComputedStyle's handle storage. Public so the member owner recovery can read it; not part
// of the API.
struct ComputedStyleStorage {
  ULDOMElement element = nullptr;
};

}  // namespace detail
/// \endcond

class ComputedStyle;

///
/// Get an element's computed style (getComputedStyle).
///
/// This is JavaScript's global getComputedStyle(), and the same as Window::getComputedStyle().
///
/// @param  element  The element to read.
///
/// @return  Returns a read-only view of the element's computed style (see ComputedStyle).
///
inline ComputedStyle getComputedStyle(const Element& element);

///
/// A read-only view of an element's computed style (getComputedStyle).
///
/// Holds the values the library uses to render an element after every style rule is applied,
/// rather than what one rule or the style attribute declares. Get one from
/// dom::getComputedStyle() (or Window::getComputedStyle()).
///
/// ## Reading Values
///
/// Every CSS property has a read-only member named like its `el.style` member (eg, `fontSize`,
/// or `cssFloat` for `float`), and reading one gives you the value as a string. You can also read
/// a property by name:
///
/// ```
/// dom::ComputedStyle computed = dom::getComputedStyle(badge);
/// std::string size = computed.fontSize;              // eg, "16px"
/// std::string weight = computed["font-weight"];      // eg, "700" ("fontWeight" works too)
/// std::string accent = computed.getPropertyValue("--accent");
/// ```
///
/// Custom properties return the value in effect for the element, so you can set one on a
/// container and read it on any element inside.
///
/// ## Typed Reads
///
/// Each member's AsStyleValue() parses its value into a dom::StyleValue, and AsColor() parses it
/// into a Color. A read by name (eg, `computed["--gap"]`) has both too:
///
/// ```
/// if (dom::StyleValue width = computed.width.AsStyleValue())
///   PlaceMinimap(width.value);                       // 300 when the width is "300px"
///
/// if (Color accent = computed.color.AsColor())
///   hud_accent = accent;
/// ```
///
/// Lengths come back in pixels (a few properties, such as `border-radius`, keep a percentage) and
/// times in seconds. A value that isn't a single number (eg, `auto` or a `transform` matrix)
/// gives an Invalid StyleValue, and an empty read gives an Empty one. AsColor() takes any CSS
/// color, so it also converts the text of a custom property (eg, `hsl(200 80% 50%)`). A value
/// that isn't a color gives an invalid Color, and an empty read gives an unset one.
///
/// A ComputedStyle is live, so you can keep one and read it again later. Each read returns the
/// value at the time of the read. Once the element's page is gone, every read returns an empty
/// string (see dom::Element).
///
/// \parblock
/// @note  A read first applies any pending style changes. A property that depends on layout
///        (eg, `width`, `margin`, `top`, or `transform`) also runs a layout pass, so make all
///        your changes first and then read.
/// \endparblock
///
/// \parblock
/// @note  The members can't be copied out (`auto w = computed.width` doesn't compile). Read one
///        into a `std::string`, or call AsStyleValue() or AsColor().
/// \endparblock
///
class ComputedStyle {
 public:
  // One read-only member per CSS property and legacy alias, named like the el.style members
  // (see the class notes). Generated from the same table as the style proxy.
#define ULTRALIGHT_DOM_V(ident, cssname) \
  ULTRALIGHT_DOM_NO_UNIQUE_ADDRESS detail::ComputedStyleProp<cssname> ident;
  ULDOM_FOR_EACH_CSS_PROPERTY(ULTRALIGHT_DOM_V)
#undef ULTRALIGHT_DOM_V
#define ULTRALIGHT_DOM_V(ident, cssname, canonical) \
  ULTRALIGHT_DOM_NO_UNIQUE_ADDRESS detail::ComputedStyleProp<cssname> ident;
  ULDOM_FOR_EACH_CSS_PROPERTY_ALIAS(ULTRALIGHT_DOM_V)
#undef ULTRALIGHT_DOM_V

  ///
  /// Internal storage (not part of the API).
  ///
  detail::ComputedStyleStorage detail_;

  ///
  /// Create an empty ComputedStyle (every read returns an empty string).
  ///
  ComputedStyle() {}

  ///
  /// Copy constructor (both read the same element's style).
  ///
  /// @param  other  The ComputedStyle to copy.
  ///
  ComputedStyle(const ComputedStyle& other)
      : detail_ { other.detail_.element ? ulCreateDOMElementRef(other.detail_.element)
                                        : nullptr } {}

  ///
  /// Move constructor (`other` becomes empty).
  ///
  /// @param  other  The ComputedStyle to move from.
  ///
  ComputedStyle(ComputedStyle&& other) noexcept : detail_ { other.detail_.element } {
    other.detail_.element = nullptr;
  }

  ///
  /// Assignment (copies or moves).
  ///
  /// @param  other  The ComputedStyle to assign from.
  ///
  /// @return  Returns this ComputedStyle.
  ///
  ComputedStyle& operator=(ComputedStyle other) noexcept {
    std::swap(detail_.element, other.detail_.element);
    return *this;
  }

  ///
  /// Destroy this view (the element isn't affected).
  ///
  ~ComputedStyle() { ulDestroyDOMElement(detail_.element); }

  ///
  /// Get a property's computed value (getPropertyValue).
  ///
  /// @param  name  The property name in CSS form (eg, `font-size`, not `fontSize`), or a custom
  ///               property (eg, `--accent`).
  ///
  /// @return  Returns the computed value, or an empty string if the property is unknown or the
  ///          element isn't in a document.
  ///
  std::string getPropertyValue(std::string_view name) const {
    detail::CString n(name);
    return detail::TakeString(ulDOMElementGetComputedStyleProperty(detail_.element, n.c_str()));
  }

  ///
  /// Get a property's computed value using a name checked at compile time (eg,
  /// `computed[dom::css<"font-size">]`).
  ///
  /// @return  Returns the computed value, which reads like a member: it converts to a
  ///          `std::string` and has AsStyleValue() and AsColor().
  ///
  /// @see  dom::css
  ///
  template <detail::FixedString Name>
  detail::StyleText operator[](CSSPropertyName<Name>) const {
    return detail::StyleText(detail::TakeString(
        ulDOMElementGetComputedStyleProperty(detail_.element, CSSPropertyName<Name>::name())));
  }

  ///
  /// Get a property's computed value by name.
  ///
  /// @param  name  The property name in CSS form (eg, `font-size`), in camelCase form (eg,
  ///               `fontSize`), or a custom property (eg, `--accent`).
  ///
  /// @return  Returns the computed value, which reads like a member: it converts to a
  ///          `std::string` and has AsStyleValue() and AsColor().
  ///
  detail::StyleText operator[](std::string_view name) const {
    return detail::StyleText(getPropertyValue(detail::CSSNameOf(name)));
  }

 private:
  friend ComputedStyle getComputedStyle(const Element& element);
  explicit ComputedStyle(ULDOMElement owned_element) : detail_ { owned_element } {}
};

// The named members recover their owner through offsetof, which is well-defined only on a
// standard-layout class: every data member is public and declared in this one class.
static_assert(std::is_standard_layout_v<ComputedStyle>, "ComputedStyle must stay standard-layout");
static_assert(!std::is_aggregate_v<ComputedStyle>, "ComputedStyle must not be an aggregate");

inline ComputedStyle getComputedStyle(const Element& element) {
  ULDOMElement handle = element.raw();
  return ComputedStyle(handle ? ulCreateDOMElementRef(handle) : nullptr);
}

/// \cond INTERNAL
namespace detail {

// Named-member offsets, in the same order as kStylePropertyNames (the StyleMemberOffsets()
// table, over ComputedStyle).
inline const size_t* ComputedStyleMemberOffsets() {
  static const size_t offsets[] = {
#define ULTRALIGHT_DOM_V(ident, cssname) offsetof(ComputedStyle, ident),
    ULDOM_FOR_EACH_CSS_PROPERTY(ULTRALIGHT_DOM_V)
#undef ULTRALIGHT_DOM_V
#define ULTRALIGHT_DOM_V(ident, cssname, canonical) offsetof(ComputedStyle, ident),
        ULDOM_FOR_EACH_CSS_PROPERTY_ALIAS(ULTRALIGHT_DOM_V)
#undef ULTRALIGHT_DOM_V
  };
  return offsets;
}

template <FixedString Name>
ComputedStyleProp<Name>::operator std::string() const {
  const ComputedStyle* owner = reinterpret_cast<const ComputedStyle*>(
      reinterpret_cast<const char*>(this) - ComputedStyleMemberOffsets()[kStyleIndex<Name>]);
  return TakeString(ulDOMElementGetComputedStyleProperty(owner->detail_.element, Name.data));
}

}  // namespace detail
/// \endcond

}  // namespace dom
}  // namespace ultralight
