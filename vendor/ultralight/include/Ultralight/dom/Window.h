///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/CAPI/CAPI_DOMWindow.h>
#include <Ultralight/dom/ComputedStyle.h>
#include <Ultralight/dom/Document.h>
#include <Ultralight/dom/Holders.h>
#include <Ultralight/dom/detail/Receivers.h>

#include <string_view>
#include <type_traits>
#include <utility>

namespace ultralight {
namespace dom {

class Selection;

///
/// The viewport of a DOM document.
///
/// A Window is a handle to the visual window that displays a document. You use it to inspect
/// viewport dimensions and to listen for events that fire only on the window, such as `load` and
/// `resize`.
///
/// This example updates a native HUD when the viewport resizes:
///
/// ```
/// dom::Window window = document.defaultView();
/// window.addEventListener("resize", [window] {
///   FitHud(window.innerWidth(), window.innerHeight(),
///          window.devicePixelRatio());
/// });
/// ```
///
/// ## Getting a Window
///
/// You can access a window through the View or a specific document:
///
/// - **Pass a View to Window(view) for the main frame.**
/// - **Call Document::defaultView() on any frame's document.** This returns the window of that
///   specific frame inside the page.
///
/// You should obtain a Window in LoadListener::OnDOMReady() or later. Before your first page loads,
/// a View holds only the frame's initial blank page, and its window goes away when the new page
/// loads.
///
/// ## Viewport Coordinates
///
/// Viewport sizes and scroll positions use CSS pixels. To convert CSS pixels to View pixels,
/// multiply by View::device_scale() (or devicePixelRatio()), or divide View pixels by the scale
/// factor to convert back.
///
/// ## Window Events
///
/// Events such as `load` and `resize` fire on the window only, so document listeners never receive
/// them. Viewport `scroll` events fire on the document first and bubble up to the window.
///
/// Listeners receive `resize` and `scroll` events when the View next paints, rather than during the
/// call that caused the change.
///
/// A `load` listener added in LoadListener::OnDOMReady() still receives the page's `load` event,
/// which fires once all external resources finish loading.
///
/// @note  Like other DOM handles, a Window is Valid, Empty, or Gone, and never keeps its page alive
///        (see dom::Element).
///
/// @see  dom::Document::defaultView(), LoadListener::OnDOMReady(),
///       dom::Element::addEventListener(), View::device_scale()
///
class Window {
 public:
  ///
  /// Create an empty Window.
  ///
  Window() = default;

  ///
  /// Get the window of a View's main frame.
  ///
  /// To get the window of a frame inside the page, call Document::defaultView() on that frame's
  /// document.
  ///
  /// @param  view  The View to get the window from (nullptr gives an empty Window).
  ///
  /// @note  Before your first page commits, this is the window of the frame's initial blank page,
  ///        and it goes away when your page commits. Get it in LoadListener::OnDOMReady() or later.
  ///
  explicit Window(View* view) : Window(Document(view).defaultView()) {}

  ///
  /// Copy constructor (both handles refer to the same window).
  ///
  /// @param  other  The Window to copy.
  ///
  Window(const Window& other)
      : handle_(other.handle_ ? ulCreateDOMWindowRef(other.handle_) : nullptr) {}

  ///
  /// Move constructor (`other` becomes empty).
  ///
  /// @param  other  The Window to move from.
  ///
  Window(Window&& other) noexcept : handle_(other.handle_) { other.handle_ = nullptr; }

  ///
  /// Assignment (copies or moves).
  ///
  /// @param  other  The Window to assign from.
  ///
  /// @return  Returns this Window.
  ///
  Window& operator=(Window other) noexcept {
    std::swap(handle_, other.handle_);
    return *this;
  }

  ///
  /// Destroy this handle (the window itself isn't affected).
  ///
  ~Window() { ulDestroyDOMWindow(handle_); }

  ///
  /// Whether or not this Window is valid (see IsAlive()).
  ///
  explicit operator bool() const { return IsAlive(); }

  ///
  /// Whether or not this Window is empty (it holds no handle).
  ///
  bool IsEmpty() const { return handle_ == nullptr; }

  ///
  /// Whether or not this Window is valid (it isn't empty and its page is still alive).
  ///
  /// @note  Safe to call from any thread.
  ///
  bool IsAlive() const { return handle_ && ulDOMWindowIsAlive(handle_); }

  // --- Viewport metrics (read-only properties are calls) -------------------------------------

  ///
  /// Get the viewport width (innerWidth).
  ///
  /// @return  Returns the width in CSS pixels, including the vertical scrollbar if there is one.
  ///
  int innerWidth() const { return ulDOMWindowGetInnerWidth(handle_); }

  ///
  /// Get the viewport height (innerHeight).
  ///
  /// @return  Returns the height in CSS pixels, including the horizontal scrollbar if there is
  ///          one.
  ///
  int innerHeight() const { return ulDOMWindowGetInnerHeight(handle_); }

  ///
  /// Get the ratio of device pixels to CSS pixels (devicePixelRatio).
  ///
  /// @return  Returns the View's device scale (see View::device_scale()).
  ///
  double devicePixelRatio() const { return ulDOMWindowGetDevicePixelRatio(handle_); }

  ///
  /// Get the viewport's horizontal scroll position (scrollX).
  ///
  /// @return  Returns the position in CSS pixels (the library currently reports whole pixels).
  ///
  double scrollX() const { return ulDOMWindowGetScrollX(handle_); }

  ///
  /// Get the viewport's vertical scroll position (scrollY).
  ///
  /// @return  Returns the position in CSS pixels (the library currently reports whole pixels).
  ///
  double scrollY() const { return ulDOMWindowGetScrollY(handle_); }

  // --- Scrolling ------------------------------------------------------------------------------

  ///
  /// Scroll the viewport to a position (scrollTo).
  ///
  /// The page scrolls at once (CSS `scroll-behavior` is ignored), and the position stays within the
  /// scrollable area.
  ///
  /// @param  x  The horizontal position in CSS pixels (any fraction is dropped).
  ///
  /// @param  y  The vertical position in CSS pixels (any fraction is dropped).
  ///
  void scrollTo(double x, double y) const { ulDOMWindowScrollTo(handle_, x, y); }

  ///
  /// Scroll the viewport by an offset (scrollBy).
  ///
  /// The page scrolls the same way as with scrollTo().
  ///
  /// @param  x  The horizontal offset in CSS pixels.
  ///
  /// @param  y  The vertical offset in CSS pixels.
  ///
  void scrollBy(double x, double y) const { ulDOMWindowScrollBy(handle_, x, y); }

  // --- Computed styles --------------------------------------------------------------------------

  ///
  /// Get an element's computed style (getComputedStyle).
  ///
  /// This is the same as dom::getComputedStyle(), which you can call without a Window.
  ///
  /// @param  element  The element to read.
  ///
  /// @return  Returns a read-only view of the element's computed style (see ComputedStyle).
  ///
  ComputedStyle getComputedStyle(const Element& element) const {
    return dom::getComputedStyle(element);
  }

  // --- Document and selection -------------------------------------------------------------------

  ///
  /// Get the window's document (document).
  ///
  /// @return  Returns the document (empty if this Window is empty).
  ///
  Document document() const { return Document::Adopt(ulDOMWindowGetDocument(handle_)); }

  ///
  /// Get the page's selection (getSelection), the caret or the highlighted text.
  ///
  /// @return  Returns the selection of this window's document (the same one
  ///          Document::getSelection() returns).
  ///
  /// @note  Include `<Ultralight/dom/Selection.h>` (or `<Ultralight/DOM.h>`) to use this.
  ///
  Selection getSelection() const;

  // --- Events -----------------------------------------------------------------------------------

  ///
  /// Dispatch a synthetic event to the window (dispatchEvent).
  ///
  /// This works like Element::dispatchEvent().
  ///
  /// @param  type  The event type to dispatch.
  ///
  /// @param  init  The event's bubbles, cancelable, and composed flags (see EventInit).
  ///
  /// @return  Returns false if a listener canceled the event with Event::preventDefault() (which
  ///          needs `init.cancelable`), or true otherwise.
  ///
  bool dispatchEvent(std::string_view type, const EventInit& init = {}) const {
    detail::CString t(type);
    return ulDOMWindowDispatchEvent(handle_, t.c_str(), detail::EventInitFlags(init),
                                    nullptr);
  }

  ///
  /// Dispatch a synthetic CustomEvent with a string payload to the window.
  ///
  /// This works like Element::dispatchCustomEvent().
  ///
  /// @param  type          The event type to dispatch.
  ///
  /// @param  event_detail  The payload as UTF-8 text, which listeners read as the event's `detail`
  ///                       (see Element::dispatchCustomEvent()).
  ///
  /// @param  init          The event's bubbles, cancelable, and composed flags (see EventInit).
  ///
  /// @return  Returns false if a listener canceled the event with Event::preventDefault() (which
  ///          needs `init.cancelable`), or true otherwise.
  ///
  bool dispatchCustomEvent(std::string_view type, std::string_view event_detail,
                           const EventInit& init = {}) const {
    detail::CString t(type);
    detail::CString d(event_detail);
    return ulDOMWindowDispatchCustomEvent(handle_, t.c_str(),
                                          detail::EventInitFlags(init), d.c_str(),
                                          nullptr);
  }

  ///
  /// Listen for an event on the window (addEventListener).
  ///
  /// This works like Element::addEventListener(). The window receives `load`, `resize`, and
  /// viewport `scroll` events (see the Window class).
  ///
  /// @param  type      The event type to listen for (eg, `resize`).
  ///
  /// @param  callback  The callable to run on each event, taking (dom::Event) or ().
  ///
  /// @param  options   The listener's options (see AddEventListenerOptions).
  ///
  /// @return  Returns a handle for removing the listener, which you can ignore. It's empty when
  ///          nothing was added (the page is gone or `options.signal` is already aborted).
  ///
  /// @note  A `load` or viewport `scroll` event reports the document as its target, so
  ///        Event::target() reads empty.
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
                                     return ulDOMWindowAddEventListener(handle_, t.c_str(), flags,
                                                                        thunk, fn, destroy);
                                   });
  }

  ///
  /// Listen for an event on the window, with options as flags (eg, `dom::Once | dom::Capture`).
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
  EventListener addEventListener(std::string_view type, F&& callback, EventListenerFlags flags) const {
    return addEventListener(type, std::forward<F>(callback), detail::ToOptions(flags));
  }

  ///
  /// Listen by calling a member function on an object you keep alive.
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
  /// @param  type     The event type to listen for.
  ///
  /// @param  holder   A shared_ptr, a weak_ptr, or another smart pointer with a HolderTraits
  ///                  specialization (see LockableHolder). While it's expired, events are skipped.
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
  /// @param  holder  A shared_ptr, a weak_ptr, or another smart pointer with a HolderTraits
  ///                 specialization (see LockableHolder). While it's expired, events are skipped.
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

  // --- Interop with the C API (most embedders never touch raw handles) -----------------------

  ///
  /// Wrap a C handle you own, taking ownership of it.
  ///
  /// @param  handle  A handle from the C API that you would otherwise destroy with
  ///                 ulDestroyDOMWindow() (NULL gives an empty Window).
  ///
  /// @return  Returns a Window that destroys `handle` when it's done.
  ///
  static Window Adopt(ULDOMWindow handle) { return Window(handle); }

  ///
  /// Wrap a C handle the library owns, adding a reference.
  ///
  /// @param  handle  The borrowed handle (NULL gives an empty Window).
  ///
  /// @return  Returns a Window with its own reference.
  ///
  static Window FromBorrowed(ULDOMWindow handle) {
    return Window(handle ? ulCreateDOMWindowRef(handle) : nullptr);
  }

  ///
  /// Get the C handle, for passing to the `<Ultralight/CAPI/CAPI_DOMWindow.h>` functions.
  ///
  /// @return  Returns the handle (NULL for an empty Window). This Window still owns it, so don't
  ///          destroy it.
  ///
  ULDOMWindow raw() const { return handle_; }

  ///
  /// Give up ownership of the C handle and return it. This Window becomes empty.
  ///
  /// @return  Returns the handle. You must call ulDestroyDOMWindow() when finished.
  ///
  ULDOMWindow LeakRef() {
    ULDOMWindow handle = handle_;
    handle_ = nullptr;
    return handle;
  }

 protected:
  explicit Window(ULDOMWindow handle) : handle_(handle) {}

 private:
  ULDOMWindow handle_ = nullptr;
};

inline Window Document::defaultView() const {
  return Window::Adopt(ulDOMDocumentGetWindow(handle_));
}

}  // namespace dom
}  // namespace ultralight
