///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///

///
/// @file CAPI_Renderer.h
///
/// Core renderer singleton for the library, coordinates all library functions.
///
/// `#include <Ultralight/CAPI/CAPI_Renderer.h>`
/// 
/// The Renderer class is responsible for creating and painting Views, managing Sessions, as well
/// as coordinating network requests, events, JavaScript execution, and more.
/// 
/// ## Creating the Renderer
///
/// \parblock
/// @note  A Renderer will be created for you automatically when you call ulCreateApp (access it
///        via ulAppGetRenderer()).
/// \endparblock
///
/// \parblock
/// @note  ulCreateApp() is part of the AppCore API and automatically manages window creation, run
///        loop, input, painting, and most platform-specific functionality. (Available on desktop
///        platforms only)
/// \endparblock
///
/// ### Defining Platform Handlers
///
/// Before creating the Renderer, you should define your platform handlers via the Platform
/// singleton (see CAPI_Platform.h). This can be used to customize file loading, font loading,
/// clipboard access, and other functionality typically provided by the OS.
///
/// Default implementations for most platform handlers ship as Zlib-licensed source in the SDK's
/// `platform` folder. You can use these stock implementations by copying the code into your
/// project, or you can write your own.
///
/// At a minimum, you must provide a ULFileSystem and a ULFontLoader. Without them, the library
/// exits the process with an error when it first needs them.
/// 
/// ### Creating the Renderer
///
/// Once you've set up the Platform handlers you can create the Renderer by calling
/// `ulCreateRenderer()`.
///
/// @par Example creation code
/// ```
///  // Setup our config.
///  ULConfig config = ulCreateConfig();
///
///  // Use the default platform font loader.
///  ulEnablePlatformFontLoader();
///
///  // Use the default platform file system to load file:/// URLs from the OS.
///  ULString base_dir = ulCreateString("./assets/");
///  ulEnablePlatformFileSystem(base_dir);
///  ulDestroyString(base_dir);
///
///  // Create the renderer.
///  ULRenderer renderer = ulCreateRenderer(config);
///
///  // Destroy the config.
///  ulDestroyConfig(config);
///
///  // Set up Views here...
/// ```
/// 
/// ## Updating Renderer Logic
/// 
/// You should call ulUpdate() from your main update loop as often as possible to give the
/// library an opportunity to dispatch events and timers:
/// 
/// @par Example update code
/// ```
/// void mainLoop()
/// {
///   while(true)
///   {  
///     // Update program logic here
///     ulUpdate(renderer);
///   }
/// }
/// ```
///
/// ## Rendering Each Frame
///
/// When your program is ready to display a new frame (usually in sync with the monitor's
/// refresh rate), you should call ulRefreshDisplay() and ulRender() so the library can render
/// all active Views as needed.
///
/// Animations and smooth scrolling advance with each ulRefreshDisplay() call. Call it once per
/// frame you present (once per display refresh for a vsynced loop, or once per rendered frame for
/// a vsync-off or variable-refresh game loop).
///
/// @par Example per-frame render code
/// ```
/// void displayFrame()
/// {
///   // Notify the renderer that the main display has refreshed. This updates animations,
///   // smooth scrolling, and window.requestAnimationFrame() for all Views on that display.
///   ulRefreshDisplay(renderer, 0);
///
///   // Render all Views as needed
///   ulRender(renderer);
///
///   // Each View renders to a
///   //   - Pixel-Buffer Surface (ulViewGetSurface())
///   //     or
///   //   - GPU texture (ulViewGetRenderTarget())
///   // based on whether CPU or GPU rendering is used.
///   //
///   // You will need to display the image data here as needed.
/// }
/// ```
///
#ifndef ULTRALIGHT_CAPI_RENDERER_H
#define ULTRALIGHT_CAPI_RENDERER_H

#include <Ultralight/CAPI/CAPI_Defines.h>

#ifdef __cplusplus
extern "C" {
#endif

///
/// Create the core renderer singleton for the library.
///
/// You should set up the Platform singleton (see CAPI_Platform.h) before calling this function.
///
/// @param  config  The configuration to use for the renderer.
///
/// @return  Returns the new renderer instance, or NULL on failure. You must call
///          ulDestroyRenderer() when finished.
///
/// \parblock
/// @note  You do not need to call this if you're using ulCreateApp() from AppCore.
/// \endparblock
///
/// \parblock
/// @warning  You must define a ULFontLoader and a ULFileSystem in the Platform singleton.
///           Without them, the library exits the process with an error when it first needs
///           them.
/// \endparblock
///
/// \parblock
/// @warning  You should only create one Renderer during the lifetime of your program.
/// \endparblock
///
ULExport ULRenderer ulCreateRenderer(ULConfig config);

///
/// Destroy a renderer previously created with ulCreateRenderer().
///
/// @param  renderer  The renderer instance to destroy (can be NULL).
///
ULExport void ulDestroyRenderer(ULRenderer renderer);

///
/// Update timers and dispatch internal callbacks (JavaScript and network).
///
/// @param  renderer  The active renderer instance.
///
ULExport void ulUpdate(ULRenderer renderer);

/// ## Rendering Each Frame
/// 
/// When your program is ready to display a new frame (usually in synchrony with the monitor
/// refresh rate), you should call ulRefreshDisplay() and ulRender() so the library can render
/// all active Views as needed.
///
/// Animations and smooth scroll advance to each ulRefreshDisplay() call. Call it once per frame
/// you present (once per display refresh for a vsynced loop, or once per rendered frame for a
/// vsync-off or variable-refresh game loop).
///

///
/// Notify the renderer that a display has refreshed.
///
/// Call this once per frame you present, for each display (usually right after vsync). It
/// advances animations, smooth scrolling, and `window.requestAnimationFrame()` for the Views on
/// that display, so they can repaint during the next ulRender().
///
/// @param  renderer    The active renderer instance.
///
/// @param  display_id  The id of the display that refreshed (see ulViewConfigSetDisplayId()).
///
/// \parblock
/// @note  Keep calling this even while ulViewGetNeedsPaint() returns false: animations only
///        request a repaint from this call.
/// \endparblock
///
/// \parblock
/// @note  Call this on the thread the renderer was created on.
/// \endparblock
///
ULExport void ulRefreshDisplay(ULRenderer renderer, unsigned int display_id);

///
/// Notify the renderer that a display has refreshed, and when the resulting frame will be
/// shown.
///
/// This works like ulRefreshDisplay(), but animation time follows your timestamps instead of the
/// wall clock, so animations are timed for the moment the frame reaches the screen.
///
/// @param  renderer          The active renderer instance.
///
/// @param  display_id        The id of the display that refreshed.
///
/// @param  target_timestamp  When this frame will be shown, in seconds. Use the system's
///                           monotonic clock (eg, the target time your display link reports),
///                           or your own timeline after calling
///                           ulRendererSetDisplayUsesCustomClock(). Timestamps must never
///                           decrease.
///
/// @note  `performance.now()` stays on the wall clock, so pages that compare it to
///        `window.requestAnimationFrame()` timestamps will see the difference.
///
/// @see  ulRendererSetDisplayRefreshRate(), ulRendererSetDisplayUsesCustomClock()
///
ULExport void ulRefreshDisplayWithTimestamp(ULRenderer renderer, unsigned int display_id,
                                            double target_timestamp);

///
/// Declare a display's refresh rate.
///
/// Pages see the declared rate through `window.requestAnimationFrame()` scheduling. Call this
/// when the display's mode changes (eg, a variable-rate display switching between 120 Hz and
/// 48 Hz). Small changes are absorbed, and a change reaches pages at most once a second.
///
/// @param  renderer      The active renderer instance.
///
/// @param  display_id    The id of the display.
///
/// @param  refresh_rate  The display mode's nominal refresh rate in Hz (eg, 60.0, 59.94,
///                       120.0), or 0 to clear it (the library then measures how often
///                       ulRefreshDisplay() is called).
///
/// @note  Call this on the thread the renderer was created on.
///
ULExport void ulRendererSetDisplayRefreshRate(ULRenderer renderer, unsigned int display_id,
                                              double refresh_rate);

///
/// Get the refresh rate declared for a display.
///
/// @param  renderer  The active renderer instance.
///
/// @param  display_id  The display ID.
///
/// @return  Returns the display's declared refresh rate (in Hz), or 0 if
///          ulRendererSetDisplayRefreshRate() was never called for it.
///
ULExport double ulRendererGetDisplayRefreshRate(ULRenderer renderer, unsigned int display_id);

///
/// Declare that a display's refresh timestamps are on your own timeline rather than the system
/// clock (Default = False).
///
/// By default, the timestamps you pass to ulRefreshDisplayWithTimestamp() are treated as
/// readings of the system's monotonic clock. Declare a custom clock when they're on a timeline
/// of your own (eg, a media timeline for offline rendering, or simulation time): animation then
/// follows your timestamps exactly.
///
/// @param  renderer           The active renderer instance.
///
/// @param  display_id         The id of the display.
///
/// @param  uses_custom_clock  True if timestamps for this display are on your own timeline.
///
/// @pre  Call this before the display's first timed refresh.
///
/// @note  Call this on the thread the renderer was created on.
///
ULExport void ulRendererSetDisplayUsesCustomClock(ULRenderer renderer, unsigned int display_id,
                                                  bool uses_custom_clock);

///
/// Get whether a display was declared to use a custom clock.
///
/// @param  renderer  The active renderer instance.
///
/// @param  display_id  The display ID.
///
/// @return  Returns the value last passed to ulRendererSetDisplayUsesCustomClock() for the
///          display, or false if it was never called.
///
ULExport bool ulRendererGetDisplayUsesCustomClock(ULRenderer renderer, unsigned int display_id);

///
/// Render all active Views to their respective surfaces and render targets.
///
/// @param  renderer  The active renderer instance.
///
ULExport void ulRender(ULRenderer renderer);

typedef void (*ULRendererTaskCallback)(void* user_data);

///
/// Post a task to run on the Renderer's thread during a future call to ulUpdate().
///
/// You can use this to hand work from your other threads to the thread that drives the Renderer
/// (the only thread that may touch Views and the DOM API).
///
/// Tasks run in posting order during the next ulUpdate() after they are posted. A task posted
/// while ulUpdate() is draining tasks runs at the following ulUpdate().
///
/// @param  renderer           The active renderer instance.
///
/// @param  task               Invoked once on the Renderer's thread with `user_data`. A NULL
///                            task posts nothing; `destroy_user_data` then runs at once.
///
/// @param  user_data          Pointer passed through to `task` (can be NULL).
///
/// @param  destroy_user_data  Invoked exactly once after `task` runs, or without `task` running
///                            if the Renderer is destroyed first or `renderer` is NULL (can be
///                            NULL).
///
/// \parblock
/// @note  Safe to call from any thread.
/// \endparblock
///
/// \parblock
/// @note  With AppCore, ulAppPostTask() posts to this same queue and also wakes the app's loop,
///        so prefer it there.
/// \endparblock
///
ULExport void ulRendererPostTask(ULRenderer renderer, ULRendererTaskCallback task, void* user_data,
                                 ULUserDataDestroyCallback destroy_user_data);

///
/// Post a task to run on the Renderer's thread once a delay has elapsed.
///
/// The delay is measured against the ulUpdate() cadence rather than a background timer. The task
/// runs during the first ulUpdate() at or after the deadline, so delivery waits while ulUpdate()
/// is not being called (for example, a paused application). Tasks whose deadlines fall in the
/// same ulUpdate() run in posting order.
///
/// @param  renderer           The active renderer instance.
///
/// @param  delay_ms           Minimum delay before the task may run, in milliseconds.
///
/// @param  task               Invoked once on the Renderer's thread with `user_data`.
///
/// @param  user_data          Pointer passed through to `task` (can be NULL).
///
/// @param  destroy_user_data  Invoked exactly once after `task` runs, or without `task` running
///                            if the Renderer is destroyed first or `renderer` is NULL (can be
///                            NULL).
///
/// @see  ulRendererPostTask()
///
ULExport void ulRendererPostDelayedTask(ULRenderer renderer, double delay_ms,
                                        ULRendererTaskCallback task, void* user_data,
                                        ULUserDataDestroyCallback destroy_user_data);

///
/// Controls how aggressively ulRecycle() reclaims memory.
///
/// @see  Renderer::RecycleMode
///
typedef enum {
  ///
  /// A quick recycle, cheap enough to call every frame (the same work the automatic recycler
  /// does).
  ///
  kRecycleMode_Lightweight = 0,

  ///
  /// A thorough recycle, for idle periods or transitions (eg, a loading screen). A call within a
  /// second of the last one does nothing.
  ///
  kRecycleMode_Full,
} ULRecycleMode;

///
/// Recycle internal caches and memory.
///
/// @param  renderer  The active renderer instance.
///
/// @param  mode      How aggressively to reclaim memory.
///
/// @see  Renderer::Recycle()
///
ULExport void ulRecycle(ULRenderer renderer, ULRecycleMode mode);

///
/// Attempt to release as much memory as possible.
///
/// This also discards all compiled JavaScript code, so pages run slower for a while as it's
/// recompiled.
///
/// @param  renderer  The active renderer instance.
///
/// @warning  Don't call this while the renderer is rendering (eg, from a GPU driver callback):
///           the rendering caches can't be released then, and are skipped with a warning.
///
ULExport void ulPurgeMemory(ULRenderer renderer);

///
/// Print detailed memory usage statistics to the log.
///
/// @param  renderer  The active renderer instance.
///
/// @see  ulPlatformSetLogger()
///
ULExport void ulLogMemoryUsage(ULRenderer renderer);

///
/// Start the remote inspector server.
///
/// While it runs, another Ultralight app (on the same machine or over the network) can inspect
/// this renderer's Views by loading this URL in a View:
///
/// \code
///   inspector://<ADDRESS>:<PORT>
/// \endcode
///
/// @param  renderer  The active renderer instance.
///
/// @param  address   The address for the server to listen on (eg, "127.0.0.1")
///
/// @param  port      The port for the server to listen on (eg, 9222)
///
/// @return  Returns whether the server started successfully or not.
///
/// @pre  Not available in the Free edition (always returns false there).
///
ULExport bool ulStartRemoteInspectorServer(ULRenderer renderer, const char* address,
                                           unsigned short port);

///
/// Describe the details of a gamepad, to be used with ulFireGamepadEvent and related
/// events below. This can be called multiple times with the same index if the details change.
///
/// @param  renderer      The active renderer instance.
///
/// @param  index         The gamepad's connection slot, and the array position the page sees. A
///                       gamepad described at index 1 arrives as `navigator.getGamepads()[1]`
///                       with `gamepad.index` 1, leaving slot 0 `null`. Number your controllers
///                       from 0.
///
/// @param  id            A string ID representing the device, this will be made available
///                       in JavaScript as gamepad.id
///
/// @param  axis_count    The number of axes on the device.
///
/// @param  button_count  The number of buttons on the device.
///
ULExport void ulSetGamepadDetails(ULRenderer renderer, unsigned int index, ULString id,
                                  unsigned int axis_count, unsigned int button_count);

///
/// Fire a gamepad event (connection / disconnection).
///
/// @note  The gamepad should first be described via ulSetGamepadDetails before calling this
///        function.
///
/// @param  renderer  The active renderer instance.
///
/// @param  evt       The event to fire.
///
/// @see  <https://developer.mozilla.org/en-US/docs/Web/API/Gamepad>
///
ULExport void ulFireGamepadEvent(ULRenderer renderer, ULGamepadEvent evt);

///
/// Fire a gamepad axis event (to be called when an axis value is changed).
///
/// @note  The gamepad should be connected via a previous call to ulFireGamepadEvent.
///
/// @param  renderer  The active renderer instance.
///
/// @param  evt       The event to fire.
///
/// @see  <https://developer.mozilla.org/en-US/docs/Web/API/Gamepad/axes>
///
ULExport void ulFireGamepadAxisEvent(ULRenderer renderer, ULGamepadAxisEvent evt);

///
/// Fire a gamepad button event (to be called when a button value is changed).
///
/// @note  The gamepad should be connected via a previous call to ulFireGamepadEvent.
///
/// @param  renderer  The active renderer instance.
///
/// @param  evt       The event to fire.
///
/// @see  <https://developer.mozilla.org/en-US/docs/Web/API/Gamepad/buttons>
///
ULExport void ulFireGamepadButtonEvent(ULRenderer renderer, ULGamepadButtonEvent evt);

///
/// Render a subset of Views to their respective surfaces and render targets.
///
/// @param  renderer        The active renderer instance.
///
/// @param  view_array      A C-array of ULView handles to render.
///
/// @param  view_array_len  The number of elements in the array.
///
/// @deprecated  Use ulRender() instead. To stop a View from painting, pause it with
///              ulViewSetVisible() (ulRender() skips hidden Views).
///
ULExport UL_DEPRECATED("Use ulRender() instead.") void ulRenderOnly(ULRenderer renderer, ULView* view_array, unsigned int view_array_len);

///
/// Get formatted memory usage statistics as a string.
///
/// This is the same information ulLogMemoryUsage() prints, in a form you can show in a debug
/// overlay or capture programmatically.
///
/// @param  renderer  The active renderer instance.
///
/// @return  Returns a new ULString holding the memory usage report. You must call
///          ulDestroyString() when finished.
///
ULExport ULString ulGetMemoryUsage(ULRenderer renderer);

///
/// Set the system color scheme that Views report to pages via the `prefers-color-scheme` CSS
/// media feature. (Default = kColorScheme_Light)
///
/// The library never reads the OS setting itself, so the scheme stays Light until you call this.
///
/// A change re-evaluates `prefers-color-scheme` media queries in every View whose preferred color
/// scheme is kColorScheme_Auto. Views pinned to Light or Dark are unaffected.
///
/// @param  renderer  The active renderer instance.
///
/// @param  scheme    kColorScheme_Light or kColorScheme_Dark. kColorScheme_Auto or an
///                   out-of-range value logs a warning and is ignored.
///
/// @see  ulViewSetPreferredColorScheme()
///
ULExport void ulRendererSetSystemColorScheme(ULRenderer renderer, ULColorScheme scheme);

///
/// Get the current system color scheme.
///
/// @param  renderer  The active renderer instance.
///
/// @return  Returns the scheme last set with ulRendererSetSystemColorScheme(), or
///          kColorScheme_Light if it was never called.
///
ULExport ULColorScheme ulRendererGetSystemColorScheme(ULRenderer renderer);

#if UL_HAS(RENDER_TRACE)
///
/// Begin recording a render trace to the specified file path.
///
/// @param  renderer  The active renderer instance.
///
/// @param  path      Output file path (eg, "render_trace.perfetto-trace").
///
/// @param  verbose   If true, enables verbose mode (args + events in addition to scopes).
///
/// @return  Returns whether or not the trace started (false if a trace is already active or the
///          trace file can't be opened).
///
/// @pre  Requires the Enterprise edition or higher.
///
ULExport bool ulRendererBeginRenderTrace(ULRenderer renderer, const char* path, bool verbose);

///
/// End the current render trace.
///
/// The trace file is finished and closed shortly after this returns (on the next ulUpdate()).
///
/// @param  renderer  The active renderer instance.
///
/// @pre  Requires the Enterprise edition or higher.
///
ULExport void ulRendererEndRenderTrace(ULRenderer renderer);

///
/// Check if a render trace is currently being recorded.
///
/// @param  renderer  The active renderer instance.
///
/// @return  Returns whether a render trace session is active.
///
/// @pre  Requires the Enterprise edition or higher.
///
ULExport bool ulRendererIsRenderTraceActive(ULRenderer renderer);
#endif // UL_HAS(RENDER_TRACE)

#if UL_HAS(GC_LISTENER)
///
/// How much compiled JavaScript code a collection discards.
///
/// Compiled code keeps some JavaScript objects alive, so discarding it lets a collection free more
/// memory. Pages then run slower for a while as their code is compiled again.
///
/// @see  ulRendererCollectNow(), ulRecycleEx()
///
typedef enum {
  ///
  /// Keep all compiled code.
  ///
  kCodeDropMode_Nothing = 0,

  ///
  /// Discard compiled code but keep parsed scripts (pages recompile but don't re-parse).
  ///
  kCodeDropMode_Linked,

  ///
  /// Discard compiled code and parsed scripts (pages re-parse and recompile).
  ///
  kCodeDropMode_All,
} ULCodeDropMode;

///
/// A snapshot of the JavaScript heap's state.
///
/// @see  ulRendererGetGCStatus()
///
typedef struct {
  ///
  /// How much reclaimable memory has built up, from 0 (none) to 1 (a collection is overdue).
  ///
  double pressure;

  ///
  /// Milliseconds since the last full collection (0 before the first one).
  ///
  double time_since_last_collect_ms;

  ///
  /// The JavaScript heap's in-use size in bytes (it drops when a collection frees memory).
  ///
  unsigned long long heap_bytes;
} ULGCInfo;

///
/// Called when the engine wants to run a collection on your thread.
///
/// The Renderer collects JavaScript garbage continuously in the background on other threads, so
/// most collection work never touches your thread. Occasionally it needs a larger collection that
/// runs on the thread that calls ulUpdate() and pauses it while it runs.
///
/// Your application knows better than the engine when a pause won't be noticed (eg, a loading
/// screen or a quiet moment in your update loop). So the engine asks first, when there's been no
/// input for a while (see ulConfigSetIdleGCEnabled()). One idle period can bring two requests: a
/// lighter one first, then, if the app stays idle, one that drops more compiled code. A request you
/// don't act on isn't offered again until new input ends the idle period.
///
/// From the callback, call ulRendererCollectNow() with `code` to collect right away, or call it
/// later at a moment you choose, or skip the collection. With no callback set, the engine collects
/// right away.
///
/// @param  user_data  The pointer you passed to ulRendererSetGCRequestGCCallback().
///
/// @param  caller     The Renderer to collect on.
///
/// @param  info       The current heap state.
///
/// @param  code       How much compiled code the engine recommends discarding (pass it to
///                    ulRendererCollectNow()).
///
typedef void (*ULGCRequestGCCallback)(void* user_data, ULRenderer caller,
                                      ULGCInfo info, ULCodeDropMode code);

///
/// Called when reclaimable memory rises past a threshold (see ULGCInfo).
///
/// You can use this to decide when to reclaim memory yourself (eg, with ulRendererCollectNow()).
/// It fires again only after the pressure drops and rises again.
///
/// @param  user_data  The pointer you passed to ulRendererSetGCPressureChangedCallback().
///
/// @param  caller     The Renderer.
///
/// @param  info       The current heap state.
///
typedef void (*ULGCPressureChangedCallback)(void* user_data, ULRenderer caller,
                                            ULGCInfo info);

///
/// Called after a collection on your thread finishes.
///
/// This covers the engine's own collections and the ones you start with ulRendererCollectNow(),
/// ulRecycle(), ulRecycleEx(), or ulPurgeMemory(). It's called during the next ulUpdate().
///
/// @param  user_data        The pointer you passed to ulRendererSetGCCollectCompleteCallback().
///
/// @param  caller           The Renderer.
///
/// @param  info             The heap state when this is called.
///
/// @param  code             How much compiled code the collection was asked to discard.
///
/// @param  duration_ms      How long the collection took in milliseconds.
///
/// @param  bytes_reclaimed  How many bytes the collection freed.
///
typedef void (*ULGCCollectCompleteCallback)(void* user_data, ULRenderer caller,
                                            ULGCInfo info, ULCodeDropMode code,
                                            double duration_ms,
                                            unsigned long long bytes_reclaimed);

///
/// Set the callback for when the engine wants to run a collection on your thread (see
/// ULGCRequestGCCallback).
///
/// Setting a callback replaces the previous one. With no callback set, the engine collects right
/// away.
///
/// @param  renderer           The Renderer.
///
/// @param  callback           The callback to invoke, or NULL to remove it.
///
/// @param  user_data          Pointer passed through to `callback` (can be NULL). Ownership
///                            transfers to the Renderer.
///
/// @param  destroy_user_data  Invoked exactly once on `user_data` when the callback is replaced or
///                            the Renderer is destroyed (never while the callback is running), or
///                            right away if `renderer` is NULL. Can be NULL.
///
/// \parblock
/// @note  All GC callbacks run during ulUpdate() on the thread that calls it.
/// \endparblock
///
/// \parblock
/// @note  Each setter owns its `user_data` separately, so pass a shared context with a NULL
///        `destroy_user_data` to all but one setter.
/// \endparblock
///
/// @warning  Don't remove a GC callback from inside a GC callback.
///
/// @pre  Requires the Pro edition or higher.
///
ULExport void ulRendererSetGCRequestGCCallback(ULRenderer renderer,
                                               ULGCRequestGCCallback callback, void* user_data,
                                               ULUserDataDestroyCallback destroy_user_data);

///
/// Set the callback for when reclaimable memory rises past a threshold (see
/// ULGCPressureChangedCallback).
///
/// @param  renderer           The Renderer.
///
/// @param  callback           The callback to invoke, or NULL to remove it.
///
/// @param  user_data          Pointer passed through to `callback` (can be NULL). Ownership
///                            transfers to the Renderer.
///
/// @param  destroy_user_data  Invoked exactly once on `user_data` when the callback is replaced or
///                            the Renderer is destroyed, or right away if `renderer` is NULL. Can
///                            be NULL.
///
/// @pre  Requires the Pro edition or higher.
///
/// @see  ulRendererSetGCRequestGCCallback()
///
ULExport void ulRendererSetGCPressureChangedCallback(ULRenderer renderer,
                                                     ULGCPressureChangedCallback callback,
                                                     void* user_data,
                                                     ULUserDataDestroyCallback destroy_user_data);

///
/// Set the callback for when a collection on your thread finishes (see
/// ULGCCollectCompleteCallback).
///
/// @param  renderer           The Renderer.
///
/// @param  callback           The callback to invoke, or NULL to remove it.
///
/// @param  user_data          Pointer passed through to `callback` (can be NULL). Ownership
///                            transfers to the Renderer.
///
/// @param  destroy_user_data  Invoked exactly once on `user_data` when the callback is replaced or
///                            the Renderer is destroyed, or right away if `renderer` is NULL. Can
///                            be NULL.
///
/// @pre  Requires the Pro edition or higher.
///
/// @see  ulRendererSetGCRequestGCCallback()
///
ULExport void ulRendererSetGCCollectCompleteCallback(ULRenderer renderer,
                                                     ULGCCollectCompleteCallback callback,
                                                     void* user_data,
                                                     ULUserDataDestroyCallback destroy_user_data);

///
/// Get a snapshot of the JavaScript heap's state.
///
/// @param  renderer  The Renderer.
///
/// @return  Returns the current heap state (see ULGCInfo).
///
/// @pre  Requires the Pro edition or higher.
///
ULExport ULGCInfo ulRendererGetGCStatus(ULRenderer renderer);

///
/// Reclaim memory, choosing how much compiled code to discard.
///
/// This works like ulRecycle(). With kRecycleMode_Full, `code` sets how much compiled code the
/// collection discards (it has no effect with kRecycleMode_Lightweight).
///
/// @param  renderer  The Renderer.
///
/// @param  mode      How aggressively to reclaim memory.
///
/// @param  code      How much compiled code to discard.
///
/// @pre  Requires the Pro edition or higher.
///
/// @see  ulRecycle()
///
ULExport void ulRecycleEx(ULRenderer renderer, ULRecycleMode mode, ULCodeDropMode code);

///
/// Collect the JavaScript heap now, leaving rendering and GPU caches intact.
///
/// Unlike ulRecycle(), this reclaims only the JavaScript heap, so rendering stays fast. You can
/// call it from your ULGCRequestGCCallback or at any moment when a short pause won't be noticed.
///
/// @param  renderer  The Renderer.
///
/// @param  code      How much compiled code to discard.
///
/// @param  sync      If true, collect before returning. If false, collect in the background.
///
/// \parblock
/// @note  If you call this while JavaScript is running (eg, from a JS callback), the collection
///        waits until the script returns, even with `sync` set to true.
/// \endparblock
///
/// \parblock
/// @note  The collect-complete callback fires only for a synchronous collection.
/// \endparblock
///
/// @pre  Requires the Pro edition or higher.
///
ULExport void ulRendererCollectNow(ULRenderer renderer, ULCodeDropMode code, bool sync);
#endif // UL_HAS(GC_LISTENER)

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ULTRALIGHT_CAPI_RENDERER_H