///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/CAPI/CAPI_DOMElement.h>
#include <Ultralight/CAPI/CAPI_DOMEvent.h>
#include <Ultralight/dom/detail/CSSPropertyTable.h>
#include <Ultralight/dom/detail/Support.h>

#include <charconv>
#include <cstddef>
#include <cstdio>
#include <string>
#include <string_view>
#include <type_traits>

// Internal machinery behind the Element property proxies: the generated style-name
// tables, the name validators, and the declarations that the definitions in
// PropertiesImpl.h fill in once Element is complete. Not part of the public API.

namespace ultralight {
namespace dom {

/// \cond INTERNAL
namespace detail {

// Recovers the owning Element's raw handle from a property-proxy member's address.
// Every proxy member sits at a compile-time offset inside Element (pinned by the
// static_asserts after the class), so the member's own address minus that offset is the
// Element. Defined after Element.
inline ULDOMElement OwnerHandle(const void* member, size_t member_offset);

// The byte offset of style-proxy member `index` from the START OF ELEMENT (the member's
// offset inside StyleProxy plus StyleProxy's offset in Element). Defined after Element.
inline size_t StyleMemberTotalOffset(size_t index);

// The byte offset of an element-level property member inside Element, keyed by its
// waist-adapter tag. Specialized per member after Element.
template <class Tag>
size_t ElementPropOffset();

// Trampolines behind addEventListener()/On(); defined after Element.
template <typename Fn>
void EventThunk(void* user_data, ULDOMEvent event);
template <typename Fn>
void DelegatedThunk(void* user_data, ULDOMEvent event, ULDOMElement matched);
template <typename Fn>
void DeleteCallable(void* user_data);

// JavaScript-style ToString for numeric property assignment (shortest round-trip form
// where the standard library provides it).
template <typename T>
inline std::string FormatNumber(T value) {
  if constexpr (std::is_same_v<T, bool>) {
    return value ? "true" : "false";
  } else {
    char buffer[40];
#if defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 201611L
    auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
    return std::string(buffer, result.ptr);
#else
    if constexpr (std::is_floating_point_v<T>)
      snprintf(buffer, sizeof(buffer), "%.17g", static_cast<double>(value));
    else if constexpr (std::is_signed_v<T>)
      snprintf(buffer, sizeof(buffer), "%lld", static_cast<long long>(value));
    else
      snprintf(buffer, sizeof(buffer), "%llu", static_cast<unsigned long long>(value));
    return std::string(buffer);
#endif
  }
}

// dataset camelCase key -> `data-*` attribute name (the HTML dataset conversion:
// each ASCII capital becomes a dash plus its lowercase form).
inline std::string DataAttributeName(std::string_view camel) {
  std::string name = "data-";
  name.reserve(5 + camel.size());
  for (char c : camel) {
    if (c >= 'A' && c <= 'Z') {
      name += '-';
      name += static_cast<char>(c - 'A' + 'a');
    } else {
      name += c;
    }
  }
  return name;
}

// DOMTokenList token validity (the web's rule): non-empty and free of ASCII whitespace.
inline constexpr bool IsValidClassToken(std::string_view token) {
  if (token.empty())
    return false;
  for (char c : token) {
    if (c == ' ' || c == '\t' || c == '\n' || c == '\f' || c == '\r')
      return false;
  }
  return true;
}

// dataset key validity (the web's rule): a key containing a '-' followed by an ASCII
// lowercase letter cannot round-trip through the data-* attribute-name conversion (the
// attribute it names would read back under a different key), so the web rejects it.
inline constexpr bool IsValidDatasetKey(std::string_view camel) {
  for (size_t i = 0; i + 1 < camel.size(); i++) {
    if (camel[i] == '-' && camel[i + 1] >= 'a' && camel[i + 1] <= 'z')
      return false;
  }
  return true;
}

// Every property name the style proxy carries, in member-declaration order (properties,
// then legacy aliases). Index positions here, in the member list, and in the offset table
// all correspond; the strings are literal-backed and null-terminated.
inline constexpr std::string_view kStylePropertyNames[] = {
#define ULTRALIGHT_DOM_V(ident, cssname) cssname,
    ULDOM_FOR_EACH_CSS_PROPERTY(ULTRALIGHT_DOM_V)
#undef ULTRALIGHT_DOM_V
#define ULTRALIGHT_DOM_V(ident, cssname, canonical) cssname,
        ULDOM_FOR_EACH_CSS_PROPERTY_ALIAS(ULTRALIGHT_DOM_V)
#undef ULTRALIGHT_DOM_V
};

inline constexpr size_t kStylePropertyCount
    = sizeof(kStylePropertyNames) / sizeof(kStylePropertyNames[0]);

constexpr size_t StyleIndexOf(std::string_view name) {
  for (size_t i = 0; i < kStylePropertyCount; i++) {
    if (kStylePropertyNames[i] == name)
      return i;
  }
  return static_cast<size_t>(-1);
}

template <FixedString Name>
inline constexpr size_t kStyleIndex = StyleIndexOf(Name.view());

constexpr bool IsKnownCSSProperty(std::string_view name) {
  if (name.size() > 2 && name[0] == '-' && name[1] == '-')
    return true;  // Custom properties have open syntax; any --name is legal.
  return StyleIndexOf(name) != static_cast<size_t>(-1);
}

// Every property and alias row keyed by its JavaScript identifier (backgroundColor, cssFloat,
// webkitTransform), sorted by identifier for the binary search in CSSNameOf().
struct CSSIdentifierRow {
  std::string_view identifier;
  std::string_view css_name;
};

inline constexpr CSSIdentifierRow kCSSIdentifierRows[] = {
#define ULTRALIGHT_DOM_V(ident, cssname) { #ident, cssname },
    ULDOM_FOR_EACH_CSS_IDENTIFIER_SORTED(ULTRALIGHT_DOM_V)
#undef ULTRALIGHT_DOM_V
};

inline constexpr size_t kCSSIdentifierCount
    = sizeof(kCSSIdentifierRows) / sizeof(kCSSIdentifierRows[0]);

// A camelCase name has no '-' and at least one ASCII capital. Every other name (kebab-case,
// a custom property, or a single lowercase word) reads the same in both forms.
constexpr bool IsCamelCaseName(std::string_view name) {
  bool has_capital = false;
  for (char c : name) {
    if (c == '-')
      return false;
    if (c >= 'A' && c <= 'Z')
      has_capital = true;
  }
  return has_capital;
}

// The CSS name for a style property name written in either form: a camelCase identifier from
// the generated table becomes its CSS name (backgroundColor -> background-color, cssFloat ->
// float, webkitTransform -> -webkit-transform); any other name comes back unchanged, so an
// unknown camelCase name reaches the engine as the unknown name it is. The result is
// null-terminated whenever the argument is (it is a table literal or the argument itself).
constexpr std::string_view CSSNameOf(std::string_view name) {
  if (!IsCamelCaseName(name))
    return name;
  size_t lo = 0;
  size_t hi = kCSSIdentifierCount;
  while (lo < hi) {
    const size_t mid = lo + (hi - lo) / 2;
    if (kCSSIdentifierRows[mid].identifier < name)
      lo = mid + 1;
    else
      hi = mid;
  }
  if (lo < kCSSIdentifierCount && kCSSIdentifierRows[lo].identifier == name)
    return kCSSIdentifierRows[lo].css_name;
  return name;
}

template <FixedString Name>
inline constexpr std::string_view kCSSName = CSSNameOf(Name.view());

}  // namespace detail
/// \endcond

}  // namespace dom
}  // namespace ultralight
