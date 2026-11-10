///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// @file CAPI_DOMEvent.h
///
/// Functions and types for listening to and dispatching DOM events in C.
///
/// `#include <Ultralight/CAPI/CAPI_DOMEvent.h>`
///
/// @note  This API is a preview and may still change after 2.0.
///
/// When actions happen on a page (like a click or key press), the library delivers DOM events to
/// your listener callbacks. They let your application handle user interaction directly in C, even
/// when JavaScript isn't enabled.
///
/// You can also dispatch events to trigger actions on the page. For the threading, handle lifetime,
/// and error rules shared by every C DOM header, refer to `<Ultralight/CAPI/CAPI_DOMDocument.h>`.
///
/// This click listener stores its state in `user_data` and is removed when done:
///
/// ```c
/// #include <Ultralight/CAPI/CAPI_DOMElement.h>
/// #include <Ultralight/CAPI/CAPI_DOMEvent.h>
/// #include <stdlib.h>
///
/// typedef struct {
///   int clicks;
/// } SaveState;
///
/// static ULDOMEventListener save_listener = NULL;
///
/// static void OnSaveClick(void* user_data, ULDOMEvent event) {
///   SaveState* state = user_data;
///   state->clicks++;
///
///   ULDOMElement button = ulDOMEventGetTarget(event);
///   ulDOMElementSetTextContent(button, "Saved", 5);
///   ulDestroyDOMElement(button);
/// }
///
/// void AddSaveListener(ULDOMDocument document) {
///   ULDOMElement save = ulDOMDocumentGetElementById(document, "save");
///   SaveState* state = calloc(1, sizeof(SaveState));
///
///   // The library calls free(state) once it's done with the listener.
///   save_listener = ulDOMElementAddEventListener(
///       save, "click", kULDOMEventFlags_None, OnSaveClick, state, free);
///   ulDestroyDOMElement(save);
/// }
///
/// void RemoveSaveListener(void) {
///   ulDOMEventListenerRemove(save_listener);
///   ulDestroyDOMEventListener(save_listener);
///   save_listener = NULL;
/// }
/// ```
///
/// ## Registering Listeners
///
/// Calling ulDOMElementAddEventListener() or ulDOMDocumentAddEventListener() registers a callback
/// for page events and returns an owned ULDOMEventListener handle. Pass bitwise OR combinations of
/// ULDOMEventFlags (such as `kULDOMEventFlags_Capture` or `kULDOMEventFlags_Once`) to customize
/// dispatch behavior, like capturing the event or removing the listener after its first event.
///
/// Destroying a ULDOMEventListener handle with ulDestroyDOMEventListener() releases only your
/// handle-- it doesn't remove the listener from the page.
///
/// A listener keeps receiving events until stopped:
///
/// - **Calling ulDOMEventListenerRemove() removes the listener.**
/// - **A listener registered with kULDOMEventFlags_Once stops after its first event.**
/// - **Its page goes away.** Navigating away, removing the frame, or destroying the View stops all
///   listeners registered on that page.
///
/// ### Delegated Listeners
///
/// Calling ulDOMElementAddDelegatedEventListener() or ulDOMDocumentAddDelegatedEventListener()
/// registers a single listener for descendant elements matching a CSS selector. The listener
/// matches elements added later too, and passes the nearest matched element to the callback as
/// `matched_target`.
///
/// ## User Data
///
/// Ownership of `user_data` transfers directly to the listener, so you shouldn't free it yourself.
/// If registration fails, the library calls `destroy_user_data` immediately before returning NULL
/// so cleanup doesn't depend on whether registration succeeded.
///
/// The `destroy_user_data` callback runs once on the Renderer's thread in these situations:
///
/// - **After you remove the listener and destroy its handle.** Cleanup runs during a later call to
///   ulUpdate().
/// - **When the page goes away.** Cleanup runs during page teardown (such as navigation or inside
///   ulDestroyView()), even if you haven't called ulDOMEventListenerRemove().
///
/// ## Reading Events
///
/// The ULDOMEvent handle passed to your callback and a delegated listener's `matched_target`
/// element are borrowed. They're valid only while the callback runs, so you must never destroy them
/// or store them across calls. To keep a `matched_target` element longer, call
/// ulCreateDOMElementRef() to obtain an owned handle.
///
/// Handles and strings returned by event inspection functions (such as ulDOMEventGetTarget() or
/// ulDOMEventGetType()) are yours to destroy.
///
/// Calling a getter intended for another kind of event returns fallback values (such as 0, false,
/// or NULL). If one callback handles multiple event types, check ulDOMEventGetType() first.
///
/// ## When Callbacks Run
///
/// Callbacks run synchronously on the Renderer's thread, inside the call that dispatches the event:
///
/// | Event              | When It Runs                                          |
/// |--------------------|-------------------------------------------------------|
/// | Input              | Inside ulViewFireMouseEvent() or ulViewFireKeyEvent() |
/// | Dispatched by you  | Inside ulDOMElementDispatchEvent()                    |
/// | Dispatched by page | During a later ulUpdate()                             |
/// | Scroll and resize  | During ulRender() when the View paints                |
///
/// ## Dispatching Events
///
/// You can dispatch DOM events to an element or document using ulDOMElementDispatchEvent() or
/// ulDOMDocumentDispatchEvent(). Pass bitwise OR combinations of ULDOMEventInitFlags (such as
/// `kULDOMEventInit_Bubbles` or `kULDOMEventInit_Cancelable`) to control whether the event bubbles
/// and whether ulDOMEventPreventDefault() can cancel it.
///
/// To send data to listeners, call ulDOMElementDispatchCustomEvent() or
/// ulDOMDocumentDispatchCustomEvent() with a UTF-8 string payload (pass JSON for structured data).
/// Native listeners read the payload with ulDOMEventGetCustomDetail(), and page scripts read it
/// through `event.detail`.
///
/// @note  Document listeners don't receive `load` or `resize` events (attach window listeners using
///        `<Ultralight/CAPI/CAPI_DOMWindow.h>` instead).
///
/// @see  ulDOMElementAddEventListener(), ulDOMElementAddDelegatedEventListener(),
///       ulDOMEventListenerRemove(), ulDOMElementDispatchCustomEvent()
///
#ifndef ULTRALIGHT_CAPI_DOMEVENT_H
#define ULTRALIGHT_CAPI_DOMEVENT_H

#include <Ultralight/CAPI/CAPI_DOMDocument.h>

#ifdef __cplusplus
extern "C" {
#endif

///
/// Opaque handle to an event being dispatched (valid only during the callback that received it).
///
typedef struct C_DOMEvent* ULDOMEvent;

///
/// Opaque handle to a listener.
///
/// @see  ulDOMElementAddEventListener(), ulDOMEventListenerRemove(), ulDestroyDOMEventListener()
///
typedef struct C_DOMEventListener* ULDOMEventListener;

///
/// The callback invoked for each event a listener receives.
///
/// @param  user_data  The pointer you passed when adding the listener.
///
/// @param  event      The event (valid only during this call).
///
typedef void (*ULDOMEventCallback)(void* user_data, ULDOMEvent event);

///
/// The callback invoked for each event a delegated listener matches.
///
/// @param  user_data       The pointer you passed when adding the listener.
///
/// @param  event           The event (valid only during this call).
///
/// @param  matched_target  The element the selector matched, which is the event's target or its
///                         nearest matching ancestor (valid only during this call).
///
typedef void (*ULDOMDelegatedEventCallback)(void* user_data, ULDOMEvent event,
                                            ULDOMElement matched_target);

///
/// Listener options (combine with bitwise OR).
///
typedef enum {
  kULDOMEventFlags_None = 0,
  /// Receive the event in the capture phase (on its way down to the target).
  kULDOMEventFlags_Capture = 1 << 0,
  /// Remove the listener after its first event.
  kULDOMEventFlags_Once = 1 << 1,
  /// Mark the listener passive, so ulDOMEventPreventDefault() does nothing in it.
  kULDOMEventFlags_Passive = 1 << 2,
  /// Mark the listener explicitly not passive. Wheel listeners on the window, the document,
  /// `<html>`, and `<body>` are passive unless you pass this.
  kULDOMEventFlags_NotPassive = 1 << 3,
} ULDOMEventFlags;

///
/// Options for an event you dispatch yourself (combine with bitwise OR).
///
/// @see  ulDOMElementDispatchEvent()
///
typedef enum {
  kULDOMEventInit_None = 0,
  /// The event bubbles up the tree (bubbles).
  kULDOMEventInit_Bubbles = 1 << 0,
  /// ulDOMEventPreventDefault() can cancel the event (cancelable).
  kULDOMEventInit_Cancelable = 1 << 1,
  /// The event crosses shadow-tree boundaries (composed).
  kULDOMEventInit_Composed = 1 << 2,
} ULDOMEventInitFlags;

///
/// Add a listener for an event on an element (addEventListener).
///
/// @param  element            The element handle.
///
/// @param  type               The event type, as null-terminated UTF-8 (eg, `click`).
///
/// @param  flags              A combination of ULDOMEventFlags values.
///
/// @param  callback           The callback to invoke for each event (must not be NULL).
///
/// @param  user_data          A pointer passed to `callback` and `destroy_user_data`.
///
/// @param  destroy_user_data  The callback invoked once when the library is done with
///                            `user_data` (can be NULL).
///
/// @return  Returns a new listener handle, or NULL if the handle is NULL or its page is gone, or
///          `type` or `callback` is NULL. You must call ulDestroyDOMEventListener() when
///          finished.
///
ULExport ULDOMEventListener ulDOMElementAddEventListener(ULDOMElement element,
                                                        const char* type, unsigned int flags,
                                                        ULDOMEventCallback callback,
                                                        void* user_data,
                                                        ULUserDataDestroyCallback
                                                            destroy_user_data);

///
/// Add a listener for an event on the document (addEventListener).
///
/// With kULDOMEventFlags_Capture, a document listener receives events that don't bubble, like
/// scrolling inside an element. Page `load` goes to window listeners only (see
/// ulDOMWindowAddEventListener()).
///
/// @param  document           The document handle.
///
/// @param  type               The event type, as null-terminated UTF-8 (eg, `click`).
///
/// @param  flags              A combination of ULDOMEventFlags values.
///
/// @param  callback           The callback to invoke for each event (must not be NULL).
///
/// @param  user_data          A pointer passed to `callback` and `destroy_user_data`.
///
/// @param  destroy_user_data  The callback invoked once when the library is done with
///                            `user_data` (can be NULL).
///
/// @return  Returns a new listener handle, or NULL if the handle is NULL or its page is gone, or
///          `type` or `callback` is NULL. You must call ulDestroyDOMEventListener() when
///          finished.
///
/// @note  By the time the DOM-ready callback runs (see ulViewSetDOMReadyCallback()), the page's
///        `DOMContentLoaded` event has already fired, so a listener you add then never receives
///        it.
///
ULExport ULDOMEventListener ulDOMDocumentAddEventListener(ULDOMDocument document,
                                                         const char* type,
                                                         unsigned int flags,
                                                         ULDOMEventCallback callback,
                                                         void* user_data,
                                                         ULUserDataDestroyCallback
                                                             destroy_user_data);

///
/// Add one listener for an event on every element below an element that matches a CSS selector
/// (event delegation).
///
/// The listener covers elements added later too. For each event, the library tests the target
/// and its ancestors (up to and including `element`) against `selector`, and runs your callback
/// with the nearest match. This works for events that bubble, and for the rest with
/// kULDOMEventFlags_Capture.
///
/// @param  element            The element handle.
///
/// @param  selector           The CSS selector list, as null-terminated UTF-8.
///
/// @param  type               The event type, as null-terminated UTF-8 (eg, `click`).
///
/// @param  flags              A combination of ULDOMEventFlags values.
///
/// @param  callback           The callback to invoke for each match (must not be NULL).
///
/// @param  user_data          A pointer passed to `callback` and `destroy_user_data`.
///
/// @param  destroy_user_data  The callback invoked once when the library is done with
///                            `user_data` (can be NULL).
///
/// @return  Returns a new listener handle, or NULL if the handle is NULL or its page is gone,
///          `selector`, `type`, or `callback` is NULL, or `selector` is malformed (the library
///          always logs a warning about a malformed selector). You must call
///          ulDestroyDOMEventListener() when finished.
///
ULExport ULDOMEventListener ulDOMElementAddDelegatedEventListener(
    ULDOMElement element, const char* selector, const char* type, unsigned int flags,
    ULDOMDelegatedEventCallback callback, void* user_data,
    ULUserDataDestroyCallback destroy_user_data);

///
/// Add one listener for an event on every element of a document that matches a CSS selector
/// (event delegation).
///
/// Matching works as described for ulDOMElementAddDelegatedEventListener().
///
/// @param  document           The document handle.
///
/// @param  selector           The CSS selector list, as null-terminated UTF-8.
///
/// @param  type               The event type, as null-terminated UTF-8 (eg, `click`).
///
/// @param  flags              A combination of ULDOMEventFlags values.
///
/// @param  callback           The callback to invoke for each match (must not be NULL).
///
/// @param  user_data          A pointer passed to `callback` and `destroy_user_data`.
///
/// @param  destroy_user_data  The callback invoked once when the library is done with
///                            `user_data` (can be NULL).
///
/// @return  Returns a new listener handle, or NULL if the handle is NULL or its page is gone,
///          `selector`, `type`, or `callback` is NULL, or `selector` is malformed (the library
///          always logs a warning about a malformed selector). You must call
///          ulDestroyDOMEventListener() when finished.
///
ULExport ULDOMEventListener ulDOMDocumentAddDelegatedEventListener(
    ULDOMDocument document, const char* selector, const char* type, unsigned int flags,
    ULDOMDelegatedEventCallback callback, void* user_data,
    ULUserDataDestroyCallback destroy_user_data);

///
/// Report an exception that your callback caught before it could reach the library.
///
/// An exception must never unwind out of a callback into the library. If your language layer
/// catches one at the callback's edge (the C++ API does this for you), call this so the
/// failure isn't silent: the library logs it as an error (at every diagnostics level), and the
/// page's event dispatch continues.
///
/// @param  where    The kind of callback that failed, as a null-terminated UTF-8 string (eg,
///                  `event listener` or `DOM-ready hook`).
///
/// @param  message  The exception's message, as a null-terminated UTF-8 string (can be NULL).
///
/// @note  Safe to call from any thread.
///
ULExport void ulDOMReportCallbackException(const char* where, const char* message);

///
/// Add a reference to a listener handle.
///
/// @param  listener  The listener handle.
///
/// @return  Returns `listener` (NULL if `listener` is NULL). You must call
///          ulDestroyDOMEventListener() when finished.
///
/// @note  Safe to call from any thread.
///
ULExport ULDOMEventListener ulCreateDOMEventListenerRef(ULDOMEventListener listener);

///
/// Destroy a listener handle.
///
/// This doesn't remove the listener. Call ulDOMEventListenerRemove() first to stop it.
///
/// @param  listener  The listener handle (can be NULL, or its page can be gone).
///
/// @note  Safe to call from any thread.
///
ULExport void ulDestroyDOMEventListener(ULDOMEventListener listener);

///
/// Remove a listener so it receives no more events (removeEventListener).
///
/// Calling this more than once, after the page is gone, or from inside the listener's own
/// callback is safe.
///
/// @param  listener  The listener handle.
///
ULExport void ulDOMEventListenerRemove(ULDOMEventListener listener);

///
/// Whether or not a listener can still receive events.
///
/// @param  listener  The listener handle.
///
/// @return  Returns false after ulDOMEventListenerRemove(), after a kULDOMEventFlags_Once
///          listener fires, or once the page is gone.
///
/// @note  Unlike the IsAlive functions, call this only on the Renderer's thread.
///
ULExport bool ulDOMEventListenerIsActive(ULDOMEventListener listener);

///
/// Get the event's type (type).
///
/// @param  event  The event handle.
///
/// @return  Returns the type (eg, `click`). You must call ulDestroyString() when finished.
///
ULExport ULString ulDOMEventGetType(ULDOMEvent event);

///
/// Get the current dispatch phase (eventPhase).
///
/// @param  event  The event handle.
///
/// @return  Returns 1 while capturing, 2 at the target, or 3 while bubbling.
///
ULExport unsigned int ulDOMEventGetPhase(ULDOMEvent event);

///
/// Whether or not the event bubbles up the tree (bubbles).
///
/// @param  event  The event handle.
///
ULExport bool ulDOMEventGetBubbles(ULDOMEvent event);

///
/// Whether or not ulDOMEventPreventDefault() can cancel the event (cancelable).
///
/// @param  event  The event handle.
///
ULExport bool ulDOMEventGetCancelable(ULDOMEvent event);

///
/// Whether or not the event came from user input or the library (isTrusted).
///
/// Events from user input and from the page itself (eg, `load`) are trusted. Events dispatched
/// with ulDOMElementDispatchEvent() or ulDOMElementClick(), or by page script, aren't.
///
/// @param  event  The event handle.
///
ULExport bool ulDOMEventIsTrusted(ULDOMEvent event);

///
/// Get the time the event was created (timeStamp).
///
/// @param  event  The event handle.
///
/// @return  Returns the time in milliseconds, measured from when the page started loading (the
///          same clock as the page's `performance.now()`).
///
ULExport double ulDOMEventGetTimeStamp(ULDOMEvent event);

///
/// Whether or not the event was canceled (defaultPrevented).
///
/// @param  event  The event handle.
///
/// @return  Returns true if a call to ulDOMEventPreventDefault() canceled the event. A call that
///          had no effect (see ulDOMEventPreventDefault()) leaves this false.
///
ULExport bool ulDOMEventGetDefaultPrevented(ULDOMEvent event);

///
/// Cancel the event's default action (preventDefault).
///
/// This stops what the library would do in response to the event (eg, submitting a form,
/// following a link, or toggling a checkbox).
///
/// @param  event  The event handle.
///
/// @note  This does nothing if the event isn't cancelable or the listener is passive. Wheel
///        listeners on the window, the document, `<html>`, and `<body>` are passive unless you
///        add them with kULDOMEventFlags_NotPassive.
///
ULExport void ulDOMEventPreventDefault(ULDOMEvent event);

///
/// Stop the event from reaching other targets (stopPropagation).
///
/// The remaining listeners on the current target still run.
///
/// @param  event  The event handle.
///
ULExport void ulDOMEventStopPropagation(ULDOMEvent event);

///
/// Stop the event from reaching any other listener (stopImmediatePropagation).
///
/// Unlike ulDOMEventStopPropagation(), this also skips the remaining listeners on the current
/// target.
///
/// @param  event  The event handle.
///
ULExport void ulDOMEventStopImmediatePropagation(ULDOMEvent event);

///
/// Get the element the event was dispatched to (target).
///
/// @param  event  The event handle.
///
/// @return  Returns the target, or NULL if it isn't an element (like the document for page
///          `load`). You must call ulDestroyDOMElement() when finished. Use
///          ulDOMEventGetTargetNode() for a target that may be a text node.
///
ULExport ULDOMElement ulDOMEventGetTarget(ULDOMEvent event);

///
/// Get the node the event was dispatched to (target).
///
/// Use this instead of ulDOMEventGetTarget() when the target can be a text node or another node
/// that isn't an element.
///
/// @param  event  The event handle.
///
/// @return  Returns the target, or NULL if it's the document or the window. You must call
///          ulDestroyDOMNode() when finished.
///
ULExport ULDOMNode ulDOMEventGetTargetNode(ULDOMEvent event);

///
/// Get the element whose listener is running (currentTarget).
///
/// @param  event  The event handle.
///
/// @return  Returns the element, or NULL for a listener on the document or the window. You must
///          call ulDestroyDOMElement() when finished.
///
/// @note  In a delegated listener, this is the element you added the listener to, not the
///        matched element (your callback receives that as `matched_target`).
///
ULExport ULDOMElement ulDOMEventGetCurrentTarget(ULDOMEvent event);

///
/// Get the other element of a mouse or focus change (relatedTarget).
///
/// On `mouseover` and `mouseenter` this is the element the pointer left. On `mouseout` and
/// `mouseleave` it's the element the pointer entered. On `focus` and `focusin` it's the element
/// losing focus, and on `blur` and `focusout` the element gaining focus.
///
/// @param  event  The event handle.
///
/// @return  Returns the element, or NULL if there's none. You must call ulDestroyDOMElement()
///          when finished.
///
ULExport ULDOMElement ulDOMEventGetRelatedTarget(ULDOMEvent event);

///
/// Get the click count of a mouse event (detail).
///
/// @param  event  The event handle.
///
/// @return  Returns 1 for a single click, 2 for a double click, and so on (0 for an event
///          without a click count, like `mousemove`).
///
ULExport int ulDOMEventGetDetail(ULDOMEvent event);

///
/// Get the pointer's x-position in the viewport, in CSS pixels (clientX).
///
/// @param  event  The event handle.
///
/// @return  Returns the position (0 for an event without a pointer position).
///
ULExport double ulDOMEventGetClientX(ULDOMEvent event);

///
/// Get the pointer's y-position in the viewport, in CSS pixels (clientY).
///
/// @param  event  The event handle.
///
/// @return  Returns the position (0 for an event without a pointer position).
///
ULExport double ulDOMEventGetClientY(ULDOMEvent event);

///
/// Get the pointer's x-position in the page, in CSS pixels (pageX).
///
/// This is the clientX position plus the page's horizontal scroll position.
///
/// @param  event  The event handle.
///
/// @return  Returns the position (0 for an event without a pointer position).
///
ULExport double ulDOMEventGetPageX(ULDOMEvent event);

///
/// Get the pointer's y-position in the page, in CSS pixels (pageY).
///
/// This is the clientY position plus the page's vertical scroll position.
///
/// @param  event  The event handle.
///
/// @return  Returns the position (0 for an event without a pointer position).
///
ULExport double ulDOMEventGetPageY(ULDOMEvent event);

///
/// Get the pointer's x-position relative to the View (screenX).
///
/// This is the position you passed to ulViewFireMouseEvent() (in the View's logical pixels), not
/// a position on the screen.
///
/// @param  event  The event handle.
///
/// @return  Returns the position (0 for an event without a pointer position).
///
ULExport double ulDOMEventGetScreenX(ULDOMEvent event);

///
/// Get the pointer's y-position relative to the View (screenY).
///
/// This is the position you passed to ulViewFireMouseEvent() (in the View's logical pixels), not
/// a position on the screen.
///
/// @param  event  The event handle.
///
/// @return  Returns the position (0 for an event without a pointer position).
///
ULExport double ulDOMEventGetScreenY(ULDOMEvent event);

///
/// Get the button that caused a mouse event (button).
///
/// @param  event  The event handle.
///
/// @return  Returns 0 for the left button, 1 for middle, 2 for right, 3 for back, or 4 for
///          forward (0 for an event no button caused, like `mousemove` or `wheel`). Returns -1
///          for a `pointermove` without a button change, or for an event that isn't a mouse
///          event.
///
ULExport int ulDOMEventGetButton(ULDOMEvent event);

///
/// Get the buttons held down during a mouse event (buttons).
///
/// @param  event  The event handle.
///
/// @return  Returns a bitmask of 1 for left, 2 for right, 4 for middle, 8 for back, and 16 for
///          forward (always 0 for `wheel`).
///
ULExport unsigned int ulDOMEventGetButtons(ULDOMEvent event);

///
/// Whether or not the Control key was held down (ctrlKey).
///
/// @param  event  The event handle.
///
/// @note  In mouse and wheel events from ulViewFireMouseEvent() and ulViewFireScrollEvent(), the
///        modifier keys (this and ulDOMEventGetShiftKey(), ulDOMEventGetAltKey(), and
///        ulDOMEventGetMetaKey()) are the ones held in the last key event you passed to
///        ulViewFireKeyEvent(), since mouse and scroll events carry no modifier state.
///
ULExport bool ulDOMEventGetCtrlKey(ULDOMEvent event);

///
/// Whether or not the Shift key was held down (shiftKey).
///
/// @param  event  The event handle.
///
ULExport bool ulDOMEventGetShiftKey(ULDOMEvent event);

///
/// Whether or not the Alt key was held down (altKey).
///
/// @param  event  The event handle.
///
ULExport bool ulDOMEventGetAltKey(ULDOMEvent event);

///
/// Whether or not the Meta key was held down (metaKey).
///
/// @param  event  The event handle.
///
ULExport bool ulDOMEventGetMetaKey(ULDOMEvent event);

///
/// Get the horizontal scroll amount of a wheel event (deltaX).
///
/// @param  event  The event handle.
///
/// @return  Returns the amount, in the unit ulDOMEventGetDeltaMode() gives.
///
ULExport double ulDOMEventGetDeltaX(ULDOMEvent event);

///
/// Get the vertical scroll amount of a wheel event (deltaY).
///
/// @param  event  The event handle.
///
/// @return  Returns the amount, in the unit ulDOMEventGetDeltaMode() gives.
///
ULExport double ulDOMEventGetDeltaY(ULDOMEvent event);

///
/// Get the unit of a wheel event's scroll amounts (deltaMode).
///
/// @param  event  The event handle.
///
/// @return  Returns 0 for pixels, 1 for lines, or 2 for pages. Wheel events from
///          ulViewFireScrollEvent() are in pixels or pages (kScrollEventType_ScrollByPixel or
///          kScrollEventType_ScrollByPage).
///
ULExport unsigned int ulDOMEventGetDeltaMode(ULDOMEvent event);

///
/// Get the key's value of a keyboard event (key).
///
/// @param  event  The event handle.
///
/// @return  Returns the value (eg, `a`, `Enter`, or `ArrowLeft`). You must call ulDestroyString()
///          when finished.
///
ULExport ULString ulDOMEventGetKey(ULDOMEvent event);

///
/// Get the physical key of a keyboard event (code).
///
/// @param  event  The event handle.
///
/// @return  Returns the physical key (eg, `KeyA` or `Numpad1`). Returns an empty string if it
///          isn't known. You must call ulDestroyString() when finished.
///
/// @note  The physical key is known only for a key event created with ulCreateKeyEventWindows().
///        For other key events this is empty, so use ulDOMEventGetKey() in code that must work
///        on every platform.
///
ULExport ULString ulDOMEventGetCode(ULDOMEvent event);

///
/// Get the key's location on the keyboard (location).
///
/// @param  event  The event handle.
///
/// @return  Returns 0 for a standard key, 1 for the left key of a pair (eg, left Shift), 2 for
///          the right one, or 3 for the numeric keypad.
///
ULExport unsigned int ulDOMEventGetKeyLocation(ULDOMEvent event);

///
/// Whether or not the key is held down and repeating (repeat).
///
/// @param  event  The event handle.
///
ULExport bool ulDOMEventGetRepeat(ULDOMEvent event);

///
/// Whether or not a keyboard event happened during IME composition (isComposing).
///
/// @param  event  The event handle.
///
ULExport bool ulDOMEventIsComposing(ULDOMEvent event);

///
/// Get the kind of edit of an `input` event (inputType).
///
/// @param  event  The event handle.
///
/// @return  Returns the kind of edit (eg, `insertText` or `deleteContentBackward`). Returns NULL
///          if the event isn't an InputEvent. You must call ulDestroyString() when finished.
///
/// @note  Changing a checkbox, a radio button, or a `<select>` fires a plain `input` event, so
///        this returns NULL for it.
///
ULExport ULString ulDOMEventGetInputType(ULDOMEvent event);

///
/// Get the text an `input` event inserted (data).
///
/// @param  event  The event handle.
///
/// @return  Returns the text (NULL if the edit inserted none, eg, a deletion). You must call
///          ulDestroyString() when finished.
///
ULExport ULString ulDOMEventGetData(ULDOMEvent event);

///
/// Get the button that submitted a form (SubmitEvent.submitter).
///
/// @param  event  The event handle.
///
/// @return  Returns the button, or NULL if no button submitted the form (eg,
///          ulDOMElementRequestSubmit() without one) or the event isn't a `submit` event. You
///          must call ulDestroyDOMElement() when finished.
///
ULExport ULDOMElement ulDOMEventGetSubmitter(ULDOMEvent event);

///
/// Dispatch an event you create to an element (dispatchEvent).
///
/// Listeners (yours and page script's) run before this returns. The event isn't trusted (see
/// ulDOMEventIsTrusted()).
///
/// The library's default action for the event type still runs, so a `click` you dispatch follows
/// a link-- but it doesn't toggle a checkbox. Use ulDOMElementClick() to click an element the
/// way a user would.
///
/// @param  element     The element handle.
///
/// @param  type        The event type, as null-terminated UTF-8 (can't be empty).
///
/// @param  init_flags  A combination of ULDOMEventInitFlags values.
///
/// @param  error       Reserved for DOM errors (can be NULL). This function never reports one.
///
/// @return  Returns false if a listener canceled the event (only possible with
///          kULDOMEventInit_Cancelable). Returns true otherwise, including when the element's
///          page is gone.
///
ULExport bool ulDOMElementDispatchEvent(ULDOMElement element, const char* type,
                                        unsigned int init_flags, ULDOMError* error);

///
/// Dispatch a CustomEvent with a string payload to an element.
///
/// Works like ulDOMElementDispatchEvent(). Native listeners read the payload with
/// ulDOMEventGetCustomDetail(), and page script reads it as `event.detail` (null if `detail` is
/// NULL). To send structured data, pass JSON and parse it in the listener.
///
/// @param  element     The element handle.
///
/// @param  type        The event type, as null-terminated UTF-8 (can't be empty).
///
/// @param  init_flags  A combination of ULDOMEventInitFlags values.
///
/// @param  detail      The payload, as null-terminated UTF-8 (can be NULL for none). Text that
///                     isn't valid UTF-8 arrives as no payload.
///
/// @param  error       Reserved for DOM errors (can be NULL). This function never reports one.
///
/// @return  Returns false if a listener canceled the event (only possible with
///          kULDOMEventInit_Cancelable). Returns true otherwise, including when the element's
///          page is gone.
///
ULExport bool ulDOMElementDispatchCustomEvent(ULDOMElement element, const char* type,
                                              unsigned int init_flags, const char* detail,
                                              ULDOMError* error);

///
/// Dispatch an event you create to the document.
///
/// This works like ulDOMElementDispatchEvent(). An event that bubbles reaches window listeners
/// too.
///
/// @param  document    The document handle.
///
/// @param  type        The event type, as null-terminated UTF-8 (can't be empty).
///
/// @param  init_flags  A combination of ULDOMEventInitFlags values.
///
/// @param  error       Reserved for DOM errors (can be NULL). This function never reports one.
///
/// @return  Returns false if a listener canceled the event (only possible with
///          kULDOMEventInit_Cancelable). Returns true otherwise, including when the document's
///          page is gone.
///
ULExport bool ulDOMDocumentDispatchEvent(ULDOMDocument document, const char* type,
                                         unsigned int init_flags, ULDOMError* error);

///
/// Dispatch a CustomEvent with a string payload to the document.
///
/// This works like ulDOMElementDispatchCustomEvent(). An event that bubbles reaches window
/// listeners too.
///
/// @param  document    The document handle.
///
/// @param  type        The event type, as null-terminated UTF-8 (can't be empty).
///
/// @param  init_flags  A combination of ULDOMEventInitFlags values.
///
/// @param  detail      The payload, as null-terminated UTF-8 (can be NULL for none). Text that
///                     isn't valid UTF-8 arrives as no payload.
///
/// @param  error       Reserved for DOM errors (can be NULL). This function never reports one.
///
/// @return  Returns false if a listener canceled the event (only possible with
///          kULDOMEventInit_Cancelable). Returns true otherwise, including when the document's
///          page is gone.
///
ULExport bool ulDOMDocumentDispatchCustomEvent(ULDOMDocument document, const char* type,
                                               unsigned int init_flags, const char* detail,
                                               ULDOMError* error);

///
/// Get the string payload of a CustomEvent (detail).
///
/// @param  event  The event handle.
///
/// @return  Returns the payload (NULL if the event has none or isn't a CustomEvent). You must
///          call ulDestroyString() when finished.
///
/// @note  Only a payload dispatched from native code is readable here. A `detail` set by page
///        script reads as NULL, so use a JavaScript API function (see
///        `<Ultralight/CAPI/CAPI_JSAPI.h>`) to get data from page script.
///
ULExport ULString ulDOMEventGetCustomDetail(ULDOMEvent event);

#ifdef __cplusplus
}
#endif

#endif  // ULTRALIGHT_CAPI_DOMEVENT_H
