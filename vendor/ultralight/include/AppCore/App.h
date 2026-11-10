///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include "Defines.h"
#include "GPUMemoryStats.h"
#include "GPUDriverCounters.h"
#include <Ultralight/Bitmap.h>
#include <Ultralight/RefPtr.h>
#include <Ultralight/Renderer.h>
#include <Ultralight/platform/Config.h>

namespace ultralight {

class Monitor;
class Window;

///
/// Interface for all App-related events.
///
/// @see  App::set_listener()
///
class AExport AppListener {
public:
  virtual ~AppListener() {}

  ///
  /// Called on every update of the app's loop. You should update all app logic here.
  ///
  /// @note  This fires right before the library updates the Renderer (see Renderer::Update()).
  ///
  virtual void OnUpdate() {}

  ///
  /// Called when the app has been idle (low CPU use and no recent input) for a while.
  ///
  /// @param  utilization  Main-thread CPU utilization (0.0-1.0) over the last ~1 second. Lower
  ///                      values mean more headroom for background work.
  ///
  /// @note  This first fires after Settings::sustained_idle_time of continuous idle, then again
  ///        at that interval while the app stays idle. It stops as soon as there's input or CPU
  ///        use rises above Settings::idle_utilization_threshold.
  ///
  virtual void OnIdle(double utilization) {}
};

///
/// App-specific settings.
///
struct AExport Settings {
  ///
  /// The name of the developer of this app.
  ///
  /// Together with app_name, this generates the per-app folders where the library keeps the log
  /// file (`ultralight.log`), profiler traces, and session data (cookies, cached resources,
  /// databases). A Config::cache_path you set yourself takes over the session data. The log and
  /// traces stay in the diagnostics folder either way.
  ///
  /// @note  The folders are:
  ///         - Windows: `%APPDATA%\<developer_name>\<app_name>`
  ///         - Linux:   `$XDG_CACHE_HOME/com.<developer_name>.<app_name>`, or
  ///                    `~/.cache/com.<developer_name>.<app_name>` when that variable is
  ///                    unset
  ///         - macOS:   `~/Library/Caches/<name>` for the log and traces, and
  ///                    `~/Library/Application Support/<name>` for session data, where
  ///                    `<name>` is the app bundle's identifier, or
  ///                    `com.<developer_name>.<app_name>` when it has none
  ///
  String developer_name = "MyCompany";

  ///
  /// The name of this app, used with developer_name to build the per-app folders (see
  /// developer_name).
  ///
  String app_name = "MyApp";

  ///
  /// The root folder for `file:///` URLs. You should set this to the relative path where all
  /// of your app data is (eg, `file:///page.html` loads `page.html` from this folder).
  ///
  /// @note  The path is relative to:
  ///         - Windows: the executable's folder
  ///         - Linux:   the executable's folder
  ///         - macOS:   `YourApp.app/Contents/Resources/`
  ///
  String file_system_path = "./assets/";

  ///
  /// Whether or not to always use the CPU renderer. By default the library uses the GPU
  /// renderer when it finds a compatible GPU.
  ///
  bool force_cpu_renderer = false;

  ///
  /// Override the device scale factor for all windows.
  ///
  /// When this is 0 or less (the default), the device scale is auto-detected from the monitor's
  /// DPI. Set a positive value (eg, 1.0, 1.5, 2.0) to use that scale for every window instead.
  ///
  double device_scale_override = 0.0;

  ///
  /// Whether or not to pick the font-appearance preset from the operating system, so text matches
  /// the platform's native look (FontProfile::WindowsLike on Windows and Linux,
  /// FontProfile::MacOSLike on macOS).
  ///
  /// This only applies while Config::font_profile is FontProfile::Default-- a profile you set
  /// yourself always wins. Set this to false to keep the Default profile's platform-neutral
  /// appearance everywhere.
  ///
  bool auto_font_profile = true;

  ///
  /// Whether or not to pick the default font families (serif, sans-serif, monospace, etc.) from
  /// the operating system, so generic CSS families resolve to the same fonts as the platform's
  /// browsers (eg, Menlo for `monospace` on macOS, Consolas on Windows).
  ///
  /// This applies to Views created without an explicit ViewConfig. A config you pass to
  /// Container::AddPanel() keeps its own font families-- start from
  /// Window::default_view_config() to inherit them. Set this to false to keep the
  /// platform-neutral families everywhere.
  ///
  bool auto_font_families = true;

  ///
  /// Whether or not text editing in the Views this app creates should follow the host OS's native
  /// conventions (eg, non-directional selections on macOS).
  ///
  /// This applies to Views created without an explicit ViewConfig. A config you pass to
  /// Container::AddPanel() uses its own ViewConfig::match_native_editing_behavior instead. Set
  /// this to false for editing behavior that is identical on every platform.
  ///
  bool match_native_editing_behavior = true;

  ///
  /// The minimum duration of user inactivity (in seconds) before idle detection begins.
  ///
  double idle_threshold = 0.5;

  ///
  /// The time (in seconds) the app must remain continuously idle before AppListener::OnIdle()
  /// fires, and the interval between repeated calls after that.
  ///
  double sustained_idle_time = 2.0;

  ///
  /// The thread CPU utilization (0.0-1.0) below which the app is considered idle.
  ///
  double idle_utilization_threshold = 0.5;

  ///
  /// Whether or not to enable the built-in trace file profiler.
  ///
  /// When enabled, the library writes a Perfetto trace file that can be opened in
  /// https://ui.perfetto.dev to visualize Ultralight's internal performance.
  ///
  /// Traces are written to a `profiler_traces` folder inside the app's diagnostics directory,
  /// alongside the log file (see developer_name for where that is). Read
  /// App::profiler_trace_path() for the exact file. This folder is the app's own, not the
  /// Config::cache_path you may have set.
  ///
  /// \parblock
  /// @note  A profiler you set with Platform::set_profiler() before App::Create() is kept and
  ///        this flag is ignored, with a warning in the log.
  /// \endparblock
  ///
  /// \parblock
  /// @note  If no trace file can be written, the log says so and nothing else changes.
  ///        App::is_profiler_active() reads false.
  /// \endparblock
  ///
  /// @pre  Requires the Pro edition or higher (the Profiler platform interface is not available
  ///       in the Free or Plus editions).
  ///
  bool enable_profiler = false;

  ///
  /// The paint rate (in frames per second) for windows created with WindowFlags::Hidden.
  ///
  /// Hidden windows receive no paint events from the OS, so by default they're never painted. Set
  /// this to a positive value to paint them at a fixed rate instead, which is what offscreen and
  /// headless capture need. The rate is independent of any monitor's refresh rate, so time-based
  /// content advances the same on every host.
  ///
  uint32_t headless_paint_fps = 0;

  ///
  /// Whether or not to run the app's update loop at full speed, without sleeping between updates.
  ///
  /// By default the loop sleeps between updates, which keeps CPU usage low but paces how quickly
  /// page work (resource loading, script, layout) advances. Enable this to update continuously
  /// instead, so that work completes as fast as the CPU allows. Display refresh and hidden-window
  /// painting are still paced (see headless_paint_fps) and painting stays invalidation-driven,
  /// so animation timing is unchanged.
  ///
  /// @warning  The loop thread busy-waits, so a single instance can fully occupy one CPU core.
  ///           This is meant for batch / offscreen workloads that render many pages back to
  ///           back, not interactive apps.
  ///
  bool full_speed_loop = false;

  ///
  /// Whether or not to time animations for the moment each frame appears on screen.
  ///
  /// A frame appears one or more display refreshes after the refresh that started it. With
  /// this on (the default), `window.requestAnimationFrame()` timestamps, CSS animations, and
  /// smooth scrolling use the time the frame will appear, which avoids lag while scrolling. Set
  /// this to false to use the time of the refresh instead.
  ///
  /// \parblock
  /// @note  With this on, `window.requestAnimationFrame()` timestamps run ahead of
  ///        `performance.now()` by the display's presentation delay (one to a few refreshes),
  ///        so pages that compare the two will see the difference.
  /// \endparblock
  ///
  /// \parblock
  /// @note  This applies on macOS and Windows, once the library has measured the display's
  ///        timing (a moment after rendering starts).
  /// \endparblock
  ///
  bool sync_animations_to_present = true;

  ///
  /// The application icon, as a 32-bit BGRA bitmap with straight (unpremultiplied) alpha.
  ///
  /// On macOS this is the Dock icon. On Windows and Linux it's the default icon for windows that
  /// haven't set their own with Window::SetIcon(). Leave it null to keep the platform default.
  ///
  RefPtr<Bitmap> app_icon;
};

class AppImpl;

///
/// Handle to a repeating timer created with App::SetInterval().
///
/// The handle owns the timer. Destroying it (or calling Cancel()) stops the timer and destroys
/// its user data. Handles are move-only and should be destroyed on the main thread.
///
class AExport TimerHandle {
public:
  ///
  /// Create an empty handle that refers to no timer.
  ///
  TimerHandle() {}

  ///
  /// Cancel the timer, if any.
  ///
  ~TimerHandle() { Cancel(); }

  ///
  /// Move constructor (the source is left empty).
  ///
  TimerHandle(TimerHandle&& other) noexcept : id_(other.id_) { other.id_ = 0; }

  ///
  /// Move assignment. Cancels the timer this handle held, if any; the source is left empty.
  ///
  TimerHandle& operator=(TimerHandle&& other) noexcept {
    if (this != &other) {
      Cancel();
      id_ = other.id_;
      other.id_ = 0;
    }
    return *this;
  }

  TimerHandle(const TimerHandle&) = delete;
  TimerHandle& operator=(const TimerHandle&) = delete;

  ///
  /// Stop the timer and destroy its user data. Safe to call more than once.
  ///
  void Cancel() {
    if (id_) {
      CancelInterval(id_);
      id_ = 0;
    }
  }

  ///
  /// Whether or not this handle still refers to a timer (false after Cancel() or a move).
  ///
  explicit operator bool() const { return id_ != 0; }

private:
  friend class AppImpl;
  explicit TimerHandle(uint64_t id) : id_(id) {}
  static void CancelInterval(uint64_t timer_id);
  uint64_t id_ = 0;
};

///
/// Main application singleton (use this if you want to let the library manage window creation).
///
/// This convenience class sets up everything you need to display web-based content in a
/// desktop application.
///
/// The App class initializes the Platform singleton with OS-specific defaults, creates a Renderer,
/// and automatically manages window creation, run loop, input events, and painting.
///
/// ## Creating the App
///
/// Call App::Create() to initialize the library and create the App singleton.
///
/// ```
///   auto app = App::Create();
/// ```
///
/// ## Creating a Window
///
/// Call Window::Create() to create one or more windows during the lifetime of your app.
///
/// ```
///   auto window = Window::Create(app->main_monitor(), 1024, 768, false,
///                                WindowFlags::Titled | WindowFlags::Resizable);
/// ```
///
/// ### Adding a Panel to a Window
///
/// A window's content is a layout of panels, each showing a View. A bare AddPanel() fills
/// the whole window.
///
/// ```
///   auto panel = window->AddPanel();
/// ```
///
/// Each Panel has a View instance that you can use to load web content into.
///
/// ```
///   panel->view()->LoadURL("https://google.com");
/// ```
///
/// ## Running the App
///
/// Call App::Run() to start the main run loop.
///
/// ```
/// #include <AppCore/AppCore.h>
///
/// using namespace ultralight;
///
/// int main() {
///   // Initialize app, window, panels, etc. here...
///
///   app->Run();
///
///   return 0;
/// }
/// ```
///
/// ## Shutting Down the App
///
/// Call App::Quit() to stop the main run loop and shut down the app.
///
/// ```
///   app->Quit();
/// ```
///
/// @note  This is optional, you can use the Renderer class directly if you want to manage your
///        own windows and run loop.
///
class AExport App : public RefCounted {
public:
  ///
  /// Create the App singleton.
  ///
  /// @param  settings  Settings to customize App runtime behavior.
  ///
  /// @param  config    Config options for the Ultralight renderer.
  ///
  /// @return  Returns a ref-pointer to the created App instance.
  ///
  /// \parblock
  /// @note  You should only create one App per application lifetime.
  /// \endparblock
  ///
  /// \parblock
  /// @note  App::Create() adjusts a few Config fields: it always sets Config::face_winding to
  ///        FaceWinding::Clockwise, fills in Config::cache_path when you leave it empty, and
  ///        picks Config::font_profile while Settings::auto_font_profile is on.
  /// \endparblock
  ///
  /// \parblock
  /// @note  A platform handler you set on Platform::instance() before App::Create() is kept (eg,
  ///        your own FileSystem, FontLoader, Logger, or Clipboard). App::Create() only installs
  ///        its default for a handler you left unset. A GPUDriver or SurfaceFactory you set
  ///        beforehand is kept too, but AppCore windows can't present Views with it.
  /// \endparblock
  ///
  static RefPtr<App> Create(Settings settings = Settings(), Config config = Config());

  ///
  /// Get the App singleton (nullptr before App::Create()).
  ///
  static App* instance();

  ///
  /// Get the settings this App was created with.
  ///
  virtual const Settings& settings() const = 0;

  ///
  /// Set an AppListener to receive callbacks for app-related events.
  ///
  /// @param  listener  A user-defined AppListener implementation, ownership remains with the
  ///                   caller. Pass a nullptr to remove the current listener.
  ///
  virtual void set_listener(AppListener* listener) = 0;

  ///
  /// Get the AppListener (can be nullptr).
  ///
  virtual AppListener* listener() = 0;

  ///
  /// Whether or not the App is running.
  ///
  virtual bool is_running() const = 0;

  ///
  /// Get the main monitor (this is never NULL).
  ///
  virtual Monitor* main_monitor() = 0;

  ///
  /// Get the number of connected monitors.
  ///
  virtual uint32_t monitor_count() const { return 1; }

  ///
  /// Get a monitor by index (from 0 to `monitor_count() - 1`).
  ///
  /// Index 0 is always the main monitor.
  ///
  /// You can pass the returned monitor to Window::Create() to open a window on that display.
  ///
  /// @param  index  The index of the monitor to get.
  ///
  /// @return  Returns the monitor, or nullptr when `index` is out of range.
  ///
  /// @note  The returned pointer is owned by the App and remains valid for the lifetime of the
  ///        App. If a monitor is disconnected, it is no longer enumerated, but its pointer remains
  ///        safe to use and its accessors return fallback values.
  ///
  virtual Monitor* monitor(uint32_t index) { return index == 0 ? main_monitor() : nullptr; }

  ///
  /// Get the underlying Renderer instance.
  ///
  virtual RefPtr<Renderer> renderer() = 0;

  ///
  /// Run the main loop (this returns after Quit()).
  ///
  virtual void Run() = 0;

  ///
  /// Quit the application.
  ///
  /// Stops the main loop so Run() returns; the process itself is not exited.
  ///
  /// @note  On macOS, a Quit() issued while a modal loop is running (eg, a native message box
  ///        or an interactive window drag) is absorbed by that inner loop and does not end
  ///        Run()-- call Quit() again once the modal loop has finished.
  ///
  virtual void Quit() = 0;

  ///
  /// Advance the app by one iteration of the main loop, on behalf of your own run loop.
  ///
  /// This pumps pending OS events, updates the renderer, and refreshes the display and repaints
  /// windows once the display interval has elapsed. When no events are pending, it waits up to
  /// `max_wait_seconds` for one.
  ///
  /// You should call this repeatedly from your own loop instead of calling Run().
  ///
  /// @param  max_wait_seconds  The longest this call may block waiting for an event or timer
  ///                           tick. Pass 0.0 to return immediately after processing whatever
  ///                           is pending.
  ///
  /// @return  Returns whether or not the app is still running (false once Quit() has been called).
  ///
  /// @note  On macOS this pumps the event queue manually rather than running the native run
  ///        loop, so menu key-equivalent routing can differ from Run().
  ///
  virtual bool RunOnce(double max_wait_seconds = 0.0) = 0;

  ///
  /// Post a task to run on the main thread during a future update of the app's loop.
  ///
  /// Tasks run in posting order, sharing one queue with Renderer::PostTask().
  ///
  /// @param  task               Invoked once on the main thread.
  ///
  /// @param  user_data          Passed to `task` (can be nullptr).
  ///
  /// @param  destroy_user_data  Invoked exactly once after `task` runs, or without `task`
  ///                            running if the App is destroyed first (can be nullptr).
  ///
  /// @note  Safe to call from any thread. Posting wakes the main loop, so a task posted from
  ///        another thread runs without waiting for the next timer tick.
  ///
  virtual void PostTask(void (*task)(void* user_data), void* user_data,
                        void (*destroy_user_data)(void* user_data) = nullptr)
      = 0;

  ///
  /// Post a task to run on the main thread once a delay has elapsed.
  ///
  /// The task runs on the first update tick after the delay elapses (ticks are about 2 ms apart).
  ///
  /// @param  delay_ms           The delay before the task runs, in milliseconds.
  ///
  /// @param  task               Invoked once on the main thread.
  ///
  /// @param  user_data          Passed to `task` (can be nullptr).
  ///
  /// @param  destroy_user_data  Invoked exactly once after `task` runs, or without `task`
  ///                            running if the App is destroyed first (can be nullptr).
  ///
  /// @note  Safe to call from any thread (it wakes the main loop like PostTask()).
  ///
  /// @see  PostTask()
  ///
  virtual void PostDelayedTask(double delay_ms, void (*task)(void* user_data), void* user_data,
                               void (*destroy_user_data)(void* user_data) = nullptr)
      = 0;

  ///
  /// Create a repeating timer that fires on the main thread.
  ///
  /// The timer fires on the first update tick after each interval elapses (ticks are about 2 ms
  /// apart). Fires missed while the loop was stalled are skipped-- the timer resumes on its
  /// interval.
  ///
  /// @param  interval_ms        The time between fires, in milliseconds.
  ///
  /// @param  callback           Invoked on each fire, on the main thread.
  ///
  /// @param  user_data          Passed to `callback` (can be nullptr).
  ///
  /// @param  destroy_user_data  Invoked exactly once when the timer is canceled or the App is
  ///                            destroyed, and never while `callback` is running (a timer may
  ///                            cancel itself from its own callback). Can be nullptr.
  ///
  /// @return  Returns a handle that owns the timer. Destroy it (or call TimerHandle::Cancel()) to
  ///          stop the timer.
  ///
  /// @note  Safe to call from any thread (it wakes the main loop like PostTask()).
  ///
  [[nodiscard]] virtual TimerHandle SetInterval(double interval_ms,
                                                void (*callback)(void* user_data),
                                                void* user_data,
                                                void (*destroy_user_data)(void* user_data)
                                                = nullptr)
      = 0;

  ///
  /// Post a callable to run on the main thread during a future update of the app's loop.
  ///
  /// Convenience overload of PostTask(). The callable is invoked once on the main thread, then
  /// destroyed (without being invoked if the App is destroyed first).
  ///
  /// @param  callback  The callable to invoke. Capture by value when posting from another thread.
  ///
  template <typename F, typename = std::enable_if_t<std::is_invocable_v<std::decay_t<F>&>>>
  void PostTask(F&& callback) {
    using Fn = std::decay_t<F>;
    auto* fn = new Fn(std::forward<F>(callback));
    PostTask(&detail::InvokeTaskCallable<Fn>, fn, &detail::DeleteTaskCallable<Fn>);
  }

  ///
  /// Post a callable to run on the main thread once a delay has elapsed.
  ///
  /// Convenience overload of PostDelayedTask(). See PostTask() for the callable's lifetime.
  ///
  /// @param  delay_ms  The delay before the callable runs, in milliseconds.
  ///
  /// @param  callback  The callable to invoke.
  ///
  template <typename F, typename = std::enable_if_t<std::is_invocable_v<std::decay_t<F>&>>>
  void PostDelayedTask(double delay_ms, F&& callback) {
    using Fn = std::decay_t<F>;
    auto* fn = new Fn(std::forward<F>(callback));
    PostDelayedTask(delay_ms, &detail::InvokeTaskCallable<Fn>, fn,
                    &detail::DeleteTaskCallable<Fn>);
  }

  ///
  /// Create a repeating timer that invokes a callable on the main thread.
  ///
  /// Convenience overload of SetInterval(). The callable is destroyed when the timer is canceled
  /// or the App is destroyed.
  ///
  /// @param  interval_ms  The time between fires, in milliseconds.
  ///
  /// @param  callback     The callable to invoke on each fire.
  ///
  /// @return  Returns a handle that owns the timer. Destroy it (or call TimerHandle::Cancel()) to
  ///          stop the timer.
  ///
  template <typename F, typename = std::enable_if_t<std::is_invocable_v<std::decay_t<F>&>>>
  [[nodiscard]] TimerHandle SetInterval(double interval_ms, F&& callback) {
    using Fn = std::decay_t<F>;
    auto* fn = new Fn(std::forward<F>(callback));
    return SetInterval(interval_ms, &detail::InvokeTaskCallable<Fn>, fn,
                       &detail::DeleteTaskCallable<Fn>);
  }

  ///
  /// Whether or not the app is currently considered idle (low CPU utilization and no recent user
  /// input, sustained past the configured threshold).
  ///
  /// @note  This reads true once idle conditions have held for at least Settings::idle_threshold,
  ///        even before the first AppListener::OnIdle() callback fires.
  ///
  virtual bool is_idle() const = 0;

  ///
  /// Get the current main-thread CPU utilization (0.0-1.0), averaged over the last ~1 second.
  ///
  /// You can use this to make adaptive scheduling decisions.
  ///
  virtual double thread_utilization() const = 0;

  ///
  /// Get GPU memory statistics from the App's GPU driver.
  ///
  /// Statistics cover the GPU resources the library has allocated through the active GPU backend
  /// (textures, render targets, geometry, swap chains) plus process-wide video memory usage and
  /// budget reported by the operating system. See GPUMemoryStats for per-field details and
  /// accuracy notes.
  ///
  /// @param  stats  Structure to fill. All fields are overwritten on success.
  ///
  /// @return  Returns true if `stats` was filled. Returns false when rendering with the CPU
  ///          renderer or when the active GPU backend does not provide memory statistics.
  ///
  /// @note  Among the stock GPU drivers, the Direct3D backends on Windows, the Metal backend on
  ///        macOS, and the OpenGL backend on Linux implement this (OpenGL reports no process
  ///        usage or budget).
  ///
  virtual bool GetGPUMemoryStats(GPUMemoryStats& stats) { return false; }

#if UL_HAS(TESTING)
  /// \cond ignore
  virtual bool GetGPUDriverCounters(GPUDriverCounters& counters) { return false; }
  virtual bool HasDisplayLinkForTesting() const { return false; }
  virtual bool DisplayLinkPausedForTesting() const { return false; }
  /// \endcond
#endif

  ///
  /// Whether or not the built-in profiler is active (see Settings::enable_profiler).
  ///
  /// This is true when Settings::enable_profiler is set, you didn't set your own profiler with
  /// Platform::set_profiler(), and your edition includes the profiler (Pro or higher).
  ///
  virtual bool is_profiler_active() const = 0;

  ///
  /// Get the file path of the active profiler trace file.
  ///
  /// @return  Returns the path to the trace file, or an empty string when is_profiler_active()
  ///          reads false.
  ///
  virtual const char* profiler_trace_path() const = 0;

protected:
  virtual ~App();
};

}  // namespace ultralight
