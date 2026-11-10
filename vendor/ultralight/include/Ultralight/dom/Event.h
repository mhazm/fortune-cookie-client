///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/CAPI/CAPI_DOMEvent.h>
#include <Ultralight/dom/detail/Support.h>

#include <optional>
#include <string>

namespace ultralight {
namespace dom {

class Element;
class Node;
class MouseEvent;
class WheelEvent;
class KeyboardEvent;
class FocusEvent;
class InputEvent;
class SubmitEvent;
class CustomEvent;

///
/// Options for an event you dispatch yourself (the web's EventInit).
///
/// @see  Element::dispatchEvent(), Element::dispatchCustomEvent()
///
struct EventInit {
  bool bubbles = false;     ///< Whether or not the event bubbles up the tree.
  bool cancelable = false;  ///< Whether or not Event::preventDefault() can cancel the event.
  bool composed = false;    ///< Whether or not the event crosses shadow-tree boundaries.
};

/// \cond INTERNAL
namespace detail {

inline unsigned EventInitFlags(const EventInit& init) {
  return (init.bubbles ? kULDOMEventInit_Bubbles : 0u)
      | (init.cancelable ? kULDOMEventInit_Cancelable : 0u)
      | (init.composed ? kULDOMEventInit_Composed : 0u);
}

}  // namespace detail
/// \endcond

///
/// A page event (such as a click or key press) received by a listener callback.
///
/// When an action occurs on a page-- like a click or key press-- the library delivers an Event to
/// your listener callback. It works like the JavaScript event object.
///
/// This listener cancels a form submission to handle it in native code:
///
/// ```
/// dom::Element signup = document.getElementById("signup");
/// signup.addEventListener("submit", [](dom::Event event) {
///   event.preventDefault();       // handle the submission natively
///   SaveProfile(event.target());  // a handle you can keep
/// });
/// ```
///
/// ## Event Lifetime
///
/// An Event is valid only during the callback that received it. Listener callbacks take it by
/// value, and you can't copy, move, or store it.
///
/// Pass an Event to helper functions by const reference:
///
/// ```
/// void HandleKey(const dom::Event& event) {
///   Log(event.AsKeyboard().key());
/// }
/// ```
///
/// Handles you get from the event (like target() and currentTarget()) are ordinary DOM handles you
/// can keep after the callback returns.
///
/// ## Typed Event Views
///
/// Each typed view provides access to properties specific to that kind of event:
///
/// | %View        | Fields Added                         |
/// |--------------|--------------------------------------|
/// | AsMouse()    | Position, buttons, and modifier keys |
/// | AsWheel()    | Scroll deltas                        |
/// | AsKeyboard() | Key, code, and modifier keys         |
/// | AsFocus()    | Element gaining or losing focus      |
/// | AsInput()    | Edit kind and inserted text          |
/// | AsSubmit()   | Button that submitted the form       |
/// | AsCustom()   | %String payload                      |
///
/// Calling a view method on an event of a different type returns fallback values rather than
/// failing. If a single callback handles multiple event types, check type() before converting the
/// event.
///
/// This listener checks the key pressed during a keyboard event:
///
/// ```
/// document.addEventListener("keydown", [](dom::Event event) {
///   if (event.AsKeyboard().key() == "Escape")
///     CloseMenu();
/// });
/// ```
///
/// @see  dom::EventListener, dom::Element::addEventListener(), dom::Element::dispatchEvent(),
///       dom::MouseEvent, dom::KeyboardEvent
///
class Event {
 public:
  ///
  /// Wrap an event handle from the C API.
  ///
  /// Listener callbacks already give you an Event, so you rarely need this.
  ///
  /// @param  borrowed  The event handle passed to a C listener callback. The Event is valid only
  ///                   during that callback.
  ///
  explicit Event(ULDOMEvent borrowed) : borrowed_(borrowed) {}

  Event(const Event&) = delete;
  Event& operator=(const Event&) = delete;

  ///
  /// Get the event's type (type).
  ///
  /// @return  Returns the type (eg, `click`).
  ///
  std::string type() const { return detail::TakeString(ulDOMEventGetType(borrowed_)); }

  ///
  /// Get the current dispatch phase (eventPhase).
  ///
  /// @return  Returns 1 while capturing, 2 at the target, or 3 while bubbling.
  ///
  unsigned eventPhase() const { return ulDOMEventGetPhase(borrowed_); }

  ///
  /// Whether or not the event bubbles up the tree (bubbles).
  ///
  bool bubbles() const { return ulDOMEventGetBubbles(borrowed_); }

  ///
  /// Whether or not preventDefault() can cancel the event (cancelable).
  ///
  bool cancelable() const { return ulDOMEventGetCancelable(borrowed_); }

  ///
  /// Whether or not the event came from user input or the library (isTrusted).
  ///
  /// Events from user input and the page (eg, `load`) are trusted. Events dispatched by page
  /// script, or with Element::dispatchEvent() or Element::click(), aren't.
  ///
  bool isTrusted() const { return ulDOMEventIsTrusted(borrowed_); }

  ///
  /// Get the time the event was created (timeStamp).
  ///
  /// @return  Returns the time in milliseconds, measured from when the page started loading (the
  ///          same clock as the page's `performance.now()`).
  ///
  double timeStamp() const { return ulDOMEventGetTimeStamp(borrowed_); }

  ///
  /// Whether or not the event was canceled (defaultPrevented).
  ///
  /// @return  Returns true if a call to preventDefault() canceled the event. A call that had no
  ///          effect (see preventDefault()) leaves this false.
  ///
  bool defaultPrevented() const { return ulDOMEventGetDefaultPrevented(borrowed_); }

  ///
  /// Cancel the event's default action (preventDefault).
  ///
  /// This stops what the library would do in response to the event (like submitting a form,
  /// following a link, or toggling a checkbox).
  ///
  /// @note  This does nothing if the event isn't cancelable or the listener is passive. Wheel
  ///        listeners on the window, the document, `<html>`, and `<body>` are passive unless you
  ///        add them with EventListenerFlags::NotPassive (or AddEventListenerOptions::passive set
  ///        to false).
  ///
  void preventDefault() const { ulDOMEventPreventDefault(borrowed_); }

  ///
  /// Stop the event from reaching other targets (stopPropagation).
  ///
  /// The remaining listeners on the current target still run.
  ///
  void stopPropagation() const { ulDOMEventStopPropagation(borrowed_); }

  ///
  /// Stop the event from reaching any other listener (stopImmediatePropagation).
  ///
  /// Unlike stopPropagation(), this also skips the remaining listeners on the current target.
  ///
  void stopImmediatePropagation() const { ulDOMEventStopImmediatePropagation(borrowed_); }

  ///
  /// Get the element the event was dispatched to (target).
  ///
  /// @return  Returns the target (empty if it isn't an element, like the document for page
  ///          `load`). Use targetNode() for a target that may be a text node.
  ///
  Element target() const;

  ///
  /// Get the node the event was dispatched to (target).
  ///
  /// Use this instead of target() when the target can be a text node or another node that isn't
  /// an element.
  ///
  /// @return  Returns the target (empty if it's the document or the window).
  ///
  Node targetNode() const;

  ///
  /// Get the element whose listener is running (currentTarget).
  ///
  /// @return  Returns the element (empty for a listener on the document or the window).
  ///
  /// @note  In a delegated listener (see Element::On()), this is the element you added the
  ///        listener to, not the matched element.
  ///
  Element currentTarget() const;

  ///
  /// View this event as a MouseEvent.
  ///
  MouseEvent AsMouse() const;

  ///
  /// View this event as a WheelEvent.
  ///
  WheelEvent AsWheel() const;

  ///
  /// View this event as a KeyboardEvent.
  ///
  KeyboardEvent AsKeyboard() const;

  ///
  /// View this event as a FocusEvent.
  ///
  FocusEvent AsFocus() const;

  ///
  /// View this event as an InputEvent.
  ///
  InputEvent AsInput() const;

  ///
  /// View this event as a SubmitEvent.
  ///
  SubmitEvent AsSubmit() const;

  ///
  /// View this event as a CustomEvent.
  ///
  CustomEvent AsCustom() const;

  ///
  /// Get the C handle, for passing to the `<Ultralight/CAPI/CAPI_DOMEvent.h>` functions.
  ///
  /// @return  Returns the handle (valid only during the callback, like this Event).
  ///
  ULDOMEvent raw() const { return borrowed_; }

 private:
  ULDOMEvent borrowed_;
};

///
/// A mouse event (MouseEvent).
///
/// Get one with Event::AsMouse(). It works for mouse events (eg, `click`), pointer events (eg,
/// `pointerdown`), and `wheel` (see WheelEvent).
///
/// @note  In mouse and wheel events from View::FireMouseEvent() and View::FireScrollEvent(), the
///        modifier keys (ctrlKey() and the rest) are the ones held in the last KeyEvent you
///        passed to View::FireKeyEvent(), since mouse and scroll events carry no modifier state.
///
class MouseEvent : public Event {
 public:
  ///
  /// Wrap an event handle from the C API (Event::AsMouse() is the usual way to get one).
  ///
  /// @param  borrowed  The event handle passed to a C listener callback.
  ///
  explicit MouseEvent(ULDOMEvent borrowed) : Event(borrowed) {}

  ///
  /// Get the click count (detail).
  ///
  /// @return  Returns 1 for a single click, 2 for a double click, and so on (0 for an event
  ///          without a click count, like `mousemove`).
  ///
  int detail() const { return ulDOMEventGetDetail(raw()); }

  ///
  /// Get the pointer's x-position in the viewport, in CSS pixels (clientX).
  ///
  /// @return  Returns the position (0 for an event without a pointer position).
  ///
  double clientX() const { return ulDOMEventGetClientX(raw()); }

  ///
  /// Get the pointer's y-position in the viewport, in CSS pixels (clientY).
  ///
  /// @return  Returns the position (0 for an event without a pointer position).
  ///
  double clientY() const { return ulDOMEventGetClientY(raw()); }

  ///
  /// Get the pointer's x-position in the page, in CSS pixels (pageX).
  ///
  /// This is clientX() plus the page's horizontal scroll position.
  ///
  /// @return  Returns the position (0 for an event without a pointer position).
  ///
  double pageX() const { return ulDOMEventGetPageX(raw()); }

  ///
  /// Get the pointer's y-position in the page, in CSS pixels (pageY).
  ///
  /// This is clientY() plus the page's vertical scroll position.
  ///
  /// @return  Returns the position (0 for an event without a pointer position).
  ///
  double pageY() const { return ulDOMEventGetPageY(raw()); }

  ///
  /// Get the pointer's x-position relative to the View (screenX).
  ///
  /// This is the position you passed to View::FireMouseEvent() (in the View's logical pixels),
  /// not a position on the screen.
  ///
  /// @return  Returns the position (0 for an event without a pointer position).
  ///
  double screenX() const { return ulDOMEventGetScreenX(raw()); }

  ///
  /// Get the pointer's y-position relative to the View (screenY).
  ///
  /// This is the position you passed to View::FireMouseEvent() (in the View's logical pixels),
  /// not a position on the screen.
  ///
  /// @return  Returns the position (0 for an event without a pointer position).
  ///
  double screenY() const { return ulDOMEventGetScreenY(raw()); }

  ///
  /// Get the button that caused the event (button).
  ///
  /// @return  Returns 0 for the left button, 1 for middle, 2 for right, 3 for back, or 4 for
  ///          forward (0 for an event no button caused, like `mousemove` or `wheel`). Returns -1
  ///          for a `pointermove` without a button change, or for an event that isn't a mouse
  ///          event.
  ///
  int button() const { return ulDOMEventGetButton(raw()); }

  ///
  /// Get the buttons held down during the event (buttons).
  ///
  /// @return  Returns a bitmask of 1 for left, 2 for right, 4 for middle, 8 for back, and 16 for
  ///          forward (always 0 for `wheel`).
  ///
  unsigned buttons() const { return ulDOMEventGetButtons(raw()); }

  ///
  /// Whether or not the Control key was held down (ctrlKey).
  ///
  bool ctrlKey() const { return ulDOMEventGetCtrlKey(raw()); }

  ///
  /// Whether or not the Shift key was held down (shiftKey).
  ///
  bool shiftKey() const { return ulDOMEventGetShiftKey(raw()); }

  ///
  /// Whether or not the Alt key was held down (altKey).
  ///
  bool altKey() const { return ulDOMEventGetAltKey(raw()); }

  ///
  /// Whether or not the Meta key was held down (metaKey).
  ///
  bool metaKey() const { return ulDOMEventGetMetaKey(raw()); }

  ///
  /// Get the other element of a mouse transition (relatedTarget).
  ///
  /// On `mouseover` and `mouseenter` this is the element the pointer left. On `mouseout` and
  /// `mouseleave` it's the element the pointer entered.
  ///
  /// @return  Returns the element (empty if there's none).
  ///
  Element relatedTarget() const;
};

///
/// A wheel event (WheelEvent).
///
/// Get one with Event::AsWheel(). It adds the scroll amounts to the MouseEvent getters. The
/// position and modifier keys work as for any mouse event, but button() and buttons() always
/// read 0.
///
class WheelEvent : public MouseEvent {
 public:
  ///
  /// Wrap an event handle from the C API (Event::AsWheel() is the usual way to get one).
  ///
  /// @param  borrowed  The event handle passed to a C listener callback.
  ///
  explicit WheelEvent(ULDOMEvent borrowed) : MouseEvent(borrowed) {}

  ///
  /// Get the horizontal scroll amount (deltaX).
  ///
  /// @return  Returns the amount, in the unit deltaMode() gives (0 for an event that isn't a
  ///          wheel event).
  ///
  double deltaX() const { return ulDOMEventGetDeltaX(raw()); }

  ///
  /// Get the vertical scroll amount (deltaY).
  ///
  /// @return  Returns the amount, in the unit deltaMode() gives (0 for an event that isn't a
  ///          wheel event).
  ///
  double deltaY() const { return ulDOMEventGetDeltaY(raw()); }

  ///
  /// Get the unit of the scroll amounts (deltaMode).
  ///
  /// @return  Returns 0 for pixels, 1 for lines, or 2 for pages. Wheel events from
  ///          View::FireScrollEvent() are in pixels or pages (ScrollEvent::kType_ScrollByPixel
  ///          or ScrollEvent::kType_ScrollByPage).
  ///
  unsigned deltaMode() const { return ulDOMEventGetDeltaMode(raw()); }
};

///
/// A keyboard event (KeyboardEvent).
///
/// Get one with Event::AsKeyboard(). It works for `keydown`, `keyup`, and `keypress`.
///
class KeyboardEvent : public Event {
 public:
  ///
  /// Wrap an event handle from the C API (Event::AsKeyboard() is the usual way to get one).
  ///
  /// @param  borrowed  The event handle passed to a C listener callback.
  ///
  explicit KeyboardEvent(ULDOMEvent borrowed) : Event(borrowed) {}

  ///
  /// Get the key's value (key).
  ///
  /// @return  Returns the value (eg, `a`, `Enter`, or `ArrowLeft`). Returns an empty string for
  ///          an event that isn't a keyboard event.
  ///
  std::string key() const { return detail::TakeString(ulDOMEventGetKey(raw())); }

  ///
  /// Get the physical key (code).
  ///
  /// @return  Returns the physical key (eg, `KeyA` or `Numpad1`). Returns an empty string if it
  ///          isn't known.
  ///
  /// @note  The physical key is known only for a KeyEvent you created from a Windows key
  ///        message. For other key events this is empty, so use key() in code that must work on
  ///        every platform.
  ///
  std::string code() const { return detail::TakeString(ulDOMEventGetCode(raw())); }

  ///
  /// Get the key's location on the keyboard (location).
  ///
  /// @return  Returns 0 for a standard key, 1 for the left key of a pair (eg, left Shift), 2 for
  ///          the right one, or 3 for the numeric keypad.
  ///
  unsigned location() const { return ulDOMEventGetKeyLocation(raw()); }

  ///
  /// Whether or not the key is held down and repeating (repeat).
  ///
  bool repeat() const { return ulDOMEventGetRepeat(raw()); }

  ///
  /// Whether or not the event happened during IME composition (isComposing).
  ///
  bool isComposing() const { return ulDOMEventIsComposing(raw()); }

  ///
  /// Whether or not the Control key was held down (ctrlKey).
  ///
  bool ctrlKey() const { return ulDOMEventGetCtrlKey(raw()); }

  ///
  /// Whether or not the Shift key was held down (shiftKey).
  ///
  bool shiftKey() const { return ulDOMEventGetShiftKey(raw()); }

  ///
  /// Whether or not the Alt key was held down (altKey).
  ///
  bool altKey() const { return ulDOMEventGetAltKey(raw()); }

  ///
  /// Whether or not the Meta key was held down (metaKey).
  ///
  bool metaKey() const { return ulDOMEventGetMetaKey(raw()); }
};

///
/// A focus event (FocusEvent).
///
/// Get one with Event::AsFocus(). It adds the other element of the focus change. It works for
/// `focus`, `blur`, `focusin`, and `focusout`.
///
class FocusEvent : public Event {
 public:
  ///
  /// Wrap an event handle from the C API (Event::AsFocus() is the usual way to get one).
  ///
  /// @param  borrowed  The event handle passed to a C listener callback.
  ///
  explicit FocusEvent(ULDOMEvent borrowed) : Event(borrowed) {}

  ///
  /// Get the other element of the focus change (relatedTarget).
  ///
  /// On `focus` and `focusin` this is the element losing focus. On `blur` and `focusout` it's the
  /// element gaining focus.
  ///
  /// @return  Returns the element (empty if there's none).
  ///
  Element relatedTarget() const;
};

///
/// An edit to an editable element (InputEvent).
///
/// Get one with Event::AsInput(). It adds the kind of edit and the inserted text. It works for
/// the `input` events that typing and editing fire.
///
/// @note  Changing a checkbox, a radio button, or a `<select>` fires a plain `input` event, so
///        both getters return nullopt for it.
///
class InputEvent : public Event {
 public:
  ///
  /// Wrap an event handle from the C API (Event::AsInput() is the usual way to get one).
  ///
  /// @param  borrowed  The event handle passed to a C listener callback.
  ///
  explicit InputEvent(ULDOMEvent borrowed) : Event(borrowed) {}

  ///
  /// Get the kind of edit (inputType).
  ///
  /// @return  Returns the kind of edit (eg, `insertText` or `deleteContentBackward`). Returns
  ///          nullopt if the event isn't an InputEvent.
  ///
  std::optional<std::string> inputType() const {
    ULString s = ulDOMEventGetInputType(raw());
    if (!s)
      return std::nullopt;
    return detail::TakeString(s);
  }

  ///
  /// Get the inserted text (data).
  ///
  /// @return  Returns the text (nullopt if the edit inserted none, eg, a deletion).
  ///
  std::optional<std::string> data() const {
    ULString s = ulDOMEventGetData(raw());
    if (!s)
      return std::nullopt;
    return detail::TakeString(s);
  }
};

///
/// A form submission event (SubmitEvent).
///
/// Get one with Event::AsSubmit() in a `submit` listener. It adds the button that submitted the
/// form.
///
class SubmitEvent : public Event {
 public:
  ///
  /// Wrap an event handle from the C API (Event::AsSubmit() is the usual way to get one).
  ///
  /// @param  borrowed  The event handle passed to a C listener callback.
  ///
  explicit SubmitEvent(ULDOMEvent borrowed) : Event(borrowed) {}

  ///
  /// Get the button that submitted the form (submitter).
  ///
  /// @return  Returns the button (empty if no button submitted the form, eg,
  ///          HTMLFormElement::requestSubmit() without one).
  ///
  Element submitter() const;
};

inline MouseEvent Event::AsMouse() const {
  return MouseEvent(borrowed_);
}
inline WheelEvent Event::AsWheel() const {
  return WheelEvent(borrowed_);
}
inline KeyboardEvent Event::AsKeyboard() const {
  return KeyboardEvent(borrowed_);
}
inline FocusEvent Event::AsFocus() const {
  return FocusEvent(borrowed_);
}
inline InputEvent Event::AsInput() const {
  return InputEvent(borrowed_);
}
inline SubmitEvent Event::AsSubmit() const {
  return SubmitEvent(borrowed_);
}

///
/// An event with a string payload (CustomEvent).
///
/// Get one with Event::AsCustom(). Dispatch one from native code with
/// Element::dispatchCustomEvent().
///
class CustomEvent : public Event {
 public:
  ///
  /// Wrap an event handle from the C API (Event::AsCustom() is the usual way to get one).
  ///
  /// @param  borrowed  The event handle passed to a C listener callback.
  ///
  explicit CustomEvent(ULDOMEvent borrowed) : Event(borrowed) {}

  ///
  /// Get the string payload (detail).
  ///
  /// @return  Returns the payload (nullopt if the event has none or isn't a CustomEvent).
  ///
  /// @note  Only a payload dispatched from native code is readable here. A `detail` set by page
  ///        script reads as nullopt, so use a js::API function to get data from page script.
  ///
  std::optional<std::string> detail() const {
    ULString payload = ulDOMEventGetCustomDetail(raw());
    if (!payload)
      return std::nullopt;
    return dom::detail::TakeString(payload);
  }
};

inline CustomEvent Event::AsCustom() const {
  return CustomEvent(borrowed_);
}

/// \cond INTERNAL
namespace detail {

// Detects a listener callable by calling it with a prvalue Event, the way the listener thunks
// do. std::is_invocable would pass an xvalue, which needs the move constructor Event lacks.
template <typename Fn, typename... Rest>
concept InvocableWithEvent = requires(Fn& fn, ULDOMEvent event, Rest&&... rest) {
  fn(Event(event), static_cast<Rest&&>(rest)...);
};

}  // namespace detail
/// \endcond

}  // namespace dom
}  // namespace ultralight
