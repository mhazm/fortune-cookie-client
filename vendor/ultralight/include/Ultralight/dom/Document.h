///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/CAPI/CAPI_DOMDocument.h>
#include <Ultralight/dom/Holders.h>
#include <Ultralight/dom/detail/Receivers.h>
#include <Ultralight/CAPI/CAPI_DOMEvent.h>
#include <Ultralight/View.h>
#include <Ultralight/dom/Element.h>
#include <Ultralight/dom/ElementList.h>
#include <Ultralight/dom/DocumentFragment.h>
#include <Ultralight/dom/NodeList.h>
#include <Ultralight/dom/Range.h>

#include <string_view>
#include <type_traits>
#include <utility>

namespace ultralight {
namespace dom {

class Selection;
class Window;

///
/// The root of a page's DOM tree in a View or frame.
///
/// dom::Document provides native C++ access to a page's DOM tree. It's the starting point for
/// querying elements and updating the user interface rendered by a View.
///
/// You typically obtain the document during page loads and keep handles to the elements you want to
/// modify later.
///
/// The following listener stores a score element once the main frame's document is parsed and
/// ready:
///
/// ```
/// class Hud : public LoadListener {
///  public:
///   void OnDOMReady(View* caller, uint64_t frame_id, bool is_main_frame,
///                   const String& url) override {
///     if (!is_main_frame)
///       return;
///     dom::Document document(caller);
///     score_ = document.getElementById("score");
///   }
///
///   void SetScore(int score) { score_.textContent = std::to_string(score); }
///
///  private:
///   dom::Element score_;
/// };
/// ```
///
/// ## Getting a Document
///
/// You obtain a document from its owning View or frame:
///
/// - **Main frame documents come from the View.** Passing a View pointer to the Document
///   constructor returns the active document of the top-level frame.
/// - **Subframe documents come from their elements.** Calling
///   dom::HTMLIFrameElement::contentDocument() on a frame element returns the document loaded
///   inside that frame.
///
/// You should construct the main frame's document inside LoadListener::OnDOMReady(), where the new
/// document is in place. Before the first page loads and while a new page is loading, the
/// constructor returns the previous document, which goes away when the new page replaces it.
///
/// LoadListener::OnDOMReady() runs for every frame on the page. Check the `is_main_frame` parameter
/// to ensure top-level setup logic runs only for the main frame.
///
/// Navigating to a new page creates a new document-- handles from the previous page belong to a
/// page that is gone (dom::Element covers handle states).
///
/// ## Relationship to Nodes
///
/// dom::Document is a distinct type that doesn't inherit from dom::Node. Because the document isn't
/// a node, the root element has no parent in the DOM tree.
///
/// On the root element, parentNode() is empty and ownerDocument() gets back to the document:
///
/// ```
/// dom::Element root = document.documentElement();
///
/// root.parentNode();     // empty
/// root.ownerDocument();  // the document
/// ```
///
/// ## Document Events
///
/// You can listen for events on the document with addEventListener() or handle matching descendants
/// using On(). Events dispatched on elements throughout the page bubble up to the document unless
/// stopped along the way.
///
/// Page `load` and viewport `resize` events target the window rather than the document, so document
/// listeners never receive them. Call defaultView() to obtain the page's dom::Window when you need
/// to handle window-level events.
///
/// @note  Don't add a `DOMContentLoaded` listener in LoadListener::OnDOMReady(). The event has
///        already fired by the time the callback runs, so a listener added there never fires.
///
/// @see  dom::Element, dom::Node, dom::Window, dom::Triggers, LoadListener::OnDOMReady()
///
class Document {
 public:
  ///
  /// The document's title (title), the text of its `<title>` element with whitespace collapsed.
  ///
  /// Assign to replace the text of the `<title>` element. If there's no `<title>`, one is added
  /// to the `<head>` (nothing happens if there's no `<head>` either).
  ///
  detail::DocumentStringProp<detail::DocumentTitleTag> title;

  ///
  /// Create an empty Document.
  ///
  Document() = default;

  ///
  /// Get the document of a View's main frame.
  ///
  /// @param  view  The View (a nullptr gives an empty Document).
  ///
  /// @note  This is the document the frame has now. Before the first page loads, and while a new
  ///        page is loading, that's the previous document, which goes away when the new page
  ///        replaces it. Getting it in LoadListener::OnDOMReady() makes sure it's your page.
  ///
  explicit Document(View* view) : handle_(view ? view->GetDOMDocument() : nullptr) {}

  ///
  /// Copy constructor (both handles refer to the same document).
  ///
  /// @param  other  The Document to copy.
  ///
  Document(const Document& other)
      : handle_(other.handle_ ? ulCreateDOMDocumentRef(other.handle_) : nullptr) {}

  ///
  /// Move constructor (`other` becomes empty).
  ///
  /// @param  other  The Document to move from.
  ///
  Document(Document&& other) noexcept : handle_(other.handle_) { other.handle_ = nullptr; }

  ///
  /// Assignment (copies or moves).
  ///
  /// @param  other  The Document to assign from.
  ///
  /// @return  Returns this Document.
  ///
  Document& operator=(Document other) noexcept {
    std::swap(handle_, other.handle_);
    return *this;
  }

  ///
  /// Destroy this handle (the document itself isn't affected).
  ///
  ~Document() { ulDestroyDOMDocument(handle_); }

  ///
  /// Whether or not this Document is valid (see IsAlive()).
  ///
  explicit operator bool() const { return IsAlive(); }

  ///
  /// Whether or not this Document is empty (it holds no handle).
  ///
  bool IsEmpty() const { return handle_ == nullptr; }

  ///
  /// Whether or not this Document is valid (it isn't empty and its page is still alive).
  ///
  /// @note  Safe to call from any thread.
  ///
  bool IsAlive() const { return handle_ && ulDOMDocumentIsAlive(handle_); }

  // --- Lookup -----------------------------------------------------------------------------

  ///
  /// Find the element with a certain id (getElementById).
  ///
  /// @param  id  The id to look for.
  ///
  /// @return  Returns the element (empty if no element has that id).
  ///
  Element getElementById(std::string_view id) const {
    detail::CString i(id);
    return Element::Adopt(ulDOMDocumentGetElementById(handle_, i.c_str()));
  }

  ///
  /// Find the first element matching a CSS selector (querySelector).
  ///
  /// @param  selectors  A CSS selector list (eg, `.item > a[href]`).
  ///
  /// @return  Returns the first match in document order (empty if nothing matches, the selector
  ///          is malformed, or the page is gone).
  ///
  Element querySelector(std::string_view selectors) const {
    detail::CString s(selectors);
    return Element::Adopt(ulDOMDocumentQuerySelector(handle_, s.c_str(), nullptr));
  }

  ///
  /// Same as querySelector(), but returns a Result with the reason for a failure (eg, a
  /// SyntaxError for a malformed selector).
  ///
  /// @return  Returns the first match in document order (an empty Element if nothing matches,
  ///          which isn't an error). Fails with a SyntaxError if the selector is malformed.
  ///
  [[nodiscard]] Result<Element> querySelector(std::string_view selectors, Checked_t) const {
    if (IsEmpty())
      return detail::EmptyHandleError();
    detail::CString s(selectors);
    detail::ErrorScope error;
    ULDOMElement result = ulDOMDocumentQuerySelector(handle_, s.c_str(), error.out());
    if (result)
      return Element::Adopt(result);
    if (error.has_exception())
      return Unexpected<Error>(error.TakeError());
    if (IsAlive())
      return Element();  // No match on a live page: success with an empty handle.
    return Unexpected<Error>(Error::PageGone());
  }

  ///
  /// Find every element matching a CSS selector (querySelectorAll).
  ///
  /// The list is a snapshot, so later changes to the page don't change it.
  ///
  /// @param  selectors  A CSS selector list (eg, `.item > a[href]`).
  ///
  /// @return  Returns the matches in document order (an empty list if nothing matches, the
  ///          selector is malformed, or the page is gone).
  ///
  ElementList querySelectorAll(std::string_view selectors) const {
    detail::CString s(selectors);
    return ElementList::Adopt(ulDOMDocumentQuerySelectorAll(handle_, s.c_str(), nullptr));
  }

  ///
  /// Same as querySelectorAll(), but returns a Result with the reason for a failure (eg, a
  /// SyntaxError for a malformed selector).
  ///
  /// @return  Returns the matches in document order (an empty list if nothing matches). Fails
  ///          with a SyntaxError if the selector is malformed.
  ///
  [[nodiscard]] Result<ElementList> querySelectorAll(std::string_view selectors,
                                                     Checked_t) const {
    if (IsEmpty())
      return detail::EmptyHandleError();
    detail::CString s(selectors);
    detail::ErrorScope error;
    ULDOMElementList result = ulDOMDocumentQuerySelectorAll(handle_, s.c_str(), error.out());
    if (result)
      return ElementList::Adopt(result);
    return Unexpected<Error>(error.TakeError());
  }

  ///
  /// Find the topmost element at a point in the viewport (elementFromPoint).
  ///
  /// @param  x  The horizontal position, in CSS pixels from the viewport's left edge.
  ///
  /// @param  y  The vertical position, in CSS pixels from the viewport's top edge.
  ///
  /// @return  Returns the topmost element at that point (empty if the point is outside the
  ///          viewport).
  ///
  /// @note  These are the same coordinates as event client positions and
  ///        Element::getBoundingClientRect(). To convert from View pixels, divide by
  ///        View::device_scale() (an 800x600 View with a device scale of 2.0 is 400x300 CSS
  ///        pixels).
  ///
  Element elementFromPoint(double x, double y) const {
    return Element::Adopt(ulDOMDocumentElementFromPoint(handle_, x, y));
  }

  ///
  /// Find every element at a point in the viewport, topmost first (elementsFromPoint).
  ///
  /// The list starts with the element elementFromPoint() returns and continues with the elements
  /// painted below it. It ends with the root `<html>` element.
  ///
  /// @param  x  The horizontal position, in CSS pixels from the viewport's left edge.
  ///
  /// @param  y  The vertical position, in CSS pixels from the viewport's top edge.
  ///
  /// @return  Returns the elements, topmost first (an empty list if the point is outside the
  ///          viewport).
  ///
  /// @note  These are the same coordinates elementFromPoint() takes. Like that method, this
  ///        updates the layout first if the page has pending changes.
  ///
  ElementList elementsFromPoint(double x, double y) const {
    return ElementList::Adopt(ulDOMDocumentElementsFromPoint(handle_, x, y));
  }

  // --- Well-known elements (read-only properties are calls) --------------------------------

  ///
  /// Get the document's root element (documentElement).
  ///
  /// @return  Returns the root element (empty if there's none). In an HTML document it's the
  ///          `<html>` element.
  ///
  Element documentElement() const {
    return Element::Adopt(ulDOMDocumentGetDocumentElement(handle_));
  }

  ///
  /// Get the document's body element (body).
  ///
  /// @return  Returns the `<body>` element (empty if there's none).
  ///
  Element body() const { return Element::Adopt(ulDOMDocumentGetBody(handle_)); }

  ///
  /// Get the document's head element (head).
  ///
  /// @return  Returns the `<head>` element (empty if there's none).
  ///
  Element head() const { return Element::Adopt(ulDOMDocumentGetHead(handle_)); }

  ///
  /// Get the element that has keyboard focus (activeElement).
  ///
  /// @return  Returns the focused element. When nothing has focus this is the body (empty if
  ///          there's no body).
  ///
  Element activeElement() const {
    return Element::Adopt(ulDOMDocumentGetActiveElement(handle_));
  }

  ///
  /// Whether or not the document has keyboard focus (hasFocus).
  ///
  /// This is true when the View has focus (see View::Focus()) and the focused frame is this
  /// document's frame or one of its subframes.
  ///
  bool hasFocus() const { return ulDOMDocumentHasFocus(handle_); }

  // --- Traversal (read-only properties are calls) -------------------------------------------

  ///
  /// Get the document's first child of any kind (firstChild).
  ///
  /// @return  Returns the first child (eg, the doctype, a comment, or the root element; empty if
  ///          there's none).
  ///
  Node firstChild() const { return Node::Adopt(ulDOMDocumentGetFirstChild(handle_)); }

  ///
  /// Get the document's last child of any kind (lastChild).
  ///
  /// @return  Returns the last child (empty if there's none).
  ///
  Node lastChild() const { return Node::Adopt(ulDOMDocumentGetLastChild(handle_)); }

  ///
  /// Get the document's first child element (firstElementChild).
  ///
  /// @return  Returns the first child element, which is the root element (empty if there's none).
  ///
  Element firstElementChild() const {
    return Element::Adopt(ulDOMDocumentGetFirstElementChild(handle_));
  }

  ///
  /// Get the document's last child element (lastElementChild).
  ///
  /// @return  Returns the last child element, which is the root element (empty if there's none).
  ///
  Element lastElementChild() const {
    return Element::Adopt(ulDOMDocumentGetLastElementChild(handle_));
  }

  ///
  /// Get the number of the document's child elements (childElementCount).
  ///
  /// @return  Returns the number of child elements (1 for a document with a root element).
  ///
  size_t childElementCount() const { return ulDOMDocumentGetChildElementCount(handle_); }

  ///
  /// Get the document's child elements in order (children).
  ///
  /// @return  Returns the child elements (an empty list if there are none).
  ///
  /// @note  Unlike the web's live collection, the list doesn't change when the document does. Call
  ///        this again to see later changes.
  ///
  ElementList children() const {
    return ElementList::Adopt(ulDOMDocumentGetChildren(handle_));
  }

  ///
  /// Get the document's children of every kind in order (childNodes).
  ///
  /// @return  Returns the children, including the doctype and any comments outside the root
  ///          element (an empty list if there are none).
  ///
  /// @note  The list is a snapshot, where the web's childNodes is live. Call this again to see
  ///        later changes.
  ///
  NodeList childNodes() const { return NodeList::Adopt(ulDOMDocumentGetChildNodes(handle_)); }

  ///
  /// Whether or not the document has any children (hasChildNodes).
  ///
  /// @return  Returns true if the document has at least one child node.
  ///
  bool hasChildNodes() const { return ulDOMDocumentHasChildNodes(handle_); }

  ///
  /// Get the document's window (defaultView).
  ///
  /// The window holds the viewport's size and scroll position. It receives the page's `load` and
  /// the viewport's `resize` events.
  ///
  /// @return  Returns the window.
  ///
  /// @note  Include `<Ultralight/dom/Window.h>` (or `<Ultralight/DOM.h>`) to call this.
  ///
  Window defaultView() const;

  ///
  /// Get the document's selection (getSelection).
  ///
  /// The selection is the highlighted text or the caret on the page. It's the same selection
  /// Window::getSelection() returns.
  ///
  /// @return  Returns the selection.
  ///
  /// @note  Include `<Ultralight/dom/Selection.h>` (or `<Ultralight/DOM.h>`) to call this.
  ///
  Selection getSelection() const;

  // --- Creation -----------------------------------------------------------------------------

  ///
  /// Create an element (createElement).
  ///
  /// The new element belongs to this document but isn't in the page yet. Add it with
  /// Element::appendChild() or another insertion method.
  ///
  /// @param  tag_name  The tag name (eg, `div`). In an HTML document it's lowercased.
  ///
  /// @return  Returns the new element (empty if `tag_name` isn't a valid element name).
  ///
  Element createElement(std::string_view tag_name) const {
    detail::CString t(tag_name);
    return Element::Adopt(ulDOMDocumentCreateElement(handle_, t.c_str(), nullptr));
  }

  ///
  /// Create a text node (createTextNode).
  ///
  /// The new node belongs to this document but isn't in the page yet. Add it with
  /// Element::appendChild() or another insertion method. The text is never parsed as markup.
  ///
  /// @param  data  The text.
  ///
  /// @return  Returns the new text node.
  ///
  Node createTextNode(std::string_view data) const {
    return Node::Adopt(
        ulDOMDocumentCreateTextNode(handle_, data.data() ? data.data() : "", data.size()));
  }

  ///
  /// Create a comment node (createComment).
  ///
  /// The new node belongs to this document but isn't in the page yet. Add it with
  /// Element::appendChild() or another insertion method.
  ///
  /// @param  data  The comment's text.
  ///
  /// @return  Returns the new comment node.
  ///
  Node createComment(std::string_view data) const {
    return Node::Adopt(
        ulDOMDocumentCreateComment(handle_, data.data() ? data.data() : "", data.size()));
  }

  ///
  /// Create an empty document fragment (createDocumentFragment).
  ///
  /// Use a fragment to build a group of nodes off the page and insert them in one step (see
  /// DocumentFragment).
  ///
  /// @return  Returns the new fragment.
  ///
  DocumentFragment createDocumentFragment() const {
    return DocumentFragment::Adopt(ulDOMDocumentCreateFragment(handle_));
  }

  ///
  /// Create a range (createRange).
  ///
  /// The range starts collapsed at the start of the document. Move it with Range::setStart() or
  /// Range::selectNodeContents().
  ///
  /// @return  Returns the new range.
  ///
  Range createRange() const { return Range::Adopt(ulDOMDocumentCreateRange(handle_)); }

  // --- Events -------------------------------------------------------------------------------

  ///
  /// Dispatch a synthetic event to the document (dispatchEvent).
  ///
  /// Listeners run before this returns. A bubbling event also reaches window listeners.
  ///
  /// @param  type  The event type (eg, `my-app-ready`).
  ///
  /// @param  init  Whether the event bubbles, can be canceled, and is composed (see EventInit).
  ///
  /// @return  Returns false if a listener canceled the event with Event::preventDefault() (only
  ///          possible when `init.cancelable` is true). Returns true otherwise.
  ///
  bool dispatchEvent(std::string_view type, const EventInit& init = {}) const {
    detail::CString t(type);
    return ulDOMDocumentDispatchEvent(handle_, t.c_str(), detail::EventInitFlags(init),
                                      nullptr);
  }

  ///
  /// Dispatch a synthetic CustomEvent with a string payload to the document.
  ///
  /// Listeners run before this returns. Native listeners read the payload with
  /// CustomEvent::detail(), and page scripts read it as `event.detail`.
  ///
  /// @param  type          The event type.
  ///
  /// @param  event_detail  The payload as UTF-8 text (see Element::dispatchCustomEvent()).
  ///
  /// @param  init          Whether the event bubbles, can be canceled, and is composed (see
  ///                       EventInit).
  ///
  /// @return  Returns false if a listener canceled the event with Event::preventDefault(). Returns
  ///          true otherwise.
  ///
  bool dispatchCustomEvent(std::string_view type, std::string_view event_detail,
                           const EventInit& init = {}) const {
    detail::CString t(type);
    detail::CString d(event_detail);
    return ulDOMDocumentDispatchCustomEvent(handle_, t.c_str(),
                                            detail::EventInitFlags(init), d.c_str(),
                                            nullptr);
  }

  ///
  /// Listen for an event on the document (addEventListener).
  ///
  /// This works like Element::addEventListener().
  ///
  /// @param  type      The event type (eg, `keydown`).
  ///
  /// @param  callback  The function to call for each event. It can take the event or nothing:
  ///                   `[](dom::Event event) { ... }` or `[] { ... }`.
  ///
  /// @param  options   The listener's options (see AddEventListenerOptions).
  ///
  /// @return  Returns a handle for removing the listener, which you can ignore. It's empty when
  ///          nothing was added (the page is gone or `options.signal` is already aborted).
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
                                     return ulDOMDocumentAddEventListener(handle_, t.c_str(),
                                                                          flags, thunk, fn,
                                                                          destroy);
                                   });
  }

  ///
  /// Listen for an event on the document, with options as flags (eg, `dom::Once | dom::Capture`).
  ///
  /// @param  type      The event type.
  ///
  /// @param  callback  The function to call for each event. It can take the event or nothing.
  ///
  /// @param  flags     The listener's options (see EventListenerFlags).
  ///
  /// @return  Returns a handle for removing the listener, which you can ignore (empty when the
  ///          page is gone).
  ///
  template <typename F>
  EventListener addEventListener(std::string_view type, F&& callback, EventListenerFlags flags) const {
    return addEventListener(type, std::forward<F>(callback), detail::ToOptions(flags));
  }

  ///
  /// Listen for an event on the document by calling a member function of an object.
  ///
  /// @param  type      The event type.
  ///
  /// @param  receiver  The object to call.
  ///
  /// @param  method    The member function to call. It can take the event or nothing.
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
  EventListener addEventListener(std::string_view type, C& receiver, M method,
                                 const AddEventListenerOptions& options = {}) const {
    return addEventListener(type, detail::WrapBorrowedMember(receiver, method), options);
  }

  ///
  /// Listen for an event on the document by calling a member function of an object, with
  /// options as flags.
  ///
  /// @param  type      The event type.
  ///
  /// @param  receiver  The object to call.
  ///
  /// @param  method    The member function to call. It can take the event or nothing.
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
  /// Listen for an event on the document by calling a member function through a smart pointer.
  ///
  /// A std::shared_ptr keeps the object alive for as long as the listener exists. A std::weak_ptr
  /// doesn't, and events are skipped once the object is gone.
  ///
  /// @param  type     The event type.
  ///
  /// @param  holder   The smart pointer (a std::shared_ptr, a std::weak_ptr, or any type with a
  ///                  HolderTraits specialization).
  ///
  /// @param  method   The member function to call. It can take the event or nothing.
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
  /// Listen for an event on the document by calling a member function through a smart pointer,
  /// with options as flags.
  ///
  /// @param  type    The event type.
  ///
  /// @param  holder  The smart pointer (a std::shared_ptr, a std::weak_ptr, or any type with a
  ///                 HolderTraits specialization).
  ///
  /// @param  method  The member function to call. It can take the event or nothing.
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
  /// Listen for an event on every element that matches a CSS selector, including elements added
  /// later (event delegation from the document).
  ///
  /// Works like Element::On().
  ///
  /// @param  selector  A CSS selector (eg, `.row button`). The callback runs when the event's
  ///                   target or one of its ancestors matches it. A malformed selector adds
  ///                   nothing and logs a warning.
  ///
  /// @param  type      The event type.
  ///
  /// @param  callback  The function to call for each matching event. It can take the event and
  ///                   the matching element, only the matching element, or nothing.
  ///
  /// @param  options   The listener's options (see AddEventListenerOptions).
  ///
  /// @return  Returns a handle for removing the listener, which you can ignore. It's empty when
  ///          nothing was added (the page is gone or `options.signal` is already aborted).
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
                                     return ulDOMDocumentAddDelegatedEventListener(
                                         handle_, sel.c_str(), t.c_str(), flags, thunk, fn,
                                         destroy);
                                   });
  }

  ///
  /// Listen for an event on every element that matches a CSS selector, with options as flags.
  ///
  /// @param  selector  A CSS selector the event's target or one of its ancestors must match.
  ///
  /// @param  type      The event type.
  ///
  /// @param  callback  The function to call for each matching event. It can take the event and
  ///                   the matching element, only the matching element, or nothing.
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
  /// @param  selector  A CSS selector the event's target or one of its ancestors must match.
  ///
  /// @param  type      The event type.
  ///
  /// @param  callback  The function to call for each matching event. It can take the event and
  ///                   the matching element, only the matching element, or nothing.
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
  /// @param  selector  A CSS selector the event's target or one of its ancestors must match.
  ///
  /// @param  type      The event type.
  ///
  /// @param  callback  The function to call for each matching event. It can take the event and
  ///                   the matching element, only the matching element, or nothing.
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
  /// Listen for an event on every element that matches a CSS selector by calling a member
  /// function of an object.
  ///
  /// @param  selector  A CSS selector the event's target or one of its ancestors must match.
  ///
  /// @param  type      The event type.
  ///
  /// @param  receiver  The object to call.
  ///
  /// @param  method    The member function to call. It can take the event and the matching
  ///                   element, only the matching element, or nothing.
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
  /// Listen for an event on every element that matches a CSS selector by calling a member
  /// function of an object, with options as flags.
  ///
  /// @param  selector  A CSS selector the event's target or one of its ancestors must match.
  ///
  /// @param  type      The event type.
  ///
  /// @param  receiver  The object to call.
  ///
  /// @param  method    The member function to call. It can take the event and the matching
  ///                   element, only the matching element, or nothing.
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
  /// Listen for an event on every element that matches a CSS selector by calling a member
  /// function through a smart pointer.
  ///
  /// A std::shared_ptr keeps the object alive for as long as the listener exists. A std::weak_ptr
  /// doesn't, and events are skipped once the object is gone.
  ///
  /// @param  selector  A CSS selector the event's target or one of its ancestors must match.
  ///
  /// @param  type      The event type.
  ///
  /// @param  holder    The smart pointer (a std::shared_ptr, a std::weak_ptr, or any type with a
  ///                   HolderTraits specialization).
  ///
  /// @param  method    The member function to call. It can take the event and the matching
  ///                   element, only the matching element, or nothing.
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
  /// Listen for an event on every element that matches a CSS selector by calling a member
  /// function through a smart pointer, with options as flags.
  ///
  /// @param  selector  A CSS selector the event's target or one of its ancestors must match.
  ///
  /// @param  type      The event type.
  ///
  /// @param  holder    The smart pointer (a std::shared_ptr, a std::weak_ptr, or any type with a
  ///                   HolderTraits specialization).
  ///
  /// @param  method    The member function to call. It can take the event and the matching
  ///                   element, only the matching element, or nothing.
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

  // --- Interop with the C API (most embedders never touch raw handles) -----------------------

  ///
  /// Wrap a C handle you own, taking ownership of it.
  ///
  /// @param  handle  A handle from the C API that you would otherwise destroy with
  ///                 ulDestroyDOMDocument() (NULL gives an empty Document).
  ///
  /// @return  Returns a Document that destroys `handle` when it's done.
  ///
  static Document Adopt(ULDOMDocument handle) { return Document(handle); }

  ///
  /// Wrap a C handle the library owns (eg, a callback argument), adding a reference.
  ///
  /// @param  handle  The borrowed handle (NULL gives an empty Document).
  ///
  /// @return  Returns a Document with its own reference, so you can keep it after the callback.
  ///
  static Document FromBorrowed(ULDOMDocument handle) {
    return Document(handle ? ulCreateDOMDocumentRef(handle) : nullptr);
  }

  ///
  /// Get the C handle, for passing to the `<Ultralight/CAPI/CAPI_DOMDocument.h>` functions.
  ///
  /// @return  Returns the handle (NULL for an empty Document). This Document still owns it, so
  ///          don't destroy it.
  ///
  ULDOMDocument raw() const { return handle_; }

  ///
  /// Give up ownership of the C handle and return it. This Document becomes empty.
  ///
  /// @return  Returns the handle. You must call ulDestroyDOMDocument() when finished.
  ///
  ULDOMDocument LeakRef() {
    ULDOMDocument handle = handle_;
    handle_ = nullptr;
    return handle;
  }

 protected:
  explicit Document(ULDOMDocument handle) : handle_(handle) {}

 private:
  ULDOMDocument handle_ = nullptr;
};

/// \cond INTERNAL
namespace detail {

// Document mixes public proxies with its private handle, so it isn't standard-layout and
// offsetof on it is conditionally supported (the same arrangement as Element; see
// PropertiesImpl.h).
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif

inline ULDOMDocument OwnerDocumentHandle(const void* member, size_t member_offset) {
  const Document* document = reinterpret_cast<const Document*>(
      reinterpret_cast<const char*>(member) - member_offset);
  return document->raw();
}

template <>
inline size_t DocumentPropOffset<DocumentTitleTag>() {
  return offsetof(Document, title);
}

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

template <class Tag>
void DocumentStringProp<Tag>::operator=(std::string_view value) const {
  Tag::Set(OwnerDocumentHandle(this, DocumentPropOffset<Tag>()), value);
}

template <class Tag>
void DocumentStringProp<Tag>::operator+=(std::string_view suffix) const {
  ULDOMDocument h = OwnerDocumentHandle(this, DocumentPropOffset<Tag>());
  std::string combined = Tag::Get(h);
  combined.append(suffix);
  Tag::Set(h, combined);
}

template <class Tag>
DocumentStringProp<Tag>::operator std::string() const {
  return Tag::Get(OwnerDocumentHandle(this, DocumentPropOffset<Tag>()));
}

}  // namespace detail
/// \endcond

inline Document Element::ownerDocument() const {
  return Document::Adopt(ulDOMElementGetDocument(raw()));
}

inline Document Node::ownerDocument() const {
  return Document::Adopt(ulDOMNodeGetOwnerDocument(detail_.handle));
}

inline Document HTMLIFrameElement::contentDocument() const {
  return Document::Adopt(ulDOMElementGetContentDocument(raw()));
}

}  // namespace dom
}  // namespace ultralight
