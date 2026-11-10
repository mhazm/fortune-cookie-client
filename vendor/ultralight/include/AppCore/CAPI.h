///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///

///
/// @file AppCore/CAPI.h
///
/// AppCore API interface.
///
/// `#include <AppCore/CAPI.h>`
///
/// AppCore is a convenient windowing system for desktop platforms built on top of the
/// Ultralight renderer.
/// 
/// It automatically sets up the Renderer, creates a run loop, and handles all window creation,
/// painting, and platform-specific operations for you.
/// 
/// ## Creating the App
/// 
/// Call ulCreateApp() to initialize the library and create the App singleton:
///
/// ```
/// // Create the App using default config and settings
/// ULApp app = ulCreateApp(NULL, NULL);
/// ```
///
/// ## Creating a Window
///
/// Call ulCreateWindow() to create one or more windows during the lifetime of your app:
///
/// ```
/// ULWindow window = ulCreateWindow(ulAppGetMainMonitor(app), 1024, 768, false,
///                                  kWindowFlags_Titled | kWindowFlags_Resizable);
/// ```
///
/// ## Adding a Panel to a Window
///
/// A window's content is a layout of panels, each showing a View (see
/// `<AppCore/CAPI/CAPI_Layout.h>`).
///
/// Call ulWindowAddPanel() to add a panel; NULL options create a full-window panel:
///
/// ```
/// ULPanel panel = ulWindowAddPanel(window, NULL, NULL);
/// ```
///
/// Each Panel has a View instance that you can use to load web content into:
///
/// ```
/// ULString url = ulCreateString("https://google.com");
/// ULView view = ulPanelGetView(panel);
/// ulViewLoadURL(view, url);
/// ulDestroyString(url);
/// ulDestroyView(view);
/// ```
///
/// ## Coordinate Systems
///
/// The OS may apply a device scale to a monitor or window (eg, 2x on a Retina display).
///
/// Sizes and positions in this API are in logical pixels (independent of the device scale),
/// unless the name says device (eg, ulWindowGetDeviceWidth()). The two are related by
/// ulWindowGetScale():
///
/// ```
/// device_pixels = round(logical_pixels * scale)
/// ```
///
/// Use ulWindowLogicalToDevice() and ulWindowDeviceToLogical() to convert between them.
///
/// Positions inside a window (mouse events, the layout, hit-test regions) are in window
/// content coordinates: logical pixels from the top-left of the window's content area.
///
/// ## Running the App
///
/// Call ulAppRun() to start the main run loop:
///
/// ```
/// #include <AppCore/CAPI.h>
///
/// int main() {
///   // Initialize app, window, panels, etc. here...
///
///   ulAppRun(app);
///
///   ulDestroyApp(app);
///   return 0;
/// }
/// ```
///
#ifndef APPCORE_CAPI_H
#define APPCORE_CAPI_H

#include <AppCore/CAPI/CAPI_Defines.h>

#ifdef __cplusplus
extern "C" {
#endif

///
/// Window creation flags (combine with `|`). @see  ulCreateWindow()
///
typedef enum {
  kWindowFlags_Borderless  = 1 << 0,  ///< No frame or title bar.
  kWindowFlags_Titled      = 1 << 1,  ///< A title bar.
  kWindowFlags_Resizable   = 1 << 2,  ///< The user can resize the window by dragging its edges.
  kWindowFlags_Maximizable = 1 << 3,  ///< Minimize and maximize buttons.
  kWindowFlags_Hidden      = 1 << 4,  ///< Start hidden (call ulWindowShow() to show it).

  ///
  /// Per-pixel transparency: anything the window leaves transparent shows what's behind it.
  /// You can only set this when you create the window.
  ///
  /// The View and the page need to be transparent too (ulViewConfigSetIsTransparent() and
  /// a transparent page background), since pages draw on an opaque white background by
  /// default.
  ///
  /// \parblock
  /// @note  On Windows, transparent CPU-rendered windows are always borderless (the frame
  ///        flags are ignored with a warning), and drawing from the clear and paint
  ///        callbacks has no effect on them (see ulWindowSetClearCallback()).
  /// \endparblock
  ///
  /// \parblock
  /// @note  Mouse input isn't tested against transparency. On Windows, GPU-rendered
  ///        transparent windows receive input over their whole rectangle, while CPU-rendered
  ///        ones let input over fully transparent pixels fall through to the window beneath.
  /// \endparblock
  ///
  /// \parblock
  /// @note  On macOS, transparent windows get a shadow shaped to their visible content.
  ///        Square corners remove it (see ulWindowSetCornerStyle()).
  /// \endparblock
  ///
  kWindowFlags_Transparent = 1 << 5,

  ///
  /// Marks a popup window. ulCreatePopupWindow() sets this for you (ulCreateWindow()
  /// ignores it with a warning).
  ///
  /// Popups are always borderless and transparent, and ignore kWindowFlags_CustomChrome.
  ///
  kWindowFlags_Popup       = 1 << 6,

  ///
  /// Draw the title bar and caption buttons yourself, as page content. The window keeps
  /// every native frame behavior (resizing, snapping, the system shadow). Declare which
  /// areas drag the window or act as caption buttons with ulWindowSetHitTestRegions() or
  /// the page's `app-region` CSS property.
  ///
  /// You don't need kWindowFlags_Titled with this. It's ignored (with a warning) together
  /// with kWindowFlags_Borderless, and on popup and fullscreen windows. See the Custom
  /// Chrome section in `<AppCore/Window.h>` for the full setup and the per-platform
  /// differences.
  ///
  kWindowFlags_CustomChrome = 1 << 7,
} ULWindowFlags;

///
/// The native backdrop materials a window can use. @see  ulWindowSetBackdrop()
///
/// Choose the material that matches what your window is (a main window, a menu, or an
/// overlay). The OS decides how each one looks.
///
typedef enum {
  kULBackdrop_None    = 0,  ///< No material.
  kULBackdrop_Window  = 1,  ///< For a main window.
  kULBackdrop_Popup   = 2,  ///< For menus, dropdowns, and other flyouts.
  kULBackdrop_Overlay = 3,  ///< For heads-up panels and on-screen displays (eg, a volume
                            ///< indicator).
} ULBackdropMaterial;

///
/// The window's current display state. @see  ulWindowSetWindowStateCallback()
///
typedef enum {
  kULWindowState_Normal    = 0,  ///< Neither minimized nor maximized.
  kULWindowState_Minimized = 1,
  kULWindowState_Maximized = 2,
} ULWindowState;

///
/// The window's corner style. @see  ulWindowSetCornerStyle()
///
/// By default, framed windows, custom-chrome windows, and popups get rounded corners, and
/// other transparent windows get square ones. Linux leaves corners to the window manager.
///
/// @note  On windows that draw their own frame (custom-chrome and transparent windows), the
///        system shadow comes with the rounded corners: squaring them removes the shadow too.
///
typedef enum {
  kULCornerStyle_Default = 0,  ///< The platform's usual corners for this kind of window.
  kULCornerStyle_Rounded = 1,  ///< Rounded corners.
  kULCornerStyle_Square  = 2,  ///< Square corners.
} ULCornerStyle;

///
/// Whether a backdrop material shows its active or inactive look.
/// @see  ulWindowSetBackdropState()
///
typedef enum {
  kULBackdropState_FollowWindow = 0,  ///< Match the window: the active look while the window
                                      ///< is active.
  kULBackdropState_Active       = 1,  ///< Always show the active look.
  kULBackdropState_Inactive     = 2,  ///< Always show the inactive look.
} ULBackdropState;

///
/// The look of a backdrop material. @see  ulWindowSetBackdrop()
///
typedef enum {
  kULBackdropVariant_Auto    = 0,  ///< The platform's usual look for the material.
  kULBackdropVariant_Frosted = 1,  ///< Translucent: a live blur of what's behind the window.
  kULBackdropVariant_Matte   = 2,  ///< Opaque: tinted by the desktop wallpaper and theme.
} ULBackdropVariant;

///
/// The light or dark look of a backdrop material. @see  ulWindowSetBackdrop()
///
typedef enum {
  kULBackdropTheme_Auto  = 0,  ///< Follow the OS theme.
  kULBackdropTheme_Light = 1,  ///< Always use the light look.
  kULBackdropTheme_Dark  = 2,  ///< Always use the dark look.
} ULBackdropTheme;

///
/// The style of an opaque kULBackdrop_Window material. @see  ulWindowSetBackdrop()
///
/// Only Windows has more than one: most windows use the standard style, and Windows' own
/// tabbed apps (eg, File Explorer) use a more colorful one. Other platforms ignore this.
///
typedef enum {
  kULBackdropRendition_Auto     = 0,  ///< The platform default (Standard).
  kULBackdropRendition_Standard = 1,  ///< The style most windows use.
  kULBackdropRendition_Vivid    = 2,  ///< The more colorful style, with more of the
                                      ///< wallpaper's color.
} ULBackdropRendition;

///
/// Optional settings for a backdrop material. @see  ulWindowSetBackdrop()
///
/// Zero-initialize the struct and set struct_size to sizeof(ULBackdropOptions); a zeroed
/// field means the default. Fields may be appended in later versions, so the size tells the
/// library which of them you set.
///
/// See BackdropOptions in `<AppCore/Window.h>` for what each field does.
///
typedef struct {
  unsigned int struct_size;  ///< Set to sizeof(ULBackdropOptions).
  int variant;               ///< The material's look, a ULBackdropVariant value (0 = Auto).
  int theme;                 ///< The material's light or dark look, a ULBackdropTheme value
                             ///< (0 = Auto). On Windows and macOS this also sets the title bar's.
  ULColor fallback_color;    ///< The color of the fallback fill (alpha ignored); unset uses a
                             ///< color from the theme.
  int rendition;             ///< The style of an opaque kULBackdrop_Window, a
                             ///< ULBackdropRendition value (0 = Auto).
} ULBackdropOptions;

///
/// Auto-dismiss policy. @see  ulWindowSetDismiss()
///
typedef enum {
  kULDismiss_Manual = 0,  ///< The window hides only when the application hides it.
  kULDismiss_Auto   = 1,  ///< The library hides the window on auto-dismiss triggers.
} ULDismiss;

///
/// What a hit-test region does. @see  ulWindowSetHitTestRegions()
///
typedef enum {
  kULHitRegion_Caption  = 0,  ///< A draggable title-bar area (double-clicking it does what
                              ///< double-clicking a title bar does on the platform). The
                              ///< page under it gets no mouse input.
  kULHitRegion_Minimize = 1,  ///< Your minimize button.
  kULHitRegion_Maximize = 2,  ///< Your maximize/restore button (on Windows 11, hovering it
                              ///< also shows the snap layouts flyout).
  kULHitRegion_Close    = 3,  ///< Your close button.
} ULHitRegionRole;

///
/// The edge or corner to resize a window from. @see  ulWindowBeginDragResize()
///
typedef enum {
  kULResizeEdge_Left = 0,
  kULResizeEdge_Top,
  kULResizeEdge_Right,
  kULResizeEdge_Bottom,
  kULResizeEdge_TopLeft,
  kULResizeEdge_TopRight,
  kULResizeEdge_BottomLeft,
  kULResizeEdge_BottomRight,
} ULResizeEdge;

///
/// The window actions available to the user (combine with `|`).
/// @see  ulWindowSetWindowButtons()
///
typedef enum {
  kULWindowButtons_None     = 0,
  kULWindowButtons_Minimize = 1 << 0,
  kULWindowButtons_Maximize = 1 << 1,
  kULWindowButtons_Close    = 1 << 2,
  kULWindowButtons_All      = 0x7,
} ULWindowButtons;

///
/// An area of a custom-chrome window and what it does. @see  ulWindowSetHitTestRegions()
///
/// Zero-initialize the struct and set struct_size to sizeof(ULHitTestRegionDesc). Every
/// element of an array passed to ulWindowSetHitTestRegions() must use the same struct_size:
/// it doubles as the array stride, so fields can be appended in later versions.
///
typedef struct {
  unsigned int struct_size;  ///< Set to sizeof(ULHitTestRegionDesc).
  int role;                  ///< What the region does, a ULHitRegionRole value.
  ULLayoutRect rect;         ///< The region's bounds (in logical pixels, window content
                             ///< coordinates).
} ULHitTestRegionDesc;

///
/// Create settings with default values (see <AppCore/App.h>).
///
/// @return  Returns a new ULSettings instance. You must call ulDestroySettings() when finished.
///
ACExport ULSettings ulCreateSettings(void);

///
/// Destroy settings previously created by ulCreateSettings().
///
ACExport void ulDestroySettings(ULSettings settings);

///
/// Set the name of the developer of this app (Default = MyCompany).
///
/// Together with the app name, this builds the per-app folders where the library keeps the
/// log file, profiler traces, and session data (see Settings::developer_name in
/// <AppCore/App.h> for where they are).
///
ACExport void ulSettingsSetDeveloperName(ULSettings settings, ULString name);

///
/// Set the name of this app (Default = MyApp), used with the developer name to build the
/// per-app folders (see ulSettingsSetDeveloperName()).
///
ACExport void ulSettingsSetAppName(ULSettings settings, ULString name);

///
/// Set the application icon (Default = NULL, the platform's standard icon).
///
/// This is the Dock icon on macOS. On Windows and Linux it's the default icon for windows that
/// haven't set their own with ulWindowSetIcon().
///
/// @param  settings  The App settings.
///
/// @param  icon      A 32-bit BGRA bitmap (kBitmapFormat_BGRA8_UNORM_SRGB) with straight
///                   (unpremultiplied) alpha, or NULL to keep the platform's standard icon.
///                   The library does not take ownership of the handle; you can destroy it
///                   with ulDestroyBitmap() once this call returns.
///
ACExport void ulSettingsSetAppIcon(ULSettings settings, ULBitmap icon);

///
/// Set the root folder for `file:///` URLs (Default = ./assets/).
///
/// You should set this to the relative path where all of your app data is (eg,
/// `file:///page.html` loads `page.html` from this folder).
///
/// @note  The path is relative to:
///         - Windows: the executable's folder
///         - Linux:   the executable's folder
///         - macOS:   `YourApp.app/Contents/Resources/`
///
ACExport void ulSettingsSetFileSystemPath(ULSettings settings, ULString path);

///
/// Set whether to always use the CPU renderer (Default = False). By default the library uses
/// the GPU renderer when it finds a compatible GPU.
///
ACExport void ulSettingsSetForceCPURenderer(ULSettings settings,
                                            bool force_cpu);

///
/// Set the paint rate, in frames per second, for windows created hidden (Default = 0,
/// hidden windows are not painted).
///
/// Hidden windows receive no OS paint events; a positive rate drives them through the
/// normal render loop at that fixed rate, which is what offscreen / headless rendering
/// needs. See Settings::headless_paint_fps in <AppCore/App.h>.
///
ACExport void ulSettingsSetHeadlessPaintFPS(ULSettings settings, unsigned int fps);

///
/// Set whether the app's update loop should run at full speed without yielding to the
/// OS scheduler between frames (Default = False).
///
/// Display refresh and hidden-window painting stay paced, and painting stays
/// invalidation-driven, so animation timing is unchanged. See Settings::full_speed_loop in
/// <AppCore/App.h>.
///
/// @warning  The loop thread busy-waits, so a single instance can fully occupy one CPU core.
///           This trades CPU for throughput and is meant for batch / offscreen workloads, not
///           interactive apps.
///
ACExport void ulSettingsSetFullSpeedLoop(ULSettings settings, bool enabled);

///
/// Set whether to time animations for the moment each frame appears on screen, instead of the
/// display refresh that started it (Default = True).
///
/// With this on, `window.requestAnimationFrame()` timestamps run ahead of `performance.now()`
/// by the display's presentation delay (one to a few refreshes). See
/// Settings::sync_animations_to_present in <AppCore/App.h>.
///
/// @note  This applies on macOS and Windows.
///
ACExport void ulSettingsSetSyncAnimationsToPresent(ULSettings settings, bool enabled);

///
/// Set whether text editing in the Views this app creates should follow the host OS's native
/// conventions (Default = True).
///
/// Set this to false for the library's identical-on-every-platform editing behavior. A
/// config you pass when creating a panel has its own value (see
/// ulViewConfigSetMatchNativeEditingBehavior() in <Ultralight/CAPI/CAPI_View.h>).
///
ACExport void ulSettingsSetMatchNativeEditingBehavior(ULSettings settings, bool enabled);

///
/// Set whether to pick the font-appearance preset from the operating system (Default = True).
///
/// Only applies while the Config's font profile is kFontProfile_Default. A profile you set
/// explicitly always wins.
///
ACExport void ulSettingsSetAutoFontProfile(ULSettings settings, bool enabled);

///
/// Set whether to pick the default font families from the operating system, so generic CSS
/// families resolve to the same fonts as the platform's browsers (Default = True).
///
/// Applies to Views created without an explicit ViewConfig; a config you pass when creating a
/// panel keeps its own font families (start from ulWindowGetDefaultViewConfig() to inherit
/// them).
///
ACExport void ulSettingsSetAutoFontFamilies(ULSettings settings, bool enabled);

///
/// Set the minimum duration of user inactivity, in seconds, before idle detection begins
/// (Default = 0.5).
///
ACExport void ulSettingsSetIdleThreshold(ULSettings settings, double seconds);

///
/// Set the sustained idle time, in seconds (Default = 2.0).
///
/// The app must remain continuously idle for this duration before the idle callback fires. This
/// is also the cooldown between repeated idle callbacks.
///
/// @see  ulAppSetIdleCallback()
///
ACExport void ulSettingsSetSustainedIdleTime(ULSettings settings, double seconds);

///
/// Set the thread CPU utilization threshold (0.0-1.0) below which the app is considered idle
/// (Default = 0.5, ie. 50%).
///
ACExport void ulSettingsSetIdleUtilizationThreshold(ULSettings settings, double threshold);

///
/// Create the App singleton.
///
/// @param  settings  Settings to customize App runtime behavior. You can pass NULL for this
///                   parameter to use default settings.
///
/// @param  config    Config options for the Ultralight renderer. You can pass NULL for this
///                   parameter to use default config.
///
/// @return  Returns a new ULApp instance. You must call ulDestroyApp() when finished.
///
/// \parblock
/// @note  You should only create one App per application lifetime.
/// \endparblock
///
/// \parblock
/// @note  ulCreateApp() adjusts a few Config fields: it always sets the face winding to
///        clockwise, fills in the cache path when you leave it empty, and picks the font
///        profile while auto font profile is on (see App::Create() in <AppCore/App.h>).
/// \endparblock
///
ACExport ULApp ulCreateApp(ULSettings settings, ULConfig config);

///
/// Destroy the App instance created by ulCreateApp().
///
/// @note  All Windows should be destroyed before calling this.
///
ACExport void ulDestroyApp(ULApp app);

///
/// The callback invoked when the App updates.
///
typedef void
(*ULUpdateCallback) (void* user_data);

///
/// Set a callback for whenever the App updates.
///
/// You should update all app logic here.
///
/// @param  app                The App instance.
///
/// @param  callback           Invoked on each update of the run loop. Pass NULL to clear a
///                            previously set callback.
///
/// @param  user_data          Passed to `callback` (can be NULL).
///
/// @param  destroy_user_data  Invoked when the callback is replaced or the App is destroyed
///                            (can be NULL).
///
/// @note  This fires right before the library updates the Renderer.
///
ACExport void ulAppSetUpdateCallback(ULApp app, ULUpdateCallback callback,
                                     void* user_data,
                                     ULUserDataDestroyCallback destroy_user_data);

///
/// Whether or not the App is running.
///
ACExport bool ulAppIsRunning(ULApp app);

///
/// Get the main monitor (this is never NULL).
///
ACExport ULMonitor ulAppGetMainMonitor(ULApp app);

///
/// Get the number of connected monitors.
///
ACExport unsigned int ulAppGetMonitorCount(ULApp app);

///
/// Get a monitor by index (from 0 to `ulAppGetMonitorCount() - 1`).
///
/// Index 0 is always the main monitor.
///
/// You can pass the returned monitor to ulCreateWindow() to open a window on that display.
///
/// @param  app    The App instance.
///
/// @param  index  The index of the monitor to get.
///
/// @return  Returns the monitor, or NULL when `index` is out of range.
///
/// @note  The returned monitor is owned by the App and remains valid for the lifetime of the
///        App. If a monitor is disconnected, it is no longer enumerated, but its handle remains
///        safe to use and its accessors return fallback values.
///
ACExport ULMonitor ulAppGetMonitor(ULApp app, unsigned int index);

///
/// Get the underlying Renderer instance.
///
/// @note  Don't destroy the returned renderer, it is owned by the App and is destroyed when
///        ulDestroyApp() is called.
///
ACExport ULRenderer ulAppGetRenderer(ULApp app);

///
/// Run the main loop (this returns after ulAppQuit()).
///
ACExport void ulAppRun(ULApp app);

///
/// Quit the application.
///
/// This stops the main loop so that ulAppRun() returns. The process itself is not exited.
///
/// @note  On macOS, a quit issued while a modal loop is running (eg, a native message box or an
///        interactive window drag) is absorbed by that inner loop and does not end ulAppRun()--
///        call this again once the modal loop has finished.
///
ACExport void ulAppQuit(ULApp app);

///
/// Advance the app by one iteration of the main loop, on behalf of your own run loop.
///
/// This pumps pending OS events (waiting up to `max_wait_seconds` for one when none are
/// pending), updates the renderer, and repaints windows when the display interval has elapsed.
/// Call it repeatedly from your application's loop instead of ulAppRun() when your app owns the
/// loop.
///
/// @param  app               The App instance.
///
/// @param  max_wait_seconds  The longest this call may block waiting for an event or timer
///                           tick. Pass 0.0 to return immediately after processing whatever is
///                           pending.
///
/// @return  Returns whether the app is still running (false once ulAppQuit() has been called).
///
/// @note  On macOS this pumps the event queue manually rather than running the native run
///        loop, so menu key-equivalent routing can differ from ulAppRun().
///
ACExport bool ulAppRunOnce(ULApp app, double max_wait_seconds);

///
/// Callback for a task posted to the main thread.
///
typedef void (*ULAppTaskCallback)(void* user_data);

///
/// Post a task to run on the main thread during a future update of the app's loop.
///
/// Safe to call from any thread. The post wakes the main loop, so a task posted from another
/// thread runs without waiting for the next timer tick. Tasks run in posting order during the
/// run loop's call to Renderer::Update.
///
/// @param  app                The App instance.
///
/// @param  task               Invoked once on the main thread.
///
/// @param  user_data          Passed to `task` (can be NULL).
///
/// @param  destroy_user_data  Invoked exactly once after `task` runs, or without `task` running
///                            if the App is destroyed first (can be NULL).
///
ACExport void ulAppPostTask(ULApp app, ULAppTaskCallback task, void* user_data,
                            ULUserDataDestroyCallback destroy_user_data);

///
/// Post a task to run on the main thread once a delay has elapsed.
///
/// Safe to call from any thread. Delivery resolution is the update tick (about 2 ms).
///
/// @param  app                The App instance.
///
/// @param  delay_ms           The delay before the task runs, in milliseconds.
///
/// @param  task               Invoked once on the main thread.
///
/// @param  user_data          Passed to `task` (can be NULL).
///
/// @param  destroy_user_data  Invoked exactly once after `task` runs, or without `task` running
///                            if the App is destroyed first (can be NULL).
///
/// @see  ulAppPostTask()
///
ACExport void ulAppPostDelayedTask(ULApp app, double delay_ms, ULAppTaskCallback task,
                                   void* user_data, ULUserDataDestroyCallback destroy_user_data);

///
/// Create a repeating timer that fires on the main thread.
///
/// Safe to call from any thread. Fire resolution is the update tick (about 2 ms); fires missed
/// while the loop was stalled are not repeated (the timer resumes on its interval).
///
/// @param  app                The App instance.
///
/// @param  interval_ms        Time between fires, in milliseconds.
///
/// @param  callback           Invoked on each fire, on the main thread.
///
/// @param  user_data          Passed to `callback` (can be NULL).
///
/// @param  destroy_user_data  Invoked exactly once when the timer is destroyed or the App is
///                            destroyed (can be NULL).
///
/// @return  Returns a new ULAppTimer instance that owns the timer. You must call
///          ulDestroyAppTimer() to stop it.
///
ACExport ULAppTimer ulAppSetInterval(ULApp app, double interval_ms, ULAppTaskCallback callback,
                                     void* user_data,
                                     ULUserDataDestroyCallback destroy_user_data);

///
/// Destroy a timer created by ulAppSetInterval().
///
/// This stops the timer and destroys its user data. You should call it on the main thread.
/// Passing NULL does nothing.
///
/// @note  A timer may destroy itself from inside its own callback; its user data is then
///        destroyed once the callback returns.
///
ACExport void ulDestroyAppTimer(ULAppTimer timer);

///
/// The callback invoked when the app has been idle for a sustained period.
///
typedef void
(*ULIdleCallback) (void* user_data, double utilization);

///
/// Set a callback for when the app is idle (low CPU use and no recent input).
///
/// @param  app                The App instance.
///
/// @param  callback           Invoked while the app is idle. Its utilization parameter
///                            (0.0-1.0) is the current main-thread CPU use. Pass NULL to clear a
///                            previously set callback.
///
/// @param  user_data          Passed to `callback` (can be NULL).
///
/// @param  destroy_user_data  Invoked when the callback is replaced or the App is destroyed
///                            (can be NULL).
///
/// @note  The callback fires once the app has been idle for the sustained idle time, then
///        repeats at that interval while the idle conditions hold.
///
/// @see  ulSettingsSetSustainedIdleTime()
///
ACExport void ulAppSetIdleCallback(ULApp app, ULIdleCallback callback,
                                   void* user_data,
                                   ULUserDataDestroyCallback destroy_user_data);

///
/// Whether or not the app is currently idle (low CPU utilization and no recent user input,
/// sustained past the configured threshold).
///
ACExport bool ulAppIsIdle(ULApp app);

///
/// Get the current main-thread CPU utilization (0.0-1.0) averaged over the last ~1 second.
///
ACExport double ulAppGetThreadUtilization(ULApp app);

///
/// GPU memory statistics reported by the App's GPU driver. @see  ulAppGetGPUMemoryStats()
///
typedef struct {
  /// Bytes allocated for sampled textures (not counting render targets).
  uint64_t texture_bytes;

  /// Number of sampled textures currently allocated.
  uint32_t texture_count;

  /// Bytes allocated for render targets, including multisample storage and
  /// any single-sample resolve textures.
  uint64_t render_target_bytes;

  /// Number of render-target textures currently allocated.
  uint32_t render_target_count;

  /// Bytes allocated for geometry (vertex and index buffers).
  uint64_t geometry_bytes;

  /// Number of geometry buffers currently allocated.
  uint32_t geometry_count;

  /// Bytes allocated for window back-buffers.
  uint64_t swap_chain_bytes;

  /// Number of swap chains currently active.
  uint32_t swap_chain_count;

  /// Bytes allocated for transient per-frame memory (upload staging and
  /// constant buffers).
  uint64_t transient_bytes;

  /// Total bytes of GPU heap memory committed by the backend's allocator.
  /// Only reported by backends that sub-allocate resources from larger
  /// heaps; zero elsewhere.
  uint64_t allocator_committed_bytes;

  /// Current video memory usage of this process, as reported by the
  /// operating system. Zero if the OS query is unavailable.
  uint64_t process_usage_bytes;

  /// Video memory budget for this process, as reported by the operating
  /// system. Zero if the OS query is unavailable.
  uint64_t process_budget_bytes;

  /// Dedicated video memory of the active display adapter, in bytes.
  uint64_t adapter_video_memory_bytes;

  /// Human-readable name of the active display adapter (UTF-8,
  /// null-terminated, truncated if necessary).
  char adapter_description[128];
} ULGPUMemoryStats;

///
/// Get GPU memory statistics from the App's GPU driver.
///
/// @param  app    The App instance.
///
/// @param  stats  Structure to fill. All fields are overwritten on success.
///
/// @return  Returns true if `stats` was filled. Returns false when rendering
///          with the CPU renderer or when the active GPU backend does not
///          provide memory statistics.
///
/// @note  Among the stock GPU drivers, the Direct3D backends on Windows, the Metal backend on
///        macOS, and the OpenGL backend on Linux implement this (OpenGL reports no process
///        usage or budget).
///
ACExport bool ulAppGetGPUMemoryStats(ULApp app, ULGPUMemoryStats* stats);

///
/// Get the monitor's DPI scale (1.0 = 100%).
///
ACExport double ulMonitorGetScale(ULMonitor monitor);

///
/// Get the width of the monitor (in logical pixels).
///
ACExport double ulMonitorGetWidth(ULMonitor monitor);

///
/// Get the height of the monitor (in logical pixels).
///
ACExport double ulMonitorGetHeight(ULMonitor monitor);

///
/// Get the width of the monitor (in device pixels).
///
ACExport unsigned int ulMonitorGetDeviceWidth(ULMonitor monitor);

///
/// Get the height of the monitor (in device pixels).
///
ACExport unsigned int ulMonitorGetDeviceHeight(ULMonitor monitor);

///
/// Create a new Window.
///
/// @param  monitor       The monitor to create the Window on.
///
/// @param  width         The width (in logical pixels).
///
/// @param  height        The height (in logical pixels).
///
/// @param  fullscreen    Whether or not to create the window fullscreen.
///
/// @param  window_flags  How the window looks and behaves (a combination of ULWindowFlags).
///
/// @return  Returns a new ULWindow instance. You must call ulDestroyWindow() when finished.
///
/// @note  The window is shown right away unless you pass kWindowFlags_Hidden (call
///        ulWindowShow() to show it later).
///
ACExport ULWindow ulCreateWindow(ULMonitor monitor, double width,
                                 double height, bool fullscreen,
                                 unsigned int window_flags);

///
/// Create a popup window owned by another window.
///
/// Popups are borderless, transparent windows that stay above their owner and don't appear
/// in the taskbar. Clicking one doesn't take focus away from the owner, and the library
/// dismisses it like a native menu (see ulWindowSetDismiss()). Use them for menus,
/// dropdowns, and other short-lived content.
///
/// @param  owner         The window that owns the popup. The popup stays above it,
///                       positions relative to it, and hides with it.
///
/// @param  x             The x-position (in logical pixels) from the left of the owner's
///                       content area.
///
/// @param  y             The y-position (in logical pixels) from the top of the owner's
///                       content area.
///
/// @param  width         The width (in logical pixels).
///
/// @param  height        The height (in logical pixels).
///
/// @param  window_flags  Extra window flags (eg, kWindowFlags_Hidden for a popup you create
///                       once and show when needed).
///
/// @return  Returns a new ULWindow instance, or NULL if `owner` is NULL. You must call
///          ulDestroyWindow() when finished.
///
/// \parblock
/// @note  Popups always position relative to their owner's content area (this includes
///        ulWindowMoveTo(), ulWindowGetPositionX(), and ulWindowGetPositionY()), so you can
///        place one at an element's position in the owner's page without converting it.
///        The position stays correct when the owner's device scale changes.
/// \endparblock
///
/// \parblock
/// @note  When the owner closes, its popups close with it (the close callback fires for
///        each). The popup handle stays valid, but ulWindowShow() and ulWindowMoveTo() then
///        only log a warning, so create reusable popups on a window that outlives them.
/// \endparblock
///
/// \parblock
/// @note  Popups get rounded corners and a drop shadow by default. For a sharp, shadow-free
///        popup, call ulWindowSetCornerStyle() with kULCornerStyle_Square.
/// \endparblock
///
/// @see  ulWindowSetBackdrop()
///
ACExport ULWindow ulCreatePopupWindow(ULWindow owner, double x, double y, double width,
                                      double height, unsigned int window_flags);

///
/// Destroy a window handle created with ulCreateWindow() or ulCreatePopupWindow().
///
/// Popup handles need this too, even after their owner has closed them.
///
/// If the window is still open, this closes it without firing the close callback (call
/// ulWindowClose() first if you need it to fire).
///
/// Each `destroy_user_data` you passed to this window's callback setters in this header (eg,
/// ulWindowSetCloseCallback()) runs here, not when the window closes. If you call this from
/// inside one of the window's callbacks, they run after that callback returns.
///
/// @param  window  The window to destroy (can be NULL).
///
ACExport void ulDestroyWindow(ULWindow window);

///
/// Callback for when a window closes.
///
typedef void
(*ULCloseCallback) (void* user_data, ULWindow window);

///
/// Set a callback to be notified when a window closes.
///
/// @param  window             The window to listen to.
///
/// @param  callback           Invoked when the window closes (eg, the user clicks the close
///                            button, or you call ulWindowClose()). Pass NULL to clear a
///                            previously set callback.
///
/// @param  user_data          Passed to `callback` (can be NULL).
///
/// @param  destroy_user_data  Invoked when the callback is replaced or the window is destroyed
///                            with ulDestroyWindow() (can be NULL).
///
ACExport void ulWindowSetCloseCallback(ULWindow window,
                                       ULCloseCallback callback,
                                       void* user_data,
                                       ULUserDataDestroyCallback destroy_user_data);

///
/// Callback for when a window resizes.
///
typedef void
(*ULResizeCallback) (void* user_data, ULWindow window, double width, double height);

///
/// Set a callback to be notified when a window resizes.
///
/// @param  window             The window to listen to.
///
/// @param  callback           Invoked with the window's new width and height (in logical
///                            pixels). Pass NULL to clear a previously set callback.
///
/// @param  user_data          Passed to `callback` (can be NULL).
///
/// @param  destroy_user_data  Invoked when the callback is replaced or the window is destroyed
///                            with ulDestroyWindow() (can be NULL).
///
ACExport void ulWindowSetResizeCallback(ULWindow window,
                                        ULResizeCallback callback,
                                        void* user_data,
                                        ULUserDataDestroyCallback destroy_user_data);

///
/// Get window width (in logical pixels).
///
ACExport double ulWindowGetWidth(ULWindow window);

///
/// Get window height (in logical pixels).
///
ACExport double ulWindowGetHeight(ULWindow window);

///
/// Get window width (in device pixels).
///
ACExport unsigned int ulWindowGetDeviceWidth(ULWindow window);

///
/// Get window height (in device pixels).
///
ACExport unsigned int ulWindowGetDeviceHeight(ULWindow window);

///
/// Move the window to a new position on the screen.
///
/// @param  window  The window to move.
///
/// @param  x       The new x-position (in logical pixels).
///
/// @param  y       The new y-position (in logical pixels).
///
/// @note  Positions are global screen coordinates (one space across all monitors). Popups
///        position relative to their owner's content area instead (see
///        ulCreatePopupWindow()).
///
ACExport void ulWindowMoveTo(ULWindow window, double x, double y);

///
/// Move the window to the center of the screen.
///
/// @note  Popups ignore this with a warning (they position relative to their owner).
///
ACExport void ulWindowMoveToCenter(ULWindow);

///
/// Get the x-position of the window (in logical pixels). @see  ulWindowMoveTo()
///
ACExport double ulWindowGetPositionX(ULWindow window);

///
/// Get the y-position of the window (in logical pixels). @see  ulWindowMoveTo()
///
ACExport double ulWindowGetPositionY(ULWindow window);

///
/// Get whether or not a window is fullscreen.
///
ACExport bool ulWindowIsFullscreen(ULWindow window);

///
/// Get the window's device scale (eg, 2.0 on a Retina display).
///
ACExport double ulWindowGetScale(ULWindow window);

///
/// Set the window title.
///
/// @param  window  The window.
///
/// @param  title   The new title, as a UTF-8 string.
///
/// @note  Custom-chrome windows don't draw a system title bar, but the OS still shows the
///        title elsewhere (eg, the taskbar and window switcher).
///
ACExport void ulWindowSetTitle(ULWindow window, const char* title);

///
/// Set the window's icon (shown in the title bar and taskbar).
///
/// @param  window  The window.
///
/// @param  icon    A 32-bit BGRA bitmap (kBitmapFormat_BGRA8_UNORM_SRGB) with straight
///                 (unpremultiplied) alpha, or NULL to restore the default icon (the app icon
///                 from ulSettingsSetAppIcon() when set, else the platform's standard icon).
///                 The library does not take ownership of the handle; you can destroy it with
///                 ulDestroyBitmap() once this call returns.
///
/// @note  macOS windows don't show an icon (the Dock icon comes from ulSettingsSetAppIcon()
///        or your app bundle), so this does nothing there.
///
ACExport void ulWindowSetIcon(ULWindow window, ULBitmap icon);

///
/// Set the cursor for a window.
///
/// @param  window  The window.
///
/// @param  cursor  The cursor to show over the window.
///
/// @note  The library sets the cursor for web content automatically (link hands, the text
///        I-beam, CSS cursor rules), so you don't need to call this for pages. A cursor you
///        set here lasts until the mouse next moves over a page.
///
ACExport void ulWindowSetCursor(ULWindow window, ULCursor cursor);

///
/// Show the window (if it was hidden).
///
/// @note  On macOS, showing a minimized window briefly brings it on screen (and takes focus)
///        before it minimizes to the Dock. On Windows it appears minimized directly.
///
ACExport void ulWindowShow(ULWindow window);

///
/// Hide the window.
///
/// @note  Hiding a window also hides the popups it owns.
///
ACExport void ulWindowHide(ULWindow window);

///
/// Show the window once its pages have settled.
///
/// Call this instead of ulWindowShow() on a window created hidden (kWindowFlags_Hidden),
/// after adding panels and starting their page loads. This avoids displaying a blank or
/// half-loaded first frame.
///
/// While waiting, the window stays hidden-- its pages continue to load, run scripts,
/// and lay out.
///
/// The window shows once every page in a visible panel has settled
/// (ulViewSetPageSettledCallback()).
///
/// A panel with no page loaded does not delay showing the window.
///
/// If the pages have not settled when the timeout elapses, the window shows anyway.
///
/// @param  window           The window.
///
/// @param  timeout_seconds  The maximum time to wait, in seconds. The usual value is 0.5.
///                          Pass 0 to show the window immediately.
///
/// \parblock
/// @note  Calling ulWindowShow() or ulWindowHide() before the window shows cancels the wait--
///        ulWindowShow() shows the window immediately, while ulWindowHide() keeps it hidden.
/// \endparblock
///
/// \parblock
/// @note  If the window is already visible, this call does nothing.
/// \endparblock
///
ACExport void ulWindowShowWhenReady(ULWindow window, double timeout_seconds);

///
/// Whether or not the window is visible (not hidden).
///
ACExport bool ulWindowIsVisible(ULWindow window);

///
/// Whether or not the window is maximized.
///
ACExport bool ulWindowIsMaximized(ULWindow window);

///
/// Whether or not the window is minimized.
///
ACExport bool ulWindowIsMinimized(ULWindow window);

///
/// Maximize the window. Popups ignore this with a warning.
///
/// @note  On macOS, maximizing a window that isn't resizable changes ulWindowIsMaximized()
///        (and fires the window-state callback) without changing the window's size, since
///        the OS only zooms resizable windows. On Windows the window resizes either way.
///
ACExport void ulWindowMaximize(ULWindow window);

///
/// Minimize the window. Popups ignore this with a warning.
///
ACExport void ulWindowMinimize(ULWindow window);

///
/// Restore the window from the minimized or maximized state. Popups ignore this with a
/// warning.
///
ACExport void ulWindowRestore(ULWindow window);

///
/// Get the ViewConfig this window uses for a bare ulWindowAddPanel().
///
/// The window fills in what it or the app's Settings decide, and leaves the rest at the
/// defaults:
///
///  - the initial device scale (the window's scale)
///  - the display id (the window's display)
///  - acceleration (whether the window composites on the GPU)
///  - native editing behavior (from ulSettingsSetMatchNativeEditingBehavior())
///  - the default font families for the host OS (while ulSettingsSetAutoFontFamilies() is
///    enabled)
///
/// You can use this as a starting point for a View you create yourself (ulCreateView()) and
/// add to a panel.
///
/// @return  Returns a new ULViewConfig instance. You must call ulDestroyViewConfig() when
///          finished. The values are a copy; a later device scale change won't update it.
///
ACExport ULViewConfig ulWindowGetDefaultViewConfig(ULWindow window);

///
/// Close a window.
///
/// The close callback fires, the popups this window owns close with it, and the window's
/// panels stop working. Calling this again does nothing. You still need to call
/// ulDestroyWindow() to release the handle.
///
ACExport void ulWindowClose(ULWindow window);

///
/// Redraw the window on an upcoming frame, even if no View has changed.
///
/// You can use this to animate content you draw from the clear and paint callbacks (see
/// ulWindowSetClearCallback()).
///
ACExport void ulWindowInvalidate(ULWindow window);

///
/// Set the window background color.
///
/// The background is drawn beneath the window's content each frame and shows wherever no
/// View covers the window. An unset color (the default) means:
///
///  - plain opaque windows: opaque white
///  - transparent windows: nothing, the window is see-through
///  - windows with a backdrop material in effect: nothing, the material shows untinted
///
/// A color you set always draws. On transparent windows and over a backdrop material its
/// alpha is used (over a material, it acts as the material's tint). On plain opaque windows
/// the alpha is ignored.
///
/// @param  window  The window.
///
/// @param  color   The new background color. Pass an unset color to go back to the default;
///                 an invalid color is ignored with a warning.
///
/// @note  On Windows and macOS, while a backdrop material is in effect, setting a background
///        color also stops the material from following later OS light/dark theme changes,
///        so the colors you picked keep matching it.
///
/// @see  ulWindowSetBackdrop()
///
ACExport void ulWindowSetBackgroundColor(ULWindow window, ULColor color);

///
/// Request a native backdrop material for this window. (Default = kULBackdrop_None)
///
/// Not every platform, OS version, or window can render a live material. When one can't,
/// the library uses the nearest look it can, down to a solid fallback fill in the theme's
/// color. Read ulWindowGetEffectiveBackdrop() and ulWindowGetEffectiveBackdropVariant() for
/// what's showing (see Window::SetBackdrop() for the full table).
///
/// On Windows and macOS, CPU-rendered and fullscreen windows can't show a material at all
/// and report kULBackdrop_None. Linux always shows at least the fallback fill.
///
/// The material shows untinted until you set a background color. To tint it, set a
/// translucent one: the lower the alpha, the more of the material shows through.
///
/// @param  window    The window.
///
/// @param  material  The material to request (kULBackdrop_None removes it).
///
/// @param  options   Optional settings (see ULBackdropOptions). Pass NULL to use the
///                   defaults.
///
/// @note  To react when what's showing changes later, set a callback with
///        ulWindowSetBackdropChangedCallback().
///
/// @see  ulWindowSetBackgroundColor()
///
ACExport void ulWindowSetBackdrop(ULWindow window, ULBackdropMaterial material,
                                  const ULBackdropOptions* options);

///
/// Get the backdrop material you requested.
///
/// @see  ulWindowSetBackdrop()
///
ACExport ULBackdropMaterial ulWindowGetBackdrop(ULWindow window);

///
/// Get the backdrop options you requested.
///
/// @param  window   The window.
///
/// @param  options  Receives the options. Set options->struct_size to
///                  sizeof(ULBackdropOptions) before the call; only the fields that size
///                  covers are filled.
///
/// @see  ulWindowSetBackdrop()
///
ACExport void ulWindowGetBackdropOptions(ULWindow window, ULBackdropOptions* options);

///
/// Get the backdrop material in effect.
///
/// @return  Returns the requested material while anything shows (a live material or the
///          fallback fill), else kULBackdrop_None.
///
/// @see  ulWindowSetBackdrop()
///
ACExport ULBackdropMaterial ulWindowGetEffectiveBackdrop(ULWindow window);

///
/// Get the look of the backdrop material in effect.
///
/// @return  Returns the look, or kULBackdropVariant_Auto when no material is in effect.
///
/// @see  ulWindowSetBackdrop()
///
ACExport ULBackdropVariant ulWindowGetEffectiveBackdropVariant(ULWindow window);

///
/// Set whether the backdrop material shows its active or inactive look.
///
/// By default the material follows the window, like native windows do. Popups always show
/// the active look by default, since a popup is never the active window.
///
/// @param  window  The window.
///
/// @param  state   The look to show.
///
/// @note  Linux ignores this. So do titled windows on newer Windows 11 builds, where the OS
///        draws the material itself and controls its inactive look.
///
ACExport void ulWindowSetBackdropState(ULWindow window, ULBackdropState state);

///
/// Get the backdrop look you requested with ulWindowSetBackdropState() (not necessarily
/// what the OS is showing).
///
ACExport ULBackdropState ulWindowGetBackdropState(ULWindow window);

///
/// Set the title bar color.
///
/// The title text color is picked automatically for contrast. This only works on Windows
/// 11; other platforms, and windows without a system title bar (eg, custom-chrome windows),
/// ignore it.
///
/// @param  window  The window.
///
/// @param  color   The new title bar color. Pass an unset color to go back to the system
///                 color; an invalid color is ignored with a warning.
///
/// @note  A set title bar color covers any backdrop material there; unset lets the material
///        show through the title bar.
///
ACExport void ulWindowSetCaptionColor(ULWindow window, ULColor color);

///
/// Mark which areas of your custom chrome drag the window or act as caption buttons.
///
/// Only custom-chrome windows use this; other windows ignore it with a warning.
///
/// Regions are in window content coordinates. They don't move or scale with the window, so
/// set them again whenever your chrome's layout changes (the resize callback is a good
/// place).
///
///  - Caption regions act like a real title bar: dragging moves the window, double-clicking
///    maximizes or restores it, and the page under them gets no mouse input. On Windows,
///    right-clicking one opens the system window menu.
///  - Button regions still send hover to the page (so your chrome can style `:hover`), but
///    a click performs the window action.
///
/// @param  window   The window.
///
/// @param  regions  The full list of regions, replacing any previous list (pass NULL to
///                  clear it). Each element's struct_size doubles as the array stride.
///
/// @param  count    The number of entries in `regions` (0 clears the list).
///
/// \parblock
/// @note  Where regions overlap, later entries win. The resize edges always win over
///        regions, and a button region for an action you turned off with
///        ulWindowSetWindowButtons() is ignored. Entries whose role isn't a ULHitRegionRole
///        value are skipped with a warning.
/// \endparblock
///
/// \parblock
/// @note  Pages can also mark drag areas with the `app-region` CSS property; regions you
///        set here win where the two overlap.
/// \endparblock
///
ACExport void ulWindowSetHitTestRegions(ULWindow window, const ULHitTestRegionDesc* regions,
                                        size_t count);

///
/// Set which window actions are available to the user.
///
/// This covers every way to trigger an action: native and custom caption buttons,
/// double-clicking the title bar, the system window menu, and OS shortcuts and snap
/// gestures. By default Close is available, and Minimize and Maximize are available when
/// the window was created with kWindowFlags_Maximizable.
///
/// @param  window        The window.
///
/// @param  buttons_mask  The actions to make available, a combination of ULWindowButtons
///                       values.
///
/// @note  Popups ignore this with a warning.
///
ACExport void ulWindowSetWindowButtons(ULWindow window, unsigned int buttons_mask);

///
/// Get the window actions available to the user.
///
/// @return  Returns the actions as a combination of ULWindowButtons values.
///
/// @see  ulWindowSetWindowButtons()
///
ACExport unsigned int ulWindowGetWindowButtons(ULWindow window);

///
/// Start moving the window with the mouse, the way dragging a title bar does.
///
/// Call this while handling a left mouse-button press (eg, from the mouse event callback).
/// The OS then moves the window until the button is released, and any press in progress
/// on the page is canceled.
///
/// @return  Returns whether or not the move started (false if the left button isn't down
///          or the window is fullscreen).
///
ACExport bool ulWindowBeginDragMove(ULWindow window);

///
/// Start resizing the window from an edge or corner with the mouse.
///
/// Call this while handling a left mouse-button press. macOS doesn't support this (the OS
/// provides its own resize edges).
///
/// @param  window  The window.
///
/// @param  edge    The edge or corner to resize from.
///
/// @return  Returns whether or not the resize started (false on macOS, if the left button
///          isn't down, or if the window is fullscreen, maximized, or not resizable).
///
ACExport bool ulWindowBeginDragResize(ULWindow window, ULResizeEdge edge);

///
/// Get the area the native window buttons cover (in window content coordinates), so you
/// can leave room for them.
///
/// Only macOS custom-chrome windows have these buttons over their content (the traffic
/// lights). Other windows, and macOS windows in fullscreen (where the buttons hide), get
/// all-zero bounds.
///
/// @param  window      The window.
///
/// @param  out_bounds  Receives the bounds.
///
/// @note  The area changes with ulWindowSetWindowControlInset(), device scale changes, and
///        fullscreen, so read it again after calling ulWindowSetWindowControlInset() and in
///        the resize callback.
///
ACExport void ulWindowGetWindowControlBounds(ULWindow window, ULLayoutRect* out_bounds);

///
/// Move the native window buttons on a macOS custom-chrome window (other windows ignore
/// this).
///
/// @param  window  The window.
///
/// @param  x       The offset (in logical pixels) from the leading edge of the content area
///                 (mirrored automatically in right-to-left layouts).
///
/// @param  y       The offset (in logical pixels) from the top of the content area.
///
/// @note  The offset positions the buttons' frames, which extend a little past the visible
///        circles.
///
/// @see  ulWindowGetWindowControlBounds()
///
ACExport void ulWindowSetWindowControlInset(ULWindow window, double x, double y);

#if UL_HAS(TESTING)
/// \cond ignore
ACExport int ulWindowHitTestChromeRegionForTesting(ULWindow window, double x, double y);
/// \endcond
#endif

///
/// Set the window's corner style (see ULCornerStyle). (Default = kULCornerStyle_Default)
///
/// On transparent windows the library rounds the window's content to match. A value that
/// isn't a ULCornerStyle is ignored with a warning.
///
/// @param  window  The window.
///
/// @param  style   The corner style to use.
///
/// @note  On macOS this only affects transparent windows; other windows keep the OS's own
///        corners and shadow.
///
ACExport void ulWindowSetCornerStyle(ULWindow window, ULCornerStyle style);

///
/// Get the window's corner style.
///
/// @see  ulWindowSetCornerStyle()
///
ACExport ULCornerStyle ulWindowGetCornerStyle(ULWindow window);

///
/// Set whether a popup can take keyboard focus. (Default = false)
///
/// Popups normally never take focus: clicking one leaves focus with the owner, and key
/// presses keep going to the owner, the way native menus work. Turn this on for popups with
/// text fields.
///
/// @param  window   The window.
///
/// @param  accepts  Whether or not the popup can take keyboard focus.
///
/// @note  Only popups use this; other windows ignore it.
///
ACExport void ulWindowSetAcceptsKeyInput(ULWindow window, bool accepts);

///
/// Whether or not a popup can take keyboard focus.
///
/// @see  ulWindowSetAcceptsKeyInput()
///
ACExport bool ulWindowAcceptsKeyInput(ULWindow window);

///
/// Set whether the library dismisses this popup automatically. (Default = kULDismiss_Auto
/// for popups)
///
/// With kULDismiss_Auto, the library hides the popup when the OS would dismiss a native
/// menu:
///
///  - a click in the owner, outside the popup
///  - a click on the owner's title bar or border (where the platform reports these)
///  - the owner starting a move or resize, scrolling, or becoming inactive
///  - Esc while the owner has focus (a popup that takes key input also closes on Esc while
///    it has focus; see ulWindowSetAcceptsKeyInput())
///
/// Dismissing hides the popup but doesn't destroy it, so you can show it again. The dismiss
/// callback fires each time.
///
/// @param  window   The window.
///
/// @param  dismiss  Whether or not to dismiss the popup automatically.
///
/// \parblock
/// @note  Only popups use this; other windows ignore it.
/// \endparblock
///
/// \parblock
/// @note  The library checks for dismissal before the window event callbacks see the event,
///        so consuming the event doesn't stop the popup from being dismissed.
/// \endparblock
///
ACExport void ulWindowSetDismiss(ULWindow window, ULDismiss dismiss);

///
/// Get the auto-dismiss setting.
///
/// @see  ulWindowSetDismiss()
///
ACExport ULDismiss ulWindowGetDismiss(ULWindow window);

///
/// Convert logical pixels to device pixels using the window's device scale (rounded to the
/// nearest pixel).
///
ACExport int ulWindowLogicalToDevice(ULWindow window, double val);

///
/// Convert device pixels to logical pixels using the window's device scale.
///
ACExport double ulWindowDeviceToLogical(ULWindow window, int val);

///
/// Get the native window handle: an `HWND` on Windows, an `NSWindow*` on macOS, and a
/// `GLFWwindow*` on Linux.
///
ACExport void* ulWindowGetNativeHandle(ULWindow window);

/******************************************************************************
 * Monitor
 *****************************************************************************/

///
/// Get the unique display ID of the monitor.
///
/// The renderer uses this ID to identify which monitor a View is on (see
/// ulViewConfigSetDisplayId()).
///
ACExport unsigned int ulMonitorGetDisplayId(ULMonitor monitor);

///
/// Get the refresh rate of the monitor (in Hz).
///
ACExport unsigned int ulMonitorGetRefreshRate(ULMonitor monitor);

/******************************************************************************
 * Window
 *****************************************************************************/

///
/// Whether or not the window renders on the GPU.
///
ACExport bool ulWindowIsAccelerated(ULWindow window);

///
/// Get the ID of the render buffer the window draws into.
///
/// You can use this with the GPUDriver interface to draw into the window (see
/// ulWindowSetClearCallback()).
///
/// @return  Returns the render buffer ID, or 0 if the window doesn't render on the GPU.
///
ACExport unsigned int ulWindowGetRenderBufferId(ULWindow window);

///
/// Capture a screenshot of the window.
///
/// @return  Returns a ULBitmap with the window's pixels in BGRA8_UNORM_SRGB format, or NULL if
///          the capture fails. You must call ulDestroyBitmap() on the returned bitmap when
///          finished.
///
/// @note  This captures what was last shown on screen, so anything drawn since the last
///        frame isn't included.
///
ACExport ULBitmap ulWindowTakeScreenshot(ULWindow window);

/******************************************************************************
 * Settings
 *****************************************************************************/

///
/// Override the device scale factor for all windows (Default = 0, auto-detected from the
/// monitor DPI).
///
/// Set this to a positive value (eg, 1.0, 1.5, 2.0) to override the detected scale. Any value
/// <= 0 restores auto-detection.
///
ACExport void ulSettingsSetDeviceScaleOverride(ULSettings settings, double scale);

///
/// Enable the built-in trace file profiler (Default = False).
///
/// When enabled, the library writes a Perfetto trace file that can be opened in
/// https://ui.perfetto.dev to visualize Ultralight's internal performance.
///
/// Traces are written to a `profiler_traces` folder inside the app's own data directory,
/// alongside the log file (see Settings::developer_name in <AppCore/App.h> for where that is).
/// This is not the cache path you may have set on the Config.
///
/// @pre  Requires the Pro edition or higher (the Profiler platform interface is not
///       available in the Free or Plus editions).
///
ACExport void ulSettingsSetEnableProfiler(ULSettings settings, bool enabled);

/******************************************************************************
 * Window Input Callbacks
 *****************************************************************************/

///
/// Callback for when the window receives a key event, before any page does.
///
/// @return  Return false to consume the event and prevent it from propagating further.
///
typedef bool (*ULWindowKeyEventCallback)(void* user_data, ULWindow window, ULKeyEvent evt);

///
/// Callback for when the window receives a mouse event, before any page does.
///
/// @return  Return false to consume the event and prevent it from propagating further.
///
typedef bool (*ULWindowMouseEventCallback)(void* user_data, ULWindow window, ULMouseEvent evt);

///
/// Callback for when the window receives a scroll event, before any page does.
///
/// @return  Return false to consume the event and prevent it from propagating further.
///
typedef bool (*ULWindowScrollEventCallback)(void* user_data, ULWindow window, ULScrollEvent evt);

///
/// Set a callback for window keyboard events.
///
/// @param  window             The window to listen to.
///
/// @param  callback           Invoked with each key event, before any page receives it. The
///                            event is valid only for the duration of the call (don't destroy
///                            it). Pass NULL to clear a previously set callback.
///
/// @param  user_data          Passed to `callback` (can be NULL).
///
/// @param  destroy_user_data  Invoked when the callback is replaced or the window is destroyed
///                            with ulDestroyWindow() (can be NULL).
///
ACExport void ulWindowSetKeyEventCallback(ULWindow window, ULWindowKeyEventCallback callback,
                                          void* user_data,
                                          ULUserDataDestroyCallback destroy_user_data);

///
/// Set a callback for window mouse events.
///
/// @param  window             The window to listen to.
///
/// @param  callback           Invoked with each mouse event, before any page receives it. The
///                            event is valid only for the duration of the call (don't destroy
///                            it). Pass NULL to clear a previously set callback.
///
/// @param  user_data          Passed to `callback` (can be NULL).
///
/// @param  destroy_user_data  Invoked when the callback is replaced or the window is destroyed
///                            with ulDestroyWindow() (can be NULL).
///
ACExport void ulWindowSetMouseEventCallback(ULWindow window, ULWindowMouseEventCallback callback,
                                            void* user_data,
                                            ULUserDataDestroyCallback destroy_user_data);

///
/// Set a callback for window scroll events.
///
/// @param  window             The window to listen to.
///
/// @param  callback           Invoked with each scroll event, before any page receives it. The
///                            event is valid only for the duration of the call (don't destroy
///                            it). Pass NULL to clear a previously set callback.
///
/// @param  user_data          Passed to `callback` (can be NULL).
///
/// @param  destroy_user_data  Invoked when the callback is replaced or the window is destroyed
///                            with ulDestroyWindow() (can be NULL).
///
ACExport void ulWindowSetScrollEventCallback(ULWindow window, ULWindowScrollEventCallback callback,
                                             void* user_data,
                                             ULUserDataDestroyCallback destroy_user_data);

///
/// Generic window callback.
///
typedef void (*ULWindowCallback)(void* user_data, ULWindow window);

///
/// Set a callback fired after the window background is drawn, before any View is drawn.
///
/// You can use this to draw custom content beneath every View. When you do:
///
///  - draw through the GPUDriver interface, into the window's render buffer (on CPU-rendered
///    windows, draw on the native window surface instead)
///  - don't count on any GPU state you set lasting past the callback
///  - don't present the frame
///
/// To animate your content, call ulWindowInvalidate() to schedule the next frame.
///
/// @note  On Windows, drawing from this callback has no effect on transparent CPU-rendered
///        windows (their pixels come from an internal surface you can't draw on).
///
ACExport void ulWindowSetClearCallback(ULWindow window, ULWindowCallback callback,
                                       void* user_data,
                                       ULUserDataDestroyCallback destroy_user_data);

///
/// Set a callback fired after every View is drawn, before the frame is presented.
///
/// You can use this to draw custom content above every View. The rules and the note on
/// ulWindowSetClearCallback() apply here too.
///
ACExport void ulWindowSetPaintCallback(ULWindow window, ULWindowCallback callback,
                                       void* user_data,
                                       ULUserDataDestroyCallback destroy_user_data);

///
/// Callback for window-state changes. `state` is a ULWindowState value.
///
typedef void (*ULWindowStateCallback)(void* user_data, ULWindow window, int state);

///
/// Set a callback fired when the window becomes normal, minimized, or maximized, however
/// the change was made (eg, ulWindowMaximize(), the caption buttons, or a snap gesture).
///
/// If you draw your own window chrome, you can swap the maximize/restore icon here.
///
/// @note  This fires after the resize callback, so the window's new size is already set.
///
ACExport void ulWindowSetWindowStateCallback(ULWindow window,
                                             ULWindowStateCallback callback,
                                             void* user_data,
                                             ULUserDataDestroyCallback destroy_user_data);

///
/// Callback for window activation changes.
///
typedef void (*ULActivationChangedCallback)(void* user_data, ULWindow window, bool active);

///
/// Set a callback fired when the window becomes, or stops being, the active window (the one
/// the user is working in).
///
/// If your window chrome dims while the window is inactive, you can restyle it here.
///
/// \parblock
/// @note  Opening one of this window's own popups doesn't make it inactive, so the window
///        keeps its active look while its menus are open.
/// \endparblock
///
/// \parblock
/// @note  On macOS, popup windows never fire this.
/// \endparblock
///
ACExport void ulWindowSetActivationChangedCallback(ULWindow window,
                                                   ULActivationChangedCallback callback,
                                                   void* user_data,
                                                   ULUserDataDestroyCallback destroy_user_data);

///
/// Set a callback fired when a hidden window is shown, by ulWindowShow() or when a
/// ulWindowShowWhenReady() wait ends.
///
/// You can start work here that only needs to run while the window is on screen (eg,
/// animations or a live data feed).
///
/// @note  This doesn't fire for a window created visible, or for a ulWindowShow() call on a
///        window that is already visible.
///
ACExport void ulWindowSetShowCallback(ULWindow window, ULWindowCallback callback,
                                      void* user_data,
                                      ULUserDataDestroyCallback destroy_user_data);

///
/// Set a callback fired when a visible window is hidden, by ulWindowHide() or by the library
/// (see ulWindowSetDismissCallback()).
///
/// \parblock
/// @note  Minimizing or closing the window doesn't fire this (see
///        ulWindowSetWindowStateCallback() and ulWindowSetCloseCallback()).
/// \endparblock
///
/// \parblock
/// @note  The popups this window owns are hidden first, so their hide callbacks fire before
///        this window's.
/// \endparblock
///
ACExport void ulWindowSetHideCallback(ULWindow window, ULWindowCallback callback,
                                      void* user_data,
                                      ULUserDataDestroyCallback destroy_user_data);

///
/// Callback for backdrop material changes. `effective` is a ULBackdropMaterial value.
///
typedef void (*ULWindowBackdropChangedCallback)(void* user_data, ULWindow window,
                                                int effective);

///
/// Set a callback fired when the backdrop material in effect changes after
/// ulWindowSetBackdrop() (eg, the user turns off transparency effects, or the material's
/// look changes).
///
/// `effective` is the material now in effect (kULBackdrop_None when none renders). If it's
/// the same as before, the look changed instead: read ulWindowGetEffectiveBackdropVariant().
///
/// @note  This doesn't fire for the initial result: read ulWindowGetEffectiveBackdrop()
///        right after ulWindowSetBackdrop() for that.
///
ACExport void ulWindowSetBackdropChangedCallback(ULWindow window,
                                                 ULWindowBackdropChangedCallback callback,
                                                 void* user_data,
                                                 ULUserDataDestroyCallback destroy_user_data);

///
/// Set a callback fired when the library hides this window for you: through its
/// auto-dismiss policy (see ulWindowSetDismiss()), or because the window that owns it was
/// hidden.
///
/// This never fires for the window you hide yourself. It fires after the hide callback.
///
/// @note  Hiding a popup yourself still fires this for the popups it owns, since the
///        library hides those for you.
///
ACExport void ulWindowSetDismissCallback(ULWindow window, ULWindowCallback callback,
                                         void* user_data,
                                         ULUserDataDestroyCallback destroy_user_data);

/******************************************************************************
 * Dialogs
 *****************************************************************************/

///
/// The icon to display in a message box.
///
typedef enum {
  kDialogIcon_Info,
  kDialogIcon_Warning,
  kDialogIcon_Error,
  kDialogIcon_Question,
} ULDialogIcon;

///
/// The button configuration for a message box.
///
typedef enum {
  kButtonType_OK,
  kButtonType_OKCancel,
  kButtonType_YesNo,
} ULButtonType;

///
/// The result of a message box (which button was pressed).
///
typedef enum {
  kButtonResult_OK,
  kButtonResult_Cancel,
  kButtonResult_Yes,
  kButtonResult_No,
} ULButtonResult;

///
/// Show a modal message box.
///
/// @param  title    The title of the message box.
///
/// @param  message  The message to display.
///
/// @param  icon     The icon to display.
///
/// @param  buttons  The button configuration.
///
/// @return  Returns which button was pressed.
///
ACExport ULButtonResult ulShowMessageBox(ULString title, ULString message,
                                         ULDialogIcon icon, ULButtonType buttons);

/******************************************************************************
 * Platform
 *****************************************************************************/

///
/// Initialize the platform font loader and set it as the current FontLoader.
///
/// This is only needed if you are not calling ulCreateApp().
///
ACExport void ulEnablePlatformFontLoader(void);

///
/// Initialize the platform file system (needed for loading file:/// URLs) and set it as the
/// current FileSystem.
///
/// This is only needed if you are not calling ulCreateApp().
///
/// @param  base_dir  A base file path that relative file paths are resolved against. You can
///                   optionally specify "@resource_path" on macOS to use the app bundle's
///                   resource path.
///
ACExport void ulEnablePlatformFileSystem(ULString base_dir);

///
/// Initialize the default logger (writes the log to a file).
///
/// This is only needed if you are not calling ulCreateApp().
///
/// @param  log_path  A writable file path to write the log to (eg, `./ultralight.log`).
///
/// @note  This logger also shows a native message box for every kLogLevel_Fatal message, so
///        log anything your app can recover from at a lower level.
///
ACExport void ulEnableDefaultLogger(ULString log_path);

///
/// Initialize the platform clipboard (needed for cut, copy, and paste) and set it as the
/// current Clipboard.
///
/// This is only needed if you are not calling ulCreateApp(). You should call this before
/// ulCreateRenderer().
///
ACExport void ulEnablePlatformClipboard(void);

#if UL_HAS(MEDIA)
///
/// Initialize the platform audio output (needed to hear `<video>` and `<audio>` elements) and
/// set it as the current AudioOutput.
///
/// This is only needed if you are not calling ulCreateApp(). You should call this before
/// ulCreateRenderer(), since media that starts playing without an audio output plays without
/// sound.
///
/// @pre  Requires the Pro edition or higher.
///
ACExport void ulEnablePlatformAudioOutput(void);
#endif

#ifdef __cplusplus
}
#endif

#include <AppCore/CAPI/CAPI_Layout.h>

#endif // APPCORE_CAPI_H
