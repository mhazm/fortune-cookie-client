///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// @file CAPI_JSRuntime.h
///
/// JavaScript runtime functions for task posting, background work, and weak references.
///
/// `#include <Ultralight/CAPI/CAPI_JSRuntime.h>`
///
/// @note  This API is a preview and may still change after 2.0.
///
/// JavaScript contexts and values must stay on the Renderer's thread, so background threads can't
/// touch them directly. This header provides the runtime functions that coordinate work between
/// background threads and a page's JavaScript environment.
///
/// This example sets a page property from a background thread with a posted task:
///
/// ```c
/// #include <stdlib.h>
///
/// /* The Renderer's thread, during a later ulUpdate(). */
/// static void ShowProgress(void* user_data, ULJSContext ctx) {
///   ULJSValue global = ulJSContextGetGlobalObject(ctx);
///   ULJSValue progress = ulCreateJSValueNumber(ctx, *(double*)user_data);
///   ulJSObjectSetProperty(global, "progress", progress,
///                         kULJSPropertyAttributes_None, NULL);
///   ulDestroyJSValue(progress);
///   ulDestroyJSValue(global);
/// }
///
/// /* Any thread (eg, a download thread). */
/// void ReportProgress(ULJSContext ctx, double fraction) {
///   double* value = (double*)malloc(sizeof(double));
///   *value = fraction;
///   ulJSContextPostTask(ctx, ShowProgress, value, free);
/// }
/// ```
///
/// ## When Callbacks Run
///
/// Deferred work follows these rules:
///
/// - **Callbacks run on the Renderer's thread during a later ulUpdate().** This applies to posted
///   tasks, worker completion callbacks, and collected callbacks.
/// - **Posted tasks and collected callbacks are skipped if their context goes away first.** The
///   library still invokes `destroy_user_data` so allocated state doesn't leak.
/// - **A context goes away when its page navigates away, its frame is removed, or its View or
///   Renderer is destroyed.** This includes pages entering the back-forward cache (a restored page
///   receives a fresh context).
///
/// ## Background Work
///
/// Call ulJSRunOnWorker() to perform blocking operations without stalling the Renderer's thread.
/// The call schedules work across two callbacks:
///
/// - **The `work` callback runs on a background worker thread.** Run slow tasks like file reads or
///   calculations here using plain native data.
/// - **The `complete` callback runs on the Renderer's thread during a later ulUpdate().** Use the
///   results computed in `work` here to touch JavaScript or update page state.
///
/// You should avoid long-running operations that occupy a worker thread for an extended time
/// because the worker pool is shared across the entire library.
///
/// @warning  Never touch JavaScript inside a worker's `work` callback (contexts and values can't be
///           created or read off the Renderer's thread).
///
/// ## Weak References
///
/// Unlike a ULJSValue handle, a weak reference doesn't keep its JavaScript object alive, allowing
/// the garbage collector to reclaim it. Use weak references for caches and observers where native
/// code needs to watch an object without preventing collection.
///
/// @warning  Never use a weak reference as the only reference to a page callback (if the page
///           doesn't hold the function, the garbage collector reclaims it).
///
/// @note  For handle ownership rules, `NULL` handling, user data cleanup, and threading conventions
///        across the JavaScript C API, see CAPI_JSValue.h.
///
/// @see  ulJSContextPostTask(), ulJSRunOnWorker(), ulCreateJSWeakRef(),
///       ulJSContextAddDestroyedCallback(), ulJSContextGetHandleStats()
///
#ifndef ULTRALIGHT_CAPI_JSRUNTIME_H
#define ULTRALIGHT_CAPI_JSRUNTIME_H

#include <Ultralight/CAPI/CAPI_JSValue.h>

#ifdef __cplusplus
extern "C" {
#endif

///
/// Opaque handle to a weak reference to a JavaScript object.
///
/// @see  ulCreateJSWeakRef(), ulDestroyJSWeakRef()
///
typedef struct C_JSWeakRef* ULJSWeakRef;

///
/// Callback invoked on the Renderer's thread with a live context (for a posted task or a
/// collected callback).
///
/// @param  user_data  The user data you passed along with the callback.
///
/// @param  ctx        The context (owned by the library, live during the callback).
///
typedef void (*ULJSTaskCallback)(void* user_data, ULJSContext ctx);

///
/// Callback invoked on the Renderer's thread when a context goes away.
///
/// The context is already gone. Use this to clear caches and destroy handles, not to run
/// script.
///
/// @param  user_data  The user data you passed to ulJSContextAddDestroyedCallback().
///
/// @param  ctx        The context that went away (owned by the library).
///
typedef void (*ULJSContextDestroyedCallback)(void* user_data, ULJSContext ctx);

///
/// Callback for ulJSRunOnWorker(): `work` runs on a background thread and `complete` runs on
/// the Renderer's thread.
///
/// @param  user_data  The user data you passed to ulJSRunOnWorker().
///
/// @warning
/// \parblock
/// Don't touch JavaScript in `work` (values can't be created or read off the Renderer's
/// thread). Compute a result there and use it in `complete`.
///
/// Don't let C++ exceptions escape either callback, since nothing catches them across the C
/// boundary.
/// \endparblock
///
typedef void (*ULJSWorkerCallback)(void* user_data);

/******************************************************************************
 * Task posting
 *****************************************************************************/

///
/// Post a task to run with a live context during a later ulUpdate().
///
/// Use this to touch JavaScript from another thread. Tasks run in the order they were posted.
/// If the context goes away before the task runs, the task is skipped.
///
/// @param  ctx                The context (you can destroy your handle right after posting).
///
/// @param  task               The callback to invoke on the Renderer's thread.
///
/// @param  user_data          Opaque user data passed to `task`.
///
/// @param  destroy_user_data  Callback invoked exactly once to destroy `user_data` after the
///                            task runs or is skipped (may be NULL).
///
/// @note  Safe to call from any thread.
///
ULExport void ulJSContextPostTask(ULJSContext ctx, ULJSTaskCallback task, void* user_data,
                                  ULUserDataDestroyCallback destroy_user_data);

/******************************************************************************
 * Background work
 *****************************************************************************/

///
/// Run blocking work on a background thread, then a completion callback on the Renderer's
/// thread.
///
/// Use this for slow work like file reads or database queries. Do the work in `work` without
/// touching JavaScript, then use the result in `complete` (eg, to settle a promise).
///
/// The work runs on a small pool of background threads shared by the library (up to 4 threads,
/// fewer on machines with few cores), so work that blocks for a long time delays the work queued
/// behind it. `complete` runs during a later ulUpdate() after `work` returns, even if
/// pages navigate or Views are destroyed in between.
///
/// @param  work               The callback to invoke on a background thread.
///
/// @param  complete           The callback to invoke on the Renderer's thread after `work`
///                            returns (may be NULL).
///
/// @param  user_data          Opaque user data passed to `work` and `complete`.
///
/// @param  destroy_user_data  Callback invoked exactly once to destroy `user_data` after
///                            `complete` runs or the work is discarded (may be NULL).
///
/// @pre  Requires a living Renderer. Work submitted while none exists, or still queued when the
///       Renderer is destroyed, is discarded (only `destroy_user_data` runs). Work already
///       running when the Renderer is destroyed finishes, and `complete` runs during teardown.
///
/// @note  Safe to call from any thread, including a worker (so work can queue more work).
///
ULExport void ulJSRunOnWorker(ULJSWorkerCallback work, ULJSWorkerCallback complete,
                              void* user_data, ULUserDataDestroyCallback destroy_user_data);

/******************************************************************************
 * Weak references
 *****************************************************************************/

///
/// Create a weak reference to a JavaScript object.
///
/// Unlike a ULJSValue handle, a weak reference doesn't keep its object alive, so the garbage
/// collector can reclaim the object. Use this for caches and observers.
///
/// @param  object  The object to observe.
///
/// @return  Returns a new ULJSWeakRef (NULL if `object` isn't an object, is NULL, or its page
///          is gone). You must call ulDestroyJSWeakRef() when finished.
///
ULExport ULJSWeakRef ulCreateJSWeakRef(ULJSValue object);

///
/// Get a handle to the object a weak reference observes.
///
/// @param  weak  The weak reference.
///
/// @return  Returns a new ULJSValue handle, or NULL if the object was collected or its page is
///          gone. You must call ulDestroyJSValue() when finished.
///
ULExport ULJSValue ulJSWeakRefLock(ULJSWeakRef weak);

///
/// Set a callback to run after the object is garbage collected.
///
/// The callback runs once on the Renderer's thread during a later ulUpdate(). Setting a new
/// callback replaces the old one (its user data is destroyed right away). If the page goes away
/// or the weak reference is destroyed before the callback runs, it's skipped.
///
/// @param  weak               The weak reference.
///
/// @param  callback           The callback to invoke after collection (NULL to clear).
///
/// @param  user_data          Opaque user data passed to `callback`.
///
/// @param  destroy_user_data  Callback invoked exactly once to destroy `user_data` after the
///                            callback runs or is skipped (may be NULL).
///
ULExport void ulJSWeakRefSetCollectedCallback(ULJSWeakRef weak, ULJSTaskCallback callback,
                                              void* user_data,
                                              ULUserDataDestroyCallback destroy_user_data);

///
/// Destroy a weak reference (NULL-safe).
///
/// A pending collected callback is skipped (its user data is still destroyed).
///
/// @param  weak  The weak reference to destroy.
///
/// @note  Safe to call from any thread. Off the Renderer's thread, this waits for a collected
///        callback that's already running, so once it returns the callback won't run.
///
ULExport void ulDestroyJSWeakRef(ULJSWeakRef weak);

/******************************************************************************
 * Context teardown notification
 *****************************************************************************/

///
/// Add a callback that runs on the Renderer's thread when a context goes away.
///
/// A context goes away when its page navigates away (including into the back-forward cache), its
/// frame is removed, or its View or Renderer is destroyed. Language bindings can use this to
/// clear their caches right away. If the context is already gone, the callback is skipped and
/// `destroy_user_data` runs right away.
///
/// @param  ctx                The context.
///
/// @param  callback           The callback to invoke when the context goes away.
///
/// @param  user_data          Opaque user data passed to `callback`.
///
/// @param  destroy_user_data  Callback invoked exactly once to destroy `user_data` after the
///                            callback runs or is skipped (may be NULL).
///
/// @note  A page restored from the back-forward cache gets a new context, so this callback means
///        this context is over, not that the page is gone for good.
///
ULExport void ulJSContextAddDestroyedCallback(ULJSContext ctx,
                                              ULJSContextDestroyedCallback callback,
                                              void* user_data,
                                              ULUserDataDestroyCallback destroy_user_data);

/******************************************************************************
 * Handle stats
 *****************************************************************************/

///
/// Counts of the live handles on one context, for finding leaks.
///
/// Value handles keep counting after the page is gone, until you destroy them. Copies made with
/// ulCreateJSValueRef() count once, the counts include a few handles the library holds for you,
/// and a destroyed handle can stay counted until the next ulUpdate().
///
typedef struct {
  size_t total_values;                     ///< Live value handles.
  size_t values_by_type[kULJSType_Count];  ///< Live value handles, indexed by ULJSType.
  size_t weak_refs;                        ///< Live weak references (0 once the context is gone).
} ULJSHandleStats;

///
/// Count the live handles on a context.
///
/// Value handles that are never destroyed keep counting after the page navigates away, which is
/// how a leak shows up.
///
/// @param  ctx    The context.
///
/// @param  stats  Receives the counts.
///
/// @return  Returns true on success (false if `ctx` or `stats` is NULL).
///
ULExport bool ulJSContextGetHandleStats(ULJSContext ctx, ULJSHandleStats* stats);

#ifdef __cplusplus
}
#endif

#endif  // ULTRALIGHT_CAPI_JSRUNTIME_H
