///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// @file CAPI_DOMWindow.h
///
/// The viewport of a DOM document.
///
/// `#include <Ultralight/CAPI/CAPI_DOMWindow.h>`
///
/// @note  This API is a preview and may still change after 2.0.
///
/// A ULDOMWindow handle represents the visual window that displays a document. You use it to
/// inspect viewport dimensions and listen for window-level events.
///
/// Every window handle you retrieve is yours to destroy with ulDestroyDOMWindow() when you're
/// finished.
///
/// This example updates a native HUD when the viewport resizes:
///
/// ```c
/// #include <Ultralight/CAPI/CAPI_DOMWindow.h>
///
/// static void OnResize(void* user_data, ULDOMEvent event) {
///   (void)event;
///   ULDOMWindow window = user_data;
///   FitHud(ulDOMWindowGetInnerWidth(window), ulDOMWindowGetInnerHeight(window),
///          ulDOMWindowGetDevicePixelRatio(window));
/// }
///
/// static void ReleaseWindow(void* user_data) {
///   ulDestroyDOMWindow(user_data);
/// }
///
/// void WatchViewport(ULDOMDocument document) {
///   ULDOMWindow window = ulDOMDocumentGetWindow(document);
///
///   // The listener owns the window handle and destroys it when it's done.
///   ULDOMEventListener listener = ulDOMWindowAddEventListener(
///       window, "resize", kULDOMEventFlags_None, OnResize, window, ReleaseWindow);
///   ulDestroyDOMEventListener(listener);  // the listener keeps running
/// }
/// ```
///
/// ## Getting a Window
///
/// Call ulDOMDocumentGetWindow() on a document to retrieve its window handle:
///
/// - **Pass the main frame's document for the top-level viewport.** You can obtain this document
///   with ulViewGetDOMDocument().
/// - **Pass a frame's document for an embedded viewport.** Calling ulDOMElementGetContentDocument()
///   on an `<iframe>` or `<frame>` element returns that frame's document.
///
/// You should get a window in the DOM-ready callback (see ulViewSetDOMReadyCallback()) or later.
/// Before your first page loads, a View holds only the frame's initial blank page, and that window
/// goes away when the new page loads.
///
/// ## Viewport Coordinates
///
/// Viewport sizes and scroll positions use CSS pixels. To convert CSS pixels to View pixels,
/// multiply by ulDOMWindowGetDevicePixelRatio() (which matches ulViewGetDeviceScale()), or divide
/// View pixels by that scale factor to convert back.
///
/// ## Window Events
///
/// Events such as `load` and `resize` fire on the window only, so document listeners never receive
/// them. Viewport `scroll` events fire on the document first and bubble up to the window.
///
/// Listeners receive `resize` and `scroll` events when the View next paints during ulRender(),
/// rather than during the call that caused the change.
///
/// A `load` listener added in the DOM-ready callback still receives the page's `load` event, which
/// fires once all external resources finish loading.
///
/// For listener handles and user data, see CAPI_DOMEvent.h.
///
/// @note  Like other DOM handles, a window handle is Valid, Empty, or Gone, and never keeps its
///        page alive (see CAPI_DOMDocument.h for rules shared across every C DOM header).
///
/// @see  ulDOMDocumentGetWindow(), ulDOMWindowAddEventListener(), ulViewSetDOMReadyCallback(),
///       ulViewGetDeviceScale()
///
#ifndef ULTRALIGHT_CAPI_DOMWINDOW_H
#define ULTRALIGHT_CAPI_DOMWINDOW_H

#include <Ultralight/CAPI/CAPI_DOMEvent.h>

#ifdef __cplusplus
extern "C" {
#endif

///
/// Opaque handle to a page's window.
///
/// @see  ulDOMDocumentGetWindow(), ulCreateDOMWindowRef(), ulDestroyDOMWindow()
///
typedef struct C_DOMWindow* ULDOMWindow;

///
/// Get the document's window (defaultView).
///
/// @param  document  The document handle.
///
/// @return  Returns a new window handle. You must call ulDestroyDOMWindow() when finished.
///
/// @note  Each call returns a new handle, and there's no function to check whether two window
///        handles refer to the same window.
///
ULExport ULDOMWindow ulDOMDocumentGetWindow(ULDOMDocument document);

///
/// Add a reference to a window handle.
///
/// @param  window  The window handle.
///
/// @return  Returns `window` (NULL if `window` is NULL). You must call ulDestroyDOMWindow() when
///          finished.
///
/// @note  Safe to call from any thread.
///
ULExport ULDOMWindow ulCreateDOMWindowRef(ULDOMWindow window);

///
/// Destroy a window handle (the window itself isn't affected).
///
/// @param  window  The window handle (can be NULL, or its page can be gone).
///
/// @note  Safe to call from any thread.
///
ULExport void ulDestroyDOMWindow(ULDOMWindow window);

///
/// Whether or not a window handle is valid (it isn't NULL and its page is still alive).
///
/// @param  window  The window handle.
///
/// @return  Returns true if the window's page is still alive.
///
/// @note  Safe to call from any thread.
///
ULExport bool ulDOMWindowIsAlive(ULDOMWindow window);

///
/// Get the window's document (document).
///
/// @param  window  The window handle.
///
/// @return  Returns a new document handle. You must call ulDestroyDOMDocument() when finished.
///
ULExport ULDOMDocument ulDOMWindowGetDocument(ULDOMWindow window);

///
/// Get the viewport width (innerWidth).
///
/// @param  window  The window handle.
///
/// @return  Returns the width in CSS pixels, including the vertical scrollbar if there is one.
///
ULExport int ulDOMWindowGetInnerWidth(ULDOMWindow window);

///
/// Get the viewport height (innerHeight).
///
/// @param  window  The window handle.
///
/// @return  Returns the height in CSS pixels, including the horizontal scrollbar if there is one.
///
ULExport int ulDOMWindowGetInnerHeight(ULDOMWindow window);

///
/// Get the ratio of device pixels to CSS pixels (devicePixelRatio).
///
/// @param  window  The window handle.
///
/// @return  Returns the View's device scale (see ulViewGetDeviceScale()).
///
ULExport double ulDOMWindowGetDevicePixelRatio(ULDOMWindow window);

///
/// Get the viewport's horizontal scroll position (scrollX).
///
/// @param  window  The window handle.
///
/// @return  Returns the position in CSS pixels (the library currently reports whole pixels).
///
ULExport double ulDOMWindowGetScrollX(ULDOMWindow window);

///
/// Get the viewport's vertical scroll position (scrollY).
///
/// @param  window  The window handle.
///
/// @return  Returns the position in CSS pixels (the library currently reports whole pixels).
///
ULExport double ulDOMWindowGetScrollY(ULDOMWindow window);

///
/// Scroll the viewport to a position (scrollTo).
///
/// The page scrolls at once (CSS `scroll-behavior` is ignored), and the position stays within the
/// scrollable area.
///
/// @param  window  The window handle.
///
/// @param  x       The horizontal position in CSS pixels (any fraction is dropped).
///
/// @param  y       The vertical position in CSS pixels (any fraction is dropped).
///
ULExport void ulDOMWindowScrollTo(ULDOMWindow window, double x, double y);

///
/// Scroll the viewport by an offset (scrollBy).
///
/// The page scrolls the same way as with ulDOMWindowScrollTo().
///
/// @param  window  The window handle.
///
/// @param  x       The horizontal offset in CSS pixels.
///
/// @param  y       The vertical offset in CSS pixels.
///
ULExport void ulDOMWindowScrollBy(ULDOMWindow window, double x, double y);

///
/// Listen for an event on the window (addEventListener).
///
/// This works like ulDOMElementAddEventListener(). The window receives `load`, `resize`, and
/// viewport `scroll` events (see the Window Events section above).
///
/// @param  window             The window handle.
///
/// @param  type               The event type, as null-terminated UTF-8 (eg, `resize`).
///
/// @param  flags              A combination of ULDOMEventFlags values.
///
/// @param  callback           Called on each event (must not be NULL).
///
/// @param  user_data          Passed to `callback` and `destroy_user_data`.
///
/// @param  destroy_user_data  Called once to free `user_data` (can be NULL). It runs on the
///                            Renderer's thread during a later ulUpdate(), once the listener is
///                            removed and its handle destroyed, or once the page goes away. If
///                            the call fails, it runs before the call returns.
///
/// @return  Returns a new listener handle, or NULL if `type` or `callback` is NULL or the window's
///          page is gone. You must call ulDestroyDOMEventListener() when finished. The listener
///          stays registered until you call ulDOMEventListenerRemove() or the page goes away.
///
/// @note  A `load` or viewport `scroll` event has the document as its target, so
///        ulDOMEventGetTarget() returns NULL.
///
ULExport ULDOMEventListener ulDOMWindowAddEventListener(ULDOMWindow window, const char* type,
                                                       unsigned int flags,
                                                       ULDOMEventCallback callback,
                                                       void* user_data,
                                                       ULUserDataDestroyCallback
                                                           destroy_user_data);

///
/// Dispatch a synthetic event to the window (dispatchEvent).
///
/// This works like ulDOMElementDispatchEvent().
///
/// @param  window      The window handle.
///
/// @param  type        The event type, as null-terminated UTF-8.
///
/// @param  init_flags  A combination of ULDOMEventInitFlags values.
///
/// @param  error       Receives a DOM error (can be NULL). Dispatching to a window never
///                     produces one.
///
/// @return  Returns false if a listener canceled the event with preventDefault() (which needs
///          kULDOMEventInit_Cancelable), or true otherwise.
///
ULExport bool ulDOMWindowDispatchEvent(ULDOMWindow window, const char* type,
                                       unsigned int init_flags, ULDOMError* error);

///
/// Dispatch a synthetic CustomEvent with a string payload to the window.
///
/// This works like ulDOMElementDispatchCustomEvent().
///
/// @param  window      The window handle.
///
/// @param  type        The event type, as null-terminated UTF-8.
///
/// @param  init_flags  A combination of ULDOMEventInitFlags values.
///
/// @param  detail      The payload as null-terminated UTF-8, or NULL for none. Listeners read it
///                     as the event's `detail` (native listeners use
///                     ulDOMEventGetCustomDetail()). Text that isn't valid UTF-8 arrives as no
///                     payload.
///
/// @param  error       Receives a DOM error (can be NULL). Dispatching to a window never
///                     produces one.
///
/// @return  Returns false if a listener canceled the event with preventDefault() (which needs
///          kULDOMEventInit_Cancelable), or true otherwise.
///
ULExport bool ulDOMWindowDispatchCustomEvent(ULDOMWindow window, const char* type,
                                             unsigned int init_flags, const char* detail,
                                             ULDOMError* error);

#ifdef __cplusplus
}
#endif

#endif  // ULTRALIGHT_CAPI_DOMWINDOW_H
