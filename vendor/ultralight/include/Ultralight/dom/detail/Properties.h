///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/CAPI/CAPI_DOMElement.h>
#include <Ultralight/CAPI/CAPI_DOMNode.h>
#include <Ultralight/Color.h>
#include <Ultralight/dom/CSSPropertyName.h>
#include <Ultralight/dom/StyleValue.h>
#include <Ultralight/dom/detail/Support.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

// The Element property-proxy types (style, classList, dataset, and the element-level
// properties). Element.h declares one member of each; their operator definitions live
// in PropertiesImpl.h, included after Element is complete. Not part of the public API.

namespace ultralight {
namespace dom {

// Defined in Element.h; the selectionDirection proxy reads and writes it.
enum class SelectionDirection : uint8_t;

/// \cond INTERNAL
namespace detail {

// ---------------------------------------------------------------------------------------
// Property-proxy types. Each is an empty member of Element (or of a sub-proxy) whose
// operators recover the owning Element from their own address (see OwnerHandle). They are
// deliberately non-copyable: a proxy copied out of its Element would carry a dangling
// owner, so `auto t = el.textContent;` is a compile error while
// `std::string t = el.textContent;` reads the value.
// ---------------------------------------------------------------------------------------

// Custom properties (--name) have no engine property ID and no numeric waist entry;
// every write to one takes the text route.
inline bool IsCustomPropertyName(std::string_view name) {
  return name.size() >= 2 && name[0] == '-' && name[1] == '-';
}

// A style value read as a color: unset when the text is empty (the property isn't set),
// invalid when it isn't a CSS color. Color::Parse() takes the full CSS color grammar, so a
// custom property's raw text (eg, hsl(200 80% 50%)) converts too.
inline ultralight::Color ColorFromStyleText(const std::string& text) {
  return text.empty() ? ultralight::Color() : ultralight::Color::Parse(text);
}

// What a computed read by name returns (both computed[...] forms): a copy of the property's
// text, with the read members StyleRef has, so every read by name reads alike. It has no
// std::string_view conversion on purpose: `std::string_view v = computed["x"];` would keep a
// view into a temporary that dies with the statement.
class StyleText {
 public:
  explicit StyleText(std::string text) : text_(std::move(text)) {}

  operator std::string() const& { return text_; }
  operator std::string() && { return std::move(text_); }

  bool empty() const { return text_.empty(); }

  StyleValue AsStyleValue() const { return StyleValue::Parse(text_); }
  ultralight::Color AsColor() const { return ColorFromStyleText(text_); }

  friend bool operator==(const StyleText& t, std::string_view v) { return t.text_ == v; }
  friend bool operator==(const StyleText& a, const StyleText& b) { return a.text_ == b.text_; }

 private:
  std::string text_;
};

// Constructs the named style members. StyleProp has no default constructor so that empty
// braces can't build one: `el.style.width = {}` would otherwise also match the deleted copy
// assignment (ambiguous, or worse, chosen, depending on the compiler) instead of the
// StyleValue overload that removes the property.
struct StyleMemberTag {};

// One named style member (el.style.backgroundColor). Distinct type per property name;
// identity lives in the type, so members may share one collapsed address. Every write is
// const: a const Element is a const handle, not a const element.
template <FixedString Name>
struct StyleProp {
  static_assert(kStyleIndex<Name> != size_t(-1),
                "StyleProp requires a property name from the generated style table");

  explicit StyleProp(StyleMemberTag) {}
  StyleProp(const StyleProp&) = delete;
  StyleProp& operator=(const StyleProp&) = delete;

  // Set from a string LITERAL, digested at compile time (numeric fast path or
  // passthrough; an empty value removes the declaration, exactly as on the web).
  void operator=(CssValue value) const;

  // Set from a runtime string. The !is_array_v exclusion must stay FIRST in the
  // conjunction (&& short-circuits during constraint satisfaction): without it, string
  // literals bind here as an exact match instead of reaching CssValue, and every
  // compile-time check silently evaporates. Do not reorder or drop the guard. The
  // nullptr_t exclusion keeps `= nullptr` from constructing a std::string_view from null.
  template <class T>
    requires(!std::is_array_v<std::remove_reference_t<T>>)
            && (!std::is_same_v<std::remove_cvref_t<T>, std::nullptr_t>)
            && std::is_convertible_v<T, std::string_view>
  void operator=(T&& value) const {
    AssignText(std::string_view(std::forward<T>(value)));
  }

  // Set from a number with a unit (the StyleValue factories), or from a bare number through
  // StyleValue's implicit constructor (the CSS <number> type, like a bare number in
  // JavaScript). The numeric waist checks it against the property, so a value the property
  // rejects keeps the old declaration (with a diagnostic), like a string. An Empty value
  // removes the declaration, and so does `= {}` (an Empty StyleValue); an Invalid one (NaN
  // included) is ignored with a diagnostic.
  void operator=(StyleValue value) const;

  // Set from a typed Color: no string formatting or parsing, the fast path for
  // per-frame color updates. An unset Color removes the declaration; an invalid one is
  // ignored with a diagnostic. Constrained to exactly Color so string literals keep
  // routing through CssValue.
  template <class T>
    requires std::is_same_v<std::remove_cvref_t<T>, ultralight::Color>
  void operator=(const T& color) const {
    AssignColor(color);
  }

 private:
  void AssignText(std::string_view value) const;
  void AssignColor(ULColor color) const;

 public:

  // Read the SPECIFIED inline value (what the style attribute declares); empty when not
  // declared inline. Computed values come from dom::getComputedStyle().
  operator std::string() const;

  // The inline value as a number with a unit (StyleValue::Parse()): Empty when not declared
  // inline, Invalid when it isn't one number with a StyleUnit unit.
  StyleValue AsStyleValue() const { return StyleValue::Parse(std::string(*this)); }

  // The inline value as a color: unset when not declared inline, invalid when it isn't a
  // color.
  ultralight::Color AsColor() const { return ColorFromStyleText(std::string(*this)); }

  friend bool operator==(const StyleProp& p, std::string_view v) {
    return std::string(p) == v;
  }
};

// The style.cssText read-write property.
struct CssTextProp {
  CssTextProp() = default;
  CssTextProp(const CssTextProp&) = delete;
  CssTextProp& operator=(const CssTextProp&) = delete;

  void operator=(std::string_view css_text) const;
  operator std::string() const;

  friend bool operator==(const CssTextProp& p, std::string_view v) {
    return std::string(p) == v;
  }
};

// A runtime-named style slot, produced by StyleProxy::operator[]. Intended as a temporary
// in an expression; it owns an element reference, so it is safe (but pointless) to keep.
class StyleRef {
 public:
  StyleRef(const StyleRef&) = delete;
  StyleRef& operator=(const StyleRef&) = delete;
  ~StyleRef() { ulDestroyDOMElement(element_); }

  // String LITERALS, digested at compile time (numeric fast path or passthrough).
  // Numeric literals on a CUSTOM property (--name) take the text route: custom
  // properties have no numeric waist entry, and their captured spelling is the value.
  void operator=(CssValue value) const {
    if (value.kind() == CssValue::Kind::Numeric && !IsCustomPropertyName(name_)) {
      *this = StyleValue { value.value(), value.unit() };
      return;
    }
    if (property_id_)
      ulDOMElementSetStylePropertyByID(element_, property_id_, value.text(), false, nullptr);
    else
      ulDOMElementSetStyleProperty(element_, name_.c_str(), value.text(), false, nullptr);
  }

  // Runtime strings. The !is_array_v exclusion stays FIRST; dropping or reordering it
  // silently reroutes literals away from the compile-time path (see the StyleProp
  // overload set, including the nullptr_t note).
  template <class T>
    requires(!std::is_array_v<std::remove_reference_t<T>>)
            && (!std::is_same_v<std::remove_cvref_t<T>, std::nullptr_t>)
            && std::is_convertible_v<T, std::string_view>
  void operator=(T&& value) const {
    AssignText(std::string_view(std::forward<T>(value)));
  }

  // Typed Color (the color fast path): an unset Color removes the declaration and an
  // invalid one is ignored with a diagnostic. A custom property (--name) takes the color as
  // its lowercase hex text. Constrained to exactly Color so string literals keep routing
  // through CssValue.
  template <class T>
    requires std::is_same_v<std::remove_cvref_t<T>, ultralight::Color>
  void operator=(const T& color) const {
    if (property_id_)
      ulDOMElementSetStylePropertyColorByID(element_, property_id_, color, false);
    else
      ulDOMElementSetStylePropertyColor(element_, name_.c_str(), color, false);
  }

  // A number with a unit, or a bare number (see the StyleProp overload). An Empty value
  // (`= {}` included) removes the declaration, custom properties included; the numeric waist
  // ignores an Invalid one or one the property rejects, with a diagnostic.
  void operator=(StyleValue value) const {
    if (value.IsEmpty()) {
      AssignText("");
      return;
    }
    if (property_id_) {
      ulDOMElementSetStylePropertyNumericByID(element_, property_id_, value.value,
                                              static_cast<ULDOMStyleUnit>(value.unit),
                                              false);
    } else {
      ulDOMElementSetStylePropertyNumeric(element_, name_.c_str(), value.value,
                                          static_cast<ULDOMStyleUnit>(value.unit), false);
    }
  }

  // The reads match StyleText's, so every read by name reads alike (the computed ones return
  // StyleText); like StyleText, no std::string_view conversion, which would dangle. Each read
  // is live.
  operator std::string() const {
    return TakeString(ulDOMElementGetStyleProperty(element_, name_.c_str()));
  }

  bool empty() const { return std::string(*this).empty(); }

  // The typed reads of StyleProp. AsColor() also converts a custom property's raw text
  // (eg, hsl(200 80% 50%)), which no named member reaches.
  StyleValue AsStyleValue() const { return StyleValue::Parse(std::string(*this)); }
  ultralight::Color AsColor() const { return ColorFromStyleText(std::string(*this)); }

  friend bool operator==(const StyleRef& r, std::string_view v) {
    return std::string(r) == v;
  }

 private:
  friend struct StyleProxy;
  // A nonzero property_id routes writes through the interned-ID fast path; the name (always
  // in CSS form) is kept for reads and the string fallback.
  StyleRef(ULDOMElement owned_element, std::string name, int property_id = 0)
      : element_(owned_element), name_(std::move(name)), property_id_(property_id) {}

  void AssignText(std::string_view value) const {
    CString v(value);
    if (property_id_)
      ulDOMElementSetStylePropertyByID(element_, property_id_, v.c_str(), false, nullptr);
    else
      ulDOMElementSetStyleProperty(element_, name_.c_str(), v.c_str(), false, nullptr);
  }

  ULDOMElement element_;
  std::string name_;
  int property_id_ = 0;
};

// Forward-declared adapter tags: each maps one element-level property onto its C API
// calls. Defined further below (they need only the CAPI surface), next to the attribute
// reflection tags.
struct TextContentTag;
struct InnerTextTag;
struct InnerHTMLTag;
struct OuterHTMLTag;
struct TabIndexTag;
struct ValueTag;
struct DefaultValueTag;
struct InputTypeTag;
struct CheckedTag;
struct DisabledTag;
struct SelectedIndexTag;
struct ScrollTopTag;
struct ScrollLeftTag;
struct SelectionStartTag;
struct SelectionEndTag;
struct SelectionDirectionTag;
struct OptionSelectedTag;
struct OptionTextTag;

// The element-level proxies below write through `const` operators: a handle's constness is
// the handle's, not the element's, so a const Element (eg, a lambda's copy) can still assign
// el.textContent.

// A read-write string property of Element (textContent, innerText, id, ...).
template <class Tag>
struct StringProp {
  StringProp() = default;
  StringProp(const StringProp&) = delete;
  StringProp& operator=(const StringProp&) = delete;

  void operator=(std::string_view value) const;

  // Numbers assign as their shortest round-trip decimal form (el.textContent = 42),
  // matching JavaScript for ordinary magnitudes (exponent spelling can differ at
  // extremes, e.g. 1e-07 here vs JS's 1e-7).
  template <typename T>
    requires std::is_arithmetic_v<T>
  void operator=(T value) const {
    *this = std::string_view(FormatNumber(value));
  }

  void operator+=(std::string_view suffix) const;

  operator std::string() const;

  friend bool operator==(const StringProp& p, std::string_view v) {
    return std::string(p) == v;
  }
};

// A read-write boolean property of Element (checked, disabled).
template <class Tag>
struct BoolProp {
  BoolProp() = default;
  BoolProp(const BoolProp&) = delete;
  BoolProp& operator=(const BoolProp&) = delete;

  void operator=(bool value) const;
  operator bool() const;
};

// A read-write integer property of Element (tabIndex).
template <class Tag>
struct IntProp {
  IntProp() = default;
  IntProp(const IntProp&) = delete;
  IntProp& operator=(const IntProp&) = delete;

  void operator=(int value) const;
  operator int() const;
  void operator+=(int delta) const { *this = static_cast<int>(*this) + delta; }
  void operator-=(int delta) const { *this = static_cast<int>(*this) - delta; }
};

// A read-write property whose type comes from its tag (Tag::Value): the scroll positions
// (double), selectedIndex and the text-control selection offsets (std::optional<size_t>,
// std::nullopt where the web reads null or -1), and selectionDirection.
template <class Tag>
struct ValueProp {
  using Value = typename Tag::Value;

  ValueProp() = default;
  ValueProp(const ValueProp&) = delete;
  ValueProp& operator=(const ValueProp&) = delete;

  void operator=(const Value& value) const;
  operator Value() const;

  void operator+=(Value delta) const
    requires std::is_arithmetic_v<Value>
  {
    *this = static_cast<Value>(*this) + delta;
  }
  void operator-=(Value delta) const
    requires std::is_arithmetic_v<Value>
  {
    *this = static_cast<Value>(*this) - delta;
  }

  friend bool operator==(const ValueProp& p, const Value& v) { return static_cast<Value>(p) == v; }
};

///
/// The el.style proxy: the element's inline style declaration, with a named member per
/// CSS property (el.style.backgroundColor = "teal"), the spec string forms
/// (setProperty(), getPropertyValue(), removeProperty(), cssText, operator[]), and the
/// compile-time-validated form (el.style[dom::css<"...">]). operator[] and dom::css also take
/// a property's camelCase JavaScript name (backgroundColor, cssFloat), translated through the
/// generated table; setProperty(), getPropertyValue(), and removeProperty() take CSS names
/// only, as on the web.
///
struct StyleProxy {
  StyleProxy() = default;
  StyleProxy(const StyleProxy&) = delete;
  StyleProxy& operator=(const StyleProxy&) = delete;

  // Set an inline style property (style.setProperty) from a string LITERAL, digested at
  // compile time. Custom properties (--name) work here too; an empty value removes the
  // declaration, exactly as on the web.
  void setProperty(std::string_view name, CssValue value, bool important = false) const;

  // Set an inline style property (style.setProperty) from a runtime string. Values that
  // fail to parse leave the old declaration in place and are otherwise silent, exactly as
  // setProperty behaves on the web. The !is_array_v exclusion stays FIRST; dropping or
  // reordering it silently reroutes literals away from the compile-time path (see the
  // StyleProp overload set, including the nullptr_t note).
  template <class T>
    requires(!std::is_array_v<std::remove_reference_t<T>>)
            && (!std::is_same_v<std::remove_cvref_t<T>, std::nullptr_t>)
            && std::is_convertible_v<T, std::string_view>
  void setProperty(std::string_view name, T&& value, bool important = false) const {
    SetPropertyText(name, std::string_view(std::forward<T>(value)), important);
  }

  // Set a color-valued inline style property from a typed Color: no string formatting
  // or parsing, the fast path for per-frame color updates. A custom property (--name)
  // takes the color as its lowercase hex text, and an unset Color removes the declaration.
  // Returns false when the property name is unknown, the element's page is gone, or the
  // color is invalid. Constrained to exactly Color so string literals keep routing through
  // the CssValue overload.
  template <class T>
    requires std::is_same_v<std::remove_cvref_t<T>, ultralight::Color>
  bool setProperty(std::string_view name, const T& color, bool important = false) const {
    return SetPropertyColor(name, color, important);
  }

  // Get an inline style property's SPECIFIED value (style.getPropertyValue): what this
  // element's style attribute declares, not the cascaded result. Empty when the property
  // is not declared inline.
  std::string getPropertyValue(std::string_view name) const;

  // Remove an inline style property (style.removeProperty) and return the removed value
  // (empty when it was not declared), exactly as on the web.
  std::string removeProperty(std::string_view name) const;

  // Set an inline style property from a number with a unit (the StyleValue factories), or
  // from a bare number. An Empty value removes the declaration (custom properties
  // included). Returns false when the property name is unknown, the element's page is gone,
  // the value is Invalid or the property rejects it (a diagnosed no-op), or the value holds
  // a number and the property is custom.
  bool setProperty(std::string_view name, StyleValue value, bool important = false) const;

  // Access a property slot by runtime name (CSS or camelCase form), for reading or writing.
  // Const like the other writes: a const Element is a const handle.
  StyleRef operator[](std::string_view name) const;

  // Access a property slot by compile-time-validated name (see dom::css).
  template <FixedString Name>
  StyleRef operator[](CSSPropertyName<Name>) const;

 private:
  void SetPropertyText(std::string_view name, std::string_view value, bool important) const;
  bool SetPropertyColor(std::string_view name, ULColor color, bool important) const;

 public:

  /// The whole inline declaration as CSS text (style.cssText). Assignment replaces it by
  /// parsing; unparsable declarations are dropped silently, exactly as on the web.
  CssTextProp cssText;

#define ULTRALIGHT_DOM_V(ident, cssname) \
  ULTRALIGHT_DOM_NO_UNIQUE_ADDRESS StyleProp<cssname> ident { StyleMemberTag() };
  ULDOM_FOR_EACH_CSS_PROPERTY(ULTRALIGHT_DOM_V)
#undef ULTRALIGHT_DOM_V
#define ULTRALIGHT_DOM_V(ident, cssname, canonical) \
  ULTRALIGHT_DOM_NO_UNIQUE_ADDRESS StyleProp<cssname> ident { StyleMemberTag() };
  ULDOM_FOR_EACH_CSS_PROPERTY_ALIAS(ULTRALIGHT_DOM_V)
#undef ULTRALIGHT_DOM_V
};

///
/// The el.classList proxy (DOMTokenList subset): class membership queries and mutations.
/// Invalid tokens (empty, or containing whitespace) make the operation a no-op; developer
/// diagnostics surface them (see the Element class doc).
///
struct ClassListProxy {
  ClassListProxy() = default;
  ClassListProxy(const ClassListProxy&) = delete;
  ClassListProxy& operator=(const ClassListProxy&) = delete;

  // Whether or not the class list contains a class (classList.contains).
  bool contains(std::string_view token) const;

  // Add classes (classList.add). Classes already present are kept (no duplicates). When
  // any token is invalid (empty, or containing whitespace), NO class is added: the whole
  // call is a diagnosed benign no-op, where the web throws.
  template <typename... T>
    requires(sizeof...(T) >= 1 && (std::is_convertible_v<T, std::string_view> && ...)
             && (!std::is_same_v<std::remove_cvref_t<T>, std::nullptr_t> && ...))
  void add(T&&... tokens) const {
    std::string_view views[] = { std::string_view(tokens)... };
    for (std::string_view token : views) {
      if (!IsValidClassToken(token)) {
        AddOne(token);  // The waist rejects it (a diagnosed no-op); nothing mutates.
        return;
      }
    }
    for (std::string_view token : views)
      AddOne(token);
  }

  // Remove classes (classList.remove). When any token is invalid (empty, or containing
  // whitespace), NO class is removed: the whole call is a diagnosed benign no-op, where
  // the web throws.
  template <typename... T>
    requires(sizeof...(T) >= 1 && (std::is_convertible_v<T, std::string_view> && ...)
             && (!std::is_same_v<std::remove_cvref_t<T>, std::nullptr_t> && ...))
  void remove(T&&... tokens) const {
    std::string_view views[] = { std::string_view(tokens)... };
    for (std::string_view token : views) {
      if (!IsValidClassToken(token)) {
        RemoveOne(token);  // The waist rejects it (a diagnosed no-op); nothing mutates.
        return;
      }
    }
    for (std::string_view token : views)
      RemoveOne(token);
  }

  // Toggle a class (classList.toggle): remove it when present, add it when absent.
  // Returns whether the class is present after the call.
  bool toggle(std::string_view token) const;

  // Toggle a class with the web's `force` argument: true always adds, false always
  // removes. Returns whether the class is present after the call.
  bool toggle(std::string_view token, bool force) const;

  // Replace a class with another, in place (classList.replace): keeps the token's
  // position in the class list. Returns whether the class was present and replaced.
  bool replace(std::string_view token, std::string_view new_token) const;

  // The number of classes (classList.length).
  size_t length() const;

  // Get a class by index, in attribute order (classList.item). Empty when out of range.
  std::string item(size_t index) const;

 private:
  void AddOne(std::string_view token) const;
  void RemoveOne(std::string_view token) const;
};

// A data-* attribute slot, produced by DatasetProxy::operator[]. Intended as a temporary
// in an expression; it owns an element reference.
class DatasetRef {
 public:
  DatasetRef(const DatasetRef&) = delete;
  DatasetRef& operator=(const DatasetRef&) = delete;
  ~DatasetRef() { ulDestroyDOMElement(element_); }

  void operator=(std::string_view value) {
    CString v(value);
    ulDOMElementSetAttribute(element_, name_.c_str(), v.c_str(), nullptr);
  }

  // Reads as an empty string when the attribute is absent.
  operator std::string() const {
    return TakeString(ulDOMElementGetAttribute(element_, name_.c_str()));
  }

  // Remove the underlying data-* attribute (the `delete el.dataset.x` analog).
  void Remove() { ulDOMElementRemoveAttribute(element_, name_.c_str()); }

  friend bool operator==(const DatasetRef& r, std::string_view v) {
    return std::string(r) == v;
  }

 private:
  friend struct DatasetProxy;
  DatasetRef(ULDOMElement owned_element, std::string name)
      : element_(owned_element), name_(std::move(name)) {}

  ULDOMElement element_;
  std::string name_;
};

///
/// The el.dataset proxy: custom data attributes keyed by their camelCase names
/// (el.dataset["userId"] is the `data-user-id` attribute), exactly the web's dataset name
/// conversion.
///
/// A key containing a `-` followed by an ASCII lowercase letter cannot round-trip through
/// that conversion, and the web throws for it. This API does not throw: writes through
/// such a key are benign no-ops and reads return an empty string (Strict diagnostics log
/// the suppressed operations).
///
struct DatasetProxy {
  DatasetProxy() = default;
  DatasetProxy(const DatasetProxy&) = delete;
  DatasetProxy& operator=(const DatasetProxy&) = delete;

  // Access a data-* slot by camelCase key, for reading or writing (a const Element writes too).
  DatasetRef operator[](std::string_view camel_key) const;
};

// ---------------------------------------------------------------------------------------
// Waist adapters for the element-level properties.
// ---------------------------------------------------------------------------------------

struct TextContentTag {
  static std::string Get(ULDOMElement h) {
    return TakeString(ulDOMElementGetTextContent(h));
  }
  static void Set(ULDOMElement h, std::string_view v) {
    ulDOMElementSetTextContent(h, v.data() ? v.data() : "", v.size());
  }
};

struct InnerTextTag {
  static std::string Get(ULDOMElement h) { return TakeString(ulDOMElementGetInnerText(h)); }
  static void Set(ULDOMElement h, std::string_view v) {
    ulDOMElementSetInnerText(h, v.data() ? v.data() : "", v.size());
  }
};

struct InnerHTMLTag {
  static std::string Get(ULDOMElement h) { return TakeString(ulDOMElementGetInnerHTML(h)); }
  static void Set(ULDOMElement h, std::string_view v) {
    ulDOMElementSetInnerHTML(h, v.data() ? v.data() : "", v.size(), nullptr);
  }
};

struct OuterHTMLTag {
  static std::string Get(ULDOMElement h) { return TakeString(ulDOMElementGetOuterHTML(h)); }
  static void Set(ULDOMElement h, std::string_view v) {
    ulDOMElementSetOuterHTML(h, v.data() ? v.data() : "", v.size(), nullptr);
  }
};

// A string property reflecting a content attribute (a missing attribute reads empty).
template <FixedString Name>
struct StringAttributeTag {
  static std::string Get(ULDOMElement h) {
    return TakeString(ulDOMElementGetAttribute(h, Name.data));
  }
  static void Set(ULDOMElement h, std::string_view v) {
    CString value(v);
    ulDOMElementSetAttribute(h, Name.data, value.c_str(), nullptr);
  }
};

// A URL property: reads resolve the attribute against the document's base URL.
template <FixedString Name>
struct URLAttributeTag : StringAttributeTag<Name> {
  static std::string Get(ULDOMElement h) {
    return TakeString(ulDOMElementGetAttributeURL(h, Name.data));
  }
};

// A boolean property reflecting a content attribute's presence.
template <FixedString Name>
struct BoolAttributeTag {
  static bool Get(ULDOMElement h) { return ulDOMElementHasAttribute(h, Name.data); }
  static void Set(ULDOMElement h, bool v) {
    ulDOMElementToggleAttribute(h, Name.data, &v, nullptr, nullptr);
  }
};

using IdTag = StringAttributeTag<"id">;
using ClassNameTag = StringAttributeTag<"class">;
using TitleTag = StringAttributeTag<"title">;
using HiddenTag = BoolAttributeTag<"hidden">;
using NameTag = StringAttributeTag<"name">;
using PlaceholderTag = StringAttributeTag<"placeholder">;
using RequiredTag = BoolAttributeTag<"required">;
using ReadOnlyTag = BoolAttributeTag<"readonly">;
using SrcTag = URLAttributeTag<"src">;
using AltTag = StringAttributeTag<"alt">;
using HrefTag = URLAttributeTag<"href">;

struct TabIndexTag {
  static int Get(ULDOMElement h) { return ulDOMElementGetTabIndex(h); }
  static void Set(ULDOMElement h, int v) { ulDOMElementSetTabIndex(h, v); }
};

struct ValueTag {
  static std::string Get(ULDOMElement h) { return TakeString(ulDOMElementGetValue(h)); }
  static void Set(ULDOMElement h, std::string_view v) {
    ulDOMElementSetValue(h, v.data() ? v.data() : "", v.size(), false, nullptr);
  }
};

struct DefaultValueTag {
  static std::string Get(ULDOMElement h) { return TakeString(ulDOMElementGetDefaultValue(h)); }
  static void Set(ULDOMElement h, std::string_view v) {
    ulDOMElementSetDefaultValue(h, v.data() ? v.data() : "", v.size());
  }
};

// Reads the engine's normalized type; writes set the attribute, like the web's setter.
struct InputTypeTag {
  static std::string Get(ULDOMElement h) { return TakeString(ulDOMElementGetType(h)); }
  static void Set(ULDOMElement h, std::string_view v) {
    CString value(v);
    ulDOMElementSetAttribute(h, "type", value.c_str(), nullptr);
  }
};

struct CheckedTag {
  static bool Get(ULDOMElement h) { return ulDOMElementGetChecked(h); }
  static void Set(ULDOMElement h, bool v) { ulDOMElementSetChecked(h, v); }
};

struct DisabledTag {
  static bool Get(ULDOMElement h) { return ulDOMElementGetDisabled(h); }
  static void Set(ULDOMElement h, bool v) { ulDOMElementSetDisabled(h, v); }
};

// The C API carries nullable indices as int64_t with -1 for the web's null (or -1).
inline std::optional<size_t> OptionalIndex(int64_t index) {
  if (index < 0)
    return std::nullopt;
  return static_cast<size_t>(index);
}

// Saturates, so an index too large for int64_t stays out of range instead of wrapping.
inline int64_t IndexArgument(size_t index) {
  constexpr size_t kMax = static_cast<size_t>(INT64_MAX);
  return static_cast<int64_t>(index > kMax ? kMax : index);
}

struct SelectedIndexTag {
  using Value = std::optional<size_t>;
  static Value Get(ULDOMElement h) { return OptionalIndex(ulDOMElementGetSelectedIndex(h)); }
  static void Set(ULDOMElement h, const Value& v) {
    ulDOMElementSetSelectedIndex(h, v ? IndexArgument(*v) : -1);
  }
};

struct ScrollTopTag {
  using Value = double;
  static double Get(ULDOMElement h) { return ulDOMElementGetScrollTop(h); }
  static void Set(ULDOMElement h, double v) { ulDOMElementSetScrollTop(h, v); }
};

struct ScrollLeftTag {
  using Value = double;
  static double Get(ULDOMElement h) { return ulDOMElementGetScrollLeft(h); }
  static void Set(ULDOMElement h, double v) { ulDOMElementSetScrollLeft(h, v); }
};

// Writing std::nullopt sets 0, like the web's conversion of null.
struct SelectionStartTag {
  using Value = std::optional<size_t>;
  static Value Get(ULDOMElement h) { return OptionalIndex(ulDOMElementGetSelectionStart(h)); }
  static void Set(ULDOMElement h, const Value& v) {
    ulDOMElementSetSelectionStart(h, IndexArgument(v.value_or(0)), nullptr);
  }
};

struct SelectionEndTag {
  using Value = std::optional<size_t>;
  static Value Get(ULDOMElement h) { return OptionalIndex(ulDOMElementGetSelectionEnd(h)); }
  static void Set(ULDOMElement h, const Value& v) {
    ulDOMElementSetSelectionEnd(h, IndexArgument(v.value_or(0)), nullptr);
  }
};

// Defined in Element.h, after SelectionDirection.
struct SelectionDirectionTag {
  using Value = SelectionDirection;
  static SelectionDirection Get(ULDOMElement h);
  static void Set(ULDOMElement h, SelectionDirection v);
};

struct OptionSelectedTag {
  static bool Get(ULDOMElement h) { return ulDOMElementGetSelected(h); }
  static void Set(ULDOMElement h, bool v) { ulDOMElementSetSelected(h, v); }
};

// Writes replace the option's children with the text, which is what the web's setter does.
struct OptionTextTag {
  static std::string Get(ULDOMElement h) { return TakeString(ulDOMElementGetText(h)); }
  static void Set(ULDOMElement h, std::string_view v) {
    ulDOMElementSetTextContent(h, v.data() ? v.data() : "", v.size());
  }
};

// Node's handle storage (Element and the typed views inherit it). Public so the proxy
// owner-recovery helpers can read it; not part of the API (use raw()).
struct NodeStorage {
  ULDOMNode handle = nullptr;
};

// Recovers the owning Node's raw handle from a node-proxy member's address (the same
// computed-offset arithmetic as OwnerHandle, one level down the hierarchy). Defined
// after Node.
inline ULDOMNode OwnerNodeHandle(const void* member, size_t member_offset);

// The byte offset of a node-level property member inside Node, keyed by its waist-adapter
// tag. Specialized per member after Node.
template <class Tag>
inline size_t NodePropOffset();

// A read-write string property of Node (nodeValue, textContent), recovered through the
// Node-level owner arithmetic. Same shape as StringProp.
template <class Tag>
struct NodeStringProp {
  NodeStringProp() = default;
  NodeStringProp(const NodeStringProp&) = delete;
  NodeStringProp& operator=(const NodeStringProp&) = delete;

  void operator=(std::string_view value) const;

  template <typename T>
    requires std::is_arithmetic_v<T>
  void operator=(T value) const {
    *this = std::string_view(FormatNumber(value));
  }

  void operator+=(std::string_view suffix) const;

  operator std::string() const;

  friend bool operator==(const NodeStringProp& p, std::string_view v) {
    return std::string(p) == v;
  }
};

struct NodeValueTag {
  static std::string Get(ULDOMNode h) { return TakeString(ulDOMNodeGetNodeValue(h)); }
  static void Set(ULDOMNode h, std::string_view v) {
    ulDOMNodeSetNodeValue(h, v.data() ? v.data() : "", v.size());
  }
};

struct NodeTextContentTag {
  static std::string Get(ULDOMNode h) { return TakeString(ulDOMNodeGetTextContent(h)); }
  static void Set(ULDOMNode h, std::string_view v) {
    ulDOMNodeSetTextContent(h, v.data() ? v.data() : "", v.size());
  }
};

// Recovers the owning Document's raw handle from a document-proxy member's address (the
// OwnerHandle arithmetic, keyed on Document). Defined after Document.
inline ULDOMDocument OwnerDocumentHandle(const void* member, size_t member_offset);

// The byte offset of a document-level property member inside Document, keyed by its
// waist-adapter tag. Specialized per member after Document.
template <class Tag>
inline size_t DocumentPropOffset();

// A read-write string property of Document (title). Same shape as StringProp.
template <class Tag>
struct DocumentStringProp {
  DocumentStringProp() = default;
  DocumentStringProp(const DocumentStringProp&) = delete;
  DocumentStringProp& operator=(const DocumentStringProp&) = delete;

  void operator=(std::string_view value) const;

  template <typename T>
    requires std::is_arithmetic_v<T>
  void operator=(T value) const {
    *this = std::string_view(FormatNumber(value));
  }

  void operator+=(std::string_view suffix) const;

  operator std::string() const;

  friend bool operator==(const DocumentStringProp& p, std::string_view v) {
    return std::string(p) == v;
  }
};

struct DocumentTitleTag {
  static std::string Get(ULDOMDocument h) { return TakeString(ulDOMDocumentGetTitle(h)); }
  static void Set(ULDOMDocument h, std::string_view v) {
    ulDOMDocumentSetTitle(h, v.data() ? v.data() : "", v.size());
  }
};

}  // namespace detail
/// \endcond

}  // namespace dom
}  // namespace ultralight
