///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
// X11's headers define None as a macro, which would break the None enumerators.
#pragma push_macro("None")
#undef None

#include <Ultralight/CAPI/CAPI_DOMEvent.h>
#include <Ultralight/CAPI/CAPI_DOMTriggers.h>
#include <Ultralight/OriginRules.h>
#include <Ultralight/View.h>
#include <Ultralight/detail/InjectionFilter.h>
#include <Ultralight/dom/Document.h>
#include <Ultralight/dom/Holders.h>
#include <Ultralight/dom/detail/Receivers.h>

#include <string_view>
#include <type_traits>
#include <utility>

namespace ultralight {
namespace dom {

/// \cond INTERNAL
namespace detail {

// Trampoline behind Triggers::OnDOMReady; the document is borrowed, so the typed view
// takes its own reference.
template <typename Fn>
void ReadyThunk(void* user_data, ULDOMDocument document) {
  Fn& fn = *static_cast<Fn*>(user_data);
  CallListener("DOM-ready or restore hook", [&] {
    if constexpr (std::is_invocable_v<Fn&, Document>)
      fn(Document::FromBorrowed(document));
    else
      fn();
  });
}

}  // namespace detail
/// \endcond

///
/// The flags for Triggers::AttachTo() (the typed form of ULDOMTriggersAttachFlags).
///
enum class AttachFlags : unsigned {
  None = 0,             ///< Add the listeners to the main frame only.
  AllFrames = 1u << 1,  ///< Add the listeners to subframes too.
};

constexpr AttachFlags operator|(AttachFlags a, AttachFlags b) {
  return static_cast<AttachFlags>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
}

constexpr AttachFlags& operator|=(AttachFlags& a, AttachFlags b) {
  return a = a | b;
}

///
/// Add the listeners to subframes too (shorthand for AttachFlags::AllFrames).
///
inline constexpr AttachFlags AllFrames = AttachFlags::AllFrames;

static_assert(static_cast<unsigned>(AttachFlags::AllFrames) == kULDOMTriggersAttachFlags_AllFrames,
              "dom::AttachFlags must mirror ULDOMTriggersAttachFlags");

///
/// Options for Triggers::AttachTo().
///
/// ```
/// if (!ui.AttachTo(view.get(), { .flags = dom::AllFrames,
///                                .origin_rules = { "https://*.mygame.com" } }))
///   Log("the attach failed");
/// ```
///
struct AttachOptions {
  ///
  /// The attach flags (see AttachFlags).
  ///
  AttachFlags flags = AttachFlags::None;

  ///
  /// The origin rules for the pages that get the listeners. Leave it empty for the default
  /// policy (your application's own content). See ultralight::OriginRules.
  ///
  OriginRules origin_rules = {};
};

///
/// Information passed to a DOM triggers filter callback.
///
/// When you set a filter callback with dom::SetInjectionFilter(), a View passes this struct each
/// time it's about to add an attached Triggers set to a page. Origin rules run first and provide an
/// initial verdict in `rules_allow`, which your callback can override.
///
/// This filter adds a developer-tools listener set only when a user setting is on:
///
/// ```
/// dom::SetInjectionFilter(view.get(), [](const dom::InjectionRequest& request) {
///   if (request.triggers == dev_tools.raw())
///     return request.rules_allow && dev_tools_enabled;
///   return request.rules_allow;
/// });
/// ```
///
/// @note  New members may be added at the end in later versions, so read members by name.
///
/// @see  dom::SetInjectionFilter(), dom::ClearInjectionFilter(), dom::Triggers,
///       ultralight::OriginRules
///
struct InjectionRequest {
  ///
  /// The View loading the page (the View you set the filter on).
  ///
  View* view;

  ///
  /// The set of DOM listeners being decided on. It's the handle your Triggers' raw() returns,
  /// so compare the two to tell your sets apart.
  ///
  ULDOMTriggers triggers;

  ///
  /// The page's security origin, serialized (eg, "https://example.com", or "null" for an opaque
  /// origin).
  ///
  const char* origin;

  ///
  /// Whether or not the page is in the View's main frame.
  ///
  bool is_main_frame;

  ///
  /// Whether or not the origin rules allow the page.
  ///
  bool rules_allow;
};

///
/// A set of DOM event listeners and lifecycle hooks that persists across page navigations.
///
/// You define DOM event listeners and setup hooks in a dom::Triggers set and attach it to a View
/// instead of re-registering handlers on every page load. One set can serve several View%s, and it
/// functions even when JavaScript is disabled.
///
/// Attach the set to a View before loading content so every page gets the listeners:
///
/// ```
/// dom::Triggers ui;
///
/// ui.On("#save", "click", [] { SaveGame(); });
/// ui.OnDOMReady([](dom::Document document) {
///   document.getElementById("status").textContent = "Ready";
/// });
///
/// if (ui.AttachTo(view.get()))
///   view->LoadURL("file:///app.html");
/// ```
///
/// ## Delivery Timing
///
/// Pages receive an attached set based on their load state:
///
/// - **A loading page gets the set once its document finishes parsing.** Any OnDOMReady() hooks run
///   right after the listeners attach.
/// - **Attaching after a page has loaded applies the listeners right away.** Any OnDOMReady() hooks
///   run synchronously inside AttachTo().
/// - **Handlers added after attaching reach only subsequent pages.** The current page keeps its
///   existing listeners unchanged.
///
/// @note  The page's own `DOMContentLoaded` event fires before the set arrives. Use OnDOMReady()
///        for setup code that runs once document parsing completes.
///
/// ## Origin Rules and Page Filtering
///
/// By default, only your own content gets the set (`file://` pages and View::LoadHTML()). Passing
/// custom origin rules in AttachOptions replaces this default policy:
///
/// - **Local pages require an explicit `"file://*"` rule.** Without it, local content stops
///   receiving the set.
/// - **Pages loaded without a URL match no origin rule.** Content loaded through View::LoadHTML()
///   without a URL has an opaque origin, so origin patterns never match it.
/// - **Subframes are excluded unless requested.** Pass `dom::AllFrames` in `flags` to deliver
///   listeners to subframe documents and run OnDOMReady() for each frame's document.
/// - **Malformed rules cancel the attachment.** If any rule fails to parse, AttachTo() returns
///   `false`, logs a warning explaining the failure, and leaves the View unchanged.
///
/// Configure origin rules and frame targeting through AttachOptions when attaching the set:
///
/// ```
/// bool attached = ui.AttachTo(view.get(), {
///     .flags = dom::AllFrames,
///     .origin_rules = { "https://*.mygame.com", "file://*" } });
/// ```
///
/// For decisions origin rules can't express (such as checking a user setting or allowing an opaque
/// origin), see dom::SetInjectionFilter() and dom::InjectionRequest.
///
/// ## Object Lifetime and Cleanup
///
/// Calling DetachFrom() removes the whole set from a View immediately, though individual
/// registrations can't be removed on their own.
///
/// Destroying a dom::Triggers instance automatically detaches it from every View and stops its
/// listeners right away. Storing the set as a member of the class whose methods its callbacks
/// invoke ensures the listeners never outlive the surrounding object:
///
/// ```
/// class Hud {
///  public:
///   explicit Hud(View* view) {
///     ui_.On("#close", "click", [this] { Dismiss(); });
///     if (!ui_.AttachTo(view))
///       Log("an origin rule failed to parse");
///   }
///
///  private:
///   void Dismiss() {}
///   dom::Triggers ui_;
/// };
/// ```
///
/// ## Back-Forward Cache Restores
///
/// When a page returns from the back-forward cache (`Config::page_cache_size` above 0), the library
/// restores its event listeners before the `pageshow` event fires. Hooks registered with
/// OnDOMReady() don't run again because the document was parsed during the initial load.
///
/// Register an OnRestore() hook for tasks that must run every time a cached page reappears:
///
/// ```
/// ui.OnRestore([](dom::Document document) {
///   document.getElementById("status").textContent = "Welcome back";
/// });
/// ```
///
/// ## Choosing Between Triggers and addEventListener
///
/// Choose between dom::Triggers and Element::addEventListener() based on whether the listeners
/// belong to one page or the whole View:
///
/// - **dom::Triggers persists across navigations and cache restores.** Use it for interface
///   elements that must remain active across page loads or multiple View%s.
/// - **Element::addEventListener() attaches to a single page's elements.** Direct listeners belong
///   to one document and are gone after a back-forward restore or navigation.
///
/// @see  dom::AttachOptions, dom::SetInjectionFilter(), dom::InjectionRequest,
///       ultralight::OriginRules, dom::Element::On(), dom::EventListener
///
class Triggers {
 public:
  ///
  /// Create a new set of DOM listeners.
  ///
  Triggers() : handle_(ulCreateDOMTriggers()) {}

  Triggers(const Triggers&) = delete;
  Triggers& operator=(const Triggers&) = delete;

  ///
  /// Move constructor (`other` becomes empty).
  ///
  /// @param  other  The Triggers to move from.
  ///
  Triggers(Triggers&& other) noexcept : handle_(other.handle_) { other.handle_ = nullptr; }

  ///
  /// Move assignment (releases the set this object held, then takes over `other`'s).
  ///
  /// @param  other  The Triggers to move from.
  ///
  /// @return  Returns this object.
  ///
  Triggers& operator=(Triggers&& other) noexcept {
    if (this != &other) {
      ulDestroyDOMTriggers(handle_);
      handle_ = other.handle_;
      other.handle_ = nullptr;
    }
    return *this;
  }

  ///
  /// Release this object's reference to the set.
  ///
  /// @note  Destroying the set detaches it from every View (see the class overview),
  ///        unless it came from FromBorrowed() or another owning handle to it exists.
  ///
  ~Triggers() { ulDestroyDOMTriggers(handle_); }

  ///
  /// Whether or not this object holds a set (false after a move or LeakRef(), or when it wraps
  /// NULL).
  ///
  explicit operator bool() const { return handle_ != nullptr; }

  ///
  /// Add a listener for events on elements matching a CSS selector.
  ///
  /// On every page that gets the set, the event target and its ancestors are tested against
  /// `selector`, and the callback fires with the nearest match. This covers elements added to
  /// the page later too (it works like Element::On() on the whole document).
  ///
  /// @param  selector  The CSS selector to match (eg, `#save`).
  ///
  /// @param  type      The event type to listen for (eg, `click`).
  ///
  /// @param  callback  The callable to invoke, taking (dom::Event, dom::Element matched),
  ///                   (dom::Element matched), or ().
  ///
  /// @param  flags     The registration options (see EventListenerFlags).
  ///
  /// @return  Returns true if the listener was added, or false if it wasn't (eg, this object
  ///          is empty, or `selector` is malformed). On false, the library destroys its copy
  ///          of `callback`. You can ignore the result, since the library logs a warning for a
  ///          refused selector.
  ///
  /// \parblock
  /// @note  Events that don't bubble (eg, `focus`) reach the listener only with dom::Capture. A
  ///        wheel listener is passive unless you pass dom::NotPassive.
  /// \endparblock
  ///
  /// \parblock
  /// @note  With dom::Once, the first `type` event that reaches the document uses up the
  ///        listener on that page, even if no element matches.
  /// \endparblock
  ///
  /// \parblock
  /// @note  Before a Renderer exists, only an empty `selector` is refused here. Each page
  ///        checks the selector when it gets the set instead, and skips a malformed one with a
  ///        warning.
  /// \endparblock
  ///
  template <typename F>
  bool On(std::string_view selector, std::string_view type, F&& callback,
          EventListenerFlags flags = EventListenerFlags::None) {
    using Fn = std::decay_t<F>;
    static_assert(detail::InvocableWithEvent<Fn, Element> || std::is_invocable_v<Fn&, Element>
                      || std::is_invocable_v<Fn&>,
                  "On takes a callable invocable with (dom::Event, dom::Element), "
                  "(dom::Element), or ()");
    detail::CString sel(selector);
    detail::CString t(type);
    // The C call takes ownership of fn either way: on failure it runs DeleteCallable.
    return ulDOMTriggersOn(handle_, sel.c_str(), t.c_str(), static_cast<unsigned>(flags),
                           &detail::DelegatedThunk<Fn>, new Fn(std::forward<F>(callback)),
                           &detail::DeleteCallable<Fn>);
  }

  ///
  /// Add a selector listener that calls a member function on a borrowed receiver (no
  /// forwarding lambda needed):
  ///
  /// ```
  /// ui.On("#save", "click", toolbar, &Toolbar::OnSave);
  /// ```
  ///
  /// @param  selector  The CSS selector to match (eg, `#save`).
  ///
  /// @param  type      The event type to listen for (eg, `click`).
  ///
  /// @param  receiver  The object to call `method` on. You must keep it alive until this
  ///                   Triggers is destroyed or the set is detached from every View.
  ///
  /// @param  method    The member function to call, taking (dom::Event, dom::Element matched),
  ///                   (dom::Element matched), or ().
  ///
  /// @param  flags     The registration options (see EventListenerFlags).
  ///
  /// @return  Returns true if the listener was added (see the callable overload).
  ///
  template <typename C, typename M>
    requires(std::is_class_v<C> && std::is_member_function_pointer_v<M>
             && !LockableHolder<C>)
  bool On(std::string_view selector, std::string_view type, C& receiver, M method,
          EventListenerFlags flags = EventListenerFlags::None) {
    return On(selector, type, detail::WrapBorrowedMember(receiver, method), flags);
  }

  ///
  /// Add a selector listener that calls a member function through a holder (a shared_ptr, a
  /// weak_ptr, or a custom smart pointer with a HolderTraits specialization).
  ///
  /// The holder is locked around each delivery, and an expired holder skips the delivery
  /// silently (see LockableHolder).
  ///
  /// @param  selector  The CSS selector to match (eg, `#save`).
  ///
  /// @param  type      The event type to listen for (eg, `click`).
  ///
  /// @param  holder    The holder to lock and call the member on.
  ///
  /// @param  method    The member function to call, taking (dom::Event, dom::Element matched),
  ///                   (dom::Element matched), or ().
  ///
  /// @param  flags     The registration options (see EventListenerFlags).
  ///
  /// @return  Returns true if the listener was added (see the callable overload).
  ///
  template <typename H, typename M>
    requires(LockableHolder<H> && std::is_member_function_pointer_v<M>)
  bool On(std::string_view selector, std::string_view type, H holder, M method,
          EventListenerFlags flags = EventListenerFlags::None) {
    return On(selector, type, detail::WrapHolderMember(std::move(holder), method), flags);
  }

  ///
  /// Add a hook that runs on each page that gets the set, once its document has finished
  /// parsing.
  ///
  /// The hook runs after the set's listeners were added to the page, with or without JavaScript
  /// enabled. With dom::AllFrames it also runs for each subframe's document. It doesn't run when
  /// a page is restored from the back-forward cache (see OnRestore()).
  ///
  /// @param  callback  The callable to invoke, taking (dom::Document) or ().
  ///
  template <typename F>
  void OnDOMReady(F&& callback) {
    using Fn = std::decay_t<F>;
    static_assert(std::is_invocable_v<Fn&, Document> || std::is_invocable_v<Fn&>,
                  "OnDOMReady takes a callable invocable with (dom::Document) or ()");
    // The C call takes ownership of the callable either way: on failure it runs DeleteCallable.
    ulDOMTriggersOnDOMReady(handle_, &detail::ReadyThunk<Fn>, new Fn(std::forward<F>(callback)),
                            &detail::DeleteCallable<Fn>);
  }

  ///
  /// Add a hook that runs each time a page that gets the set is restored from the back-forward
  /// cache (Config::page_cache_size > 0).
  ///
  /// The hook runs after the set's listeners were added to the page again, and before the
  /// page's `pageshow` event. It never runs on a normal load.
  ///
  /// @param  callback  The callable to invoke, taking (dom::Document) or ().
  ///
  template <typename F>
  void OnRestore(F&& callback) {
    using Fn = std::decay_t<F>;
    static_assert(std::is_invocable_v<Fn&, Document> || std::is_invocable_v<Fn&>,
                  "OnRestore takes a callable invocable with (dom::Document) or ()");
    // The C call takes ownership of the callable either way: on failure it runs DeleteCallable.
    ulDOMTriggersOnRestore(handle_, &detail::ReadyThunk<Fn>, new Fn(std::forward<F>(callback)),
                           &detail::DeleteCallable<Fn>);
  }

  ///
  /// Attach this set to a View.
  ///
  /// The listeners are added to the View's current page (if its document has finished parsing
  /// and the origin rules allow it) and to every page the View loads afterward. The View
  /// keeps the set attached until you detach it or destroy this Triggers instance. Attaching
  /// again updates the flags and origin rules for later pages.
  ///
  /// ```
  /// // Your own content, subframes included:
  /// if (!ui.AttachTo(view.get(), { .flags = dom::AllFrames }))
  ///   Log("the attach failed");
  /// ```
  ///
  /// @param  view     The View to attach to.
  ///
  /// @param  options  The attach flags and the origin rules for the pages that get the
  ///                  listeners (see AttachOptions).
  ///
  /// @return  Returns true on success, or false if `view` is nullptr, this object holds no set,
  ///          a flag is unknown, or a rule failed to parse. Nothing changes then. An unknown
  ///          flag or a rule that failed to parse also logs a warning that says why.
  ///
  [[nodiscard]] bool AttachTo(View* view, const AttachOptions& options = {}) {
    return view && handle_
        && view->AttachDOMTriggers(handle_, static_cast<unsigned>(options.flags),
                                   options.origin_rules.data(), options.origin_rules.size());
  }

  ///
  /// Detach this set from a View.
  ///
  /// The listeners are removed from the View's pages right away, and no later page gets them.
  ///
  /// @param  view  The View to detach from.
  ///
  /// @note  If you attach the set again, the current page gets it again (and its DOM-ready
  ///        hooks run again there).
  ///
  void DetachFrom(View* view) {
    if (view && handle_)
      view->DetachDOMTriggers(handle_);
  }

  // --- Interop with the C API (most embedders never touch raw handles) -------------------

  ///
  /// Wrap a C handle you own, taking ownership of it.
  ///
  /// @param  handle  A handle from the C API that you would otherwise destroy with
  ///                 ulDestroyDOMTriggers() (NULL gives an empty object).
  ///
  /// @return  Returns a Triggers that releases `handle` when it's done.
  ///
  static Triggers Adopt(ULDOMTriggers handle) { return Triggers(handle, AdoptTag {}); }

  ///
  /// Wrap a C handle someone else owns, without owning the set (eg, the `triggers` member of a
  /// dom::InjectionRequest).
  ///
  /// The result works like any Triggers, but destroying it never detaches the set, and it
  /// doesn't keep the set attached once its owners are gone.
  ///
  /// @param  handle  The borrowed handle (NULL gives an empty object).
  ///
  /// @return  Returns a Triggers with its own (non-owning) reference, so you can keep it after
  ///          the callback. Its raw() is a different handle than `handle`.
  ///
  static Triggers FromBorrowed(ULDOMTriggers handle) {
    return Triggers(handle ? ulCreateDOMTriggersBorrowedRef(handle) : nullptr, AdoptTag {});
  }

  ///
  /// Get the C handle, for passing to the `<Ultralight/CAPI/CAPI_DOMTriggers.h>` functions.
  ///
  /// @return  Returns the handle (NULL if this object holds no set). This object still owns it,
  ///          so don't destroy it.
  ///
  ULDOMTriggers raw() const { return handle_; }

  ///
  /// Give up ownership of the C handle and return it. This object becomes empty.
  ///
  /// @return  Returns the handle. You must call ulDestroyDOMTriggers() when finished.
  ///
  ULDOMTriggers LeakRef() {
    ULDOMTriggers handle = handle_;
    handle_ = nullptr;
    return handle;
  }

 private:
  struct AdoptTag {};
  Triggers(ULDOMTriggers handle, AdoptTag) : handle_(handle) {}

  ULDOMTriggers handle_ = nullptr;
};

}  // namespace dom

/// \cond INTERNAL
namespace detail {
template <>
struct InjectionRequestTraits<ULDOMTriggersInjectionRequest> {
  static dom::InjectionRequest ToRequest(View* view,
                                         const ULDOMTriggersInjectionRequest& request) {
    return { view, request.triggers, request.origin, request.is_main_frame,
             request.rules_allow };
  }
  static void ReportException(const char* what) {
    ulDOMReportCallbackException("DOM triggers injection filter", what);
  }
};
}  // namespace detail
/// \endcond

namespace dom {

///
/// Set a View's DOM triggers filter with a C++ callable (the typed form of
/// View::SetDOMTriggersInjectionFilter()).
///
/// The View calls the filter each time it's about to add an attached set of DOM listeners to a
/// page (when a page finishes parsing, when you attach a set, and when a page returns from the
/// back-forward cache). The origin rules run first, and `rules_allow` holds their result.
/// Return true to add the listeners or false to skip the page, whatever the rules decided. Use
/// it for decisions that origin rules can't express, such as checking a user setting or allowing
/// a page whose origin is opaque.
///
/// ```
/// dom::SetInjectionFilter(view.get(), [](const dom::InjectionRequest& request) {
///   return request.rules_allow && request.is_main_frame;
/// });
/// ```
///
/// @param  view    The View to set the filter on (nothing happens if it's nullptr).
///
/// @param  filter  A callable invocable as `bool(const dom::InjectionRequest&)`. The new filter
///                 replaces (and destroys) the previous one.
///
/// \parblock
/// @note  Call this on the Renderer's thread. The filter runs there too.
/// \endparblock
///
/// \parblock
/// @note  The filter runs on subframes only for sets attached with dom::AllFrames. The request
///        is valid only during the call.
/// \endparblock
///
/// \parblock
/// @note  A filter that throws a C++ exception skips that page.
/// \endparblock
///
/// @see  ClearInjectionFilter()
///
template <typename F>
void SetInjectionFilter(View* view, F&& filter) {
  using Fn = std::decay_t<F>;
  static_assert(std::is_invocable_r_v<bool, Fn&, const InjectionRequest&>,
                "dom::SetInjectionFilter takes a callable invocable as "
                "bool(const dom::InjectionRequest&)");
  if (!view)
    return;
  using Adapter = ultralight::detail::InjectionFilterAdapter<Fn, ULDOMTriggersInjectionRequest>;
  view->SetDOMTriggersInjectionFilter(&Adapter::Invoke,
                                      new Adapter { std::forward<F>(filter), view },
                                      &Adapter::Destroy);
}

///
/// Remove a View's DOM triggers filter, so the origin rules alone decide which pages get each
/// set.
///
/// @param  view  The View to clear the filter on (nothing happens if it's nullptr).
///
/// @note  Call this on the Renderer's thread.
///
inline void ClearInjectionFilter(View* view) {
  if (view)
    view->SetDOMTriggersInjectionFilter(nullptr, nullptr, nullptr);
}

}  // namespace dom
}  // namespace ultralight

#pragma pop_macro("None")
