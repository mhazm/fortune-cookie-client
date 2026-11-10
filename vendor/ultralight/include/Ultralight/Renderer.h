///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/Defines.h>
#include <Ultralight/RefPtr.h>
#include <Ultralight/Session.h>
#include <Ultralight/View.h>
#include <Ultralight/GamepadEvent.h>
#include <Ultralight/GCListener.h>

#include <type_traits>
#include <utility>

namespace ultralight {

/// \cond INTERNAL
namespace detail {

// Trampolines behind the Renderer::PostTask / PostDelayedTask callable overloads.
template <typename Fn>
void InvokeTaskCallable(void* user_data) {
  (*static_cast<Fn*>(user_data))();
}
template <typename Fn>
void DeleteTaskCallable(void* user_data) {
  delete static_cast<Fn*>(user_data);
}

}  // namespace detail
/// \endcond

///
/// Controls how aggressively Renderer::Recycle() reclaims memory.
///
enum class RecycleMode : uint8_t {
  ///
  /// A quick recycle, cheap enough to call every frame (the same work the automatic recycler
  /// does).
  ///
  Lightweight,

  ///
  /// A thorough recycle, for idle periods or transitions (eg, a loading screen). A call within a
  /// second of the last one does nothing.
  ///
  Full,
};

///
/// Core renderer singleton for the library, coordinates all library functions.
///
/// The Renderer class is responsible for creating and painting View%s, managing Session%s, as well
/// as coordinating network requests, events, JavaScript execution, and more.
///
/// ## Creating the Renderer
///
/// \parblock
/// @note  A Renderer is created for you when you call App::Create() (access it with
///        App::renderer()).
/// \endparblock
///
/// \parblock
/// @note  App::Create() is part of the AppCore API and automatically manages window creation, run
///        loop, input, painting, and most platform-specific functionality. (Available on desktop
///        platforms only)
/// \endparblock
///
/// ### Defining Platform Handlers
///
/// Before creating the Renderer, you should define your platform handlers via the Platform
/// singleton. This can be used to customize file loading, font loading, clipboard access, and other
/// functionality typically provided by the OS.
///
/// Default implementations for most platform handlers ship as Zlib-licensed source in the SDK's
/// `platform` folder. You can use these stock implementations by copying the code into your
/// project, or you can write your own.
///
/// At a minimum, you must provide a FileSystem and a FontLoader. Without them, the library exits
/// the process with an error when it first needs them.
///
/// ### Setting Up the Config
///
/// You can configure various library options by creating a Config object and passing it to
/// `Platform::instance().set_config()`.
/// 
/// ### Creating the Renderer
///
/// Once you've set up the Platform handlers and Config, you can create the Renderer by calling
/// `Renderer::Create()`. You should store the result in a RefPtr to keep it alive.
///
/// @par Example creation code
/// ```
///   // Get the Platform singleton (maintains global library state)
///   auto& platform = Platform::instance();
/// 
///   // Setup config
///   Config my_config;
///   platform.set_config(my_config);
/// 
///   // Create platform handlers (these are the minimum required)
///   // (This is pseudo-code, you will need to define your own)
///   MyFileSystem* file_system = new MyFileSystem();
///   MyFontLoader* font_loader = new MyFontLoader();
///   
///   // Setup platform handlers
///   platform.set_file_system(file_system);
///   platform.set_font_loader(font_loader);
/// 
///   // Create the Renderer
///   RefPtr<Renderer> renderer = Renderer::Create();
/// 
///   // Create Views here
/// ```
/// 
/// ## Updating Renderer Logic
///
/// You should call Renderer::Update() from your main update loop as often as possible to give the
/// library an opportunity to dispatch events and timers:
///
/// @par Example update code
/// ```
/// void mainLoop()
/// {
///   while(true)
///   {
///     // Update program logic here
///     renderer->Update();
///   }
/// }
/// ```
///
/// ## Rendering Each Frame
///
/// When your program is ready to display a new frame (usually in sync with the monitor's
/// refresh rate), you should call Renderer::RefreshDisplay() and Renderer::Render() so the
/// library can render all active View%s as needed.
///
/// Animations and smooth scrolling advance with each RefreshDisplay() call. Call it once per
/// frame you present (once per display refresh for a vsynced loop, or once per rendered frame for
/// a vsync-off or variable-refresh game loop).
///
/// @par Example per-frame render code
/// ```
/// void displayFrame()
/// {
///   // Notify the renderer that the main display has refreshed. This updates animations,
///   // smooth scrolling, and window.requestAnimationFrame() for all Views on that display.
///   renderer->RefreshDisplay(0);
///
///   // Render all Views as needed
///   renderer->Render();
///
///   // Each View renders to a
///   //   - Pixel-Buffer Surface (View::surface())
///   //     or
///   //   - GPU texture (View::render_target())
///   // based on whether CPU or GPU rendering is used.
///   //
///   // You will need to display the image data here as needed.
/// }
/// ```
///
class UExport Renderer : public RefCounted {
 public:
  ///
  /// Create the core renderer singleton for the library.
  ///
  /// You should set up the Platform singleton before calling this function.
  ///
  /// @return  Returns a ref-pointer to a new Renderer instance, or nullptr on failure. Store it
  ///          in a RefPtr to keep it alive.
  ///
  /// \parblock
  /// @note  You do not need to call this if you're using the App class from AppCore.
  /// \endparblock
  ///
  /// \parblock
  /// @warning  You must define a FontLoader and a FileSystem in the Platform singleton. Without
  ///           them, the library exits the process with an error when it first needs them.
  /// \endparblock
  ///
  /// \parblock
  /// @warning  You should only create one Renderer during the lifetime of your program.
  /// \endparblock
  ///
  static RefPtr<Renderer> Create();

  ///
  /// Create a unique, named Session to store browsing data in (cookies, local storage,
  /// application cache, indexed db, etc).
  ///
  /// @param  is_persistent  Whether or not to store the session on disk. Persistent sessions are
  ///                        stored in a subfolder of Config::cache_path with this session's name.
  ///
  /// @param  name           A unique name for this session.
  ///
  /// @return  Returns a ref-pointer to a new Session instance.
  ///
  /// \parblock
  /// @note  A default, persistent Session is already created for you. You only need to call this
  ///        if you want a private, in-memory session or a separate session for each View.
  /// \endparblock
  ///
  /// \parblock
  /// @note  The library doesn't check that names are unique: two persistent sessions with the
  ///        same name share one folder.
  /// \endparblock
  ///
  virtual RefPtr<Session> CreateSession(bool is_persistent, const String& name) = 0;

  ///
  /// Get the default Session. This session is persistent (backed to disk) and has the name
  /// "default".
  ///
  virtual RefPtr<Session> default_session() = 0;

  ///
  /// Create a new View to load and display web pages in.
  /// 
  /// Views are similar to a tab in a browser. They have certain dimensions but are rendered to an
  /// offscreen surface and must be forwarded all input events.
  ///
  /// @param  width    The initial width, in pixels.
  ///
  /// @param  height   The initial height, in pixels.
  ///
  /// @param  config   Configuration details for the View.
  ///
  /// @param  session  The session to store local data in. Pass a nullptr to use the default
  ///                  session.
  /// 
  /// @return  Returns a ref-pointer to a new View instance.
  ///
  virtual RefPtr<View> CreateView(uint32_t width, uint32_t height, const ViewConfig& config,
                                  RefPtr<Session> session)
      = 0;

  ///
  /// Update timers and dispatch callbacks.
  /// 
  /// You should call this as often as you can from your application's run loop.
  ///
  virtual void Update() = 0;

  ///
  /// Notify the renderer that a display has refreshed.
  ///
  /// Call this once per frame you present, for each display (usually right after vsync). It
  /// advances animations, smooth scrolling, and `window.requestAnimationFrame()` for the Views on
  /// that display, so they can repaint during the next Render().
  ///
  /// @param  display_id  The id of the display that refreshed (see ViewConfig::display_id).
  ///
  /// \parblock
  /// @note  Keep calling this even while View::needs_paint() returns false: animations only
  ///        request a repaint from this call.
  /// \endparblock
  ///
  /// \parblock
  /// @note  Call this on the thread the Renderer was created on.
  /// \endparblock
  ///
  virtual void RefreshDisplay(uint32_t display_id) = 0;

  ///
  /// Notify the renderer that a display has refreshed, and when the resulting frame will be
  /// shown.
  ///
  /// This works like RefreshDisplay(display_id), but animation time follows your timestamps
  /// instead of the wall clock, so animations are timed for the moment the frame reaches the
  /// screen.
  ///
  /// @param  display_id        The id of the display that refreshed.
  ///
  /// @param  target_timestamp  When this frame will be shown, in seconds. Use the system's
  ///                           monotonic clock (eg, the target time your display link reports),
  ///                           or your own timeline after calling SetDisplayUsesCustomClock().
  ///                           Timestamps must never decrease.
  ///
  /// @note  `performance.now()` stays on the wall clock, so pages that compare it to
  ///        `window.requestAnimationFrame()` timestamps will see the difference.
  ///
  /// @see  SetDisplayRefreshRate(), SetDisplayUsesCustomClock()
  ///
  virtual void RefreshDisplay(uint32_t display_id, double target_timestamp) = 0;

  ///
  /// Post a task to run on the Renderer's thread during a future call to Update().
  ///
  /// You can use this to hand work from your other threads to the thread that drives the Renderer
  /// (the only thread that may touch View%s and the DOM API).
  ///
  /// Tasks run in posting order during the next Update() after they are posted. A task posted
  /// while Update() is draining tasks runs at the following Update().
  ///
  /// @param  task               Invoked once on the Renderer's thread with `user_data`.
  ///
  /// @param  user_data          Pointer passed through to `task` (can be nullptr).
  ///
  /// @param  destroy_user_data  Invoked exactly once after `task` runs, or without `task`
  ///                            running if the Renderer is destroyed first (can be nullptr).
  ///
  /// \parblock
  /// @note  Safe to call from any thread.
  /// \endparblock
  ///
  /// \parblock
  /// @note  With AppCore, App::PostTask() posts to this same queue and also wakes the app's
  ///        loop, so prefer it there.
  /// \endparblock
  ///
  virtual void PostTask(void (*task)(void* user_data), void* user_data,
                        void (*destroy_user_data)(void* user_data) = nullptr)
      = 0;

  ///
  /// Post a task to run on the Renderer's thread once a delay has elapsed.
  ///
  /// The clock is the Update() cadence, not a background timer. The task runs during the first
  /// Update() at or after the deadline, so delivery waits while Update() is not being called
  /// (eg, a paused application).
  ///
  /// Tasks whose deadlines fall in the same Update() run in posting order.
  ///
  /// @param  delay_ms           Minimum delay before the task may run, in milliseconds.
  ///
  /// @param  task               Invoked once on the Renderer's thread with `user_data`.
  ///
  /// @param  user_data          Pointer passed through to `task` (can be nullptr).
  ///
  /// @param  destroy_user_data  Invoked exactly once after `task` runs, or without `task`
  ///                            running if the Renderer is destroyed first (can be nullptr).
  ///
  /// @note  Safe to call from any thread.
  ///
  /// @see  PostTask()
  ///
  virtual void PostDelayedTask(double delay_ms, void (*task)(void* user_data), void* user_data,
                               void (*destroy_user_data)(void* user_data) = nullptr)
      = 0;

  ///
  /// Post a callable to run on the Renderer's thread during a future call to Update().
  ///
  /// Convenience overload of PostTask(). The callable is invoked once on the Renderer's thread,
  /// then destroyed (destroyed without being invoked if the Renderer is destroyed first).
  ///
  /// Captures are the natural way to hand data across threads. You should capture by value.
  ///
  /// ```
  /// worker_thread.OnFinished([renderer, result] {
  ///   renderer->PostTask([result] { ApplyResult(result); });
  /// });
  /// ```
  ///
  /// @param  callback  The callable to invoke on the Renderer's thread.
  ///
  /// @note  Safe to call from any thread.
  ///
  template <typename F, typename = std::enable_if_t<std::is_invocable_v<std::decay_t<F>&>>>
  void PostTask(F&& callback) {
    using Fn = std::decay_t<F>;
    auto* fn = new Fn(std::forward<F>(callback));
    PostTask(&detail::InvokeTaskCallable<Fn>, fn, &detail::DeleteTaskCallable<Fn>);
  }

  ///
  /// Post a callable to run on the Renderer's thread once a delay has elapsed.
  ///
  /// Convenience overload of PostDelayedTask(). See PostTask() for the callable's lifetime.
  ///
  /// @param  delay_ms  Minimum delay before the callable may run, in milliseconds.
  ///
  /// @param  callback  The callable to invoke on the Renderer's thread.
  ///
  /// @note  Safe to call from any thread.
  ///
  template <typename F, typename = std::enable_if_t<std::is_invocable_v<std::decay_t<F>&>>>
  void PostDelayedTask(double delay_ms, F&& callback) {
    using Fn = std::decay_t<F>;
    auto* fn = new Fn(std::forward<F>(callback));
    PostDelayedTask(delay_ms, &detail::InvokeTaskCallable<Fn>, fn,
                    &detail::DeleteTaskCallable<Fn>);
  }

  ///
  /// Render all active views to their respective render-targets/surfaces.
  ///
  /// @note  Views are only repainted if they actually need painting.
  ///
  virtual void Render() = 0;

  ///
  /// Render a subset of views to their respective surfaces and render targets.
  ///
  /// @param  view_array      A C-array containing a list of View pointers.
  ///
  /// @param  view_array_len  The length of the C-array.
  ///
  /// @deprecated  Use Render() instead. To stop a View from painting, pause it with
  ///              View::set_visible(false) (Render() skips hidden Views).
  ///
  [[deprecated("Use Render() instead.")]]
  virtual void RenderOnly(View** view_array, size_t view_array_len) = 0;

  ///
  /// Recycle internal caches and reclaim memory.
  ///
  /// @note  You normally do not need to call this. The library recycles automatically on a timer
  ///        (see Config::recycle_delay), and letting it drive recycling is recommended for best
  ///        performance. Call Recycle() yourself only when you want to control when reclamation
  ///        happens, for example to force a thorough reclaim during a loading screen or scene
  ///        transition.
  ///
  /// @param  mode  How aggressively to reclaim memory:
  ///               - **Lightweight**: cheap enough to call every frame. This is the same work the
  ///                 automatic recycler performs.
  ///               - **Full**: a thorough, heavier reclaim best reserved for idle periods or
  ///                 transitions. It can take noticeably longer on a busy page.
  ///
  /// @see  PurgeMemory()
  ///
  virtual void Recycle(RecycleMode mode = RecycleMode::Lightweight) = 0;

#if UL_HAS(GC_LISTENER)
  ///
  /// Reclaim memory, choosing how much compiled code to discard.
  ///
  /// Like Recycle(), but `code` sets how much compiled code a Full recycle discards. Discarding
  /// more frees more memory, but pages run slower for a while as their code is compiled again.
  ///
  /// You can use this to shrink the JavaScript footprint at a moment you choose (eg, a loading
  /// screen or level transition).
  ///
  /// @param  mode  See Recycle().
  ///
  /// @param  code  How aggressively to discard compiled code (see CodeDropMode).
  ///
  /// @note  With `RecycleMode::Lightweight` the `code` parameter has no effect (no collection
  ///        runs).
  ///
  /// @pre  Requires the Pro edition or higher.
  ///
  /// @see  Recycle(), PurgeMemory()
  ///
  virtual void Recycle(RecycleMode mode, CodeDropMode code) = 0;

  ///
  /// Collect the JavaScript heap now, leaving rendering and GPU caches intact.
  ///
  /// Unlike Recycle(), this reclaims only the JavaScript heap. It runs a garbage collection
  /// (optionally discarding compiled code, see CodeDropMode) and returns the freed memory to the
  /// system. Rendering and GPU caches are left alone, so rendering stays fast.
  ///
  /// You can call this from GCListener::OnRequestGC() or at any moment when a short pause won't be
  /// noticed (eg, a quiet point in your update loop).
  ///
  /// @param  code  How aggressively to discard compiled code (see CodeDropMode).
  ///
  /// @param  sync  If true (the default), collect synchronously before returning, briefly blocking
  ///               the calling thread. If false, start a collection that completes in the
  ///               background.
  ///
  /// \parblock
  /// @note  If you call this while JavaScript is running (eg, from a JS callback), the
  ///        collection waits until the script returns, even with `sync` set to true.
  /// \endparblock
  ///
  /// \parblock
  /// @note  GCListener::OnCollectComplete() fires only for a synchronous collection.
  /// \endparblock
  ///
  /// @pre  Requires the Pro edition or higher.
  ///
  /// @see  Recycle(), PurgeMemory(), GCListener::OnRequestGC()
  ///
  virtual void CollectNow(CodeDropMode code, bool sync = true) = 0;
#endif

  ///
  /// Attempt to release as much memory as possible.
  ///
  /// This also discards all compiled JavaScript code, so pages run slower for a while as it's
  /// recompiled.
  ///
  /// @warning  Don't call this while the Renderer is rendering (eg, from a GPUDriver callback):
  ///           the rendering caches can't be released then, and are skipped with a warning.
  ///
  virtual void PurgeMemory() = 0;

  ///
  /// Print detailed memory usage statistics to the log.
  ///
  /// @see  Platform::set_logger()
  ///
  virtual void LogMemoryUsage() = 0;

  ///
  /// Get formatted memory usage statistics as a string.
  ///
  /// @return  Returns the same information as LogMemoryUsage(), as a String you can display in a
  ///          debug overlay or capture programmatically.
  ///
  virtual String GetMemoryUsage() = 0;

  ///
  /// Set the system color scheme that Views report to pages via the `prefers-color-scheme`
  /// CSS media feature.
  ///
  /// The library never reads the OS setting itself; the scheme remains Light until this is
  /// called. A change re-evaluates `prefers-color-scheme` media queries in every View whose
  /// ViewConfig::preferred_color_scheme is ColorScheme::Auto; Views pinned Light or Dark are
  /// unaffected.
  ///
  /// @param  scheme  ColorScheme::Light or ColorScheme::Dark. Passing ColorScheme::Auto is
  ///                 invalid here (a warning is logged and the value is ignored).
  ///
  /// @note  This is automatically managed for you when App::Create() is used (the OS setting
  ///        is fed at startup and tracked as it changes).
  ///
  /// @see  View::set_preferred_color_scheme()
  ///
  virtual void set_system_color_scheme(ColorScheme scheme) = 0;

  ///
  /// The current system color scheme. (Default: ColorScheme::Light)
  ///
  /// @see  set_system_color_scheme()
  ///
  virtual ColorScheme system_color_scheme() const = 0;

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
  /// @param  address  The address for the server to listen on (eg, "127.0.0.1")
  ///
  /// @param  port     The port for the server to listen on (eg, 9222)
  ///
  /// @return  Returns whether the server started successfully or not.
  ///
  /// @pre  Not available in the Free edition (always returns false there).
  ///
  virtual bool StartRemoteInspectorServer(const char* address, uint16_t port) = 0;

  ///
  /// Describe the details of a gamepad, to be used with FireGamepadEvent and related
  /// events below. This can be called multiple times with the same index if the details change.
  ///
  /// @param  index         The gamepad's connection slot, and the array position the page sees: a
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
  virtual void SetGamepadDetails(uint32_t index, const String& id, uint32_t axis_count,
                                 uint32_t button_count)
      = 0;

  ///
  /// Fire a gamepad event (connection / disconnection).
  ///
  /// @note  The gamepad should first be described via SetGamepadDetails before calling this
  ///        function.
  ///
  /// @see  <https://developer.mozilla.org/en-US/docs/Web/API/Gamepad>
  ///
  virtual void FireGamepadEvent(const GamepadEvent& evt) = 0;

  ///
  /// Fire a gamepad axis event (to be called when an axis value is changed).
  ///
  /// @note  The gamepad should be connected via a previous call to FireGamepadEvent.
  ///
  /// @see  <https://developer.mozilla.org/en-US/docs/Web/API/Gamepad/axes>
  ///
  virtual void FireGamepadAxisEvent(const GamepadAxisEvent& evt) = 0;

  ///
  /// Fire a gamepad button event (to be called when a button value is changed).
  ///
  /// @note  The gamepad should be connected via a previous call to FireGamepadEvent.
  ///
  /// @see  <https://developer.mozilla.org/en-US/docs/Web/API/Gamepad/buttons>
  ///
  virtual void FireGamepadButtonEvent(const GamepadButtonEvent& evt) = 0;

#if UL_HAS(RENDER_TRACE)
  ///
  /// Begin recording a render trace to the specified file path.
  ///
  /// The trace file captures detailed rendering pipeline diagnostics in Perfetto format,
  /// viewable at https://ui.perfetto.dev. This operates independently of the Profiler--
  /// both can be active simultaneously.
  ///
  /// @param  output_path  Path to the output .perfetto-trace file.
  ///
  /// @param  verbose      If true, captures detailed annotations in addition to scope timing.
  ///
  /// @return  Returns whether or not the trace started (false if a trace is already active or
  ///          the trace file can't be opened).
  ///
  /// @pre  Requires the Enterprise edition or higher.
  ///
  virtual bool BeginRenderTrace(const char* output_path, bool verbose = true) = 0;

  ///
  /// End the current render trace.
  ///
  /// The trace file is finished and closed shortly after this returns (on the next Update()).
  ///
  /// @pre  Requires the Enterprise edition or higher.
  ///
  virtual void EndRenderTrace() = 0;

  ///
  /// Whether or not a render trace is currently being recorded.
  ///
  /// @pre  Requires the Enterprise edition or higher.
  ///
  virtual bool is_render_trace_active() const = 0;
#endif // UL_HAS(RENDER_TRACE)

#if UL_HAS(GC_LISTENER)
  ///
  /// Set a GCListener to observe and control the Renderer's JavaScript garbage collections.
  ///
  /// @param  listener  A user-defined GCListener implementation, ownership remains with the
  ///                   caller. Pass a nullptr to detach the current listener.
  ///
  /// @note  The listener must outlive the Renderer, or be cleared before the Renderer is
  ///        destroyed.
  ///
  /// @pre  Requires the Pro edition or higher.
  ///
  /// @see  GCListener
  ///
  virtual void set_gc_listener(GCListener* listener) = 0;

  ///
  /// Get the attached GCListener, if any.
  ///
  /// @return  Returns the attached listener, or nullptr if none is set.
  ///
  /// @pre  Requires the Pro edition or higher.
  ///
  virtual GCListener* gc_listener() const = 0;

  ///
  /// Get a snapshot of the current JS-heap reclamation state.
  ///
  /// @return  Returns the current reclamation state (see GCInfo).
  ///
  /// @pre  Requires the Pro edition or higher.
  ///
  virtual GCInfo gc_status() const = 0;
#endif // UL_HAS(GC_LISTENER)

  ///
  /// Declare a display's refresh rate.
  ///
  /// Pages see the declared rate through `window.requestAnimationFrame()` scheduling. Call this
  /// when the display's mode changes (eg, a variable-rate display switching between 120 Hz and
  /// 48 Hz). Small changes are absorbed, and a change reaches pages at most once a second.
  ///
  /// @param  display_id    The id of the display.
  ///
  /// @param  refresh_rate  The display mode's nominal refresh rate in Hz (eg, 60.0, 59.94,
  ///                       120.0), or 0 to clear it (the library then measures how often
  ///                       RefreshDisplay() is called).
  ///
  /// @note  Call this on the thread the Renderer was created on.
  ///
  /// @see  RefreshDisplay()
  ///
  virtual void SetDisplayRefreshRate(uint32_t display_id, double refresh_rate) = 0;

  ///
  /// Get the refresh rate declared for a display.
  ///
  /// @param  display_id  The id of the display.
  ///
  /// @return  Returns the display's declared refresh rate (in Hz), or 0 if
  ///          SetDisplayRefreshRate() was never called for it.
  ///
  virtual double display_refresh_rate(uint32_t display_id) const = 0;

  ///
  /// Declare that a display's refresh timestamps are on your own timeline rather than the
  /// system clock.
  ///
  /// By default, the timestamps you pass to RefreshDisplay(display_id, target_timestamp) are
  /// treated as readings of the system's monotonic clock. Declare a custom clock when they're on
  /// a timeline of your own (eg, a media timeline for offline rendering, or simulation time):
  /// animation then follows your timestamps exactly.
  ///
  /// @param  display_id         The id of the display.
  ///
  /// @param  uses_custom_clock  True if timestamps for this display are on your own timeline.
  ///
  /// @pre  Call this before the display's first timed refresh.
  ///
  /// @note  Call this on the thread the Renderer was created on.
  ///
  /// @see  RefreshDisplay()
  ///
  virtual void SetDisplayUsesCustomClock(uint32_t display_id, bool uses_custom_clock) = 0;

  ///
  /// Get whether a display was declared to use a custom clock.
  ///
  /// @param  display_id  The id of the display.
  ///
  /// @return  Returns the value last passed to SetDisplayUsesCustomClock() for the display, or
  ///          false if it was never called.
  ///
  virtual bool display_uses_custom_clock(uint32_t display_id) const = 0;

#if UL_HAS(TESTING)
  /// \cond ignore
  virtual void SetMockScrollbarsEnabled(bool enabled) = 0;
  virtual void SetAllowAnySSLCertificate(bool allow) = 0;
  virtual String GetHeapDiagnostics() = 0;
  virtual String DumpHeapSnapshot(const String& path) = 0;
  /// \endcond
#endif // UL_HAS(TESTING)

 protected:
  virtual ~Renderer();
};

} // namespace ultralight
