///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/detail/Exceptions.h>

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

// The property-proxy offset tables and operator definitions. Included by
// <Ultralight/dom/Element.h> after the Element definition (everything below needs the
// complete type); do not include this header directly.

namespace ultralight {
namespace dom {

/// \cond INTERNAL
namespace detail {

// Element is not standard-layout (its data spans Node and Element), so offsetof on it
// is conditionally supported and GCC and clang warn at every use. The offsets recover
// the owning Element from a proxy member, which the suites pin; the warning would
// otherwise reach every GCC and clang consumer of the DOM headers.
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif

inline ULDOMElement OwnerHandle(const void* member, size_t member_offset) {
  const Element* element = reinterpret_cast<const Element*>(
      reinterpret_cast<const char*>(member) - member_offset);
  return element->raw();
}

// Style member offsets, in the same order as kStylePropertyNames. Under attribute
// collapse every entry is zero; without it each member keeps a distinct offset. Either
// way the recovery arithmetic below is exact.
inline const size_t* StyleMemberOffsets() {
  static const size_t offsets[] = {
#define ULTRALIGHT_DOM_V(ident, cssname) offsetof(StyleProxy, ident),
    ULDOM_FOR_EACH_CSS_PROPERTY(ULTRALIGHT_DOM_V)
#undef ULTRALIGHT_DOM_V
#define ULTRALIGHT_DOM_V(ident, cssname, canonical) offsetof(StyleProxy, ident),
        ULDOM_FOR_EACH_CSS_PROPERTY_ALIAS(ULTRALIGHT_DOM_V)
#undef ULTRALIGHT_DOM_V
  };
  return offsets;
}

inline size_t StyleMemberTotalOffset(size_t index) {
  return StyleMemberOffsets()[index] + offsetof(Element, style);
}

#define ULTRALIGHT_DOM_PROP_OFFSET(Tag, member) \
  template <> \
  inline size_t ElementPropOffset<Tag>() { \
    return offsetof(Element, member); \
  }
ULTRALIGHT_DOM_PROP_OFFSET(TextContentTag, textContent)
ULTRALIGHT_DOM_PROP_OFFSET(InnerTextTag, innerText)
ULTRALIGHT_DOM_PROP_OFFSET(InnerHTMLTag, innerHTML)
ULTRALIGHT_DOM_PROP_OFFSET(OuterHTMLTag, outerHTML)
ULTRALIGHT_DOM_PROP_OFFSET(IdTag, id)
ULTRALIGHT_DOM_PROP_OFFSET(ClassNameTag, className)
ULTRALIGHT_DOM_PROP_OFFSET(TitleTag, title)
ULTRALIGHT_DOM_PROP_OFFSET(HiddenTag, hidden)
ULTRALIGHT_DOM_PROP_OFFSET(TabIndexTag, tabIndex)
ULTRALIGHT_DOM_PROP_OFFSET(ValueTag, value)
ULTRALIGHT_DOM_PROP_OFFSET(NameTag, name)
ULTRALIGHT_DOM_PROP_OFFSET(CheckedTag, checked)
ULTRALIGHT_DOM_PROP_OFFSET(DisabledTag, disabled)
ULTRALIGHT_DOM_PROP_OFFSET(SelectedIndexTag, selectedIndex)
ULTRALIGHT_DOM_PROP_OFFSET(ScrollTopTag, scrollTop)
ULTRALIGHT_DOM_PROP_OFFSET(ScrollLeftTag, scrollLeft)
#undef ULTRALIGHT_DOM_PROP_OFFSET

// View-hosted proxies. A tag carried by several views lives at the SAME offset in each of
// them (the views declare their shared proxies first, in one order; pinned below), so one
// specialization per tag serves them all; the owner recovery still lands on the Element
// subobject (same address, single inheritance).
#define ULTRALIGHT_DOM_SHARED_OFFSET(ViewA, ViewB, member) \
  static_assert(offsetof(ViewA, member) == offsetof(ViewB, member), \
                "a proxy carried by several views must sit at one shared offset")
ULTRALIGHT_DOM_SHARED_OFFSET(HTMLInputElement, HTMLTextAreaElement, required);
ULTRALIGHT_DOM_SHARED_OFFSET(HTMLInputElement, HTMLSelectElement, required);
ULTRALIGHT_DOM_SHARED_OFFSET(HTMLInputElement, HTMLTextAreaElement, placeholder);
ULTRALIGHT_DOM_SHARED_OFFSET(HTMLInputElement, HTMLTextAreaElement, readOnly);
ULTRALIGHT_DOM_SHARED_OFFSET(HTMLInputElement, HTMLTextAreaElement, defaultValue);
ULTRALIGHT_DOM_SHARED_OFFSET(HTMLInputElement, HTMLTextAreaElement, selectionStart);
ULTRALIGHT_DOM_SHARED_OFFSET(HTMLInputElement, HTMLTextAreaElement, selectionEnd);
ULTRALIGHT_DOM_SHARED_OFFSET(HTMLInputElement, HTMLTextAreaElement, selectionDirection);
ULTRALIGHT_DOM_SHARED_OFFSET(HTMLImageElement, HTMLIFrameElement, src);
#undef ULTRALIGHT_DOM_SHARED_OFFSET

#define ULTRALIGHT_DOM_VIEW_PROP_OFFSET(Tag, View, member) \
  template <> \
  inline size_t ElementPropOffset<Tag>() { \
    return offsetof(View, member); \
  }
ULTRALIGHT_DOM_VIEW_PROP_OFFSET(RequiredTag, HTMLInputElement, required)
ULTRALIGHT_DOM_VIEW_PROP_OFFSET(PlaceholderTag, HTMLInputElement, placeholder)
ULTRALIGHT_DOM_VIEW_PROP_OFFSET(ReadOnlyTag, HTMLInputElement, readOnly)
ULTRALIGHT_DOM_VIEW_PROP_OFFSET(DefaultValueTag, HTMLInputElement, defaultValue)
ULTRALIGHT_DOM_VIEW_PROP_OFFSET(SelectionStartTag, HTMLInputElement, selectionStart)
ULTRALIGHT_DOM_VIEW_PROP_OFFSET(SelectionEndTag, HTMLInputElement, selectionEnd)
ULTRALIGHT_DOM_VIEW_PROP_OFFSET(SelectionDirectionTag, HTMLInputElement, selectionDirection)
ULTRALIGHT_DOM_VIEW_PROP_OFFSET(InputTypeTag, HTMLInputElement, type)
ULTRALIGHT_DOM_VIEW_PROP_OFFSET(OptionSelectedTag, HTMLOptionElement, selected)
ULTRALIGHT_DOM_VIEW_PROP_OFFSET(OptionTextTag, HTMLOptionElement, text)
ULTRALIGHT_DOM_VIEW_PROP_OFFSET(SrcTag, HTMLImageElement, src)
ULTRALIGHT_DOM_VIEW_PROP_OFFSET(AltTag, HTMLImageElement, alt)
ULTRALIGHT_DOM_VIEW_PROP_OFFSET(HrefTag, HTMLAnchorElement, href)
#undef ULTRALIGHT_DOM_VIEW_PROP_OFFSET

// Per-property interned-ID cache: resolved once per instantiation at first write, then
// every write skips the waist's per-call name lookup. IDs are runtime-only (engine
// property IDs shift across versions; nothing is baked at compile time), and a zero
// resolution (custom or engine-unknown name) falls back to the string route forever.
// Name may be camelCase (dom::css<"backgroundColor">); the engine only knows the CSS name.
template <FixedString Name>
inline int InternedStylePropertyID() {
  static const int id = ulDOMInternStyleProperty(kCSSName<Name>.data());
  return id;
}

template <FixedString Name>
void StyleProp<Name>::AssignText(std::string_view value) const {
  ULDOMElement h = OwnerHandle(this, StyleMemberTotalOffset(kStyleIndex<Name>));
  CString v(value);
  if (int id = InternedStylePropertyID<Name>())
    ulDOMElementSetStylePropertyByID(h, id, v.c_str(), false, nullptr);
  else
    ulDOMElementSetStyleProperty(h, Name.data, v.c_str(), false, nullptr);
}

template <FixedString Name>
void StyleProp<Name>::operator=(CssValue value) const {
  if (value.kind() == CssValue::Kind::Numeric) {
    *this = StyleValue { value.value(), value.unit() };
    return;
  }
  ULDOMElement h = OwnerHandle(this, StyleMemberTotalOffset(kStyleIndex<Name>));
  if (int id = InternedStylePropertyID<Name>())
    ulDOMElementSetStylePropertyByID(h, id, value.text(), false, nullptr);
  else
    ulDOMElementSetStyleProperty(h, Name.data, value.text(), false, nullptr);
}

template <FixedString Name>
void StyleProp<Name>::AssignColor(ULColor color) const {
  ULDOMElement h = OwnerHandle(this, StyleMemberTotalOffset(kStyleIndex<Name>));
  if (int id = InternedStylePropertyID<Name>())
    ulDOMElementSetStylePropertyColorByID(h, id, color, false);
  else
    ulDOMElementSetStylePropertyColor(h, Name.data, color, false);
}

template <FixedString Name>
void StyleProp<Name>::operator=(StyleValue value) const {
  if (value.IsEmpty()) {
    AssignText("");
    return;
  }
  ULDOMElement h = OwnerHandle(this, StyleMemberTotalOffset(kStyleIndex<Name>));
  if (int id = InternedStylePropertyID<Name>()) {
    ulDOMElementSetStylePropertyNumericByID(h, id, value.value,
                                            static_cast<ULDOMStyleUnit>(value.unit), false);
  } else {
    ulDOMElementSetStylePropertyNumeric(h, Name.data, value.value,
                                        static_cast<ULDOMStyleUnit>(value.unit), false);
  }
}

template <FixedString Name>
StyleProp<Name>::operator std::string() const {
  ULDOMElement h = OwnerHandle(this, StyleMemberTotalOffset(kStyleIndex<Name>));
  return TakeString(ulDOMElementGetStyleProperty(h, Name.data));
}

inline void CssTextProp::operator=(std::string_view css_text) const {
  ULDOMElement h
      = OwnerHandle(this, offsetof(StyleProxy, cssText) + offsetof(Element, style));
  ulDOMElementSetStyleCssText(h, css_text.data() ? css_text.data() : "", css_text.size());
}

inline CssTextProp::operator std::string() const {
  ULDOMElement h
      = OwnerHandle(this, offsetof(StyleProxy, cssText) + offsetof(Element, style));
  return TakeString(ulDOMElementGetStyleCssText(h));
}

inline void StyleProxy::SetPropertyText(std::string_view name, std::string_view value,
                                        bool important) const {
  ULDOMElement h = OwnerHandle(this, offsetof(Element, style));
  CString n(name);
  CString v(value);
  ulDOMElementSetStyleProperty(h, n.c_str(), v.c_str(), important, nullptr);
}

inline void StyleProxy::setProperty(std::string_view name, CssValue value,
                                     bool important) const {
  ULDOMElement h = OwnerHandle(this, offsetof(Element, style));
  CString n(name);
  // Numeric literals on a CUSTOM property (--name) take the text route: custom
  // properties have no numeric waist entry, and their captured spelling is the value.
  if (value.kind() == CssValue::Kind::Numeric && !IsCustomPropertyName(name)) {
    ulDOMElementSetStylePropertyNumeric(h, n.c_str(), value.value(),
                                        static_cast<ULDOMStyleUnit>(value.unit()), important);
    return;
  }
  ulDOMElementSetStyleProperty(h, n.c_str(), value.text(), important, nullptr);
}

inline bool StyleProxy::SetPropertyColor(std::string_view name, ULColor color,
                                         bool important) const {
  ULDOMElement h = OwnerHandle(this, offsetof(Element, style));
  CString n(name);
  return ulDOMElementSetStylePropertyColor(h, n.c_str(), color, important);
}

inline std::string StyleProxy::getPropertyValue(std::string_view name) const {
  ULDOMElement h = OwnerHandle(this, offsetof(Element, style));
  CString n(name);
  return TakeString(ulDOMElementGetStyleProperty(h, n.c_str()));
}

inline std::string StyleProxy::removeProperty(std::string_view name) const {
  ULDOMElement h = OwnerHandle(this, offsetof(Element, style));
  CString n(name);
  std::string old = TakeString(ulDOMElementGetStyleProperty(h, n.c_str()));
  ulDOMElementRemoveStyleProperty(h, n.c_str());
  return old;
}

inline bool StyleProxy::setProperty(std::string_view name, StyleValue value,
                                    bool important) const {
  ULDOMElement h = OwnerHandle(this, offsetof(Element, style));
  CString n(name);
  if (value.IsEmpty())
    return ulDOMElementSetStyleProperty(h, n.c_str(), "", important, nullptr);
  return ulDOMElementSetStylePropertyNumeric(h, n.c_str(), value.value,
                                             static_cast<ULDOMStyleUnit>(value.unit),
                                             important);
}

inline StyleRef StyleProxy::operator[](std::string_view name) const {
  ULDOMElement h = OwnerHandle(this, offsetof(Element, style));
  return StyleRef(h ? ulCreateDOMElementRef(h) : nullptr, std::string(CSSNameOf(name)));
}

template <FixedString Name>
StyleRef StyleProxy::operator[](CSSPropertyName<Name>) const {
  ULDOMElement h = OwnerHandle(this, offsetof(Element, style));
  return StyleRef(h ? ulCreateDOMElementRef(h) : nullptr, std::string(kCSSName<Name>),
                  InternedStylePropertyID<Name>());
}

inline bool ClassListProxy::contains(std::string_view token) const {
  ULDOMElement h = OwnerHandle(this, offsetof(Element, classList));
  CString t(token);
  return ulDOMElementClassListContains(h, t.c_str());
}

inline void ClassListProxy::AddOne(std::string_view token) const {
  ULDOMElement h = OwnerHandle(this, offsetof(Element, classList));
  CString t(token);
  ulDOMElementClassListAdd(h, t.c_str(), nullptr);
}

inline void ClassListProxy::RemoveOne(std::string_view token) const {
  ULDOMElement h = OwnerHandle(this, offsetof(Element, classList));
  CString t(token);
  ulDOMElementClassListRemove(h, t.c_str(), nullptr);
}

inline bool ClassListProxy::toggle(std::string_view token) const {
  ULDOMElement h = OwnerHandle(this, offsetof(Element, classList));
  CString t(token);
  bool present = false;
  ulDOMElementClassListToggle(h, t.c_str(), nullptr, &present, nullptr);
  return present;
}

inline bool ClassListProxy::toggle(std::string_view token, bool force) const {
  ULDOMElement h = OwnerHandle(this, offsetof(Element, classList));
  CString t(token);
  bool present = false;
  ulDOMElementClassListToggle(h, t.c_str(), &force, &present, nullptr);
  return present;
}

inline bool ClassListProxy::replace(std::string_view token,
                                    std::string_view new_token) const {
  ULDOMElement h = OwnerHandle(this, offsetof(Element, classList));
  CString t(token);
  CString n(new_token);
  bool replaced = false;
  ulDOMElementClassListReplace(h, t.c_str(), n.c_str(), &replaced, nullptr);
  return replaced;
}

inline size_t ClassListProxy::length() const {
  ULDOMElement h = OwnerHandle(this, offsetof(Element, classList));
  return ulDOMElementClassListGetLength(h);
}

inline std::string ClassListProxy::item(size_t index) const {
  ULDOMElement h = OwnerHandle(this, offsetof(Element, classList));
  return TakeString(ulDOMElementClassListGetItem(h, index));
}

inline DatasetRef DatasetProxy::operator[](std::string_view camel_key) const {
  ULDOMElement h = OwnerHandle(this, offsetof(Element, dataset));
  // An unrepresentable key yields a null-element slot: every operation on it is a benign
  // no-op (validated BEFORE any attribute mutation; the web throws instead).
  if (!IsValidDatasetKey(camel_key))
    h = nullptr;
  return DatasetRef(h ? ulCreateDOMElementRef(h) : nullptr, DataAttributeName(camel_key));
}

template <class Tag>
void StringProp<Tag>::operator=(std::string_view value) const {
  Tag::Set(OwnerHandle(this, ElementPropOffset<Tag>()), value);
}

template <class Tag>
void StringProp<Tag>::operator+=(std::string_view suffix) const {
  ULDOMElement h = OwnerHandle(this, ElementPropOffset<Tag>());
  std::string combined = Tag::Get(h);
  combined.append(suffix);
  Tag::Set(h, combined);
}

template <class Tag>
StringProp<Tag>::operator std::string() const {
  return Tag::Get(OwnerHandle(this, ElementPropOffset<Tag>()));
}

template <class Tag>
void BoolProp<Tag>::operator=(bool value) const {
  Tag::Set(OwnerHandle(this, ElementPropOffset<Tag>()), value);
}

template <class Tag>
BoolProp<Tag>::operator bool() const {
  return Tag::Get(OwnerHandle(this, ElementPropOffset<Tag>()));
}

template <class Tag>
void IntProp<Tag>::operator=(int value) const {
  Tag::Set(OwnerHandle(this, ElementPropOffset<Tag>()), value);
}

template <class Tag>
IntProp<Tag>::operator int() const {
  return Tag::Get(OwnerHandle(this, ElementPropOffset<Tag>()));
}

template <class Tag>
void ValueProp<Tag>::operator=(const Value& value) const {
  Tag::Set(OwnerHandle(this, ElementPropOffset<Tag>()), value);
}

template <class Tag>
ValueProp<Tag>::operator Value() const {
  return Tag::Get(OwnerHandle(this, ElementPropOffset<Tag>()));
}

// Runs a listener or hook (`where` names its kind for the log). With exceptions enabled, one
// that throws is reported and skipped and the page's event dispatch continues, since
// unwinding into the library is undefined behavior.
template <typename Body>
void CallListener(const char* where, Body&& body) {
  ultralight::detail::CallCatchingExceptions(
      std::forward<Body>(body),
      [where](const char* what) { ulDOMReportCallbackException(where, what); });
}

template <typename Fn>
void EventThunk(void* user_data, ULDOMEvent event) {
  Fn& fn = *static_cast<Fn*>(user_data);
  CallListener("event listener", [&] {
    if constexpr (InvocableWithEvent<Fn>)
      fn(Event(event));
    else
      fn();
  });
}

template <typename Fn>
void DelegatedThunk(void* user_data, ULDOMEvent event, ULDOMElement matched) {
  Fn& fn = *static_cast<Fn*>(user_data);
  CallListener("delegated event listener", [&] {
    if constexpr (InvocableWithEvent<Fn, Element>)
      fn(Event(event), Element::FromBorrowed(matched));
    else if constexpr (std::is_invocable_v<Fn&, Element>)
      fn(Element::FromBorrowed(matched));
    else
      fn();
  });
}

template <typename Fn>
void DeleteCallable(void* user_data) {
  delete static_cast<Fn*>(user_data);
}

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

}  // namespace detail
/// \endcond

}  // namespace dom
}  // namespace ultralight
