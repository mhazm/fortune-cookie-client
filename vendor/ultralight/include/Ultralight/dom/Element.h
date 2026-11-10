///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
// X11's headers define None as a macro, which would break the None enumerators.
#pragma push_macro("None")
#undef None

#include <Ultralight/CAPI/CAPI_DOMElement.h>
#include <Ultralight/CAPI/CAPI_DOMEvent.h>
#include <Ultralight/dom/CSSPropertyName.h>
#include <Ultralight/dom/DOMRect.h>
#include <Ultralight/dom/Error.h>
#include <Ultralight/dom/Event.h>
#include <Ultralight/dom/EventListener.h>
#include <Ultralight/dom/Holders.h>
#include <Ultralight/dom/Node.h>
#include <Ultralight/dom/StyleValue.h>
#include <Ultralight/dom/detail/Properties.h>
#include <Ultralight/dom/detail/Receivers.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace ultralight {
namespace dom {

class Document;
class DocumentFragment;
class Element;
class ElementList;
class HTMLInputElement;
class HTMLTextAreaElement;
class HTMLSelectElement;
class HTMLOptionElement;
class HTMLFormElement;
class HTMLIFrameElement;
class HTMLImageElement;
class HTMLAnchorElement;

/// \cond INTERNAL
namespace detail {

// The dom::Checked result of a delegated registration (Element::On(), Document::On()). The C
// call refuses a malformed selector, the only refusal left once the receiver is alive and the
// signal isn't aborted.
inline Result<EventListener> CheckedDelegation(EventListener listener, bool receiver_alive,
                                               const AddEventListenerOptions& options,
                                               std::string_view selector) {
  if (listener.raw() || options.signal.aborted())
    return listener;
  if (!receiver_alive)
    return Unexpected<Error>(Error::PageGone());
  std::string message = "'" + std::string(selector) + "' is not a valid selector.";
  ErrorScope error;
  error.out()->code = kULDOMErrorCode_SyntaxError;
  error.out()->message = ulCreateStringUTF8(message.data(), message.size());
  return Unexpected<Error>(error.TakeError());
}

}  // namespace detail
/// \endcond

///
/// A handle to an element on a page.
///
/// dom::Element is a safe handle to an element on a page, providing direct access to document
/// content and events from C++. Its syntax matches JavaScript's DOM, and it doesn't require
/// JavaScript to be enabled.
///
/// You can store handles in native objects to update your interface when application state changes,
/// or attach callbacks to respond to user interactions.
///
/// This example updates an element and listens for a click:
///
/// ```
/// dom::Element hud = document.querySelector("#hud");
///
/// hud.textContent = "Ready";                   // properties you can set are members
/// hud.classList.add("visible");
/// hud.style.width = dom::StyleValue::Pct(42);
/// dom::Element wrapper = hud.parentElement();  // read-only ones are methods
///
/// hud.addEventListener("click", [] { Log("clicked"); });
/// ```
///
/// ## Handle Lifetime
///
/// Copying a handle creates another reference to the same element rather than duplicating the
/// element itself.
///
/// A handle keeps its element alive in memory for as long as its page lives, even after calling
/// remove() to take it out of the document tree.
///
/// ## Handle States
///
/// A handle's status depends on whether it holds an element and whether its page remains active:
///
/// - **Valid handles point to an element on an active page.** Operations on them run normally.
/// - **Empty handles hold no element.** Default-constructed handles, moved-from handles, and
///   queries that match nothing are empty, and calls on them do nothing.
/// - **Gone handles hold elements whose page was destroyed, navigated away, or had its frame
///   removed.** Calls on them do nothing.
///
/// Every call is safe in any state, so you can chain operations without null checks:
///
/// ```
/// document.querySelector("#save").click();  // does nothing if there's no match
/// ```
///
/// A handle whose page is gone never becomes valid again, even if that page comes back from the
/// back-forward cache. Every DOM handle follows these lifetime and safety rules.
///
/// ### Inspecting Failures
///
/// To find out why an operation failed, pass dom::Checked as an extra argument to return a
/// dom::Result holding a dom::Error. A misspelled selector yields an empty handle, so subsequent
/// calls do nothing silently unless Config::diagnostics.dom is set to DiagnosticsLevel::Strict.
///
/// ## Typed Element Views
///
/// You can narrow a generic element to a specific control type using helper methods like AsInput(),
/// AsTextArea(), and AsSelect(). An `As*()` view returns an empty handle if the element has a
/// different tag.
///
/// Converting an element to an input gives access to tag-specific properties:
///
/// ```
/// dom::HTMLInputElement name_input = document.getElementById("name").AsInput();
/// name_input.placeholder = "Your name";
/// ```
///
/// @see  dom::Error, dom::Checked, dom::Node, dom::EventListener, dom::getComputedStyle()
///
class Element : public Node {
 public:
  ///
  /// The element's inline style (style).
  ///
  /// Set a property through its member, by name, or all at once with cssText:
  ///
  /// ```
  /// el.style.width = "50%";                    // a string
  /// el.style.opacity = 0.5;                    // a number
  /// el.style.left = dom::StyleValue::Px(x);    // a number with a unit
  /// el.style["fontSize"] = "12px";             // by name ("font-size" works too)
  /// el.style.setProperty("--accent", "teal");  // by CSS name (custom properties too)
  /// el.style.cssText = "color: red; margin: 0";
  /// ```
  ///
  /// A value that doesn't parse is ignored and the old value stays, like the web. That includes
  /// a number the property doesn't take (eg, StyleValue::Px() for `opacity`), which also logs a
  /// warning. A mistyped numeric literal (eg, `"50pxx"`) is a compile error instead. An empty
  /// string, an Empty dom::StyleValue (or `{}`), or an unset Color removes the property, and an
  /// Invalid StyleValue or an invalid Color is ignored.
  ///
  /// Reading a member (or a property by name) gives you its value as a string. AsStyleValue() and
  /// AsColor() convert it. A property that isn't set gives an Empty StyleValue or an unset Color,
  /// and a value that doesn't convert gives an Invalid StyleValue or an invalid Color:
  ///
  /// ```
  /// std::string width = el.style.width;                   // eg, "50%"
  /// dom::StyleValue left = el.style.left.AsStyleValue();  // eg, 12 (px)
  /// Color accent = el.style["--accent"].AsColor();        // any CSS color text
  /// ```
  ///
  /// @note  Reads return what's set inline on this element, not the value that applies to it. Use
  ///        dom::getComputedStyle() for that.
  ///
  /// @see  <Ultralight/dom/StyleValue.h>
  ///
  detail::StyleProxy style;

  ///
  /// The element's classes (classList).
  ///
  /// Use contains(), add(), remove(), toggle(), replace(), length(), and item(). add() and remove()
  /// take several classes at once:
  ///
  /// ```
  /// el.classList.add("card", "selected");
  /// bool open = el.classList.toggle("open");
  /// ```
  ///
  /// @note  An empty class or one containing whitespace makes the whole call do nothing (the web
  ///        throws instead).
  ///
  detail::ClassListProxy classList;

  ///
  /// The element's `data-*` attributes by camelCase name (dataset).
  ///
  /// ```
  /// el.dataset["userId"] = "42";              // sets data-user-id="42"
  /// std::string id = el.dataset["userId"];    // "" if the attribute is missing
  /// el.dataset["userId"].Remove();            // removes the attribute
  /// ```
  ///
  /// @note  A name with a `-` followed by a lowercase letter (eg, `"user-id"`) can't be converted.
  ///        Writes through it do nothing (the web throws) and reads return an empty string.
  ///
  detail::DatasetProxy dataset;

  ///
  /// The text of this element and all its descendants (textContent).
  ///
  /// Assign to replace the element's children with the text (an empty string removes them).
  ///
  detail::StringProp<detail::TextContentTag> textContent;

  ///
  /// The element's text as it's rendered (innerText).
  ///
  /// Assign to replace the element's children with the text. Each line break becomes a `<br>`
  /// element.
  ///
  /// \parblock
  /// @note  Reading this updates the layout first if the page has pending changes. Use textContent
  ///        when you don't need the rendered form.
  /// \endparblock
  ///
  /// \parblock
  /// @note  Assigning does nothing on an element that isn't an HTML element (eg, SVG). Assign
  ///        textContent there.
  /// \endparblock
  ///
  detail::StringProp<detail::InnerTextTag> innerText;

  ///
  /// The markup of the element's children (innerHTML). Assign to replace the children with the
  /// parsed markup.
  ///
  /// \parblock
  /// @note  Scripts in assigned markup never run. Event handler attributes (eg, `onclick`) are
  ///        kept and work when JavaScript is enabled.
  /// \endparblock
  ///
  /// \parblock
  /// @note  In an XML document (eg, XHTML), assigning markup that isn't well-formed does nothing
  ///        (the web throws a SyntaxError).
  /// \endparblock
  ///
  detail::StringProp<detail::InnerHTMLTag> innerHTML;

  ///
  /// The markup of the element and its children (outerHTML).
  ///
  /// Assign to replace the element with the parsed markup (scripts in it never run). This handle
  /// then refers to the removed element.
  ///
  /// @note  Assigning does nothing if the element's parent isn't an element (eg, it's detached or
  ///        it's the `<html>` root). The web throws a NoModificationAllowedError there. Use
  ///        ulDOMElementSetOuterHTML() if you need the error.
  ///
  detail::StringProp<detail::OuterHTMLTag> outerHTML;

  ///
  /// The element's `id` attribute (id). Reads as an empty string if there's none.
  ///
  detail::StringProp<detail::IdTag> id;

  ///
  /// The element's `class` attribute as one string (className). Use classList to work with
  /// single classes.
  ///
  detail::StringProp<detail::ClassNameTag> className;

  ///
  /// The element's `title` attribute (title). Reads as an empty string if there's none.
  ///
  detail::StringProp<detail::TitleTag> title;

  ///
  /// Whether or not the element has a `hidden` attribute (hidden). Assign to add or remove it.
  ///
  /// A hidden element isn't displayed, unless a CSS rule gives it a `display` value.
  ///
  detail::BoolProp<detail::HiddenTag> hidden;

  ///
  /// The element's position in the keyboard focus order (tabIndex).
  ///
  /// This reads the `tabindex` attribute. Without it, elements that take focus by default (eg,
  /// links and form controls) read 0 and the rest read -1. Assign to set the attribute (-1 makes
  /// the element focusable with focus() but skipped by the Tab key).
  ///
  detail::IntProp<detail::TabIndexTag> tabIndex;

  ///
  /// The current value of a form control (value).
  ///
  /// This works on `<input>`, `<textarea>`, `<select>`, and `<option>`. Other elements read an
  /// empty string and ignore writes.
  ///
  /// @note  Assigning fires no events, like the web. Use SetValue() on HTMLInputElement,
  ///        HTMLTextAreaElement, or HTMLSelectElement to fire `input` and `change` like a user
  ///        edit.
  ///
  detail::StringProp<detail::ValueTag> value;

  ///
  /// The element's `name` attribute (name). Reads as an empty string if there's none.
  ///
  /// Form controls use it as the field name when their form is submitted.
  ///
  detail::StringProp<detail::NameTag> name;

  ///
  /// Whether or not a checkbox or radio button is checked (checked). Other elements read false.
  ///
  /// @note  Assigning fires no events, like the web. click() toggles it like a user would, firing
  ///        `input` and `change`.
  ///
  detail::BoolProp<detail::CheckedTag> checked;

  ///
  /// Whether or not the element has a `disabled` attribute (disabled). Assign to add or remove it.
  ///
  /// @note  A control inside a disabled `<fieldset>` is disabled without the attribute. Use
  ///        `matches(":disabled")` to check for that.
  ///
  detail::BoolProp<detail::DisabledTag> disabled;

  ///
  /// The index of the selected option in a `<select>` (selectedIndex), as a
  /// `std::optional<size_t>`.
  ///
  /// It reads std::nullopt when no option is selected (the web reads -1), and other elements read
  /// std::nullopt and ignore writes. Assigning std::nullopt or an index that's out of range clears
  /// the selection. Assigning fires no `change` event.
  ///
  /// ```
  /// std::optional<size_t> index = select.selectedIndex;
  /// select.selectedIndex = 2;
  /// ```
  ///
  detail::ValueProp<detail::SelectedIndexTag> selectedIndex;

  ///
  /// How far the element's content is scrolled down, in CSS pixels (scrollTop).
  ///
  /// Assign to scroll. The position is clamped to the scrollable range, and scrolling is instant
  /// (CSS `scroll-behavior` is ignored). For the root `<html>` element (`<body>` in a quirks-mode
  /// page), this scrolls the page.
  ///
  /// @note  The library currently scrolls elements in whole pixels, so a fraction is dropped.
  ///        Reads and writes update the layout first if the page has pending changes.
  ///
  detail::ValueProp<detail::ScrollTopTag> scrollTop;

  ///
  /// How far the element's content is scrolled right, in CSS pixels (scrollLeft).
  ///
  /// Works like scrollTop.
  ///
  detail::ValueProp<detail::ScrollLeftTag> scrollLeft;

  ///
  /// Create an empty Element.
  ///
  Element() {}

  ///
  /// Copy constructor (both handles refer to the same element).
  ///
  /// @param  other  The Element to copy.
  ///
  Element(const Element& other) : Node(other) {}

  ///
  /// Move constructor (`other` becomes empty).
  ///
  /// @param  other  The Element to move from.
  ///
  Element(Element&& other) noexcept : Node(std::move(other)) {}

  ///
  /// Assignment (copies or moves).
  ///
  /// @param  other  The Element to assign from.
  ///
  /// @return  Returns this Element.
  ///
  Element& operator=(Element other) noexcept {
    std::swap(detail_.handle, other.detail_.handle);
    return *this;
  }

  // Validity (operator bool, IsEmpty, IsAlive), IsSame(), destruction, and the node-level
  // surface (nodeType(), nodeValue, firstChild(), before(), remove(), ...) are inherited from
  // Node: an Element IS a Node, and the handle families share one object underneath.

  // --- Lookup -----------------------------------------------------------------------------

  ///
  /// Find the first element below this one that matches a CSS selector (querySelector).
  ///
  /// @param  selectors  One or more CSS selectors (eg, `.item > a[href]`).
  ///
  /// @return  Returns the first match in document order (empty if nothing matches, the selector
  ///          is malformed, or the page is gone).
  ///
  Element querySelector(std::string_view selectors) const {
    detail::CString s(selectors);
    return Element(ulDOMElementQuerySelector(raw(), s.c_str(), nullptr));
  }

  ///
  /// Same as querySelector(), but returns a Result with the reason for a failure (eg, a
  /// SyntaxError for a malformed selector).
  ///
  /// @return  Returns the first match in document order (empty if nothing matches). Fails with a
  ///          SyntaxError if the selector is malformed.
  ///
  [[nodiscard]] Result<Element> querySelector(std::string_view selectors, Checked_t) const {
    if (IsEmpty())
      return detail::EmptyHandleError();
    detail::CString s(selectors);
    detail::ErrorScope error;
    ULDOMElement result = ulDOMElementQuerySelector(raw(), s.c_str(), error.out());
    if (result)
      return Element(result);
    if (error.has_exception())
      return Unexpected<Error>(error.TakeError());
    if (IsAlive())
      return Element();  // No match on a live element: success with an empty handle.
    return Unexpected<Error>(Error::PageGone());
  }

  ///
  /// Find every element below this one that matches a CSS selector (querySelectorAll).
  ///
  /// This is defined in `<Ultralight/dom/ElementList.h>`. Include that header (or
  /// `<Ultralight/DOM.h>`) to call it.
  ///
  /// @param  selectors  One or more CSS selectors (eg, `.item > a[href]`).
  ///
  /// @return  Returns the matches in document order (an empty list if nothing matches, the
  ///          selector is malformed, or the page is gone).
  ///
  /// @note  The list doesn't change when the document does. Query again to see later changes.
  ///
  ElementList querySelectorAll(std::string_view selectors) const;

  ///
  /// Same as querySelectorAll(), but returns a Result with the reason for a failure (eg, a
  /// SyntaxError for a malformed selector).
  ///
  /// @return  Returns the matches in document order (an empty list if nothing matches). Fails with
  ///          a SyntaxError if the selector is malformed.
  ///
  [[nodiscard]] Result<ElementList> querySelectorAll(std::string_view selectors, Checked_t) const;

  ///
  /// Whether or not this element matches a CSS selector (matches).
  ///
  /// @param  selectors  One or more CSS selectors (eg, `.item > a[href]`).
  ///
  /// @return  Returns whether the element matches (false if the selector is malformed or the page
  ///          is gone).
  ///
  bool matches(std::string_view selectors) const {
    detail::CString s(selectors);
    return ulDOMElementMatches(raw(), s.c_str(), nullptr);
  }

  ///
  /// Same as matches(), but returns a Result with the reason for a failure (eg, a SyntaxError for
  /// a malformed selector).
  ///
  /// @return  Returns whether the element matches. Fails with a SyntaxError if the selector is
  ///          malformed.
  ///
  [[nodiscard]] Result<bool> matches(std::string_view selectors, Checked_t) const {
    if (IsEmpty())
      return detail::EmptyHandleError();
    detail::CString s(selectors);
    detail::ErrorScope error;
    bool result = ulDOMElementMatches(raw(), s.c_str(), error.out());
    if (error.has_exception())
      return Unexpected<Error>(error.TakeError());
    if (!result && !IsAlive())
      return Unexpected<Error>(Error::PageGone());
    return result;
  }

  ///
  /// Find the closest ancestor that matches a CSS selector, starting with this element itself
  /// (closest).
  ///
  /// @param  selectors  One or more CSS selectors (eg, `.item > a[href]`).
  ///
  /// @return  Returns the closest match (empty if nothing matches, the selector is malformed, or
  ///          the page is gone).
  ///
  Element closest(std::string_view selectors) const {
    detail::CString s(selectors);
    return Element(ulDOMElementClosest(raw(), s.c_str(), nullptr));
  }

  ///
  /// Same as closest(), but returns a Result with the reason for a failure (eg, a SyntaxError for
  /// a malformed selector).
  ///
  /// @return  Returns the closest match (empty if nothing matches). Fails with a SyntaxError if
  ///          the selector is malformed.
  ///
  [[nodiscard]] Result<Element> closest(std::string_view selectors, Checked_t) const {
    if (IsEmpty())
      return detail::EmptyHandleError();
    detail::CString s(selectors);
    detail::ErrorScope error;
    ULDOMElement result = ulDOMElementClosest(raw(), s.c_str(), error.out());
    if (result)
      return Element(result);
    if (error.has_exception())
      return Unexpected<Error>(error.TakeError());
    if (IsAlive())
      return Element();
    return Unexpected<Error>(Error::PageGone());
  }

  // --- Attributes -------------------------------------------------------------------------

  ///
  /// Get an attribute's value (getAttribute).
  ///
  /// @param  name  The attribute name (eg, `href`). HTML elements ignore its case.
  ///
  /// @return  Returns the value (nullopt if the element doesn't have the attribute). An attribute
  ///          with no value (eg, `<input disabled>`) returns an empty string.
  ///
  std::optional<std::string> getAttribute(std::string_view name) const {
    detail::CString n(name);
    ULString s = ulDOMElementGetAttribute(raw(), n.c_str());
    if (!s)
      return std::nullopt;
    return detail::TakeString(s);
  }

  ///
  /// Whether or not the element has an attribute (hasAttribute).
  ///
  /// @param  name  The attribute name (eg, `href`). HTML elements ignore its case.
  ///
  bool hasAttribute(std::string_view name) const {
    detail::CString n(name);
    return ulDOMElementHasAttribute(raw(), n.c_str());
  }

  ///
  /// Set an attribute's value (setAttribute).
  ///
  /// @param  name       The attribute name (eg, `href`). HTML elements store it in lowercase.
  ///
  /// @param  new_value  The value to set.
  ///
  /// @note  An invalid name (eg, one containing a space) makes this do nothing. The web throws an
  ///        InvalidCharacterError instead.
  ///
  void setAttribute(std::string_view name, std::string_view new_value) const {
    detail::CString n(name);
    detail::CString v(new_value);
    ulDOMElementSetAttribute(raw(), n.c_str(), v.c_str(), nullptr);
  }

  ///
  /// Remove an attribute (removeAttribute). Does nothing if the element doesn't have it.
  ///
  /// @param  name  The attribute name (eg, `href`).
  ///
  void removeAttribute(std::string_view name) const {
    detail::CString n(name);
    ulDOMElementRemoveAttribute(raw(), n.c_str());
  }

  ///
  /// Toggle an attribute (toggleAttribute). This removes the attribute if the element has it, or
  /// adds it with an empty value if not.
  ///
  /// @param  name  The attribute name (eg, `hidden`).
  ///
  /// @return  Returns whether the element has the attribute afterward (false for an invalid name).
  ///
  bool toggleAttribute(std::string_view name) const {
    detail::CString n(name);
    bool present = false;
    ulDOMElementToggleAttribute(raw(), n.c_str(), nullptr, &present, nullptr);
    return present;
  }

  ///
  /// Add or remove an attribute (toggleAttribute with `force`).
  ///
  /// @param  name   The attribute name (eg, `hidden`).
  ///
  /// @param  force  True to add the attribute (with an empty value if it's missing), false to
  ///                remove it.
  ///
  /// @return  Returns whether the element has the attribute afterward (false for an invalid name).
  ///
  bool toggleAttribute(std::string_view name, bool force) const {
    detail::CString n(name);
    bool present = false;
    ulDOMElementToggleAttribute(raw(), n.c_str(), &force, &present, nullptr);
    return present;
  }

  ///
  /// Get the names of the element's attributes in order (getAttributeNames).
  ///
  /// @return  Returns the names (empty if there are none).
  ///
  std::vector<std::string> getAttributeNames() const {
    std::vector<std::string> names;
    ULDOMStringList list = ulDOMElementGetAttributeNames(raw());
    if (!list)
      return names;
    size_t count = ulDOMStringListGetLength(list);
    names.reserve(count);
    for (size_t i = 0; i < count; i++)
      names.push_back(detail::TakeString(ulDOMStringListGetString(list, i)));
    ulDestroyDOMStringList(list);
    return names;
  }

  ///
  /// Whether or not another element is this element or one of its descendants (contains).
  ///
  /// This takes an element only (the web's `contains()` takes any node).
  ///
  /// @param  other  The element to look for.
  ///
  /// @return  Returns true if `other` is this element or inside it.
  ///
  bool contains(const Element& other) const {
    return ulDOMElementContains(raw(), other.raw());
  }

  ///
  /// Get the element's child elements in order (children).
  ///
  /// This is defined in `<Ultralight/dom/ElementList.h>`. Include that header (or
  /// `<Ultralight/DOM.h>`) to call it.
  ///
  /// @return  Returns the child elements (an empty list if there are none).
  ///
  /// @note  Unlike the web's live collection, the list doesn't change when the document does. Call
  ///        this again to see later changes.
  ///
  ElementList children() const;

  // --- Tree -------------------------------------------------------------------------------

  ///
  /// Add an element as the last child of this element (appendChild).
  ///
  /// If `child` is already in a document, it moves from its old position. An element from another
  /// document (eg, an iframe's) moves into this one.
  ///
  /// @param  child  The element to add.
  ///
  /// @return  Returns `child` (empty if nothing was added, eg, because `child` is this element or
  ///          one of its ancestors).
  ///
  /// @note  A handle belongs to the page it came from. If you move an element here from another
  ///        document, its handle still stops working when that document's page goes away.
  ///
  Element appendChild(const Element& child) const {
    return ulDOMElementAppendChild(raw(), child.raw(), nullptr) ? child : Element();
  }

  ///
  /// Same as appendChild(), but returns a Result with the reason for a failure (eg, a
  /// HierarchyRequestError for a cycle).
  ///
  /// @return  Returns `child`. Fails with a HierarchyRequestError if `child` is this element or one
  ///          of its ancestors.
  ///
  [[nodiscard]] Result<Element> appendChild(const Element& child, Checked_t) const {
    if (IsEmpty() || child.IsEmpty())
      return detail::EmptyHandleError();
    detail::ErrorScope error;
    if (ulDOMElementAppendChild(raw(), child.raw(), error.out()))
      return child;
    return Unexpected<Error>(error.TakeError());
  }

  ///
  /// Add a node of any kind as the last child of this element (appendChild).
  ///
  /// Use this for text and comment nodes (eg, from Document::createTextNode()). An Element
  /// argument goes to the overload that returns an Element, and a DocumentFragment argument to
  /// the overload that returns nothing.
  ///
  /// @param  child  The node to add. If it's already in a document, it moves.
  ///
  /// @return  Returns `child` (empty if nothing was added, eg, because `child` is this element or
  ///          one of its ancestors).
  ///
  Node appendChild(const Node& child) const {
    return ulDOMNodeAppendChild(Node::raw(), child.raw(), nullptr) ? child : Node();
  }

  ///
  /// Same as appendChild() with a Node, but returns a Result with the reason for a failure (eg,
  /// a HierarchyRequestError for a cycle).
  ///
  /// @return  Returns `child`. Fails with a HierarchyRequestError if `child` can't go there (eg,
  ///          it's this element or one of its ancestors).
  ///
  [[nodiscard]] Result<Node> appendChild(const Node& child, Checked_t) const {
    if (IsEmpty() || child.IsEmpty())
      return detail::EmptyHandleError();
    detail::ErrorScope error;
    if (ulDOMNodeAppendChild(Node::raw(), child.raw(), error.out()))
      return child;
    return Unexpected<Error>(error.TakeError());
  }

  ///
  /// Move all of a fragment's children to the end of this element (appendChild).
  ///
  /// The fragment is empty afterward. This returns nothing, where JavaScript returns the empty
  /// fragment.
  ///
  /// This is defined in `<Ultralight/dom/DocumentFragment.h>`. Include that header (or
  /// `<Ultralight/DOM.h>`) to call it.
  ///
  /// @param  fragment  The fragment whose children to move.
  ///
  void appendChild(const DocumentFragment& fragment) const;

  ///
  /// Same as appendChild() with a fragment, but returns a Result with the reason for a failure.
  ///
  /// @return  Returns success (it fails only when a handle is empty or its page is gone).
  ///
  [[nodiscard]] Result<void> appendChild(const DocumentFragment& fragment, Checked_t) const;

  ///
  /// Add nodes and strings to the end of this element, after its last child (append).
  ///
  /// Pass any mix of nodes (including elements and fragments) and strings. Each string is added
  /// as a new text node (it's never parsed as markup), and a fragment adds its children and is
  /// left empty:
  ///
  /// ```
  /// dom::Element item = doc.createElement("li");
  /// item.append(icon, " Score: ", std::to_string(score));
  /// ```
  ///
  /// @param  nodes  The nodes and strings to add, in order. A node that's already in a document
  ///                moves.
  ///
  /// @note  With more than one argument, the nodes leave their old positions before the insertion
  ///        is checked (like the web), so a call that fails can leave them out of the page.
  ///
  template <typename... T>
    requires((detail::NodeOrString<T> && ...))
  void append(T&&... nodes) const {
    auto items = detail::NodeOrStringItems(std::forward<T>(nodes)...);
    ulDOMElementAppend(raw(), items.data(), items.size(), nullptr);
  }

  ///
  /// Same as append(), but returns a Result with the reason for a failure (eg, a
  /// HierarchyRequestError for a cycle).
  ///
  /// dom::Checked comes first here, before the nodes (`item.append(dom::Checked, icon, "x")`).
  ///
  /// @return  Returns success. Fails with a HierarchyRequestError if one of the nodes can't go
  ///          there (eg, it's this element or one of its ancestors).
  ///
  template <typename... T>
    requires((detail::NodeOrString<T> && ...))
  [[nodiscard]] Result<void> append(Checked_t, T&&... nodes) const {
    auto items = detail::NodeOrStringItems(std::forward<T>(nodes)...);
    if (IsEmpty() || detail::HasEmptyItem(items))
      return detail::EmptyHandleError();
    detail::ErrorScope error;
    if (ulDOMElementAppend(raw(), items.data(), items.size(), error.out()))
      return {};
    return Unexpected<Error>(error.TakeError());
  }

  ///
  /// Add nodes and strings to the start of this element, before its first child (prepend).
  ///
  /// Works like append().
  ///
  /// @param  nodes  The nodes and strings to add, in order. A node that's already in a document
  ///                moves.
  ///
  /// @note  With more than one argument, the nodes leave their old positions before the insertion
  ///        is checked (like the web), so a call that fails can leave them out of the page.
  ///
  template <typename... T>
    requires((detail::NodeOrString<T> && ...))
  void prepend(T&&... nodes) const {
    auto items = detail::NodeOrStringItems(std::forward<T>(nodes)...);
    ulDOMElementPrepend(raw(), items.data(), items.size(), nullptr);
  }

  ///
  /// Same as prepend(), but returns a Result with the reason for a failure (eg, a
  /// HierarchyRequestError for a cycle).
  ///
  /// dom::Checked comes first here, before the nodes (`item.prepend(dom::Checked, icon, "x")`).
  ///
  /// @return  Returns success. Fails with a HierarchyRequestError if one of the nodes can't go
  ///          there (eg, it's this element or one of its ancestors).
  ///
  template <typename... T>
    requires((detail::NodeOrString<T> && ...))
  [[nodiscard]] Result<void> prepend(Checked_t, T&&... nodes) const {
    auto items = detail::NodeOrStringItems(std::forward<T>(nodes)...);
    if (IsEmpty() || detail::HasEmptyItem(items))
      return detail::EmptyHandleError();
    detail::ErrorScope error;
    if (ulDOMElementPrepend(raw(), items.data(), items.size(), error.out()))
      return {};
    return Unexpected<Error>(error.TakeError());
  }

  ///
  /// Replace all of this element's children with nodes and strings (replaceChildren).
  ///
  /// Pass any mix of nodes (including elements and fragments) and strings, like append(). With
  /// no arguments, this removes all the children.
  ///
  /// @param  new_children  The nodes and strings to add, in order. A node that's already in a
  ///                       document moves.
  ///
  /// @note  With more than one argument, the nodes leave their old positions before the insertion
  ///        is checked (like the web), so a call that fails can leave them out of the page.
  ///
  template <typename... T>
    requires((detail::NodeOrString<T> && ...))
  void replaceChildren(T&&... new_children) const {
    auto items = detail::NodeOrStringItems(std::forward<T>(new_children)...);
    ulDOMElementReplaceChildren(raw(), items.data(), items.size(), nullptr);
  }

  ///
  /// Same as replaceChildren(), but returns a Result with the reason for a failure (eg, a
  /// HierarchyRequestError for a cycle).
  ///
  /// dom::Checked comes first here, before the nodes:
  ///
  /// ```
  /// dom::Result<void> result = list.replaceChildren(dom::Checked, header, row);
  /// ```
  ///
  /// @return  Returns success. Fails with a HierarchyRequestError if one of the nodes can't go
  ///          there (eg, it's this element or one of its ancestors).
  ///
  template <typename... T>
    requires((detail::NodeOrString<T> && ...))
  [[nodiscard]] Result<void> replaceChildren(Checked_t, T&&... new_children) const {
    auto items = detail::NodeOrStringItems(std::forward<T>(new_children)...);
    if (IsEmpty() || detail::HasEmptyItem(items))
      return detail::EmptyHandleError();
    detail::ErrorScope error;
    if (ulDOMElementReplaceChildren(raw(), items.data(), items.size(), error.out()))
      return {};
    return Unexpected<Error>(error.TakeError());
  }

  ///
  /// Insert an element before one of this element's children (insertBefore).
  ///
  /// @param  child      The element to insert. If it's already in a document, it moves.
  ///
  /// @param  ref_child  The child to insert before. Pass an empty Element to add `child` at the
  ///                    end.
  ///
  /// @return  Returns `child` (empty if nothing was inserted, eg, because `ref_child` isn't a
  ///          child of this element).
  ///
  Element insertBefore(const Element& child, const Element& ref_child) const {
    return ulDOMElementInsertBefore(raw(), child.raw(), ref_child.raw(), nullptr) ? child
                                                                                 : Element();
  }

  ///
  /// Same as insertBefore(), but returns a Result with the reason for a failure (eg, a
  /// NotFoundError when `ref_child` isn't a child of this element).
  ///
  /// @return  Returns `child`. Fails with a NotFoundError if `ref_child` isn't a child of this
  ///          element, or a HierarchyRequestError if `child` is this element or one of its
  ///          ancestors.
  ///
  [[nodiscard]] Result<Element> insertBefore(const Element& child, const Element& ref_child,
                                             Checked_t) const {
    if (IsEmpty() || child.IsEmpty())
      return detail::EmptyHandleError();
    detail::ErrorScope error;
    if (ulDOMElementInsertBefore(raw(), child.raw(),
                                 ref_child.raw(), error.out()))
      return child;
    return Unexpected<Error>(error.TakeError());
  }

  ///
  /// Insert a node of any kind before one of this element's children (insertBefore).
  ///
  /// Use this for text and comment nodes, or a reference child that isn't an element. With two
  /// Element arguments, the overload that returns an Element is used.
  ///
  /// @param  child      The node to insert. If it's already in a document, it moves.
  ///
  /// @param  ref_child  The child to insert before. Pass an empty Node to add `child` at the end.
  ///
  /// @return  Returns `child` (empty if nothing was inserted, eg, because `ref_child` isn't a
  ///          child of this element).
  ///
  Node insertBefore(const Node& child, const Node& ref_child) const {
    return ulDOMNodeInsertBefore(Node::raw(), child.raw(), ref_child.raw(), nullptr) ? child
                                                                                    : Node();
  }

  ///
  /// Same as insertBefore() with Nodes, but returns a Result with the reason for a failure (eg,
  /// a NotFoundError when `ref_child` isn't a child of this element).
  ///
  /// @return  Returns `child`. Fails with a NotFoundError if `ref_child` isn't a child of this
  ///          element, or a HierarchyRequestError if `child` can't go there (eg, it's this
  ///          element or one of its ancestors).
  ///
  [[nodiscard]] Result<Node> insertBefore(const Node& child, const Node& ref_child,
                                          Checked_t) const {
    if (IsEmpty() || child.IsEmpty())
      return detail::EmptyHandleError();
    detail::ErrorScope error;
    if (ulDOMNodeInsertBefore(Node::raw(), child.raw(), ref_child.raw(), error.out()))
      return child;
    return Unexpected<Error>(error.TakeError());
  }

  ///
  /// Replace one of this element's children with another element (replaceChild).
  ///
  /// @param  new_child  The element to put in its place (it comes first, as on the web). If it's
  ///                    already in a document, it moves.
  ///
  /// @param  old_child  The child to replace.
  ///
  /// @return  Returns `old_child`, now removed from the document (empty if nothing was replaced,
  ///          eg, because `old_child` isn't a child of this element).
  ///
  Element replaceChild(const Element& new_child, const Element& old_child) const {
    return ulDOMElementReplaceChild(raw(), new_child.raw(), old_child.raw(), nullptr)
               ? old_child
               : Element();
  }

  ///
  /// Same as replaceChild(), but returns a Result with the reason for a failure (eg, a
  /// NotFoundError when `old_child` isn't a child of this element).
  ///
  /// @return  Returns `old_child`, now removed from the document. Fails with a NotFoundError if
  ///          `old_child` isn't a child of this element, or a HierarchyRequestError if `new_child`
  ///          is this element or one of its ancestors.
  ///
  [[nodiscard]] Result<Element> replaceChild(const Element& new_child, const Element& old_child,
                                             Checked_t) const {
    if (IsEmpty() || new_child.IsEmpty() || old_child.IsEmpty())
      return detail::EmptyHandleError();
    detail::ErrorScope error;
    if (ulDOMElementReplaceChild(raw(), new_child.raw(),
                                 old_child.raw(), error.out()))
      return old_child;
    return Unexpected<Error>(error.TakeError());
  }

  ///
  /// Replace one of this element's children, of any kind, with a node of any kind
  /// (replaceChild).
  ///
  /// With two Element arguments, the overload that returns an Element is used.
  ///
  /// @param  new_child  The node to put in its place (it comes first, as on the web). If it's
  ///                    already in a document, it moves.
  ///
  /// @param  old_child  The child to replace.
  ///
  /// @return  Returns `old_child`, now removed from the document (empty if nothing was replaced,
  ///          eg, because `old_child` isn't a child of this element).
  ///
  Node replaceChild(const Node& new_child, const Node& old_child) const {
    return ulDOMNodeReplaceChild(Node::raw(), new_child.raw(), old_child.raw(), nullptr)
               ? old_child
               : Node();
  }

  ///
  /// Same as replaceChild() with Nodes, but returns a Result with the reason for a failure (eg,
  /// a NotFoundError when `old_child` isn't a child of this element).
  ///
  /// @return  Returns `old_child`, now removed from the document. Fails with a NotFoundError if
  ///          `old_child` isn't a child of this element, or a HierarchyRequestError if
  ///          `new_child` can't go there (eg, it's this element or one of its ancestors).
  ///
  [[nodiscard]] Result<Node> replaceChild(const Node& new_child, const Node& old_child,
                                          Checked_t) const {
    if (IsEmpty() || new_child.IsEmpty() || old_child.IsEmpty())
      return detail::EmptyHandleError();
    detail::ErrorScope error;
    if (ulDOMNodeReplaceChild(Node::raw(), new_child.raw(), old_child.raw(), error.out()))
      return old_child;
    return Unexpected<Error>(error.TakeError());
  }

  ///
  /// Remove one of this element's children (removeChild).
  ///
  /// @param  child  The child to remove.
  ///
  /// @return  Returns `child`, now removed from the document (empty if `child` isn't a child of
  ///          this element).
  ///
  Element removeChild(const Element& child) const {
    return ulDOMElementRemoveChild(raw(), child.raw(), nullptr) ? child : Element();
  }

  ///
  /// Same as removeChild(), but returns a Result with the reason for a failure (eg, a
  /// NotFoundError when `child` isn't a child of this element).
  ///
  /// @return  Returns `child`, now removed from the document. Fails with a NotFoundError if `child`
  ///          isn't a child of this element.
  ///
  [[nodiscard]] Result<Element> removeChild(const Element& child, Checked_t) const {
    if (IsEmpty() || child.IsEmpty())
      return detail::EmptyHandleError();
    detail::ErrorScope error;
    if (ulDOMElementRemoveChild(raw(), child.raw(), error.out()))
      return child;
    return Unexpected<Error>(error.TakeError());
  }

  ///
  /// Remove one of this element's children, of any kind (removeChild).
  ///
  /// With an Element argument, the overload that returns an Element is used.
  ///
  /// @param  child  The child to remove.
  ///
  /// @return  Returns `child`, now removed from the document (empty if `child` isn't a child of
  ///          this element).
  ///
  Node removeChild(const Node& child) const {
    return ulDOMNodeRemoveChild(Node::raw(), child.raw(), nullptr) ? child : Node();
  }

  ///
  /// Same as removeChild() with a Node, but returns a Result with the reason for a failure (eg,
  /// a NotFoundError when `child` isn't a child of this element).
  ///
  /// @return  Returns `child`, now removed from the document. Fails with a NotFoundError if `child`
  ///          isn't a child of this element.
  ///
  [[nodiscard]] Result<Node> removeChild(const Node& child, Checked_t) const {
    if (IsEmpty() || child.IsEmpty())
      return detail::EmptyHandleError();
    detail::ErrorScope error;
    if (ulDOMNodeRemoveChild(Node::raw(), child.raw(), error.out()))
      return child;
    return Unexpected<Error>(error.TakeError());
  }

  ///
  /// Copy this element (cloneNode). The copy has the same attributes and isn't in the document.
  ///
  /// @param  deep  True to also copy the element's descendants.
  ///
  /// @return  Returns the copy.
  ///
  Element cloneNode(bool deep = false) const {
    return Element(ulDOMElementCloneNode(raw(), deep));
  }

  ///
  /// Parse markup and insert it relative to this element (insertAdjacentHTML).
  ///
  /// @param  position  Where to insert. `beforebegin` and `afterend` insert before and after this
  ///                   element, and `afterbegin` and `beforeend` insert before its first child
  ///                   and after its last child. Case doesn't matter.
  ///
  /// @param  html      The markup to insert. Scripts in it never run.
  ///
  /// @note  Nothing is inserted for an unknown `position`, or for `beforebegin` or `afterend` when
  ///        the element has no parent or is the `<html>` root.
  ///
  void insertAdjacentHTML(std::string_view position, std::string_view html) const {
    detail::CString p(position);
    ulDOMElementInsertAdjacentHTML(raw(), p.c_str(), html.data() ? html.data() : "", html.size(),
                                   nullptr);
  }

  ///
  /// Same as insertAdjacentHTML(), but returns a Result with the reason for a failure (eg, a
  /// SyntaxError for an unknown `position`).
  ///
  /// @return  Returns success. Fails with a SyntaxError for an unknown `position`, or a
  ///          NoModificationAllowedError for `beforebegin` or `afterend` when the element has no
  ///          parent or is the `<html>` root.
  ///
  [[nodiscard]] Result<void> insertAdjacentHTML(std::string_view position, std::string_view html,
                                                Checked_t) const {
    if (IsEmpty())
      return detail::EmptyHandleError();
    detail::CString p(position);
    detail::ErrorScope error;
    if (ulDOMElementInsertAdjacentHTML(raw(), p.c_str(),
                                       html.data() ? html.data() : "", html.size(),
                                       error.out()))
      return {};
    return Unexpected<Error>(error.TakeError());
  }

  ///
  /// Insert an element relative to this element (insertAdjacentElement).
  ///
  /// @param  position  Where to insert. `beforebegin` and `afterend` insert before and after this
  ///                   element, and `afterbegin` and `beforeend` insert before its first child
  ///                   and after its last child. Case doesn't matter.
  ///
  /// @param  other     The element to insert. If it's already in a document, it moves.
  ///
  /// @return  Returns `other` (empty if nothing was inserted, eg, for an unknown `position`, or for
  ///          `beforebegin` or `afterend` when this element has no parent).
  ///
  Element insertAdjacentElement(std::string_view position, const Element& other) const {
    detail::CString p(position);
    return Element(ulDOMElementInsertAdjacentElement(raw(), p.c_str(), other.raw(), nullptr));
  }

  ///
  /// Same as insertAdjacentElement(), but returns a Result with the reason for a failure (eg, a
  /// SyntaxError for an unknown `position`).
  ///
  /// @return  Returns `other`, or an empty Element if `position` is `beforebegin` or `afterend`
  ///          and this element has no parent (nothing is inserted, like the web). Fails with a
  ///          SyntaxError for an unknown `position`, or a HierarchyRequestError if the insertion
  ///          isn't allowed (eg, `other` is an ancestor of this element).
  ///
  [[nodiscard]] Result<Element> insertAdjacentElement(std::string_view position,
                                                      const Element& other, Checked_t) const {
    if (IsEmpty() || other.IsEmpty())
      return detail::EmptyHandleError();
    detail::CString p(position);
    detail::ErrorScope error;
    ULDOMElement result = ulDOMElementInsertAdjacentElement(
        raw(), p.c_str(), other.raw(), error.out());
    if (result)
      return Element(result);
    if (error.has_exception())
      return Unexpected<Error>(error.TakeError());
    if (IsAlive() && other.IsAlive())
      return Element();  // The spec's parentless silent no-op.
    return Unexpected<Error>(Error::PageGone());
  }

  ///
  /// Insert text relative to this element (insertAdjacentText).
  ///
  /// The text is never parsed as markup, so this is safe for untrusted text.
  ///
  /// @param  position  Where to insert. `beforebegin` and `afterend` insert before and after this
  ///                   element, and `afterbegin` and `beforeend` insert before its first child
  ///                   and after its last child. Case doesn't matter.
  ///
  /// @param  text      The text to insert.
  ///
  /// @note  Nothing is inserted for an unknown `position`, or for `beforebegin` or `afterend` when
  ///        this element has no parent or is the `<html>` root.
  ///
  void insertAdjacentText(std::string_view position, std::string_view text) const {
    detail::CString p(position);
    ulDOMElementInsertAdjacentText(raw(), p.c_str(), text.data() ? text.data() : "", text.size(),
                                   nullptr);
  }

  ///
  /// Same as insertAdjacentText(), but returns a Result with the reason for a failure (eg, a
  /// SyntaxError for an unknown `position`).
  ///
  /// @return  Returns success (nothing is inserted for `beforebegin` or `afterend` when this
  ///          element has no parent, like the web). Fails with a SyntaxError for an unknown
  ///          `position`, or a HierarchyRequestError for `beforebegin` or `afterend` on the
  ///          `<html>` root.
  ///
  [[nodiscard]] Result<void> insertAdjacentText(std::string_view position, std::string_view text,
                                                Checked_t) const {
    if (IsEmpty())
      return detail::EmptyHandleError();
    detail::CString p(position);
    detail::ErrorScope error;
    if (ulDOMElementInsertAdjacentText(raw(), p.c_str(),
                                       text.data() ? text.data() : "", text.size(),
                                       error.out()))
      return {};
    return Unexpected<Error>(error.TakeError());
  }

  // --- Traversal (read-only properties are calls; see the class doc) -----------------------

  ///
  /// Get the element's tag name (tagName).
  ///
  /// @return  Returns the tag name (uppercase for HTML elements, eg, `DIV`).
  ///
  std::string tagName() const {
    return detail::TakeString(ulDOMElementGetTagName(raw()));
  }

  ///
  /// Get the element's parent element (parentElement).
  ///
  /// @return  Returns the parent element (empty if the parent isn't an element).
  ///
  Element parentElement() const { return Element(ulDOMElementGetParentElement(raw())); }

  ///
  /// Get the element's first child element (firstElementChild).
  ///
  /// @return  Returns the first child element (empty if there's none).
  ///
  Element firstElementChild() const {
    return Element(ulDOMElementGetFirstElementChild(raw()));
  }

  ///
  /// Get the element's last child element (lastElementChild).
  ///
  /// @return  Returns the last child element (empty if there's none).
  ///
  Element lastElementChild() const {
    return Element(ulDOMElementGetLastElementChild(raw()));
  }

  ///
  /// Get the element's next sibling element (nextElementSibling).
  ///
  /// @return  Returns the next sibling element (empty if there's none).
  ///
  Element nextElementSibling() const {
    return Element(ulDOMElementGetNextElementSibling(raw()));
  }

  ///
  /// Get the element's previous sibling element (previousElementSibling).
  ///
  /// @return  Returns the previous sibling element (empty if there's none).
  ///
  Element previousElementSibling() const {
    return Element(ulDOMElementGetPreviousElementSibling(raw()));
  }

  ///
  /// Get the number of the element's child elements (childElementCount).
  ///
  /// @return  Returns the number of child elements.
  ///
  size_t childElementCount() const { return ulDOMElementGetChildElementCount(raw()); }

  ///
  /// Get the document this element belongs to (ownerDocument).
  ///
  /// This is defined in `<Ultralight/dom/Document.h>`. Include that header (or
  /// `<Ultralight/DOM.h>`) to call it.
  ///
  /// @return  Returns the document of the page this handle came from.
  ///
  Document ownerDocument() const;

  // --- Geometry and scrolling ---------------------------------------------------------------

  ///
  /// Get the element's bounding rectangle relative to the viewport (getBoundingClientRect).
  ///
  /// @return  Returns the rectangle in CSS pixels (all zeros if the element isn't displayed). For
  ///          an element in an iframe, it's relative to the iframe's viewport.
  ///
  /// @note  This updates the layout first if the page has pending changes (so do the offset,
  ///        client, and scroll size reads). Make all your changes first, then read.
  ///
  DOMRect getBoundingClientRect() const {
    ULDOMRect r = ulDOMElementGetBoundingClientRect(raw());
    return DOMRect { r.x, r.y, r.width, r.height };
  }

  ///
  /// Get the element's width in CSS pixels, including its padding and borders (offsetWidth).
  ///
  /// @return  Returns the width in whole pixels (0 if the element isn't displayed).
  ///
  int offsetWidth() const { return ulDOMElementGetOffsetWidth(raw()); }

  ///
  /// Get the element's height in CSS pixels, including its padding and borders (offsetHeight).
  ///
  /// @return  Returns the height in whole pixels (0 if the element isn't displayed).
  ///
  int offsetHeight() const { return ulDOMElementGetOffsetHeight(raw()); }

  ///
  /// Get the distance from the element's top border edge to its offsetParent()'s top padding
  /// edge, in CSS pixels (offsetTop).
  ///
  /// @return  Returns the distance in whole pixels (0 if the element isn't displayed).
  ///
  int offsetTop() const { return ulDOMElementGetOffsetTop(raw()); }

  ///
  /// Get the distance from the element's left border edge to its offsetParent()'s left padding
  /// edge, in CSS pixels (offsetLeft).
  ///
  /// @return  Returns the distance in whole pixels (0 if the element isn't displayed).
  ///
  int offsetLeft() const { return ulDOMElementGetOffsetLeft(raw()); }

  ///
  /// Get the element that offsetTop() and offsetLeft() measure from (offsetParent).
  ///
  /// This is the nearest positioned ancestor (or a table cell or table around the element), or
  /// the `<body>` if there's none.
  ///
  /// @return  Returns the offset parent (empty if the element isn't displayed, has fixed
  ///          positioning, or is the `<html>` or `<body>` element).
  ///
  Element offsetParent() const { return Element(ulDOMElementGetOffsetParent(raw())); }

  ///
  /// Get the element's inner width in CSS pixels, including its padding but not its borders or
  /// scrollbar (clientWidth).
  ///
  /// @return  Returns the width in whole pixels (0 if the element isn't displayed or is inline).
  ///
  int clientWidth() const { return ulDOMElementGetClientWidth(raw()); }

  ///
  /// Get the element's inner height in CSS pixels, including its padding but not its borders or
  /// scrollbar (clientHeight).
  ///
  /// @return  Returns the height in whole pixels (0 if the element isn't displayed or is inline).
  ///
  int clientHeight() const { return ulDOMElementGetClientHeight(raw()); }

  ///
  /// Get the width of the element's top border, in CSS pixels (clientTop).
  ///
  /// @return  Returns the width in whole pixels (0 if the element isn't displayed or is inline).
  ///
  int clientTop() const { return ulDOMElementGetClientTop(raw()); }

  ///
  /// Get the width of the element's left border, in CSS pixels (clientLeft).
  ///
  /// @return  Returns the width in whole pixels (0 if the element isn't displayed or is inline).
  ///
  int clientLeft() const { return ulDOMElementGetClientLeft(raw()); }

  ///
  /// Get the width of the element's content in CSS pixels, including any part scrolled out of
  /// view (scrollWidth).
  ///
  /// @return  Returns the width in whole pixels.
  ///
  int scrollWidth() const { return ulDOMElementGetScrollWidth(raw()); }

  ///
  /// Get the height of the element's content in CSS pixels, including any part scrolled out of
  /// view (scrollHeight).
  ///
  /// @return  Returns the height in whole pixels.
  ///
  int scrollHeight() const { return ulDOMElementGetScrollHeight(raw()); }

  ///
  /// Scroll the element's ancestors so the element is visible (scrollIntoView).
  ///
  /// Scrolling is instant (CSS `scroll-behavior` is ignored). This does nothing if the element
  /// isn't displayed.
  ///
  /// @param  align_to_top  True to line up the element's top with the top of the visible area,
  ///                       false to line up the bottoms.
  ///
  void scrollIntoView(bool align_to_top = true) const {
    ulDOMElementScrollIntoView(raw(), align_to_top);
  }

  ///
  /// Scroll the element's content to a position (scrollTo).
  ///
  /// The content scrolls at once (CSS `scroll-behavior` is ignored), and the position stays
  /// within the scrollable area. This does nothing if the element isn't displayed or has nothing
  /// to scroll.
  ///
  /// @param  x  The horizontal position in CSS pixels (any fraction is dropped). A value that
  ///            isn't finite (eg, NaN) counts as 0.
  ///
  /// @param  y  The vertical position in CSS pixels (any fraction is dropped). A value that isn't
  ///            finite counts as 0.
  ///
  void scrollTo(double x, double y) const { ulDOMElementScrollTo(raw(), x, y); }

  ///
  /// Scroll the element's content by an offset (scrollBy).
  ///
  /// The content scrolls the same way as with scrollTo().
  ///
  /// @param  x  The horizontal offset in CSS pixels. A value that isn't finite counts as 0.
  ///
  /// @param  y  The vertical offset in CSS pixels. A value that isn't finite counts as 0.
  ///
  void scrollBy(double x, double y) const { ulDOMElementScrollBy(raw(), x, y); }

  // --- Focus and activation -----------------------------------------------------------------

  ///
  /// Give the element keyboard focus (focus).
  ///
  /// This scrolls the element into view, like the web. It does nothing if the element can't take
  /// focus or isn't in the document.
  ///
  void focus() const { ulDOMElementFocus(raw()); }

  ///
  /// Remove keyboard focus from the element (blur). Does nothing if it doesn't have focus.
  ///
  void blur() const { ulDOMElementBlur(raw()); }

  ///
  /// Click the element (click).
  ///
  /// This fires one `click` event and runs the element's click behavior. For example, a checkbox
  /// toggles and fires `input` and `change`, a button activates, and a link navigates. There's no
  /// mouse movement and no `mousedown` or `mouseup`, like the web. The event's `isTrusted` is
  /// false.
  ///
  /// @note  This does nothing on a disabled form control or a non-HTML element (eg, SVG). Use
  ///        View::FireMouseEvent() to click like a real user.
  ///
  void click() const { ulDOMElementClick(raw()); }

  // --- Events -------------------------------------------------------------------------------

  ///
  /// Dispatch a synthetic event to this element (dispatchEvent).
  ///
  /// Every listener (native and page JavaScript) runs before this returns. The event's `isTrusted`
  /// is false.
  ///
  /// A default action that depends only on the event type still runs, so a synthetic `click`
  /// follows a link. Other default actions don't run. A synthetic `click` doesn't toggle a
  /// checkbox, and a synthetic `submit` doesn't submit its form (use click() or
  /// HTMLFormElement::requestSubmit() for those).
  ///
  /// @param  type  The event type (eg, `click`). An empty type dispatches nothing.
  ///
  /// @param  init  The event's bubbles, cancelable, and composed flags (see EventInit). They're all
  ///               false by default.
  ///
  /// @return  Returns false if a listener canceled the event with Event::preventDefault() (which
  ///          needs `init.cancelable`), or true otherwise.
  ///
  bool dispatchEvent(std::string_view type, const EventInit& init = {}) const {
    detail::CString t(type);
    return ulDOMElementDispatchEvent(raw(), t.c_str(),
                                     detail::EventInitFlags(init), nullptr);
  }

  ///
  /// Dispatch a synthetic CustomEvent with a string payload to this element.
  ///
  /// This works like dispatchEvent(). Listeners read the payload as the event's `detail`. Page
  /// JavaScript gets it as a string, and a native listener gets it from Event::AsCustom() (this
  /// works with JavaScript disabled too). To send structured data, pass JSON and parse it in the
  /// listener.
  ///
  /// @param  type          The event type (eg, `game-saved`). An empty type dispatches nothing.
  ///
  /// @param  event_detail  The payload as UTF-8 text. It ends at the first null character, and
  ///                       text that isn't valid UTF-8 arrives as no payload.
  ///
  /// @param  init          The event's bubbles, cancelable, and composed flags (see EventInit).
  ///                       They're all false by default.
  ///
  /// @return  Returns false if a listener canceled the event with Event::preventDefault() (which
  ///          needs `init.cancelable`), or true otherwise.
  ///
  /// @see  js::API::Emit()
  ///
  bool dispatchCustomEvent(std::string_view type, std::string_view event_detail,
                           const EventInit& init = {}) const {
    detail::CString t(type);
    detail::CString d(event_detail);
    return ulDOMElementDispatchCustomEvent(raw(), t.c_str(),
                                           detail::EventInitFlags(init), d.c_str(),
                                           nullptr);
  }

  ///
  /// Listen for an event on this element (addEventListener).
  ///
  /// The callback can take the event or nothing:
  ///
  /// ```
  /// button.addEventListener("click", [](dom::Event event) { Log(event.type()); });
  /// button.addEventListener("mouseenter", [] { Highlight(); });
  /// button.addEventListener("keydown", OnKey, { .capture = true, .signal = controller.signal() });
  /// ```
  ///
  /// The callback runs on the Renderer's thread while the event is dispatched (eg, inside
  /// View::FireMouseEvent() or dispatchEvent()). It can change the DOM and add or remove
  /// listeners, its own included.
  ///
  /// @param  type      The event type to listen for (eg, `click`).
  ///
  /// @param  callback  The callable to run on each event, taking (dom::Event) or ().
  ///
  /// @param  options   The listener's options (see AddEventListenerOptions).
  ///
  /// @return  Returns a handle for removing the listener, which you can ignore. It's empty when
  ///          nothing was added (the page is gone or `options.signal` is already aborted).
  ///
  /// @see  EventListener, AbortController
  ///
  template <typename F>
  EventListener addEventListener(std::string_view type, F&& callback,
                                 const AddEventListenerOptions& options = {}) const {
    using Fn = std::decay_t<F>;
    static_assert(detail::InvocableWithEvent<Fn> || std::is_invocable_v<Fn&>,
                  "addEventListener takes a callable invocable with (dom::Event) or ()");
    detail::CString t(type);
    ULDOMEventCallback thunk = &detail::EventThunk<Fn>;
    ULUserDataDestroyCallback destroy = &detail::DeleteCallable<Fn>;
    return detail::AddListener<Fn>(std::forward<F>(callback), options,
                                   [&](unsigned flags, void* fn) {
                                     return ulDOMElementAddEventListener(raw(), t.c_str(), flags,
                                                                         thunk, fn, destroy);
                                   });
  }

  ///
  /// Listen for an event on this element, with options as flags (eg, `dom::Once | dom::Capture`).
  ///
  /// @param  type      The event type to listen for.
  ///
  /// @param  callback  The callable to run on each event, taking (dom::Event) or ().
  ///
  /// @param  flags     The listener's options (see EventListenerFlags).
  ///
  /// @return  Returns a handle for removing the listener, which you can ignore (empty when the
  ///          page is gone).
  ///
  template <typename F>
  EventListener addEventListener(std::string_view type, F&& callback,
                                 EventListenerFlags flags) const {
    return addEventListener(type, std::forward<F>(callback), detail::ToOptions(flags));
  }

  ///
  /// Listen by calling a member function on an object you keep alive.
  ///
  /// ```
  /// button.addEventListener("click", *this, &Hud::OnSave, { .signal = listeners_.signal() });
  /// ```
  ///
  /// @param  type      The event type to listen for.
  ///
  /// @param  receiver  The object to call `method` on.
  ///
  /// @param  method    The member function to call, taking (dom::Event) or ().
  ///
  /// @param  options   The listener's options (see AddEventListenerOptions).
  ///
  /// @return  Returns a handle for removing the listener, which you can ignore. It's empty when
  ///          nothing was added (the page is gone or `options.signal` is already aborted).
  ///
  /// @warning  The listener calls `receiver` for as long as it lives, so `receiver` must outlive
  ///           the page, or the listener must be added with the signal of an AbortController
  ///           that `receiver` owns (as above). The smart-pointer overload with a std::weak_ptr
  ///           skips calls once the object is gone instead.
  ///
  template <typename C, typename M>
    requires(std::is_class_v<C> && std::is_member_function_pointer_v<M>
             && !LockableHolder<C>)
  EventListener addEventListener(std::string_view type, C& receiver, M method,
                                 const AddEventListenerOptions& options = {}) const {
    return addEventListener(type, detail::WrapBorrowedMember(receiver, method), options);
  }

  ///
  /// Listen by calling a member function on an object you keep alive, with options as flags.
  ///
  /// @param  type      The event type to listen for.
  ///
  /// @param  receiver  The object to call `method` on.
  ///
  /// @param  method    The member function to call, taking (dom::Event) or ().
  ///
  /// @param  flags     The listener's options (see EventListenerFlags).
  ///
  /// @return  Returns a handle for removing the listener, which you can ignore (empty when the
  ///          page is gone).
  ///
  /// @warning  The listener calls `receiver` for as long as it lives, so `receiver` must outlive
  ///           the page. To tie the listener to `receiver` instead, use the options overload
  ///           with the signal of an AbortController that `receiver` owns.
  ///
  template <typename C, typename M>
    requires(std::is_class_v<C> && std::is_member_function_pointer_v<M>
             && !LockableHolder<C>)
  EventListener addEventListener(std::string_view type, C& receiver, M method,
                                 EventListenerFlags flags) const {
    return addEventListener(type, detail::WrapBorrowedMember(receiver, method), flags);
  }

  ///
  /// Listen by calling a member function through a smart pointer that's checked before each call.
  ///
  /// A std::shared_ptr keeps the object alive for as long as the listener exists. A std::weak_ptr
  /// doesn't, and events are skipped once the object is gone.
  ///
  /// @param  type     The event type to listen for.
  ///
  /// @param  holder   A std::shared_ptr, a std::weak_ptr, or another smart pointer with a
  ///                  HolderTraits specialization (see LockableHolder).
  ///
  /// @param  method   The member function to call, taking (dom::Event) or ().
  ///
  /// @param  options  The listener's options (see AddEventListenerOptions).
  ///
  /// @return  Returns a handle for removing the listener, which you can ignore. It's empty when
  ///          nothing was added (the page is gone or `options.signal` is already aborted).
  ///
  template <typename H, typename M>
    requires(LockableHolder<H> && std::is_member_function_pointer_v<M>)
  EventListener addEventListener(std::string_view type, H holder, M method,
                                 const AddEventListenerOptions& options = {}) const {
    return addEventListener(type, detail::WrapHolderMember(std::move(holder), method), options);
  }

  ///
  /// Listen by calling a member function through a smart pointer, with options as flags.
  ///
  /// @param  type    The event type to listen for.
  ///
  /// @param  holder  A std::shared_ptr, a std::weak_ptr, or another smart pointer with a
  ///                 HolderTraits specialization (see LockableHolder).
  ///
  /// @param  method  The member function to call, taking (dom::Event) or ().
  ///
  /// @param  flags   The listener's options (see EventListenerFlags).
  ///
  /// @return  Returns a handle for removing the listener, which you can ignore (empty when the
  ///          page is gone).
  ///
  template <typename H, typename M>
    requires(LockableHolder<H> && std::is_member_function_pointer_v<M>)
  EventListener addEventListener(std::string_view type, H holder, M method,
                                 EventListenerFlags flags) const {
    return addEventListener(type, detail::WrapHolderMember(std::move(holder), method), flags);
  }

  ///
  /// Listen for an event on the elements inside this one that match a CSS selector, including
  /// elements added later (event delegation).
  ///
  /// One listener on this element covers the whole subtree. When an event reaches this element,
  /// the library checks the event's target and its ancestors (up to and including this element)
  /// against `selector`. The callback runs with the closest match, and doesn't run if nothing
  /// matches:
  ///
  /// ```
  /// table.On("tr.item", "click", [](dom::Event event, dom::Element row) { Select(row); });
  /// ```
  ///
  /// Events that don't bubble (eg, `focus`) reach this element only in the capture phase, so
  /// pass `{ .capture = true }` (or dom::Capture) for them.
  ///
  /// @param  selector  A CSS selector (eg, `tr.item`). A malformed selector adds nothing and
  ///                   logs a warning.
  ///
  /// @param  type      The event type to listen for.
  ///
  /// @param  callback  The callable to run for each match, taking (dom::Event, dom::Element),
  ///                   (dom::Element), or ().
  ///
  /// @param  options   The listener's options (see AddEventListenerOptions).
  ///
  /// @return  Returns a handle for removing the listener, which you can ignore. It's empty when
  ///          nothing was added (the page is gone or `options.signal` is already aborted).
  ///
  /// @note  With `once`, the listener is removed after the first event that reaches this
  ///        element, even if nothing matched.
  ///
  template <typename F>
  EventListener On(std::string_view selector, std::string_view type, F&& callback,
                   const AddEventListenerOptions& options = {}) const {
    using Fn = std::decay_t<F>;
    static_assert(detail::InvocableWithEvent<Fn, Element> || std::is_invocable_v<Fn&, Element>
                      || std::is_invocable_v<Fn&>,
                  "On takes a callable invocable with (dom::Event, dom::Element), "
                  "(dom::Element), or ()");
    detail::CString sel(selector);
    detail::CString t(type);
    ULDOMDelegatedEventCallback thunk = &detail::DelegatedThunk<Fn>;
    ULUserDataDestroyCallback destroy = &detail::DeleteCallable<Fn>;
    return detail::AddListener<Fn>(std::forward<F>(callback), options,
                                   [&](unsigned flags, void* fn) {
                                     return ulDOMElementAddDelegatedEventListener(
                                         raw(), sel.c_str(), t.c_str(), flags, thunk, fn,
                                         destroy);
                                   });
  }

  ///
  /// Listen for an event on matching elements, with options as flags (see the options overload).
  ///
  /// @param  selector  A CSS selector (eg, `tr.item`).
  ///
  /// @param  type      The event type to listen for.
  ///
  /// @param  callback  The callable to run for each match, taking (dom::Event, dom::Element),
  ///                   (dom::Element), or ().
  ///
  /// @param  flags     The listener's options (see EventListenerFlags).
  ///
  /// @return  Returns a handle for removing the listener, which you can ignore (empty when the
  ///          page is gone).
  ///
  template <typename F>
  EventListener On(std::string_view selector, std::string_view type, F&& callback,
                   EventListenerFlags flags) const {
    return On(selector, type, std::forward<F>(callback), detail::ToOptions(flags));
  }

  ///
  /// Same as On(), but returns a Result with the reason for a failure (eg, a SyntaxError for a
  /// malformed selector).
  ///
  /// @param  selector  A CSS selector (eg, `tr.item`).
  ///
  /// @param  type      The event type to listen for.
  ///
  /// @param  callback  The callable to run for each match, taking (dom::Event, dom::Element),
  ///                   (dom::Element), or ().
  ///
  /// @param  options   The listener's options (see AddEventListenerOptions).
  ///
  /// @return  Returns a handle for removing the listener (empty when `options.signal` is
  ///          already aborted). Fails with a SyntaxError if `selector` is malformed.
  ///
  template <typename F>
  [[nodiscard]] Result<EventListener> On(std::string_view selector, std::string_view type,
                                         F&& callback, const AddEventListenerOptions& options,
                                         Checked_t) const {
    if (IsEmpty())
      return detail::EmptyHandleError();
    return detail::CheckedDelegation(On(selector, type, std::forward<F>(callback), options),
                                     IsAlive(), options, selector);
  }

  ///
  /// Same as On() with default options, but returns a Result with the reason for a failure.
  ///
  /// @param  selector  A CSS selector (eg, `tr.item`).
  ///
  /// @param  type      The event type to listen for.
  ///
  /// @param  callback  The callable to run for each match, taking (dom::Event, dom::Element),
  ///                   (dom::Element), or ().
  ///
  /// @return  Returns a handle for removing the listener. Fails with a SyntaxError if
  ///          `selector` is malformed.
  ///
  template <typename F>
  [[nodiscard]] Result<EventListener> On(std::string_view selector, std::string_view type,
                                         F&& callback, Checked_t) const {
    return On(selector, type, std::forward<F>(callback), AddEventListenerOptions {}, Checked);
  }

  ///
  /// Listen for an event on matching elements by calling a member function on an object you keep
  /// alive (see the callable overload).
  ///
  /// @param  selector  A CSS selector (eg, `tr.item`).
  ///
  /// @param  type      The event type to listen for.
  ///
  /// @param  receiver  The object to call `method` on.
  ///
  /// @param  method    The member function to call, taking (dom::Event, dom::Element),
  ///                   (dom::Element), or ().
  ///
  /// @param  options   The listener's options (see AddEventListenerOptions).
  ///
  /// @return  Returns a handle for removing the listener, which you can ignore. It's empty when
  ///          nothing was added (the page is gone or `options.signal` is already aborted).
  ///
  /// @warning  The listener calls `receiver` for as long as it lives, so `receiver` must outlive
  ///           the page, or the listener must be added with the signal of an AbortController
  ///           that `receiver` owns. The smart-pointer overload with a std::weak_ptr skips calls
  ///           once the object is gone instead.
  ///
  template <typename C, typename M>
    requires(std::is_class_v<C> && std::is_member_function_pointer_v<M>
             && !LockableHolder<C>)
  EventListener On(std::string_view selector, std::string_view type, C& receiver, M method,
                   const AddEventListenerOptions& options = {}) const {
    return On(selector, type, detail::WrapBorrowedMember(receiver, method), options);
  }

  ///
  /// Listen for an event on matching elements by calling a member function on an object you keep
  /// alive, with options as flags.
  ///
  /// @param  selector  A CSS selector (eg, `tr.item`).
  ///
  /// @param  type      The event type to listen for.
  ///
  /// @param  receiver  The object to call `method` on.
  ///
  /// @param  method    The member function to call, taking (dom::Event, dom::Element),
  ///                   (dom::Element), or ().
  ///
  /// @param  flags     The listener's options (see EventListenerFlags).
  ///
  /// @return  Returns a handle for removing the listener, which you can ignore (empty when the
  ///          page is gone).
  ///
  /// @warning  The listener calls `receiver` for as long as it lives, so `receiver` must outlive
  ///           the page. To tie the listener to `receiver` instead, use the options overload
  ///           with the signal of an AbortController that `receiver` owns.
  ///
  template <typename C, typename M>
    requires(std::is_class_v<C> && std::is_member_function_pointer_v<M>
             && !LockableHolder<C>)
  EventListener On(std::string_view selector, std::string_view type, C& receiver, M method,
                   EventListenerFlags flags) const {
    return On(selector, type, detail::WrapBorrowedMember(receiver, method), flags);
  }

  ///
  /// Listen for an event on matching elements by calling a member function through a smart
  /// pointer that's checked before each call (see the callable overload).
  ///
  /// A std::shared_ptr keeps the object alive for as long as the listener exists. A std::weak_ptr
  /// doesn't, and events are skipped once the object is gone.
  ///
  /// @param  selector  A CSS selector (eg, `tr.item`).
  ///
  /// @param  type      The event type to listen for.
  ///
  /// @param  holder    A std::shared_ptr, a std::weak_ptr, or another smart pointer with a
  ///                   HolderTraits specialization (see LockableHolder).
  ///
  /// @param  method    The member function to call, taking (dom::Event, dom::Element),
  ///                   (dom::Element), or ().
  ///
  /// @param  options   The listener's options (see AddEventListenerOptions).
  ///
  /// @return  Returns a handle for removing the listener, which you can ignore. It's empty when
  ///          nothing was added (the page is gone or `options.signal` is already aborted).
  ///
  template <typename H, typename M>
    requires(LockableHolder<H> && std::is_member_function_pointer_v<M>)
  EventListener On(std::string_view selector, std::string_view type, H holder, M method,
                   const AddEventListenerOptions& options = {}) const {
    return On(selector, type, detail::WrapHolderMember(std::move(holder), method), options);
  }

  ///
  /// Listen for an event on matching elements by calling a member function through a smart
  /// pointer, with options as flags.
  ///
  /// @param  selector  A CSS selector (eg, `tr.item`).
  ///
  /// @param  type      The event type to listen for.
  ///
  /// @param  holder    A std::shared_ptr, a std::weak_ptr, or another smart pointer with a
  ///                   HolderTraits specialization (see LockableHolder).
  ///
  /// @param  method    The member function to call, taking (dom::Event, dom::Element),
  ///                   (dom::Element), or ().
  ///
  /// @param  flags     The listener's options (see EventListenerFlags).
  ///
  /// @return  Returns a handle for removing the listener, which you can ignore (empty when the
  ///          page is gone).
  ///
  template <typename H, typename M>
    requires(LockableHolder<H> && std::is_member_function_pointer_v<M>)
  EventListener On(std::string_view selector, std::string_view type, H holder, M method,
                   EventListenerFlags flags) const {
    return On(selector, type, detail::WrapHolderMember(std::move(holder), method), flags);
  }

  // --- Typed element views --------------------------------------------------------------------

  ///
  /// Get this element as an `<input>` (HTMLInputElement).
  ///
  /// @return  Returns the element as an HTMLInputElement (empty if it isn't an `<input>`), so
  ///          `if (auto input = el.AsInput())` checks and converts in one step.
  ///
  /// @note  In an XHTML document, this and the other As*() functions always return an empty view.
  ///
  HTMLInputElement AsInput() const;

  ///
  /// Get this element as a `<textarea>` (HTMLTextAreaElement).
  ///
  /// @return  Returns the element as an HTMLTextAreaElement (empty if it isn't a `<textarea>`).
  ///
  HTMLTextAreaElement AsTextArea() const;

  ///
  /// Get this element as a `<select>` (HTMLSelectElement).
  ///
  /// @return  Returns the element as an HTMLSelectElement (empty if it isn't a `<select>`).
  ///
  HTMLSelectElement AsSelect() const;

  ///
  /// Get this element as an `<option>` (HTMLOptionElement).
  ///
  /// @return  Returns the element as an HTMLOptionElement (empty if it isn't an `<option>`).
  ///
  HTMLOptionElement AsOption() const;

  ///
  /// Get this element as a `<form>` (HTMLFormElement).
  ///
  /// @return  Returns the element as an HTMLFormElement (empty if it isn't a `<form>`).
  ///
  HTMLFormElement AsForm() const;

  ///
  /// Get this element as an `<iframe>` (HTMLIFrameElement).
  ///
  /// @return  Returns the element as an HTMLIFrameElement (empty if it isn't an `<iframe>` or a
  ///          legacy `<frame>`).
  ///
  HTMLIFrameElement AsIFrame() const;

  ///
  /// Get this element as an `<img>` (HTMLImageElement).
  ///
  /// @return  Returns the element as an HTMLImageElement (empty if it isn't an `<img>`).
  ///
  HTMLImageElement AsImage() const;

  ///
  /// Get this element as an `<a>` (HTMLAnchorElement).
  ///
  /// @return  Returns the element as an HTMLAnchorElement (empty if it isn't an `<a>`).
  ///
  HTMLAnchorElement AsAnchor() const;

  // --- Interop with the C API (most embedders never touch raw handles) -----------------------
  //
  // These deliberately hide Node's same-named members with element-typed versions: the two
  // handle families name one object underneath, so the narrower type is the only
  // difference (the ULDOMNode/ULDOMElement casts below are exact by construction).

  ///
  /// Wrap a C handle you own, taking ownership of it.
  ///
  /// @param  handle  A handle from the C API that you would otherwise destroy with
  ///                 ulDestroyDOMElement() (NULL gives an empty Element).
  ///
  /// @return  Returns an Element that destroys `handle` when it's done.
  ///
  static Element Adopt(ULDOMElement handle) { return Element(handle); }

  ///
  /// Wrap a C handle the library owns (eg, a callback argument), adding a reference.
  ///
  /// @param  handle  The borrowed handle (NULL gives an empty Element).
  ///
  /// @return  Returns an Element with its own reference, so you can keep it after the callback.
  ///
  static Element FromBorrowed(ULDOMElement handle) {
    return Element(handle ? ulCreateDOMElementRef(handle) : nullptr);
  }

  ///
  /// Get the C handle, for passing to the `<Ultralight/CAPI/CAPI_DOMElement.h>` functions.
  ///
  /// @return  Returns the handle (NULL for an empty Element). This Element still owns it, so don't
  ///          destroy it.
  ///
  ULDOMElement raw() const { return reinterpret_cast<ULDOMElement>(detail_.handle); }

  ///
  /// Give up ownership of the C handle and return it. This Element becomes empty.
  ///
  /// @return  Returns the handle. You must call ulDestroyDOMElement() when finished.
  ///
  ULDOMElement LeakRef() {
    return reinterpret_cast<ULDOMElement>(Node::LeakRef());
  }

 protected:
  explicit Element(ULDOMElement handle)
      : Node(reinterpret_cast<ULDOMNode>(handle)) {}

  ///
  /// Whether or not this element's tag name is `upper_tag`.
  ///
  /// @param  upper_tag  The tag name to compare with, in uppercase (eg, `INPUT`).
  ///
  bool TagIs(const char* upper_tag) const {
    return raw() && tagName() == upper_tag;
  }
};

// The property proxies rely on this layout: the storage lives in the Node base, the style
// pack is the first member Element declares, and the types are non-aggregate (heap
// aggregate-initialization of the collapsed pack crashes some older compilers). Element is
// not standard-layout (its data spans Node and Element), so offsetof on it is
// conditionally-supported; clang defines it for this single-inheritance shape, exactly as
// the typed element views below already rely on for their own members. Node itself stays
// standard-layout (asserted in Node.h).
static_assert(std::is_standard_layout_v<detail::StyleProxy>,
              "StyleProxy must stay standard-layout");
static_assert(!std::is_aggregate_v<Element>, "Element must not be an aggregate");
static_assert(!std::is_aggregate_v<detail::StyleProxy>,
              "StyleProxy must not be an aggregate");
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif
static_assert(offsetof(Element, style) == sizeof(Node),
              "the style proxy must stay the first member Element declares (directly "
              "after the Node base subobject)");
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

///
/// The direction of a text control's selection (selectionDirection).
///
enum class SelectionDirection : uint8_t {
  None,      ///< No direction.
  Forward,   ///< The selection was made toward the end of the text.
  Backward,  ///< The selection was made toward the start of the text.
};

///
/// Where the selection goes after HTMLInputElement::setRangeText() replaces text (selectMode).
///
enum class SelectionMode : uint8_t {
  Preserve,  ///< Keep the selection, adjusted for the edit.
  Select,    ///< Select the inserted text.
  Start,     ///< Put the caret before the inserted text.
  End,       ///< Put the caret after the inserted text.
};

///
/// The ways a form control fails its constraints (ValidityState).
///
/// Get one from HTMLInputElement::validity() (or the textarea and select versions). It's a
/// snapshot, so unlike the web's live ValidityState it doesn't change when the control does.
///
/// ```
/// dom::ValidityState v = input.validity();
/// if (v.valueMissing)
///   input.setCustomValidity("Enter a name");
/// ```
///
struct ValidityState {
  bool valueMissing = false;     ///< A `required` control has no value.
  bool typeMismatch = false;     ///< The value doesn't fit the input type (eg, a bad email).
  bool patternMismatch = false;  ///< The value doesn't match the `pattern` attribute.
  bool tooLong = false;          ///< The user made the value longer than `maxlength`.
  bool tooShort = false;         ///< The user made the value shorter than `minlength`.
  bool rangeUnderflow = false;   ///< The value is less than `min`.
  bool rangeOverflow = false;    ///< The value is greater than `max`.
  bool stepMismatch = false;     ///< The value doesn't fit the `step` attribute.
  bool badInput = false;         ///< The user entered something the control can't convert.
  bool customError = false;      ///< A custom error message is set.
  bool valid = false;            ///< None of the above are true.
};

/// \cond INTERNAL
namespace detail {

inline ValidityState ReadValidity(ULDOMElement element) {
  ULDOMValidityState state;
  ulDOMElementGetValidity(element, &state);
  return ValidityState { state.value_missing,   state.type_mismatch, state.pattern_mismatch,
                         state.too_long,        state.too_short,     state.range_underflow,
                         state.range_overflow,  state.step_mismatch, state.bad_input,
                         state.custom_error,    state.valid };
}

inline const char* SelectionDirectionName(SelectionDirection direction) {
  switch (direction) {
    case SelectionDirection::Forward:
      return "forward";
    case SelectionDirection::Backward:
      return "backward";
    default:
      return "none";
  }
}

inline SelectionDirection SelectionDirectionTag::Get(ULDOMElement h) {
  std::string name = TakeString(ulDOMElementGetSelectionDirection(h));
  if (name == "forward")
    return SelectionDirection::Forward;
  if (name == "backward")
    return SelectionDirection::Backward;
  return SelectionDirection::None;
}

inline void SelectionDirectionTag::Set(ULDOMElement h, SelectionDirection v) {
  ulDOMElementSetSelectionDirection(h, SelectionDirectionName(v), nullptr);
}

inline ULDOMSelectionMode SelectionModeValue(SelectionMode mode) {
  switch (mode) {
    case SelectionMode::Select:
      return kULDOMSelectionMode_Select;
    case SelectionMode::Start:
      return kULDOMSelectionMode_Start;
    case SelectionMode::End:
      return kULDOMSelectionMode_End;
    default:
      return kULDOMSelectionMode_Preserve;
  }
}

}  // namespace detail
/// \endcond

///
/// An `<input>` element (HTMLInputElement).
///
/// Get one with Element::AsInput(). It adds the input's own attributes (eg, `type` and
/// `placeholder`), form validation, the text selection API, and SetValue(), which fires events
/// like a user edit.
///
/// ## Validation
///
/// Mark the constraints in the page (eg, `%required` or `pattern`) or set them from native code,
/// then check them:
///
/// ```
/// dom::HTMLInputElement name_input = document.getElementById("name").AsInput();
/// name_input.required = true;
/// if (!name_input.checkValidity())
///   Log(name_input.validationMessage());
/// ```
///
/// ## Text Selection
///
/// The selection members and methods work on the input types that support selection (`text`,
/// `search`, `tel`, `url`, and `password`), like the web. Offsets count UTF-16 code units. On
/// any other type (eg, `email` or `number`):
///
///  - **The offsets read std::nullopt and the direction reads SelectionDirection::None** (the
///    web reads null).
///  - **Assigning to the selection members does nothing.**
///  - **The methods fail** with an InvalidStateError.
///
/// select() is the exception. It works on every text field, `email` and `number` included.
///
/// A change to the selection fires a `%select` event during a later Renderer::Update(), not
/// during the call.
///
/// @note  Changing the selection of a displayed input that doesn't have focus also gives it
///        keyboard focus (when the View has focus).
///
class HTMLInputElement : public Element {
 public:
  // The members shared with HTMLTextAreaElement and HTMLSelectElement come first, in the same
  // order in each view (one offset per property; see PropertiesImpl.h).

  ///
  /// Whether or not the input has a `required` attribute (required). Assign to add or remove it.
  ///
  /// A required input with an empty value fails checkValidity().
  ///
  detail::BoolProp<detail::RequiredTag> required;

  ///
  /// The input's `placeholder` attribute (placeholder), the hint it shows while it's empty.
  /// Reads as an empty string if there's none.
  ///
  detail::StringProp<detail::PlaceholderTag> placeholder;

  ///
  /// Whether or not the input has a `readonly` attribute (readOnly). Assign to add or remove it.
  ///
  /// The user can't edit a read-only input, and checkValidity() skips it.
  ///
  detail::BoolProp<detail::ReadOnlyTag> readOnly;

  ///
  /// The input's default value (defaultValue), which is its `value` attribute.
  ///
  /// The input goes back to this value when its form is reset. Assigning also changes the
  /// current value, unless the user has edited it or you've assigned `value`.
  ///
  detail::StringProp<detail::DefaultValueTag> defaultValue;

  ///
  /// The offset where the selection starts (selectionStart), as a `std::optional<size_t>`.
  /// Assign to move it.
  ///
  /// It reads std::nullopt on an input type without selection (see "Text Selection" above).
  /// Values past the end of the text are clamped to it.
  ///
  /// ```
  /// std::optional<size_t> start = input.selectionStart;
  /// input.selectionStart = 0;
  /// ```
  ///
  detail::ValueProp<detail::SelectionStartTag> selectionStart;

  ///
  /// The offset just past the end of the selection (selectionEnd). Works like selectionStart.
  ///
  detail::ValueProp<detail::SelectionEndTag> selectionEnd;

  ///
  /// The direction of the selection (selectionDirection), as a SelectionDirection. Assign to
  /// change it.
  ///
  /// @note  By default a selection always has a direction, so SelectionDirection::None reads back
  ///        as SelectionDirection::Forward (see ViewConfig::match_native_editing_behavior).
  ///
  detail::ValueProp<detail::SelectionDirectionTag> selectionDirection;

  ///
  /// The input's type (type), such as `text`, `email`, or `checkbox`.
  ///
  /// Reads are lowercase, and an input whose `type` attribute is missing or unknown reads `text`,
  /// like the web. Assign to set the `type` attribute.
  ///
  detail::StringProp<detail::InputTypeTag> type;

  ///
  /// Create an empty HTMLInputElement.
  ///
  HTMLInputElement() {}

  ///
  /// Copy constructor (both handles refer to the same element).
  ///
  /// @param  other  The HTMLInputElement to copy.
  ///
  HTMLInputElement(const HTMLInputElement& other) : Element(other) {}

  ///
  /// Move constructor (`other` becomes empty).
  ///
  /// @param  other  The HTMLInputElement to move from.
  ///
  HTMLInputElement(HTMLInputElement&& other) noexcept : Element(std::move(other)) {}

  ///
  /// Assignment (copies or moves).
  ///
  /// @param  other  The HTMLInputElement to assign from.
  ///
  /// @return  Returns this HTMLInputElement.
  ///
  HTMLInputElement& operator=(HTMLInputElement other) noexcept {
    Element::operator=(std::move(other));
    return *this;
  }

  ///
  /// Set the input's value and fire `input` and `change` events, like a user edit.
  ///
  /// Both events fire before this returns, and only if the value changed. Checkbox, radio,
  /// hidden, and button-type inputs just take the new value (no events fire).
  ///
  /// Assigning the inherited `value` member sets the value without events.
  ///
  /// @param  new_value  The value to set.
  ///
  /// @note  A non-empty value on a file input is ignored.
  ///
  void SetValue(std::string_view new_value) const {
    ulDOMElementSetValue(raw(), new_value.data() ? new_value.data() : "", new_value.size(), true,
                         nullptr);
  }

  ///
  /// Same as SetValue(), but returns a Result with the reason for a failure (eg, an
  /// InvalidStateError for a non-empty value on a file input).
  ///
  /// @return  Returns success. Fails with an InvalidStateError for a non-empty value on a file
  ///          input.
  ///
  [[nodiscard]] Result<void> SetValue(std::string_view new_value, Checked_t) const {
    if (IsEmpty())
      return detail::EmptyHandleError();
    detail::ErrorScope error;
    if (ulDOMElementSetValue(raw(), new_value.data() ? new_value.data() : "",
                             new_value.size(), true, error.out()))
      return {};
    return Unexpected<Error>(error.TakeError());
  }

  ///
  /// Get the form the input belongs to (form).
  ///
  /// @return  Returns the input's form (empty if it isn't in one).
  ///
  HTMLFormElement form() const;

  ///
  /// Check whether the input meets its constraints (checkValidity).
  ///
  /// Constraints come from attributes like `required`, `pattern`, and `maxlength`, from the
  /// input's type (eg, `email`), and from setCustomValidity(). An invalid input gets a
  /// cancelable `invalid` event, which doesn't bubble.
  ///
  /// @return  Returns true if the input is valid. An input excluded from validation (eg, a
  ///          disabled input or a read-only text field) is always valid.
  ///
  bool checkValidity() const { return ulDOMElementCheckValidity(raw()); }

  ///
  /// Check whether the input meets its constraints, and report a failure to the user
  /// (reportValidity).
  ///
  /// This works like checkValidity(). If the input is invalid and no listener canceled its
  /// `invalid` event, the input also gets keyboard focus (and scrolls into view).
  ///
  /// @return  Returns true if the input is valid.
  ///
  /// @note  The library doesn't display a validation message. To show one, read
  ///        validationMessage() in an `invalid` listener and display it yourself.
  ///
  bool reportValidity() const { return ulDOMElementReportValidity(raw()); }

  ///
  /// Set a custom validation error (setCustomValidity).
  ///
  /// While the message isn't empty, the input is invalid and validationMessage() returns it.
  ///
  /// @param  error  The error message. Pass an empty string to clear the error.
  ///
  void setCustomValidity(std::string_view error) const {
    ulDOMElementSetCustomValidity(raw(), error.data() ? error.data() : "", error.size());
  }

  ///
  /// Get the message that describes why the input is invalid (validationMessage).
  ///
  /// @return  Returns the message (eg, the one passed to setCustomValidity(), or the library's
  ///          message for a missing required value), or an empty string if the input is valid or
  ///          excluded from validation.
  ///
  std::string validationMessage() const {
    return detail::TakeString(ulDOMElementGetValidationMessage(raw()));
  }

  ///
  /// Get how the input fails its constraints (validity).
  ///
  /// @return  Returns a snapshot of the validity flags (all false if this HTMLInputElement is
  ///          empty or its page is gone). Call this again after the value or the constraints
  ///          change.
  ///
  ValidityState validity() const { return detail::ReadValidity(raw()); }

  ///
  /// Select all of the input's text (select).
  ///
  /// This works on every text field (`email` and `number` too) and does nothing on other input
  /// types.
  ///
  void select() const { ulDOMElementSelect(raw()); }

  ///
  /// Set the selection (setSelectionRange).
  ///
  /// Offsets past the end of the text are clamped to it.
  ///
  /// @param  start      The offset where the selection starts.
  ///
  /// @param  end        The offset just past the end of the selection.
  ///
  /// @param  direction  The direction of the selection.
  ///
  /// @note  This does nothing if the input type doesn't support selection.
  ///
  void setSelectionRange(size_t start, size_t end,
                         SelectionDirection direction = SelectionDirection::None) const {
    ulDOMElementSetSelectionRange(raw(), detail::IndexArgument(start), detail::IndexArgument(end),
                                  detail::SelectionDirectionName(direction), nullptr);
  }

  ///
  /// Same as setSelectionRange(), but returns a Result with the reason for a failure (eg, an
  /// InvalidStateError if the input type doesn't support selection).
  ///
  /// @return  Returns success. Fails with an InvalidStateError if the input type doesn't support
  ///          selection.
  ///
  [[nodiscard]] Result<void> setSelectionRange(size_t start, size_t end, Checked_t) const {
    return setSelectionRange(start, end, SelectionDirection::None, Checked);
  }

  ///
  /// Same as setSelectionRange() with a `direction`, but returns a Result with the reason for a
  /// failure.
  ///
  /// @return  Returns success. Fails with an InvalidStateError if the input type doesn't support
  ///          selection.
  ///
  [[nodiscard]] Result<void> setSelectionRange(size_t start, size_t end,
                                               SelectionDirection direction, Checked_t) const {
    if (IsEmpty())
      return detail::EmptyHandleError();
    detail::ErrorScope error;
    if (ulDOMElementSetSelectionRange(raw(), detail::IndexArgument(start),
                                      detail::IndexArgument(end),
                                      detail::SelectionDirectionName(direction), error.out()))
      return {};
    return Unexpected<Error>(error.TakeError());
  }

  ///
  /// Replace the selected text (setRangeText).
  ///
  /// This fires no `input` or `change` events, like the web.
  ///
  /// @param  replacement  The text to put in place of the selection.
  ///
  /// @note  This does nothing if the input type doesn't support selection.
  ///
  void setRangeText(std::string_view replacement) const {
    ulDOMElementSetRangeText(raw(), replacement.data() ? replacement.data() : "",
                             replacement.size(), nullptr);
  }

  ///
  /// Same as setRangeText(), but returns a Result with the reason for a failure (eg, an
  /// InvalidStateError if the input type doesn't support selection).
  ///
  /// @return  Returns success. Fails with an InvalidStateError if the input type doesn't support
  ///          selection.
  ///
  [[nodiscard]] Result<void> setRangeText(std::string_view replacement, Checked_t) const {
    if (IsEmpty())
      return detail::EmptyHandleError();
    detail::ErrorScope error;
    if (ulDOMElementSetRangeText(raw(), replacement.data() ? replacement.data() : "",
                                 replacement.size(), error.out()))
      return {};
    return Unexpected<Error>(error.TakeError());
  }

  ///
  /// Replace a range of the text (setRangeText with a range).
  ///
  /// Offsets past the end of the text are clamped to it. This fires no `input` or `change`
  /// events, like the web.
  ///
  /// @param  replacement  The text to put in place of the range.
  ///
  /// @param  start        The offset where the range starts.
  ///
  /// @param  end          The offset just past the end of the range.
  ///
  /// @param  mode         Where the selection goes afterward.
  ///
  /// @note  This does nothing if `start` is greater than `end` or the input type doesn't support
  ///        selection.
  ///
  void setRangeText(std::string_view replacement, size_t start, size_t end,
                    SelectionMode mode = SelectionMode::Preserve) const {
    ulDOMElementSetRangeTextInRange(raw(), replacement.data() ? replacement.data() : "",
                                    replacement.size(), detail::IndexArgument(start),
                                    detail::IndexArgument(end), detail::SelectionModeValue(mode),
                                    nullptr);
  }

  ///
  /// Same as setRangeText() with a range, but returns a Result with the reason for a failure (eg,
  /// an IndexSizeError if `start` is greater than `end`).
  ///
  /// @return  Returns success. Fails with an IndexSizeError if `start` is greater than `end`, or an
  ///          InvalidStateError if the input type doesn't support selection.
  ///
  [[nodiscard]] Result<void> setRangeText(std::string_view replacement, size_t start,
                                          size_t end, Checked_t) const {
    return setRangeText(replacement, start, end, SelectionMode::Preserve, Checked);
  }

  ///
  /// Same as setRangeText() with a range and a `mode`, but returns a Result with the reason for a
  /// failure.
  ///
  /// @return  Returns success. Fails with an IndexSizeError if `start` is greater than `end`, or an
  ///          InvalidStateError if the input type doesn't support selection.
  ///
  [[nodiscard]] Result<void> setRangeText(std::string_view replacement, size_t start,
                                          size_t end, SelectionMode mode, Checked_t) const {
    if (IsEmpty())
      return detail::EmptyHandleError();
    detail::ErrorScope error;
    if (ulDOMElementSetRangeTextInRange(raw(), replacement.data() ? replacement.data() : "",
                                        replacement.size(), detail::IndexArgument(start),
                                        detail::IndexArgument(end),
                                        detail::SelectionModeValue(mode), error.out()))
      return {};
    return Unexpected<Error>(error.TakeError());
  }

 private:
  friend class Element;
  explicit HTMLInputElement(Element base) : Element(std::move(base)) {}
};

///
/// A `<textarea>` element (HTMLTextAreaElement).
///
/// Get one with Element::AsTextArea(). It adds the textarea's attributes, form validation, and
/// the text selection API. These work like HTMLInputElement's, except that a textarea always
/// supports selection.
///
class HTMLTextAreaElement : public Element {
 public:
  // Same order as the shared members of HTMLInputElement (one offset per property).

  ///
  /// Whether or not the textarea has a `required` attribute (required). Works like
  /// HTMLInputElement::required.
  ///
  detail::BoolProp<detail::RequiredTag> required;

  ///
  /// The textarea's `placeholder` attribute (placeholder). Works like
  /// HTMLInputElement::placeholder.
  ///
  detail::StringProp<detail::PlaceholderTag> placeholder;

  ///
  /// Whether or not the textarea has a `readonly` attribute (readOnly). Works like
  /// HTMLInputElement::readOnly.
  ///
  detail::BoolProp<detail::ReadOnlyTag> readOnly;

  ///
  /// The textarea's default value (defaultValue), which is the text between its tags.
  ///
  /// The textarea goes back to this value when its form is reset. Assigning replaces the text
  /// between the tags, and also changes the current value unless the user has edited it or
  /// you've assigned `value`.
  ///
  detail::StringProp<detail::DefaultValueTag> defaultValue;

  ///
  /// The offset where the selection starts (selectionStart). Works like
  /// HTMLInputElement::selectionStart.
  ///
  detail::ValueProp<detail::SelectionStartTag> selectionStart;

  ///
  /// The offset just past the end of the selection (selectionEnd). Works like selectionStart.
  ///
  detail::ValueProp<detail::SelectionEndTag> selectionEnd;

  ///
  /// The direction of the selection (selectionDirection). Works like
  /// HTMLInputElement::selectionDirection.
  ///
  detail::ValueProp<detail::SelectionDirectionTag> selectionDirection;

  ///
  /// Create an empty HTMLTextAreaElement.
  ///
  HTMLTextAreaElement() {}

  ///
  /// Copy constructor (both handles refer to the same element).
  ///
  /// @param  other  The HTMLTextAreaElement to copy.
  ///
  HTMLTextAreaElement(const HTMLTextAreaElement& other) : Element(other) {}

  ///
  /// Move constructor (`other` becomes empty).
  ///
  /// @param  other  The HTMLTextAreaElement to move from.
  ///
  HTMLTextAreaElement(HTMLTextAreaElement&& other) noexcept : Element(std::move(other)) {}

  ///
  /// Assignment (copies or moves).
  ///
  /// @param  other  The HTMLTextAreaElement to assign from.
  ///
  /// @return  Returns this HTMLTextAreaElement.
  ///
  HTMLTextAreaElement& operator=(HTMLTextAreaElement other) noexcept {
    Element::operator=(std::move(other));
    return *this;
  }

  ///
  /// Set the textarea's value and fire `input` and `change` events, like a user edit.
  ///
  /// Both events fire before this returns, and only if the value changed. Assigning the inherited
  /// `value` member sets the value without events.
  ///
  /// @param  new_value  The value to set.
  ///
  void SetValue(std::string_view new_value) const {
    ulDOMElementSetValue(raw(), new_value.data() ? new_value.data() : "", new_value.size(), true,
                         nullptr);
  }

  ///
  /// Same as SetValue(), but returns a Result with the reason for a failure.
  ///
  /// @return  Returns success (it fails only when this HTMLTextAreaElement is empty or its page
  ///          is gone).
  ///
  [[nodiscard]] Result<void> SetValue(std::string_view new_value, Checked_t) const {
    if (IsEmpty())
      return detail::EmptyHandleError();
    detail::ErrorScope error;
    if (ulDOMElementSetValue(raw(), new_value.data() ? new_value.data() : "",
                             new_value.size(), true, error.out()))
      return {};
    return Unexpected<Error>(error.TakeError());
  }

  ///
  /// Get the form the textarea belongs to (form).
  ///
  /// @return  Returns the textarea's form (empty if it isn't in one).
  ///
  HTMLFormElement form() const;

  ///
  /// Check whether the textarea meets its constraints (checkValidity). Works like
  /// HTMLInputElement::checkValidity().
  ///
  /// @return  Returns true if the textarea is valid.
  ///
  bool checkValidity() const { return ulDOMElementCheckValidity(raw()); }

  ///
  /// Check whether the textarea meets its constraints, and report a failure to the user
  /// (reportValidity). Works like HTMLInputElement::reportValidity().
  ///
  /// @return  Returns true if the textarea is valid.
  ///
  bool reportValidity() const { return ulDOMElementReportValidity(raw()); }

  ///
  /// Set a custom validation error (setCustomValidity). Works like
  /// HTMLInputElement::setCustomValidity().
  ///
  /// @param  error  The error message. Pass an empty string to clear the error.
  ///
  void setCustomValidity(std::string_view error) const {
    ulDOMElementSetCustomValidity(raw(), error.data() ? error.data() : "", error.size());
  }

  ///
  /// Get the message that describes why the textarea is invalid (validationMessage).
  ///
  /// @return  Returns the message, or an empty string if the textarea is valid or excluded from
  ///          validation.
  ///
  std::string validationMessage() const {
    return detail::TakeString(ulDOMElementGetValidationMessage(raw()));
  }

  ///
  /// Get how the textarea fails its constraints (validity). Works like
  /// HTMLInputElement::validity().
  ///
  /// @return  Returns a snapshot of the validity flags.
  ///
  ValidityState validity() const { return detail::ReadValidity(raw()); }

  ///
  /// Select all of the text (select).
  ///
  void select() const { ulDOMElementSelect(raw()); }

  ///
  /// Set the selection (setSelectionRange).
  ///
  /// Offsets past the end of the text are clamped to it.
  ///
  /// @param  start      The offset where the selection starts.
  ///
  /// @param  end        The offset just past the end of the selection.
  ///
  /// @param  direction  The direction of the selection.
  ///
  void setSelectionRange(size_t start, size_t end,
                         SelectionDirection direction = SelectionDirection::None) const {
    ulDOMElementSetSelectionRange(raw(), detail::IndexArgument(start), detail::IndexArgument(end),
                                  detail::SelectionDirectionName(direction), nullptr);
  }

  ///
  /// Same as setSelectionRange(), but returns a Result with the reason for a failure.
  ///
  /// @return  Returns success (it fails only when this element is empty or its page is gone).
  ///
  [[nodiscard]] Result<void> setSelectionRange(size_t start, size_t end, Checked_t) const {
    return setSelectionRange(start, end, SelectionDirection::None, Checked);
  }

  ///
  /// Same as setSelectionRange() with a `direction`, but returns a Result with the reason for a
  /// failure.
  ///
  /// @return  Returns success (it fails only when this element is empty or its page is gone).
  ///
  [[nodiscard]] Result<void> setSelectionRange(size_t start, size_t end,
                                               SelectionDirection direction, Checked_t) const {
    if (IsEmpty())
      return detail::EmptyHandleError();
    detail::ErrorScope error;
    if (ulDOMElementSetSelectionRange(raw(), detail::IndexArgument(start),
                                      detail::IndexArgument(end),
                                      detail::SelectionDirectionName(direction), error.out()))
      return {};
    return Unexpected<Error>(error.TakeError());
  }

  ///
  /// Replace the selected text (setRangeText).
  ///
  /// This fires no `input` or `change` events, like the web.
  ///
  /// @param  replacement  The text to put in place of the selection.
  ///
  void setRangeText(std::string_view replacement) const {
    ulDOMElementSetRangeText(raw(), replacement.data() ? replacement.data() : "",
                             replacement.size(), nullptr);
  }

  ///
  /// Same as setRangeText(), but returns a Result with the reason for a failure.
  ///
  /// @return  Returns success (it fails only when this element is empty or its page is gone).
  ///
  [[nodiscard]] Result<void> setRangeText(std::string_view replacement, Checked_t) const {
    if (IsEmpty())
      return detail::EmptyHandleError();
    detail::ErrorScope error;
    if (ulDOMElementSetRangeText(raw(), replacement.data() ? replacement.data() : "",
                                 replacement.size(), error.out()))
      return {};
    return Unexpected<Error>(error.TakeError());
  }

  ///
  /// Replace a range of the text (setRangeText with a range).
  ///
  /// Offsets past the end of the text are clamped to it. This fires no `input` or `change`
  /// events, like the web.
  ///
  /// @param  replacement  The text to put in place of the range.
  ///
  /// @param  start        The offset where the range starts.
  ///
  /// @param  end          The offset just past the end of the range.
  ///
  /// @param  mode         Where the selection goes afterward.
  ///
  /// @note  This does nothing if `start` is greater than `end`.
  ///
  void setRangeText(std::string_view replacement, size_t start, size_t end,
                    SelectionMode mode = SelectionMode::Preserve) const {
    ulDOMElementSetRangeTextInRange(raw(), replacement.data() ? replacement.data() : "",
                                    replacement.size(), detail::IndexArgument(start),
                                    detail::IndexArgument(end), detail::SelectionModeValue(mode),
                                    nullptr);
  }

  ///
  /// Same as setRangeText() with a range, but returns a Result with the reason for a failure (eg,
  /// an IndexSizeError if `start` is greater than `end`).
  ///
  /// @return  Returns success. Fails with an IndexSizeError if `start` is greater than `end`.
  ///
  [[nodiscard]] Result<void> setRangeText(std::string_view replacement, size_t start,
                                          size_t end, Checked_t) const {
    return setRangeText(replacement, start, end, SelectionMode::Preserve, Checked);
  }

  ///
  /// Same as setRangeText() with a range and a `mode`, but returns a Result with the reason for a
  /// failure.
  ///
  /// @return  Returns success. Fails with an IndexSizeError if `start` is greater than `end`.
  ///
  [[nodiscard]] Result<void> setRangeText(std::string_view replacement, size_t start,
                                          size_t end, SelectionMode mode, Checked_t) const {
    if (IsEmpty())
      return detail::EmptyHandleError();
    detail::ErrorScope error;
    if (ulDOMElementSetRangeTextInRange(raw(), replacement.data() ? replacement.data() : "",
                                        replacement.size(), detail::IndexArgument(start),
                                        detail::IndexArgument(end),
                                        detail::SelectionModeValue(mode), error.out()))
      return {};
    return Unexpected<Error>(error.TakeError());
  }

 private:
  friend class Element;
  explicit HTMLTextAreaElement(Element base) : Element(std::move(base)) {}
};

///
/// A `<select>` element (HTMLSelectElement).
///
/// Get one with Element::AsSelect(). It adds ways to list the options, the select's attributes,
/// and form validation (which works like HTMLInputElement's).
///
/// Use the inherited `value` and `selectedIndex` members to read or change the selection.
/// Changing it that way fires no `change` event, like the web. SetValue() changes it like a user
/// would.
///
/// length() is read-only here (the web can also shorten or lengthen the list through it). Add and
/// remove `<option>` elements to change the list.
///
class HTMLSelectElement : public Element {
 public:
  // Same order as the shared members of HTMLInputElement (one offset per property).

  ///
  /// Whether or not the select has a `required` attribute (required). Assign to add or remove
  /// it.
  ///
  /// A required select fails checkValidity() while no option is selected, or while the selected
  /// option is a placeholder (the first option, with an empty value, in a select that shows one
  /// option at a time).
  ///
  detail::BoolProp<detail::RequiredTag> required;

  ///
  /// Create an empty HTMLSelectElement.
  ///
  HTMLSelectElement() {}

  ///
  /// Copy constructor (both handles refer to the same element).
  ///
  /// @param  other  The HTMLSelectElement to copy.
  ///
  HTMLSelectElement(const HTMLSelectElement& other) : Element(other) {}

  ///
  /// Move constructor (`other` becomes empty).
  ///
  /// @param  other  The HTMLSelectElement to move from.
  ///
  HTMLSelectElement(HTMLSelectElement&& other) noexcept : Element(std::move(other)) {}

  ///
  /// Assignment (copies or moves).
  ///
  /// @param  other  The HTMLSelectElement to assign from.
  ///
  /// @return  Returns this HTMLSelectElement.
  ///
  HTMLSelectElement& operator=(HTMLSelectElement other) noexcept {
    Element::operator=(std::move(other));
    return *this;
  }

  ///
  /// Select the option with a value and fire `input` and `change` events, like a user choice.
  ///
  /// Both events fire before this returns, and only if the selection changed. If no option has
  /// the value, no option is selected (like assigning the inherited `value` member, which fires no
  /// events).
  ///
  /// @param  new_value  The value of the option to select.
  ///
  void SetValue(std::string_view new_value) const {
    ulDOMElementSetValue(raw(), new_value.data() ? new_value.data() : "", new_value.size(), true,
                         nullptr);
  }

  ///
  /// Same as SetValue(), but returns a Result with the reason for a failure.
  ///
  /// @return  Returns success (it fails only when this HTMLSelectElement is empty or its page is
  ///          gone).
  ///
  [[nodiscard]] Result<void> SetValue(std::string_view new_value, Checked_t) const {
    if (IsEmpty())
      return detail::EmptyHandleError();
    detail::ErrorScope error;
    if (ulDOMElementSetValue(raw(), new_value.data() ? new_value.data() : "",
                             new_value.size(), true, error.out()))
      return {};
    return Unexpected<Error>(error.TakeError());
  }

  ///
  /// Get the form the select belongs to (form).
  ///
  /// @return  Returns the select's form (empty if it isn't in one).
  ///
  HTMLFormElement form() const;

  ///
  /// Check whether the select meets its constraints (checkValidity). Works like
  /// HTMLInputElement::checkValidity().
  ///
  /// @return  Returns true if the select is valid.
  ///
  bool checkValidity() const { return ulDOMElementCheckValidity(raw()); }

  ///
  /// Check whether the select meets its constraints, and report a failure to the user
  /// (reportValidity). Works like HTMLInputElement::reportValidity().
  ///
  /// @return  Returns true if the select is valid.
  ///
  bool reportValidity() const { return ulDOMElementReportValidity(raw()); }

  ///
  /// Set a custom validation error (setCustomValidity). Works like
  /// HTMLInputElement::setCustomValidity().
  ///
  /// @param  error  The error message. Pass an empty string to clear the error.
  ///
  void setCustomValidity(std::string_view error) const {
    ulDOMElementSetCustomValidity(raw(), error.data() ? error.data() : "", error.size());
  }

  ///
  /// Get the message that describes why the select is invalid (validationMessage).
  ///
  /// @return  Returns the message, or an empty string if the select is valid or excluded from
  ///          validation.
  ///
  std::string validationMessage() const {
    return detail::TakeString(ulDOMElementGetValidationMessage(raw()));
  }

  ///
  /// Get how the select fails its constraints (validity). Works like
  /// HTMLInputElement::validity().
  ///
  /// @return  Returns a snapshot of the validity flags.
  ///
  ValidityState validity() const { return detail::ReadValidity(raw()); }

  ///
  /// Get the number of options (length), including those inside an `<optgroup>`.
  ///
  /// @return  Returns the number of options.
  ///
  size_t length() const { return ulDOMElementGetOptionCount(raw()); }

  ///
  /// Get an option by its position (item).
  ///
  /// @param  index  The option's position, counting the options inside an `<optgroup>` too.
  ///
  /// @return  Returns the option (empty if `index` is out of range).
  ///
  HTMLOptionElement item(size_t index) const;

  ///
  /// Get the selected options in order (selectedOptions).
  ///
  /// This is defined in `<Ultralight/dom/ElementList.h>`. Include that header (or
  /// `<Ultralight/DOM.h>`) to call it.
  ///
  /// @return  Returns the selected options (an empty list if none is selected).
  ///
  /// @note  Unlike the web's live collection, the list doesn't change when the selection does.
  ///        Call this again to see later changes.
  ///
  ElementList selectedOptions() const;

 private:
  friend class Element;
  explicit HTMLSelectElement(Element base) : Element(std::move(base)) {}
};

///
/// An `<option>` element (HTMLOptionElement).
///
/// Get one with Element::AsOption() or HTMLSelectElement::item(). It adds the `selected` and
/// `text` members and the option's position.
///
/// The inherited `value` member reads the option's text when it has no `%value` attribute, like
/// the web.
///
class HTMLOptionElement : public Element {
 public:
  ///
  /// Whether or not the option is selected (selected). Assign to select or deselect it.
  ///
  /// This is the current state, not the `selected` attribute. Assigning fires no `change` event,
  /// like the web.
  ///
  detail::BoolProp<detail::OptionSelectedTag> selected;

  ///
  /// The option's text with whitespace collapsed (text).
  ///
  /// Assign to replace the option's children with the text, like assigning textContent.
  ///
  detail::StringProp<detail::OptionTextTag> text;

  ///
  /// Create an empty HTMLOptionElement.
  ///
  HTMLOptionElement() {}

  ///
  /// Copy constructor (both handles refer to the same element).
  ///
  /// @param  other  The HTMLOptionElement to copy.
  ///
  HTMLOptionElement(const HTMLOptionElement& other) : Element(other) {}

  ///
  /// Move constructor (`other` becomes empty).
  ///
  /// @param  other  The HTMLOptionElement to move from.
  ///
  HTMLOptionElement(HTMLOptionElement&& other) noexcept : Element(std::move(other)) {}

  ///
  /// Assignment (copies or moves).
  ///
  /// @param  other  The HTMLOptionElement to assign from.
  ///
  /// @return  Returns this HTMLOptionElement.
  ///
  HTMLOptionElement& operator=(HTMLOptionElement other) noexcept {
    Element::operator=(std::move(other));
    return *this;
  }

  ///
  /// Get the option's position in its `<select>` (index).
  ///
  /// @return  Returns the position (0 if the option isn't in a `<select>`, like the web, and also
  ///          for an empty HTMLOptionElement or one whose page is gone).
  ///
  size_t index() const { return ulDOMElementGetIndex(raw()); }

 private:
  friend class Element;
  friend class HTMLSelectElement;
  explicit HTMLOptionElement(Element base) : Element(std::move(base)) {}
};

///
/// A `<form>` element (HTMLFormElement).
///
/// Get one with Element::AsForm() or the form() method of a form control. It adds the form's
/// controls and ways to validate, submit, and reset the form:
///
/// ```
/// for (dom::Element control : form.elements())
///   Log(control.name);
/// ```
///
class HTMLFormElement : public Element {
 public:
  ///
  /// Create an empty HTMLFormElement.
  ///
  HTMLFormElement() {}

  ///
  /// Get the form's controls in document order (elements).
  ///
  /// The list includes controls outside the form whose `form` attribute holds the form's id.
  /// Image buttons (`<input type="image">`) aren't included, like the web.
  ///
  /// This is defined in `<Ultralight/dom/ElementList.h>`. Include that header (or
  /// `<Ultralight/DOM.h>`) to call it.
  ///
  /// @return  Returns the controls (an empty list if there are none).
  ///
  /// @note  Unlike the web's live collection, the list doesn't change when the form does. Call
  ///        this again to see later changes.
  ///
  ElementList elements() const;

  ///
  /// Check whether every control in the form meets its constraints (checkValidity).
  ///
  /// Each invalid control gets a cancelable `invalid` event (see
  /// HTMLInputElement::checkValidity()).
  ///
  /// @return  Returns true if every control is valid.
  ///
  bool checkValidity() const { return ulDOMElementCheckValidity(raw()); }

  ///
  /// Check whether every control in the form meets its constraints, and report a failure to the
  /// user (reportValidity).
  ///
  /// This works like checkValidity(). The first invalid control whose `invalid` event no
  /// listener canceled also gets keyboard focus (and scrolls into view).
  ///
  /// @return  Returns true if every control is valid.
  ///
  /// @note  The library doesn't display a validation message. To show one, read the control's
  ///        message (eg, HTMLInputElement::validationMessage()) in an `invalid` listener and
  ///        display it yourself.
  ///
  bool reportValidity() const { return ulDOMElementReportValidity(raw()); }

  ///
  /// Submit the form (submit).
  ///
  /// Like the web's form.submit(), this skips validation and fires no `submit` event.
  ///
  void submit() const { ulDOMElementSubmit(raw()); }

  ///
  /// Submit the form like a user would (requestSubmit).
  ///
  /// This checks the form's fields first. If they're valid, it fires a cancelable `submit` event
  /// and then submits. Call Event::preventDefault() in a `submit` listener to handle the
  /// submission natively instead.
  ///
  void requestSubmit() const { ulDOMElementRequestSubmit(raw(), nullptr, nullptr); }

  ///
  /// Same as requestSubmit(), but returns a Result with the reason for a failure.
  ///
  /// @return  Returns success, even when invalid fields stop the submission (it fails only when
  ///          this form is empty or its page is gone).
  ///
  [[nodiscard]] Result<void> requestSubmit(Checked_t) const {
    if (IsEmpty())
      return detail::EmptyHandleError();
    detail::ErrorScope error;
    if (ulDOMElementRequestSubmit(raw(), nullptr, error.out()))
      return {};
    return Unexpected<Error>(error.TakeError());
  }

  ///
  /// Submit the form like a user would, as if `submitter` was clicked (requestSubmit with a
  /// submitter).
  ///
  /// The submitter's attributes (eg, `formaction`) apply to the submission.
  ///
  /// @param  submitter  A submit button in this form. An empty Element works like requestSubmit()
  ///                    with no submitter.
  ///
  /// @note  Nothing is submitted if `submitter` isn't a submit button in this form.
  ///
  void requestSubmit(const Element& submitter) const {
    ulDOMElementRequestSubmit(raw(), submitter.raw(), nullptr);
  }

  ///
  /// Same as requestSubmit() with a submitter, but returns a Result with the reason for a failure
  /// (eg, a TypeError if `submitter` isn't a submit button).
  ///
  /// @return  Returns success. Fails with a TypeError if `submitter` isn't a submit button, or a
  ///          NotFoundError if it isn't in this form.
  ///
  [[nodiscard]] Result<void> requestSubmit(const Element& submitter, Checked_t) const {
    if (IsEmpty())
      return detail::EmptyHandleError();
    detail::ErrorScope error;
    if (ulDOMElementRequestSubmit(raw(), submitter.raw(), error.out()))
      return {};
    return Unexpected<Error>(error.TakeError());
  }

  ///
  /// Reset the form's controls to their default values (reset).
  ///
  /// This fires a cancelable `reset` event first.
  ///
  void reset() const { ulDOMElementReset(raw()); }

 private:
  friend class Element;
  friend class HTMLInputElement;
  friend class HTMLTextAreaElement;
  friend class HTMLSelectElement;
  explicit HTMLFormElement(Element base) : Element(std::move(base)) {}
};

///
/// An `<iframe>` element (HTMLIFrameElement).
///
/// Get one with Element::AsIFrame(), which also accepts a legacy `<frame>`. It adds the frame's
/// `src` and contentDocument() for reaching the page inside the frame.
///
class HTMLIFrameElement : public Element {
 public:
  // At the same offset as HTMLImageElement::src (one offset per property).

  ///
  /// The URL of the page in the frame (src).
  ///
  /// Reads return the absolute URL, resolved against the document's base URL (an empty string if
  /// there's no `src` attribute), like the web. Assign to set the `src` attribute, which loads
  /// the new page in the frame.
  ///
  detail::StringProp<detail::SrcTag> src;

  ///
  /// Create an empty HTMLIFrameElement.
  ///
  HTMLIFrameElement() {}

  ///
  /// Copy constructor (both handles refer to the same element).
  ///
  /// @param  other  The HTMLIFrameElement to copy.
  ///
  HTMLIFrameElement(const HTMLIFrameElement& other) : Element(other) {}

  ///
  /// Move constructor (`other` becomes empty).
  ///
  /// @param  other  The HTMLIFrameElement to move from.
  ///
  HTMLIFrameElement(HTMLIFrameElement&& other) noexcept : Element(std::move(other)) {}

  ///
  /// Assignment (copies or moves).
  ///
  /// @param  other  The HTMLIFrameElement to assign from.
  ///
  /// @return  Returns this HTMLIFrameElement.
  ///
  HTMLIFrameElement& operator=(HTMLIFrameElement other) noexcept {
    Element::operator=(std::move(other));
    return *this;
  }

  ///
  /// Get the document inside the frame (contentDocument).
  ///
  /// This is defined in `<Ultralight/dom/Document.h>`. Include that header (or
  /// `<Ultralight/DOM.h>`) to call it.
  ///
  /// @return  Returns the frame's document (empty if the frame has no document). This works for a
  ///          frame from any origin.
  ///
  /// @note  The document's handles stop working when the frame navigates or is removed, even while
  ///        this page lives on.
  ///
  Document contentDocument() const;

 private:
  friend class Element;
  explicit HTMLIFrameElement(Element base) : Element(std::move(base)) {}
};

///
/// An `<img>` element (HTMLImageElement).
///
/// Get one with Element::AsImage(). It adds the image's `src` and `alt` members.
///
class HTMLImageElement : public Element {
 public:
  // At the same offset as HTMLIFrameElement::src (one offset per property).

  ///
  /// The URL of the image (src).
  ///
  /// Reads return the absolute URL, resolved against the document's base URL (an empty string if
  /// there's no `src` attribute), like the web. Assign to set the `src` attribute, which loads the
  /// new image.
  ///
  detail::StringProp<detail::SrcTag> src;

  ///
  /// The image's text alternative (alt), from its `alt` attribute. Reads as an empty string if
  /// there's none.
  ///
  detail::StringProp<detail::AltTag> alt;

  ///
  /// Create an empty HTMLImageElement.
  ///
  HTMLImageElement() {}

  ///
  /// Copy constructor (both handles refer to the same element).
  ///
  /// @param  other  The HTMLImageElement to copy.
  ///
  HTMLImageElement(const HTMLImageElement& other) : Element(other) {}

  ///
  /// Move constructor (`other` becomes empty).
  ///
  /// @param  other  The HTMLImageElement to move from.
  ///
  HTMLImageElement(HTMLImageElement&& other) noexcept : Element(std::move(other)) {}

  ///
  /// Assignment (copies or moves).
  ///
  /// @param  other  The HTMLImageElement to assign from.
  ///
  /// @return  Returns this HTMLImageElement.
  ///
  HTMLImageElement& operator=(HTMLImageElement other) noexcept {
    Element::operator=(std::move(other));
    return *this;
  }

 private:
  friend class Element;
  explicit HTMLImageElement(Element base) : Element(std::move(base)) {}
};

///
/// An `<a>` element (HTMLAnchorElement).
///
/// Get one with Element::AsAnchor(). It adds the link's `href` member.
///
class HTMLAnchorElement : public Element {
 public:
  ///
  /// The URL the link points to (href).
  ///
  /// Reads return the absolute URL, resolved against the document's base URL (an empty string if
  /// there's no `href` attribute), like the web. Assign to set the `href` attribute.
  ///
  detail::StringProp<detail::HrefTag> href;

  ///
  /// Create an empty HTMLAnchorElement.
  ///
  HTMLAnchorElement() {}

  ///
  /// Copy constructor (both handles refer to the same element).
  ///
  /// @param  other  The HTMLAnchorElement to copy.
  ///
  HTMLAnchorElement(const HTMLAnchorElement& other) : Element(other) {}

  ///
  /// Move constructor (`other` becomes empty).
  ///
  /// @param  other  The HTMLAnchorElement to move from.
  ///
  HTMLAnchorElement(HTMLAnchorElement&& other) noexcept : Element(std::move(other)) {}

  ///
  /// Assignment (copies or moves).
  ///
  /// @param  other  The HTMLAnchorElement to assign from.
  ///
  /// @return  Returns this HTMLAnchorElement.
  ///
  HTMLAnchorElement& operator=(HTMLAnchorElement other) noexcept {
    Element::operator=(std::move(other));
    return *this;
  }

 private:
  friend class Element;
  explicit HTMLAnchorElement(Element base) : Element(std::move(base)) {}
};

inline HTMLInputElement Element::AsInput() const {
  return HTMLInputElement(TagIs("INPUT") ? *this : Element());
}

inline HTMLTextAreaElement Element::AsTextArea() const {
  return HTMLTextAreaElement(TagIs("TEXTAREA") ? *this : Element());
}

inline HTMLSelectElement Element::AsSelect() const {
  return HTMLSelectElement(TagIs("SELECT") ? *this : Element());
}

inline HTMLOptionElement Element::AsOption() const {
  return HTMLOptionElement(TagIs("OPTION") ? *this : Element());
}

inline HTMLOptionElement HTMLSelectElement::item(size_t index) const {
  // The waist guarantees the returned handle is an option, so no tag re-check is needed.
  return HTMLOptionElement(Element::Adopt(ulDOMElementGetOption(raw(), index)));
}

inline HTMLFormElement Element::AsForm() const {
  return HTMLFormElement(TagIs("FORM") ? *this : Element());
}

inline HTMLIFrameElement Element::AsIFrame() const {
  return HTMLIFrameElement(TagIs("IFRAME") || TagIs("FRAME") ? *this : Element());
}

inline HTMLImageElement Element::AsImage() const {
  return HTMLImageElement(TagIs("IMG") ? *this : Element());
}

inline HTMLAnchorElement Element::AsAnchor() const {
  return HTMLAnchorElement(TagIs("A") ? *this : Element());
}

// The waist returns only a form (or NULL), so no tag re-check is needed.
inline HTMLFormElement HTMLInputElement::form() const {
  return HTMLFormElement(Element::Adopt(ulDOMElementGetForm(raw())));
}

inline HTMLFormElement HTMLTextAreaElement::form() const {
  return HTMLFormElement(Element::Adopt(ulDOMElementGetForm(raw())));
}

inline HTMLFormElement HTMLSelectElement::form() const {
  return HTMLFormElement(Element::Adopt(ulDOMElementGetForm(raw())));
}

inline Element Node::parentElement() const {
  return Element::Adopt(ulDOMNodeGetParentElement(detail_.handle));
}

inline Element Node::AsElement() const {
  return Element::Adopt(ulDOMNodeAsElement(detail_.handle));
}

inline Element Event::target() const {
  return Element::Adopt(ulDOMEventGetTarget(borrowed_));
}

inline Node Event::targetNode() const {
  return Node::Adopt(ulDOMEventGetTargetNode(borrowed_));
}

inline Element Event::currentTarget() const {
  return Element::Adopt(ulDOMEventGetCurrentTarget(borrowed_));
}

inline Element MouseEvent::relatedTarget() const {
  return Element::Adopt(ulDOMEventGetRelatedTarget(raw()));
}

inline Element FocusEvent::relatedTarget() const {
  return Element::Adopt(ulDOMEventGetRelatedTarget(raw()));
}

inline Element SubmitEvent::submitter() const {
  return Element::Adopt(ulDOMEventGetSubmitter(raw()));
}

}  // namespace dom
}  // namespace ultralight

// The property-proxy offset tables and operator definitions require the complete
// Element type above.
#include <Ultralight/dom/detail/PropertiesImpl.h>

#pragma pop_macro("None")
