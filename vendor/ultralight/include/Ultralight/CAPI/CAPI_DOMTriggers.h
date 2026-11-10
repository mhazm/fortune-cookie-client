///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// @file CAPI_DOMTriggers.h
///
/// Sets of DOM event listeners and lifecycle hooks that persist across page navigations.
///
/// `#include <Ultralight/CAPI/CAPI_DOMTriggers.h>`
///
/// @note  This API is a preview and may still change after 2.0.
///
/// A ULDOMTriggers handle represents a reusable set of DOM event listeners and document lifecycle
/// hooks. Instead of registering handlers on each page load, you define your listeners once and
/// attach the set to a View.
///
/// The set applies to every page that View loads, even when JavaScript is disabled. One set can
/// attach to several Views to share interface handling across windows.
///
/// Define listeners and hooks on a triggers set, then attach it to a View:
///
/// ```c
/// static ULDOMTriggers ui = NULL;
///
/// static void OnAnySave(void* user_data, ULDOMEvent event,
///                       ULDOMElement button) {
///   (void)user_data;
///   (void)event;
///   // Borrowed: don't destroy button.
///   ulDOMElementSetTextContent(button, "Saved", 5);
/// }
///
/// static void OnReady(void* user_data, ULDOMDocument document) {
///   (void)user_data;
///   ULDOMElement status = ulDOMDocumentGetElementById(document, "status");
///   ulDOMElementSetTextContent(status, "Ready", 5);
///   ulDestroyDOMElement(status);
/// }
///
/// bool SetupUI(ULView view) {
///   ui = ulCreateDOMTriggers();
///   ulDOMTriggersOn(ui, "#save", "click", kULDOMEventFlags_None, OnAnySave,
///                   NULL, NULL);
///   ulDOMTriggersOnDOMReady(ui, OnReady, NULL, NULL);
///
///   // Attach before loading content so the first page gets the set too.
///   return ulViewAttachDOMTriggers(view, ui, kULDOMTriggersAttachFlags_None,
///                                  NULL, 0);
/// }
///
/// void ShutdownUI(void) {
///   // The last owning handle: the set detaches from every View.
///   ulDestroyDOMTriggers(ui);
///   ui = NULL;
/// }
/// ```
///
/// ## Delivery Timing and Restores
///
/// Pages receive an attached set based on their load state:
///
/// - **A loading page gets the set once its document finishes parsing.** Hooks registered with
///   ulDOMTriggersOnDOMReady() run right after the listeners attach.
/// - **Attaching after a page has loaded applies the listeners right away.** Any DOM-ready hooks
///   run synchronously inside ulViewAttachDOMTriggers().
/// - **Handlers added after attaching reach only subsequent pages.** The current page keeps its
///   existing listeners unchanged.
/// - **Pages restored from the back-forward cache regain their listeners before `pageshow` fires.**
///   Hooks registered with ulDOMTriggersOnDOMReady() don't run again on restore. Call
///   ulDOMTriggersOnRestore() to register callbacks that run each time a cached page returns.
///
/// The page's own `DOMContentLoaded` event fires before the set arrives, so use
/// ulDOMTriggersOnDOMReady() for setup code that requires a parsed document. For the complete rules
/// governing delivery order and back-forward navigation, see `<Ultralight/dom/Triggers.h>`.
///
/// ## Callbacks and User Data
///
/// Callback arguments and registration state follow specific ownership rules:
///
/// - **Element and document handles are valid only while the callback runs.** Don't destroy them or
///   store them across calls. Call ulCreateDOMElementRef() or ulCreateDOMDocumentRef() to obtain an
///   owned handle you can keep.
/// - **Ownership of user data transfers to the triggers set.** Each registration accepts its own
///   pointer and an optional `destroy_user_data` hook, which never runs while the callback
///   executes.
///
/// ## Origin Rules and Page Filtering
///
/// By default, only your application's own content gets the set (local `file://` pages, pages
/// loaded with ulViewLoadHTML(), and subframes sharing their origin). Passing custom origin rules
/// replaces this default policy:
///
/// - **Origin rules pass as an array of UTF-8 strings and an entry count.** If any rule fails to
///   parse, ulViewAttachDOMTriggers() returns `false`, logs a warning explaining why, and leaves
///   the View unchanged.
/// - **Custom rules must include `file://*` to keep local pages.** Omitting `"file://*"` prevents
///   local content from receiving the set.
/// - **Pages loaded with ulViewLoadHTML() match no origin rules.** Content loaded this way produces
///   an opaque origin (`"null"`), which fails all rule checks.
///
/// For decisions that origin rules can't express (such as checking a user setting or allowing an
/// opaque origin), set a filter callback on the View with ulViewSetDOMTriggersInjectionFilter().
/// The origin rules evaluate first and pass their verdict in `rules_allow`. Return true to add the
/// set to the page, or false to skip it.
///
/// Pass origin rules and attach flags to allow remote domains and include subframes:
///
/// ```c
/// static const char* const rules[] = { "https://*.mygame.com", "file://*" };
///
/// bool AttachEverywhere(ULView view) {
///   return ulViewAttachDOMTriggers(view, ui, kULDOMTriggersAttachFlags_AllFrames,
///                                  rules, 2);
/// }
/// ```
///
/// ## Handles and Lifetime
///
/// The lifetime of an attached set depends on the kind of handle you hold:
///
/// - **Owning handles keep the set attached to its Views.** You obtain one from
///   ulCreateDOMTriggers() or ulCreateDOMTriggersRef(), and must call ulDestroyDOMTriggers() when
///   finished with each handle.
/// - **Destroying the last owning handle detaches the set everywhere.** Listeners stop firing
///   across all attached Views immediately, and registered user data cleans up on the Renderer's
///   thread.
/// - **Borrowed handles don't keep the set attached.** Calling ulCreateDOMTriggersBorrowedRef()
///   returns a non-owning handle. Once all owning handles are destroyed, the set detaches and
///   subsequent registrations on borrowed handles return `false`.
/// - **Calling ulViewDetachDOMTriggers() detaches the set from a single View.** Other attached
///   Views remain active, and the set itself isn't destroyed.
///
/// For shared conventions covering handle states (Valid, Empty, and Gone), passing `NULL` handles,
/// error out-parameters, and calling DOM functions on the Renderer's thread, see
/// `<Ultralight/CAPI/CAPI_DOMDocument.h>`.
///
/// @see  `<Ultralight/dom/Triggers.h>`, ulViewAttachDOMTriggers(), ulDOMTriggersOn(),
///       ulDOMTriggersOnDOMReady(), ulDOMTriggersOnRestore(), ulViewSetDOMTriggersInjectionFilter()
///
#ifndef ULTRALIGHT_CAPI_DOMTRIGGERS_H
#define ULTRALIGHT_CAPI_DOMTRIGGERS_H

#include <Ultralight/CAPI/CAPI_DOMEvent.h>

#ifdef __cplusplus
extern "C" {
#endif

///
/// Opaque handle to a set of DOM listeners.
///
/// @see  ulCreateDOMTriggers(), ulDestroyDOMTriggers()
///
typedef struct C_DOMTriggers* ULDOMTriggers;

///
/// The callback invoked by a DOM-ready hook (see ulDOMTriggersOnDOMReady()) or a restore hook
/// (see ulDOMTriggersOnRestore()).
///
/// @param  user_data  The pointer you passed when registering the hook.
///
/// @param  document   The page's document. It's owned by the library and valid only during the
///                    callback (call ulCreateDOMDocumentRef() to keep it).
///
typedef void (*ULDOMTriggersReadyCallback)(void* user_data, ULDOMDocument document);

///
/// Details of a page passed to a View's DOM triggers filter (see
/// ULDOMTriggersInjectionFilterCallback).
///
/// New members may be added at the end in later versions, so don't create or copy this struct
/// yourself.
///
typedef struct ULDOMTriggersInjectionRequest {
  ///
  /// The View loading the page.
  ///
  ULView view;

  ///
  /// The set of DOM listeners being decided on. It's the handle ulCreateDOMTriggers()
  /// returned, so you can compare it with yours. It's valid only during the callback (call
  /// ulCreateDOMTriggersBorrowedRef() to keep it).
  ///
  ULDOMTriggers triggers;

  ///
  /// The page's origin, serialized (eg, "https://example.com" or "file://"). Opaque origins
  /// serialize as "null".
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
} ULDOMTriggersInjectionRequest;

///
/// Callback invoked to decide whether a set of DOM listeners is added to a page, after the
/// origin rules have been checked.
///
/// The verdict of the origin rules arrives in `request->rules_allow`-- your return value is
/// final in both directions. Use this for policy the origin rules can't express, such as a user
/// setting, or to allow a page with an opaque origin (eg, a sandboxed frame).
///
/// @param  user_data  The user data passed to ulViewSetDOMTriggersInjectionFilter().
///
/// @param  request    The set of DOM listeners, the page, and the verdict of the origin rules.
///                    It's owned by the library and valid only during the callback.
///
/// @return  Return true to add the listeners to the page, or false to skip it.
///
/// @note  Invoked on the Renderer's thread, for the main frame and (when the set is attached
///        with kULDOMTriggersAttachFlags_AllFrames) for subframes.
///
typedef bool (*ULDOMTriggersInjectionFilterCallback)(
    void* user_data, const ULDOMTriggersInjectionRequest* request);

///
/// Options for ulViewAttachDOMTriggers().
///
typedef enum {
  kULDOMTriggersAttachFlags_None = 0,       ///< Add the listeners to the main frame only.
  kULDOMTriggersAttachFlags_AllFrames = 1 << 1,  ///< Add the listeners to subframes too.
} ULDOMTriggersAttachFlags;

///
/// Create a new set of DOM listeners.
///
/// @return  Returns a new ULDOMTriggers instance. You must call ulDestroyDOMTriggers() when
///          finished.
///
ULExport ULDOMTriggers ulCreateDOMTriggers(void);

///
/// Add an owning reference to a set of DOM listeners.
///
/// @param  triggers   The set of DOM listeners.
///
/// @return  Returns an owning handle to the set (NULL if `triggers` is NULL). It's the same
///          handle ulCreateDOMTriggers() returned. You must call ulDestroyDOMTriggers() when
///          finished.
///
/// @note  Safe to call from any thread.
///
ULExport ULDOMTriggers ulCreateDOMTriggersRef(ULDOMTriggers triggers);

///
/// Add a reference to a set of DOM listeners that doesn't own it.
///
/// A borrowed handle works with every function in this header, but it doesn't keep the set
/// attached: once the last owning handle is destroyed, the set detaches from its Views as
/// ulDestroyDOMTriggers() describes, and the borrowed handle stays safe to use (registering
/// on it fails).
///
/// @param  triggers   The set of DOM listeners.
///
/// @return  Returns a new borrowed handle (NULL if `triggers` is NULL). You must call
///          ulDestroyDOMTriggers() when finished. It's a different handle than `triggers`, so
///          compare the handles you own instead.
///
/// @note  Safe to call from any thread.
///
ULExport ULDOMTriggers ulCreateDOMTriggersBorrowedRef(ULDOMTriggers triggers);

///
/// Destroy a handle to a set of DOM listeners.
///
/// The View keeps the set attached while an owning handle to it exists (one from
/// ulCreateDOMTriggers() or ulCreateDOMTriggersRef()). When you destroy the last one, the set
/// detaches from every View (its listeners stop right away), and the `destroy_user_data`
/// callbacks of its registrations run on the Renderer's thread: during this call when nothing
/// else refers to the set, or else during a later ulUpdate(). User data is never destroyed while
/// its callback runs.
///
/// @param  triggers   The set of DOM listeners (can be NULL).
///
/// @note  Safe to call from any thread.
///
ULExport void ulDestroyDOMTriggers(ULDOMTriggers triggers);

///
/// Add a listener for events on elements matching a CSS selector.
///
/// On every page that gets the set, the listener is added to the page's document like
/// ulDOMDocumentAddDelegatedEventListener(). The event target and its ancestors are tested
/// against `selector`, and `callback` gets the nearest match. This covers elements added to the
/// page later too.
///
/// @param  triggers           The set of DOM listeners.
///
/// @param  selector           The CSS selector to match, as a null-terminated UTF-8 string (eg,
///                            `#save`).
///
/// @param  type               The event type, as a null-terminated UTF-8 string (eg, `click`).
///
/// @param  flags              A bitwise OR of ULDOMEventFlags values.
///
/// @param  callback           The callback to invoke for each match (must not be NULL).
///
/// @param  user_data          Passed to `callback` unchanged.
///
/// @param  destroy_user_data  Called exactly once to destroy `user_data` (can be NULL): when the
///                            set's last owning handle is destroyed, or right away if this
///                            call fails.
///
/// @return  Returns true on success, or false if a pointer parameter is NULL, `selector` is
///          malformed (the library logs a warning saying why), or the set's last owning handle
///          was destroyed.
///
/// \parblock
/// @note  Events that don't bubble (eg, `focus`) reach the listener only with
///        kULDOMEventFlags_Capture. A wheel listener is passive unless you pass
///        kULDOMEventFlags_NotPassive.
/// \endparblock
///
/// \parblock
/// @note  With kULDOMEventFlags_Once, the first `type` event that reaches the document uses up
///        the listener on that page, even if no element matches.
/// \endparblock
///
/// \parblock
/// @note  Before a Renderer exists, only an empty `selector` is refused here. Each page checks
///        the selector when it gets the set instead, and skips a malformed one with a warning.
/// \endparblock
///
ULExport bool ulDOMTriggersOn(ULDOMTriggers triggers, const char* selector, const char* type,
                              unsigned int flags, ULDOMDelegatedEventCallback callback,
                              void* user_data,
                              ULUserDataDestroyCallback destroy_user_data);

///
/// Add a hook that runs on each page that gets the set, once its document has finished
/// parsing.
///
/// The hook runs after the set's listeners were added to the page, with or without JavaScript
/// enabled. With kULDOMTriggersAttachFlags_AllFrames it also runs for each subframe's document.
/// It doesn't run when a page is restored from the back-forward cache (see
/// ulDOMTriggersOnRestore()).
///
/// @param  triggers           The set of DOM listeners.
///
/// @param  callback           The hook (must not be NULL).
///
/// @param  user_data          Passed to `callback` unchanged.
///
/// @param  destroy_user_data  Called exactly once to destroy `user_data` (can be NULL): when the
///                            set's last owning handle is destroyed, or right away if this
///                            call fails.
///
/// @return  Returns true on success, or false if `triggers` or `callback` is NULL, or the set's
///          last owning handle was destroyed.
///
ULExport bool ulDOMTriggersOnDOMReady(ULDOMTriggers triggers,
                                      ULDOMTriggersReadyCallback callback, void* user_data,
                                      ULUserDataDestroyCallback destroy_user_data);

///
/// Add a hook that runs each time a page that gets the set is restored from the back-forward
/// cache (see ulConfigSetPageCacheSize()).
///
/// The hook runs after the set's listeners were added to the page again, and before the page's
/// `pageshow` event. It never runs on a normal load (see ulDOMTriggersOnDOMReady()).
///
/// @param  triggers           The set of DOM listeners.
///
/// @param  callback           The hook (must not be NULL).
///
/// @param  user_data          Passed to `callback` unchanged.
///
/// @param  destroy_user_data  Called exactly once to destroy `user_data` (can be NULL): when the
///                            set's last owning handle is destroyed, or right away if this
///                            call fails.
///
/// @return  Returns true on success, or false if `triggers` or `callback` is NULL, or the set's
///          last owning handle was destroyed.
///
ULExport bool ulDOMTriggersOnRestore(ULDOMTriggers triggers,
                                     ULDOMTriggersReadyCallback callback, void* user_data,
                                     ULUserDataDestroyCallback destroy_user_data);

///
/// Attach a set of DOM listeners to a View.
///
/// The library adds the listeners to the View's current page (if its document has finished
/// parsing and the origin rules allow it) and to every page the View loads afterward. The View
/// keeps the set attached while an owning handle to it exists (see ulDestroyDOMTriggers()).
/// Attaching a set that's already attached replaces its flags and origin rules for later pages.
///
/// `origin_rules` lists the pages that get the listeners. The rules work like those of
/// ulViewAttachJSAPI() (see that function for the syntax). With no rules, only your
/// application's own content gets the listeners (local `file://` pages, pages loaded with
/// ulViewLoadHTML() or ulViewLoadHTMLWithParams(), and subframes with the same origin as those
/// pages). Passing any rules replaces that default entirely. Use
/// ulViewSetDOMTriggersInjectionFilter() for decisions the rules can't express.
///
/// When a page doesn't get the listeners, the library logs why when DOM diagnostics are on.
/// With them off, it logs the first page each View keeps from the set (at Info), so a page
/// served by a development server (eg, `http://localhost:5173`, which needs a rule such as
/// `http://localhost:*`) doesn't fail silently.
///
/// @param  view              The View.
///
/// @param  triggers          The set of DOM listeners to attach.
///
/// @param  flags             A bitwise OR of ULDOMTriggersAttachFlags values. Unknown bits fail
///                           the attach.
///
/// @param  origin_rules      An array of `num_origin_rules` rules, or NULL for the default
///                           rules.
///
/// @param  num_origin_rules  The number of entries in `origin_rules` (0 for the default rules).
///
/// @return  Returns true on success. Returns false if `view` or `triggers` is NULL, `flags` has
///          an unknown bit, `origin_rules` is NULL but `num_origin_rules` isn't 0, a rule fails
///          to parse, or the set's last owning handle was destroyed. Nothing changes then, and
///          a logged warning says why.
///
/// @note  Without kULDOMTriggersAttachFlags_AllFrames, only the main frame gets the listeners.
///
ULExport bool ulViewAttachDOMTriggers(ULView view, ULDOMTriggers triggers, unsigned int flags,
                                      const char* const* origin_rules,
                                      size_t num_origin_rules);

///
/// Detach a set of DOM listeners from a View.
///
/// The listeners are removed from the View's pages right away, and no later page gets them.
///
/// @param  view       The View.
///
/// @param  triggers   The set of DOM listeners to detach.
///
/// @note  If you attach the set again, the current page gets it again (and its DOM-ready hooks
///        run again there).
///
ULExport void ulViewDetachDOMTriggers(ULView view, ULDOMTriggers triggers);

///
/// Set the View's filter for deciding which pages get each set of DOM listeners.
///
/// The View consults the filter after the origin rules, every time an attached set could be
/// added to a page.
///
/// @param  view               The View.
///
/// @param  callback           The filter (see ULDOMTriggersInjectionFilterCallback), or NULL to
///                            remove the current one.
///
/// @param  user_data          Opaque user data passed to every invocation of `callback`.
///
/// @param  destroy_user_data  Callback invoked exactly once to destroy `user_data` when the
///                            filter is replaced or removed, or the View is destroyed (may be
///                            NULL). When `callback` is NULL, it runs immediately.
///
ULExport void ulViewSetDOMTriggersInjectionFilter(ULView view,
                                                  ULDOMTriggersInjectionFilterCallback
                                                      callback,
                                                  void* user_data,
                                                  ULUserDataDestroyCallback destroy_user_data);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // ULTRALIGHT_CAPI_DOMTRIGGERS_H
