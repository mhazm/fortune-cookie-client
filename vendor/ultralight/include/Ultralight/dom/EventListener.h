///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
// X11's headers define None as a macro, which would break the None enumerators.
#pragma push_macro("None")
#undef None

#include <Ultralight/CAPI/CAPI_DOMEvent.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace ultralight {
namespace dom {

///
/// Options for adding an event listener, as flags (addEventListener() and On()).
///
/// Combine them with `|`:
///
/// ```
/// el.addEventListener("click", OnClick, dom::Once | dom::Capture);
/// ```
///
/// Passiveness takes two flags because the web has three choices. dom::Passive makes the
/// listener passive (its preventDefault() calls are ignored), dom::NotPassive makes it not
/// passive, and with neither flag the page's default applies.
///
/// To add a listener with an AbortSignal, use AddEventListenerOptions instead.
///
/// @note  Wheel listeners on the window, the document, `<html>`, and `<body>` are passive by
///        default (like the web), so pass dom::NotPassive to cancel wheel events there.
///
enum class EventListenerFlags : unsigned {
  None = 0,
  ///
  /// Receive the event in the capture phase (on its way down to the target).
  ///
  Capture = 1u << 0,
  ///
  /// Remove the listener after the first event it receives.
  ///
  Once = 1u << 1,
  ///
  /// Make the listener passive (its preventDefault() calls are ignored).
  ///
  Passive = 1u << 2,
  ///
  /// Make the listener not passive, even where the default is passive.
  ///
  NotPassive = 1u << 3,
};

constexpr EventListenerFlags operator|(EventListenerFlags a, EventListenerFlags b) {
  return static_cast<EventListenerFlags>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
}

constexpr EventListenerFlags& operator|=(EventListenerFlags& a, EventListenerFlags b) {
  return a = a | b;
}

///
/// Receive the event in the capture phase (shorthand for EventListenerFlags::Capture).
///
inline constexpr EventListenerFlags Capture = EventListenerFlags::Capture;

///
/// Remove the listener after the first event it receives (shorthand for
/// EventListenerFlags::Once).
///
inline constexpr EventListenerFlags Once = EventListenerFlags::Once;

///
/// Make the listener passive (shorthand for EventListenerFlags::Passive).
///
inline constexpr EventListenerFlags Passive = EventListenerFlags::Passive;

///
/// Make the listener not passive (shorthand for EventListenerFlags::NotPassive).
///
inline constexpr EventListenerFlags NotPassive = EventListenerFlags::NotPassive;

static_assert(static_cast<unsigned>(EventListenerFlags::Capture) == kULDOMEventFlags_Capture
                  && static_cast<unsigned>(EventListenerFlags::Once) == kULDOMEventFlags_Once
                  && static_cast<unsigned>(EventListenerFlags::Passive) == kULDOMEventFlags_Passive
                  && static_cast<unsigned>(EventListenerFlags::NotPassive)
                      == kULDOMEventFlags_NotPassive,
              "dom::EventListenerFlags must mirror ULDOMEventFlags");

///
/// A handle to a registered DOM event listener.
///
/// Calling addEventListener() registers a callback for DOM events and returns an EventListener
/// handle. You use it to remove the listener.
///
/// Store the handle to call Remove() later:
///
/// ```
/// dom::Element card = document.getElementById("card");
/// dom::EventListener on_hover =
///     card.addEventListener("mouseenter", [] { ShowPreview(); });
///
/// // Later:
/// on_hover.Remove();
/// ```
///
/// ## Why Listeners Return Handles
///
/// Because C++ can't compare lambdas, the library can't provide a removeEventListener() function
/// that takes a callback.
///
/// When a listener should run as long as the page stays open, you can ignore the returned handle:
///
/// ```
/// dom::Element save = document.getElementById("save");
/// save.addEventListener("click", [] { SaveGame(); });
/// ```
///
/// ## Stopping a Listener
///
/// Destroying an EventListener handle doesn't remove the listener-- the callback keeps receiving
/// events. Call IsActive() to check whether a listener can still fire.
///
/// A listener stops for good when:
///
/// - **You call Remove() on its handle.**
/// - **Its signal aborts.** Passing an AbortSignal from a dom::AbortController lets you remove
///   several listeners at once by calling dom::AbortController::abort().
/// - **A once listener receives its first event.** Setting `once` in AddEventListenerOptions or
///   passing dom::Once in the flags removes the listener automatically after it runs.
/// - **Its page is gone.** Navigating away or destroying the View stops all listeners registered on
///   that page.
///
/// Listeners added with addEventListener() belong to the current page only and don't survive
/// back-forward cache restores. To attach listeners to every page a View loads, use dom::Triggers
/// instead.
///
/// @warning  A callback that captures `this`, a pointer, or a reference can outlive its object.
///           Stop the listener before the object is destroyed (such as by storing a
///           dom::AbortController) or capture a `std::weak_ptr` instead.
///
/// @see  dom::Element::addEventListener(), dom::Element::On(), dom::AbortController, dom::Triggers
///
class EventListener {
 public:
  ///
  /// Create an empty EventListener (it refers to no listener).
  ///
  EventListener() = default;

  ///
  /// Copy constructor (both handles refer to the same listener).
  ///
  /// @param  other  The EventListener to copy.
  ///
  EventListener(const EventListener& other)
      : handle_(ulCreateDOMEventListenerRef(other.handle_)) {}

  ///
  /// Copy assignment (both handles refer to the same listener).
  ///
  /// @param  other  The EventListener to copy.
  ///
  /// @return  Returns this EventListener.
  ///
  EventListener& operator=(const EventListener& other) {
    if (this != &other) {
      ULDOMEventListener handle = ulCreateDOMEventListenerRef(other.handle_);
      ulDestroyDOMEventListener(handle_);
      handle_ = handle;
    }
    return *this;
  }

  ///
  /// Move constructor (`other` becomes empty).
  ///
  /// @param  other  The EventListener to move from.
  ///
  EventListener(EventListener&& other) noexcept : handle_(other.handle_) {
    other.handle_ = nullptr;
  }

  ///
  /// Move assignment (`other` becomes empty).
  ///
  /// @param  other  The EventListener to move from.
  ///
  /// @return  Returns this EventListener.
  ///
  EventListener& operator=(EventListener&& other) noexcept {
    if (this != &other) {
      ulDestroyDOMEventListener(handle_);
      handle_ = other.handle_;
      other.handle_ = nullptr;
    }
    return *this;
  }

  ///
  /// Destroy this handle (the listener keeps firing).
  ///
  ~EventListener() { ulDestroyDOMEventListener(handle_); }

  ///
  /// Whether or not the listener can still fire.
  ///
  /// @return  Returns false for an empty EventListener, after Remove(), after the listener's
  ///          AbortSignal is aborted, after a `once` listener received its event, or once the
  ///          page is gone.
  ///
  bool IsActive() const { return handle_ && ulDOMEventListenerIsActive(handle_); }

  ///
  /// Remove the listener so it receives no more events (removeEventListener).
  ///
  /// Safe to call more than once, after the page is gone, and from inside the listener's own
  /// callback. This EventListener still refers to the listener, and IsActive() returns false
  /// afterward.
  ///
  void Remove() const {
    if (handle_)
      ulDOMEventListenerRemove(handle_);
  }

  ///
  /// Release this handle (the listener keeps firing) and empty this EventListener.
  ///
  /// This does the same as destroying the handle.
  ///
  void Release() {
    ulDestroyDOMEventListener(handle_);
    handle_ = nullptr;
  }

  // --- Interop with the C API (most embedders never touch raw handles) -------------------

  ///
  /// Wrap a C listener handle you own, taking ownership of it.
  ///
  /// @param  handle  A handle from the C API that you would otherwise destroy with
  ///                 ulDestroyDOMEventListener() (NULL gives an empty EventListener).
  ///
  /// @return  Returns an EventListener that destroys the handle (not the listener) when
  ///          destroyed.
  ///
  static EventListener Adopt(ULDOMEventListener handle) { return EventListener(handle); }

  ///
  /// Get the C handle, for passing to the `<Ultralight/CAPI/CAPI_DOMEvent.h>` functions.
  ///
  /// @return  Returns the handle (NULL for an empty EventListener). This EventListener still owns
  ///          it, so don't destroy it.
  ///
  ULDOMEventListener raw() const { return handle_; }

  ///
  /// Give up ownership of the C handle and return it. This EventListener becomes empty.
  ///
  /// @return  Returns the handle. You must call ulDestroyDOMEventListener() when finished.
  ///
  ULDOMEventListener LeakRef() {
    ULDOMEventListener handle = handle_;
    handle_ = nullptr;
    return handle;
  }

 protected:
  explicit EventListener(ULDOMEventListener handle) : handle_(handle) {}

 private:
  ULDOMEventListener handle_ = nullptr;
};

class AbortSignal;

/// \cond INTERNAL
namespace detail {

// The state an AbortController shares with its signals. The listener list is touched only on
// the Renderer's thread (at registration and at abort); the flag is read from any thread.
class AbortState {
 public:
  AbortState() = default;
  AbortState(const AbortState&) = delete;
  AbortState& operator=(const AbortState&) = delete;

  ~AbortState() {
    for (ULDOMEventListener listener : listeners_)
      ulDestroyDOMEventListener(listener);
  }

  bool aborted() const { return aborted_.load(std::memory_order_acquire); }

  void Track(ULDOMEventListener listener) {
    // Pruning at a doubling threshold keeps a long-lived controller full of spent Once
    // listeners bounded at amortized constant cost per add.
    if (listeners_.size() >= prune_at_) {
      PruneInactive();
      prune_at_ = std::max(kMinPruneAt, listeners_.size() * 2);
    }
    listeners_.push_back(ulCreateDOMEventListenerRef(listener));
  }

  void Abort() {
    if (aborted_.exchange(true, std::memory_order_acq_rel))
      return;
    std::vector<ULDOMEventListener> listeners = std::move(listeners_);
    listeners_.clear();
    for (ULDOMEventListener listener : listeners) {
      ulDOMEventListenerRemove(listener);
      ulDestroyDOMEventListener(listener);
    }
  }

 private:
  static constexpr size_t kMinPruneAt = 16;

  void PruneInactive() {
    auto spent = std::remove_if(listeners_.begin(), listeners_.end(),
                                [](ULDOMEventListener listener) {
                                  if (ulDOMEventListenerIsActive(listener))
                                    return false;
                                  ulDestroyDOMEventListener(listener);
                                  return true;
                                });
    listeners_.erase(spent, listeners_.end());
  }

  std::atomic<bool> aborted_{ false };
  std::vector<ULDOMEventListener> listeners_;
  size_t prune_at_ = kMinPruneAt;
};

inline void TrackListener(const AbortSignal& signal, ULDOMEventListener listener);

}  // namespace detail
/// \endcond

///
/// A signal that removes event listeners when an AbortController aborts.
///
/// Calling AbortController::signal() returns a signal tied to that controller. When the controller
/// aborts or is destroyed, the library removes every listener registered with that signal.
///
/// Pass a controller's signal in AddEventListenerOptions when adding a listener:
///
/// ```
/// dom::AbortController menu_listeners;
/// document.addEventListener("keydown", OnMenuKey,
///                           { .signal = menu_listeners.signal() });
/// ```
///
/// ## Signal States
///
/// - **An already aborted signal prevents the listener from being added.** Calling
///   addEventListener() with it returns an empty EventListener.
/// - **A default-constructed signal never aborts.** Because it doesn't belong to an
///   AbortController, listeners added with it stay active until removed or until their page is
///   gone.
///
/// @note  Calling aborted() is safe from any thread.
///
/// @see  dom::AbortController, dom::AddEventListenerOptions, dom::EventListener
///
class AbortSignal {
 public:
  ///
  /// Create a signal that belongs to no controller (it never aborts).
  ///
  AbortSignal() = default;

  ///
  /// Whether or not the signal's controller has aborted (aborted).
  ///
  /// @return  Returns true once its AbortController has aborted or been destroyed.
  ///
  bool aborted() const { return state_ && state_->aborted(); }

 private:
  friend class AbortController;
  friend void detail::TrackListener(const AbortSignal& signal, ULDOMEventListener listener);

  explicit AbortSignal(std::shared_ptr<detail::AbortState> state) : state_(std::move(state)) {}

  std::shared_ptr<detail::AbortState> state_;
};

///
/// A controller that removes a group of DOM event listeners together.
///
/// An AbortController provides an AbortSignal that can be shared across multiple event listeners.
/// Calling abort() removes every listener registered with that signal at once, without needing to
/// keep individual EventListener handles.
///
/// Pass the controller's signal to each listener:
///
/// ```
/// dom::AbortController drag;
/// document.addEventListener("mousemove", OnDrag, { .signal = drag.signal() });
/// document.addEventListener("mouseup", OnDrop, { .signal = drag.signal() });
///
/// // Later:
/// drag.abort();
/// ```
///
/// ## Tying Listeners to an Object
///
/// A callback that captures `this` lives as long as the page. Unlike on the web, destroying an
/// AbortController aborts it automatically.
///
/// You should store a controller as a class member so its listeners stop firing when the owning
/// object is destroyed:
///
/// ```
/// class Hud {
///  public:
///   explicit Hud(dom::Document document) {
///     document.getElementById("close").addEventListener(
///         "click", [this] { CloseHud(); }, { .signal = listeners_.signal() });
///   }
///
///  private:
///   void CloseHud() {}
///   dom::AbortController listeners_;
/// };
/// ```
///
/// ## Replacing a Controller
///
/// An AbortController aborts only once. After aborting, its signal stays aborted and won't add new
/// listeners.
///
/// To start over, assign a fresh controller to the variable (the old controller aborts first):
///
/// ```
/// drag = dom::AbortController();
/// ```
///
/// @note  Once its signal has added a listener, you must abort, destroy, or assign over the
///        controller on the Renderer's thread.
///
/// @see  dom::AbortSignal, dom::AddEventListenerOptions, dom::EventListener
///
class AbortController {
 public:
  ///
  /// Create a controller that hasn't aborted.
  ///
  AbortController() : state_(std::make_shared<detail::AbortState>()) {}

  AbortController(const AbortController&) = delete;
  AbortController& operator=(const AbortController&) = delete;

  ///
  /// Move constructor (`other` becomes empty, and its signal() then returns a signal that never
  /// aborts).
  ///
  /// @param  other  The AbortController to move from.
  ///
  AbortController(AbortController&& other) noexcept = default;

  ///
  /// Move assignment (aborts this controller, then takes over `other`'s signal).
  ///
  /// @param  other  The AbortController to move from.
  ///
  /// @return  Returns this AbortController.
  ///
  AbortController& operator=(AbortController&& other) noexcept {
    if (this != &other) {
      abort();
      state_ = std::move(other.state_);
    }
    return *this;
  }

  ///
  /// Destroy this controller and abort it.
  ///
  ~AbortController() { abort(); }

  ///
  /// Get the signal to pass in AddEventListenerOptions (signal).
  ///
  /// @return  Returns this controller's signal. Every call returns the same signal.
  ///
  AbortSignal signal() const { return AbortSignal(state_); }

  ///
  /// Remove every listener added with this controller's signal (abort).
  ///
  /// After this, signal().aborted() returns true and listeners added with the signal are never
  /// added. Calling it again does nothing. Safe to call from inside a listener's callback.
  ///
  void abort() {
    if (state_)
      state_->Abort();
  }

 private:
  std::shared_ptr<detail::AbortState> state_;
};

///
/// Options for adding an event listener, like the web's options object for addEventListener().
///
/// Name only the members you need, in the order they're declared here (capture, once, passive,
/// signal), since C++ requires designated initializers in declaration order:
///
/// ```
/// el.addEventListener("click", OnClick, { .once = true, .signal = controller.signal() });
/// ```
///
/// EventListenerFlags is the shorter form when you don't need a signal.
///
/// @see  Element::addEventListener(), Element::On()
///
struct AddEventListenerOptions {
  ///
  /// Whether or not to receive the event in the capture phase (on its way down to the target).
  ///
  bool capture = false;

  ///
  /// Whether or not to remove the listener after the first event it receives.
  ///
  bool once = false;

  ///
  /// Whether or not the listener is passive (its preventDefault() calls are ignored). Leave it
  /// empty to use the page's default.
  ///
  /// @note  Wheel listeners on the window, the document, `<html>`, and `<body>` are passive by
  ///        default (like the web), so set this to false to cancel wheel events there.
  ///
  std::optional<bool> passive = std::nullopt;

  ///
  /// A signal that removes the listener when its AbortController aborts (see AbortSignal).
  ///
  AbortSignal signal = {};
};

/// \cond INTERNAL
namespace detail {

inline void TrackListener(const AbortSignal& signal, ULDOMEventListener listener) {
  if (signal.state_)
    signal.state_->Track(listener);
}

inline unsigned ToFlags(const AddEventListenerOptions& options) {
  unsigned flags = kULDOMEventFlags_None;
  if (options.capture)
    flags |= kULDOMEventFlags_Capture;
  if (options.once)
    flags |= kULDOMEventFlags_Once;
  if (options.passive)
    flags |= *options.passive ? kULDOMEventFlags_Passive : kULDOMEventFlags_NotPassive;
  return flags;
}

// Passive wins when both passive flags are set, as it does in the C API.
inline AddEventListenerOptions ToOptions(EventListenerFlags flags) {
  auto has = [flags](EventListenerFlags flag) {
    return (static_cast<unsigned>(flags) & static_cast<unsigned>(flag)) != 0;
  };
  AddEventListenerOptions options;
  options.capture = has(EventListenerFlags::Capture);
  options.once = has(EventListenerFlags::Once);
  if (has(EventListenerFlags::Passive))
    options.passive = true;
  else if (has(EventListenerFlags::NotPassive))
    options.passive = false;
  return options;
}

// The registration step every typed addEventListener() / On() shares. `add` performs the C
// call with the flags and the heap callable as user_data; it must not name Fn itself (a
// function template's local alias used inside a nested lambda miscompiles on MSVC), so
// callers compute the thunk pointers outside the lambda.
template <typename Fn, typename F, typename Add>
EventListener AddListener(F&& callback, const AddEventListenerOptions& options, Add&& add) {
  if (options.signal.aborted())
    return EventListener();
  // The C call owns fn either way: on failure it runs DeleteCallable before returning NULL.
  Fn* fn = new Fn(std::forward<F>(callback));
  ULDOMEventListener handle = add(ToFlags(options), static_cast<void*>(fn));
  if (!handle)
    return EventListener();
  EventListener listener = EventListener::Adopt(handle);
  TrackListener(options.signal, handle);
  return listener;
}

}  // namespace detail
/// \endcond

}  // namespace dom
}  // namespace ultralight

#pragma pop_macro("None")
