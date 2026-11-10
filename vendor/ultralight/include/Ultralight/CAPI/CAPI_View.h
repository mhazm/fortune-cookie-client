///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///

///
/// @file CAPI_View.h
///
/// Web-page container rendered to an offscreen surface.
///
/// `#include <Ultralight/CAPI/CAPI_View.h>`
///
/// The View class is responsible for loading and rendering web-pages to an offscreen surface. It
/// is completely isolated from the OS windowing system, you must forward all input events to it
/// from your application.
///
/// ## Creating a View
///
/// You can create a View by calling ulCreateView():
///
/// ```
/// // Create a ULViewConfig with default values
/// ULViewConfig view_config = ulCreateViewConfig();
///
/// // Create a View, 500 by 500 pixels in size, using the default Session
/// ULView view = ulCreateView(renderer, 500, 500, view_config, NULL);
///
/// // Clean up the ULViewConfig
/// ulDestroyViewConfig(view_config);
/// ```
///
/// @note  When using ulCreateApp(), the library will automatically create a View for you when
///        you call ulWindowAddPanel() (see `<AppCore/CAPI/CAPI_Layout.h>`).
///
/// ## Callbacks
///
/// Each ulViewSet*Callback() function holds one callback. Setting a callback replaces the previous
/// one, and a NULL callback removes it.
///
/// Ownership of `user_data` transfers to the View. `destroy_user_data` (may be NULL) is invoked
/// exactly once when the callback is replaced or the ULView handle is destroyed (never while the
/// callback itself is running). If `view` is NULL, nothing is set and `destroy_user_data` runs
/// right away.
///
/// Each setter owns its `user_data` separately, so a pointer registered with a destroy hook on two
/// setters is released twice. Share one context by passing it with a NULL `destroy_user_data` to
/// all but one setter, or give each setter its own allocation.
///
/// Callbacks belong to the ULView handle you set them on, and each callback receives that handle.
/// A View delivers each group of callbacks to one handle: the load events (below), the download
/// callbacks, the network-request callback, the editing callbacks (editable state and
/// composition), and all the others. Setting a callback on a second handle for the same View (eg,
/// another ulPanelGetView() result) moves its group to that handle, which turns off the first
/// handle's callbacks in the group. Set all of a View's callbacks through one handle. Destroying a
/// handle turns off only its own callbacks.
///
/// Callbacks run on the Renderer's thread.
///
/// ## Load Events
///
/// A page load calls these callbacks in order:
///
/// | Callback setter                      | When                               |
/// |--------------------------------------|------------------------------------|
/// | ulViewSetBeginLoadingCallback()      | The load starts                    |
/// | ulViewSetWindowObjectReadyCallback() | Before the page's scripts run      |
/// | ulViewSetDOMReadyCallback()          | The document is parsed             |
/// | ulViewSetFinishLoadingCallback()     | The load ends                      |
/// | ulViewSetPageSettledCallback()       | Loading and layout have gone quiet |
///
/// If the load fails, the fail-loading callback comes just before the finish-loading callback.
///
/// The window-object-ready and DOM-ready callbacks are called for every frame on the page (check
/// `is_main_frame`). The begin, finish, and fail loading callbacks are called once per load for the
/// frame that started it (usually the main frame).
///
/// @note  The window-object-ready and DOM-ready callbacks aren't called when a page is restored
///        from the back-forward cache.
///
#ifndef ULTRALIGHT_CAPI_VIEW_H
#define ULTRALIGHT_CAPI_VIEW_H

#include <Ultralight/CAPI/CAPI_Color.h>
#include <Ultralight/CAPI/CAPI_Defines.h>
#include <Ultralight/CAPI/CAPI_Editor.h>
#include <Ultralight/CAPI/CAPI_JSValue.h>

#ifdef __cplusplus
extern "C" {
#endif

///
/// The source of a console message.
///
/// @see  ULAddConsoleMessageCallback
///
typedef enum {
  kMessageSource_XML = 0,
  kMessageSource_JS,
  kMessageSource_Network,
  kMessageSource_ConsoleAPI,
  kMessageSource_Storage,
  kMessageSource_AppCache,
  kMessageSource_Rendering,
  kMessageSource_CSS,
  kMessageSource_Security,
  kMessageSource_ContentBlocker,
  kMessageSource_Media,
  kMessageSource_MediaSource,
  kMessageSource_WebRTC,
  kMessageSource_ITPDebug,
  kMessageSource_PrivateClickMeasurement,
  kMessageSource_PaymentRequest,
  kMessageSource_Other,
  kMessageSource_NativeAPI,  ///< Diagnostics from application-bound JavaScript APIs.
} ULMessageSource;

///
/// The severity level of a console message.
///
/// @see  ULAddConsoleMessageCallback
///
typedef enum {
  kMessageLevel_Log = 0,
  kMessageLevel_Warning,
  kMessageLevel_Error,
  kMessageLevel_Debug,
  kMessageLevel_Info,
} ULMessageLevel;

///
/// Cursor types.
///
/// @see  ULChangeCursorCallback
///
typedef enum {
  kCursor_Pointer = 0,
  kCursor_Cross,
  kCursor_Hand,
  kCursor_IBeam,
  kCursor_Wait,
  kCursor_Help,
  kCursor_EastResize,
  kCursor_NorthResize,
  kCursor_NorthEastResize,
  kCursor_NorthWestResize,
  kCursor_SouthResize,
  kCursor_SouthEastResize,
  kCursor_SouthWestResize,
  kCursor_WestResize,
  kCursor_NorthSouthResize,
  kCursor_EastWestResize,
  kCursor_NorthEastSouthWestResize,
  kCursor_NorthWestSouthEastResize,
  kCursor_ColumnResize,
  kCursor_RowResize,
  kCursor_MiddlePanning,
  kCursor_EastPanning,
  kCursor_NorthPanning,
  kCursor_NorthEastPanning,
  kCursor_NorthWestPanning,
  kCursor_SouthPanning,
  kCursor_SouthEastPanning,
  kCursor_SouthWestPanning,
  kCursor_WestPanning,
  kCursor_Move,
  kCursor_VerticalText,
  kCursor_Cell,
  kCursor_ContextMenu,
  kCursor_Alias,
  kCursor_Progress,
  kCursor_NoDrop,
  kCursor_Copy,
  kCursor_None,
  kCursor_NotAllowed,
  kCursor_ZoomIn,
  kCursor_ZoomOut,
  kCursor_Grab,
  kCursor_Grabbing,
  kCursor_Custom
} ULCursor;

/******************************************************************************
 * ViewConfig
 *****************************************************************************/

///
/// Create view configuration with default values (see <Ultralight/View.h>).
///
/// @return  Returns a new ULViewConfig instance. You must call ulDestroyViewConfig() when
///          finished.
///
ULExport ULViewConfig ulCreateViewConfig(void);

///
/// Destroy a view configuration created by ulCreateViewConfig().
///
/// @param  config  The view configuration to destroy (can be NULL).
///
ULExport void ulDestroyViewConfig(ULViewConfig config);

///
/// Set a user-generated id of the display (monitor, TV, or screen) that the View will be shown on.
///
/// Animations are driven based on the physical refresh rate of the display. Multiple Views can
/// share the same display.
///
/// @note  This is automatically managed for you when ulCreateApp() is used.
///
/// @see  ulRefreshDisplay()
///
ULExport void ulViewConfigSetDisplayId(ULViewConfig config, unsigned int display_id);

///
/// Set whether to render using the GPU renderer (accelerated) or the CPU renderer (unaccelerated).
///
/// This option is only valid if you're managing the Renderer yourself (eg, you've previously
/// called ulCreateRenderer() instead of ulCreateApp()).
///
/// When true, the View will be rendered to an offscreen GPU texture using the GPU driver set in
/// ulPlatformSetGPUDriver(). You can fetch details for the texture via ulViewGetRenderTarget().
///
/// When false (the default), the View will be rendered to an offscreen pixel buffer using the
/// multithreaded CPU renderer. This pixel buffer can optionally be provided by the user--
/// for more info see ulViewGetSurface().
///
/// @note  You must set a GPU driver before creating an accelerated View (the process exits
///        with an error otherwise).
///
ULExport void ulViewConfigSetIsAccelerated(ULViewConfig config, bool is_accelerated);

///
/// Set whether or not the View should support transparent backgrounds. (Default = False)
///
/// The page needs a transparent background too:
///
/// ```
/// html, body { background: transparent; }
/// ```
///
ULExport void ulViewConfigSetIsTransparent(ULViewConfig config, bool is_transparent);

///
/// Set the base background color the page renders on before any page styling applies, the color
/// visible while a page loads and wherever the page itself paints nothing.
/// (Default = unset, meaning opaque white)
///
/// Dark applications set a dark base so pages never flash white during load.
///
/// Transparent Views keep a fully transparent base regardless of this value. On opaque Views a
/// translucent color is composited over white, so the View itself stays opaque.
///
ULExport void ulViewConfigSetBackgroundColor(ULViewConfig config, ULColor color);

///
/// Set the color scheme the View reports to pages via the `prefers-color-scheme` CSS media
/// feature. (Default = kColorScheme_Auto)
///
/// kColorScheme_Auto follows the system scheme set via ulRendererSetSystemColorScheme() (Light
/// until it is fed a different value). Light or Dark pins the scheme for this View.
///
/// An out-of-range value logs a warning and leaves the config unchanged.
///
/// @note  The scheme is only a signal to the page. Pages style themselves via media queries, and
///        built-in UI (form controls, scrollbars, the default white canvas) keeps its light
///        appearance under a dark scheme.
///
/// @see  ulViewSetPreferredColorScheme()
///
ULExport void ulViewConfigSetPreferredColorScheme(ULViewConfig config, ULColorScheme scheme);

///
/// Set which pages may read the clipboard from script, eg, a page's own Paste button calling
/// `document.execCommand('paste')`.
/// (Default = kClipboardReadPolicy_AllowForAppContent)
///
/// The library only considers requests made during a user gesture (a click or key press);
/// script running outside a gesture is always refused. Pastes the user performs directly
/// (Ctrl+V, or ulEditorExecute with kEditorCommand_Paste) are never affected by this policy.
///
/// kClipboardReadPolicy_AllowForAppContent (the default) grants requests from your
/// application's own content (local `file:///` pages and pages loaded from
/// application-supplied data, eg, ulViewLoadHTML()) and refuses them from other pages.
/// kClipboardReadPolicy_Allow grants requests from any page; kClipboardReadPolicy_Deny
/// refuses them all.
///
/// An out-of-range value logs a warning and leaves the config unchanged.
///
ULExport void ulViewConfigSetClipboardReadPolicy(ULViewConfig config, ULClipboardReadPolicy policy);

///
/// Set the initial device scale, ie. the amount to scale page units to screen pixels. This
/// should be set to the scaling factor of the device that the View is displayed on.
/// (Default = 1.0)
///
/// @note  1.0 is equal to 100% zoom (no scaling), 2.0 is equal to 200% zoom (2x scaling)
///
ULExport void ulViewConfigSetInitialDeviceScale(ULViewConfig config, double initial_device_scale);

///
/// Set whether or not the View should initially have input focus. (Default = True)
///
ULExport void ulViewConfigSetInitialFocus(ULViewConfig config, bool is_focused);

///
/// Set whether images should be enabled (Default = True).
///
ULExport void ulViewConfigSetEnableImages(ULViewConfig config, bool enabled);

///
/// Set whether JavaScript should be enabled (Default = True).
///
ULExport void ulViewConfigSetEnableJavaScript(ULViewConfig config, bool enabled);

///
/// Set whether text editing should follow the host OS's native conventions instead of the
/// library's cross-platform behavior (Default = False).
///
/// When false, text selection and editing behave identically on every platform (a
/// directionless selection is promoted to forward, the convention Windows browsers use).
/// Set this to true for desktop applications that should feel native to each platform.
///
/// @note  ulCreateApp() turns this on for the Views it creates for a panel added without a
///        ViewConfig (see ulSettingsSetMatchNativeEditingBehavior() in <AppCore/CAPI.h>). A
///        config you pass yourself keeps this value.
///
ULExport void ulViewConfigSetMatchNativeEditingBehavior(ULViewConfig config, bool enabled);

///
/// Set the default font family, used for text that doesn't set one (Default = Times New Roman).
///
ULExport void ulViewConfigSetFontFamilyStandard(ULViewConfig config, ULString font_name);

///
/// Set the default monospace font family, eg, for `pre` and `code` (Default = Courier New).
///
ULExport void ulViewConfigSetFontFamilyFixed(ULViewConfig config, ULString font_name);

///
/// Set the default font family for the CSS `serif` generic (Default = Times New Roman).
///
ULExport void ulViewConfigSetFontFamilySerif(ULViewConfig config, ULString font_name);

///
/// Set the default font family for the CSS `sans-serif` generic (Default = Arial).
///
ULExport void ulViewConfigSetFontFamilySansSerif(ULViewConfig config, ULString font_name);

///
/// Set the default font family for the CSS `cursive` generic (Default = Comic Sans MS).
///
ULExport void ulViewConfigSetFontFamilyCursive(ULViewConfig config, ULString font_name);

///
/// Set the default font family for the CSS `fantasy` generic (Default = Impact).
///
ULExport void ulViewConfigSetFontFamilyFantasy(ULViewConfig config, ULString font_name);

///
/// Set the default font family for the CSS `-webkit-pictograph` generic. Pass an empty string
/// to use the standard font family (Default = empty).
///
ULExport void ulViewConfigSetFontFamilyPictograph(ULViewConfig config, ULString font_name);

///
/// Set the default font size, in pixels (Default = 16).
///
ULExport void ulViewConfigSetFontSizeDefault(ULViewConfig config, unsigned int size);

///
/// Set the default font size for monospace text, eg, `pre` and `code`, in pixels
/// (Default = 13).
///
ULExport void ulViewConfigSetFontSizeFixed(ULViewConfig config, unsigned int size);

///
/// Set user agent string (See <Ultralight/platform/Config.h> for the default).
///
ULExport void ulViewConfigSetUserAgent(ULViewConfig config, ULString agent_string);

///
/// Set whether or not compositing should be enabled. (Default = True)
///
/// When enabled, certain content (eg, 3D transforms, `will-change`, animated transforms or
/// opacity, and video) paints into separate composited layers, making transform and opacity
/// changes cheap to animate.
///
ULExport void ulViewConfigSetEnableCompositor(ULViewConfig config, bool enabled);

///
/// Set whether or not to display compositor debug information. (Default = False)
///
/// @note  Only valid when the compositor is enabled.
///
ULExport void ulViewConfigSetEnableCompositorDebugInfo(ULViewConfig config, bool enabled);

///
/// Set whether or not the HTML5 Canvas Filters API (`CanvasRenderingContext2D.filter`) is
/// enabled. (Default = True)
///
ULExport void ulViewConfigSetEnableCanvasFilters(ULViewConfig config, bool enabled);

///
/// Set whether to throttle JavaScript timers (setTimeout / setInterval) while the View is hidden
/// (Default = False).
///
/// When enabled, repeating timers in a hidden View are aligned to roughly one-second boundaries
/// (about 1 Hz) to reduce CPU usage for off-screen Views. They return to their normal rate once
/// the View is shown again.
///
/// @note  This does not affect requestAnimationFrame, which is always suspended while the View
///        is hidden.
///
/// @see  ulViewSetVisible()
///
ULExport void ulViewConfigSetEnableHiddenTimerThrottling(ULViewConfig config, bool enabled);

///
/// Set the maximum rate, in frames per second, at which the View advances its animations and
/// repaints. (Default = 0, unthrottled)
///
/// This caps the View's whole frame loop (requestAnimationFrame, CSS and Web animations, smooth
/// scrolling, and painting). A value of 0 leaves the View unthrottled, advancing at the display's
/// refresh rate.
///
/// @note  The View is always capped at your edition's maximum frame rate.
///
ULExport void ulViewConfigSetMaxRenderFps(ULViewConfig config, unsigned int fps);

///
/// Set whether or not a script can open a window with `window.open()` without a user gesture
/// (Default = False).
///
/// When false, Ultralight behaves like a typical browser: a `window.open()` call succeeds only
/// while the page is handling a user gesture, such as from a click handler. A call made on its
/// own (on page load, or from a timer) is blocked as a popup. Set this to true to let scripts
/// call `window.open()` at any time. The same gesture rule applies to a form submission that
/// targets a new window.
///
/// @note  Even when this is allowed, a window is only created if your create-child-view callback
///        returns one; return NULL there to block it.
///
/// @see  ulViewSetCreateChildViewCallback()
///
ULExport void ulViewConfigSetJavaScriptCanOpenWindowsAutomatically(ULViewConfig config,
                                                                   bool enabled);

///
/// Set whether or not pages loaded from `file:///` URLs can access content from any origin
/// (Default = True).
///
/// When true, a `file:///` page can read other frames' documents and make requests to any origin
/// without cross-origin checks. Set this to false to give `file:///` pages the same cross-origin
/// rules as web pages (they stay same-origin with other `file:///` pages).
///
ULExport void ulViewConfigSetAllowUniversalAccessFromFileURLs(ULViewConfig config, bool enabled);

/******************************************************************************
 * View
 *****************************************************************************/

///
/// Create a View with certain size (in pixels).
///
/// @param  renderer     The active renderer instance.
///
/// @param  width        The initial width, in pixels.
///
/// @param  height       The initial height, in pixels.
///
/// @param  view_config  Configuration details for the View. Pass NULL to use the default
///                      configuration.
///
/// @param  session      The session to store local data in. Pass NULL to use the default session.
///
/// @return  Returns a new ULView instance. You must call ulDestroyView() when finished.
///
ULExport ULView ulCreateView(ULRenderer renderer, unsigned int width, unsigned int height,
                             ULViewConfig view_config, ULSession session);

///
/// Destroy a View previously created with ulCreateView().
///
/// @param  view  The View to destroy (can be NULL).
///
ULExport void ulDestroyView(ULView view);

///
/// Get current URL.
///
/// @note  Don't destroy the returned string, it is owned by the View. This value is reset with
///        every call-- if you want to retain it you should copy it via ulCreateStringFromCopy().
///
ULExport ULString ulViewGetURL(ULView view);

///
/// Get current title.
///
/// @note  Don't destroy the returned string, it is owned by the View. This value is reset with
///        every call-- if you want to retain it you should copy it via ulCreateStringFromCopy().
///
ULExport ULString ulViewGetTitle(ULView view);

///
/// Get the width, in pixels.
///
ULExport unsigned int ulViewGetWidth(ULView view);

///
/// Get the height, in pixels.
///
ULExport unsigned int ulViewGetHeight(ULView view);

///
/// Get the display id of the View.
///
ULExport unsigned int ulViewGetDisplayId(ULView view);

///
/// Set the display id of the View.
///
/// You should call this when the View moves to another display.
///
/// @param  view        The View.
///
/// @param  display_id  The id of the new display (see ulViewConfigSetDisplayId()).
///
/// @note  This is automatically managed for you for Views hosted in an AppCore window.
///
ULExport void ulViewSetDisplayId(ULView view, unsigned int display_id);

///
/// Get the device scale, ie. the amount to scale page units to screen pixels.
///
/// For example, a value of 1.0 is equivalent to 100% zoom. A value of 2.0 is 200% zoom.
///
ULExport double ulViewGetDeviceScale(ULView view);

///
/// Set the device scale.
///
/// @param  view   The View.
///
/// @param  scale  The new device scale (see ulViewGetDeviceScale()).
///
/// @note  To change the View's size at the same time, use ulViewResizeWithScale() instead,
///        which applies both in one pass.
///
ULExport void ulViewSetDeviceScale(ULView view, double scale);

///
/// Whether or not the View is GPU-accelerated. If this is false, the page will be rendered
/// via the CPU renderer.
///
ULExport bool ulViewIsAccelerated(ULView view);

///
/// Whether or not the View supports transparent backgrounds.
///
ULExport bool ulViewIsTransparent(ULView view);

///
/// Check if the main frame of the page is currently loading.
///
ULExport bool ulViewIsLoading(ULView view);

///
/// Check whether the page has settled after loading.
///
/// This is the queryable form of the page-settled callback. It flips true when that callback
/// fires and resets to false on each new navigation.
///
/// @return  Returns true once the page's network and layout activity have gone idle following
///          the main frame's onload event, and false otherwise.
///
/// @see  ulViewSetPageSettledCallback()
///
ULExport bool ulViewIsSettled(ULView view);

///
/// Get the RenderTarget for the View.
///
/// @note  Only valid if this View is GPU accelerated.
/// 
///        You can use this with your GPUDriver implementation to bind and display the
///        corresponding texture in your application.
///
ULExport ULRenderTarget ulViewGetRenderTarget(ULView view);

///
/// Get the Surface for the View (native pixel buffer that the CPU renderer draws into).
///
/// @note  This operation is only valid if you're managing the Renderer yourself (eg, you've
///        previously called ulCreateRenderer() instead of ulCreateApp()).
///
///        This returns NULL if the View uses the GPU renderer.
///
///        The default Surface is BitmapSurface, but you can provide your own Surface
///        implementation with ulPlatformSetSurfaceDefinition().
///
///        When using the default Surface, you can retrieve the underlying bitmap by casting
///        ULSurface to ULBitmapSurface and calling ulBitmapSurfaceGetBitmap().
///
ULExport ULSurface ulViewGetSurface(ULView view);

///
/// Load a raw string of HTML, the View will navigate to it as a new page.
///
ULExport void ulViewLoadHTML(ULView view, ULString html_string);

///
/// Load a raw string of HTML with additional options.
///
/// @param  view            The View.
///
/// @param  html_string     The raw HTML string to load.
///
/// @param  url             An optional URL for this load (to make it appear as if the HTML was
///                         loaded from a certain URL). Can be used for resolving relative URLs
///                         and cross-origin rules. Pass an empty ULString to use no URL.
///
/// @param  add_to_history  Whether or not this load should be added to the session's history
///                         (eg, the back/forward list).
///
ULExport void ulViewLoadHTMLWithParams(ULView view, ULString html_string, ULString url,
                                       bool add_to_history);

///
/// Load a URL, the View will navigate to it as a new page.
///
/// @param  view        The View.
///
/// @param  url_string  The URL to load.
///
/// @note  You can use file URLs (eg, `file:///page.html`), but you must provide your own file
///        system if you aren't using AppCore (see ulPlatformSetFileSystem()).
///
ULExport void ulViewLoadURL(ULView view, ULString url_string);

///
/// Resize view to a certain width and height (in pixels).
///
ULExport void ulViewResize(ULView view, unsigned int width, unsigned int height);

///
/// Resize view and apply a new device scale in the same pass.
///
/// Prefer this over separate ulViewSetDeviceScale() and ulViewResize() calls when both change at
/// once, for example when a window moves to a display with a different DPI. Applying them
/// together resizes the page once instead of twice.
///
/// @param  view          The View.
///
/// @param  width         The new width, in pixels.
///
/// @param  height        The new height, in pixels.
///
/// @param  device_scale  The new device scale (see ulViewSetDeviceScale()).
///
/// @see  ulViewResize()
///
ULExport void ulViewResizeWithScale(ULView view, unsigned int width, unsigned int height,
                                    double device_scale);

///
/// Acquire the page's JSContext for use with JavaScriptCore API.
///
/// \parblock
/// @note  This locks the JavaScript VM for the current thread. You should call
///        ulViewUnlockJSContext() when you're done so other threads can use JavaScript.
/// \endparblock
///
/// \parblock
/// @note  The lock is recursive, it's okay to call this multiple times as long as you call
///        ulViewUnlockJSContext() the same number of times.
/// \endparblock
///
ULExport JSContextRef ulViewLockJSContext(ULView view);

///
/// Unlock the page's JSContext after a previous call to ulViewLockJSContext().
///
ULExport void ulViewUnlockJSContext(ULView view);

///
/// Evaluate a string of JavaScript and return result.
///
/// @param  view       The View.
///
/// @param  js_string  The string of JavaScript to evaluate.
///
/// @param  exception  The address of a ULString that receives the exception message if the
///                    script throws, or an empty string if it doesn't. Pass NULL to ignore this.
///                    Don't destroy the exception string returned, it's owned by the View.
///
/// \parblock
/// @note  Don't destroy the returned string, it's owned by the View. This value is reset with
///        every call-- if you want to retain it you should copy the result to a new string via
///        ulCreateStringFromCopy().
/// \endparblock
///
/// \parblock
/// @note  The result is converted to a string (eg, `undefined` becomes "undefined"), or is empty
///        if the page can't run scripts.
/// \endparblock
///
/// ```
/// ULString script = ulCreateString("1 + 1");
/// ULString exception;
/// ULString result = ulViewEvaluateScript(view, script, &exception);
/// /* Use the result ("2") and exception description (if any) here. */
/// ulDestroyString(script);
/// ```
///
ULExport ULString ulViewEvaluateScript(ULView view, ULString js_string, ULString* exception);

///
/// Acquire the JSContext of a specific frame for use with JavaScriptCore API.
///
/// @param  view   The View.
///
/// @param  frame  The name of the frame to access. Pass an empty string for the main frame, or
///                the name attribute of one of the main frame's iframes.
///
/// @return  Returns the JSContextRef for the frame, or NULL if the frame doesn't exist or can't
///          run scripts.
///
/// \parblock
/// @note  This locks the JavaScript VM for the current thread. You should call
///        ulViewUnlockJSContextWithFrame() with the same frame name when you're done so other
///        threads can use JavaScript.
/// \endparblock
///
/// \parblock
/// @note  The lock is recursive, it's okay to call this multiple times as long as you call
///        ulViewUnlockJSContextWithFrame() the same number of times.
/// \endparblock
///
ULExport JSContextRef ulViewLockJSContextWithFrame(ULView view, ULString frame);

///
/// Unlock the JSContext of a specific frame after a previous call to
/// ulViewLockJSContextWithFrame().
///
/// @param  view   The View.
///
/// @param  frame  The frame name you passed to ulViewLockJSContextWithFrame().
///
/// @note  Must be called the same number of times as ulViewLockJSContextWithFrame() for the same
///        frame name (the lock is recursive).
///
ULExport void ulViewUnlockJSContextWithFrame(ULView view, ULString frame);

///
/// Get a ULJS handle to the main frame's JavaScript context.
///
/// Unlike ulViewLockJSContext(), the returned handle never keeps the page alive. When the page
/// navigates away or the View is destroyed, the handle stops working and operations on it fail
/// safely. Use it with the functions in `<Ultralight/CAPI/CAPI_JSValue.h>`.
///
/// @return  Returns a new ULJSContext handle for the current page, or NULL if the main frame
///          can't run scripts (eg, when ulViewConfigSetEnableJavaScript() disabled JavaScript,
///          or the document is sandboxed against scripts). You must call ulDestroyJSContext()
///          when finished.
///
/// @note  Each page gets a fresh context. After a navigation, call this again to obtain a handle
///        to the new page's context. The callbacks set via ulViewSetWindowObjectReadyCallback()
///        and ulViewSetDOMReadyCallback() are good acquisition points.
///
ULExport ULJSContext ulViewGetJSContext(ULView view);

///
/// Get a handle to the internal JavaScriptCore VM for a specific frame.
///
/// @param  view   The View.
///
/// @param  frame  The name of the frame to access. Pass an empty string for the main frame, or
///                the name attribute of one of the main frame's iframes.
///
/// @return  Returns a pointer to the VM, or NULL if the frame doesn't exist or can't run
///          scripts. Every frame and View shares one VM.
///
ULExport void* ulViewJavaScriptVMWithFrame(ULView view, ULString frame);

///
/// Evaluate a string of JavaScript in a specific frame and return the result.
///
/// @param  view       The View.
///
/// @param  js_string  The string of JavaScript to evaluate.
///
/// @param  exception  The address of a ULString that receives the exception message if the
///                    script throws, or an empty string if it doesn't. Pass NULL to ignore this.
///                    Don't destroy the exception string returned, it's owned by the View.
///
/// @param  frame      The name of the frame to evaluate the script in. Pass an empty string for
///                    the main frame, or the name attribute of one of the main frame's iframes.
///
/// @return  Returns the result converted to a string (eg, `undefined` becomes "undefined"), or
///          an empty string if the frame doesn't exist or can't run scripts. Don't destroy the
///          returned string, it's owned by the View. This value is reset with every call-- if
///          you want to retain it you should copy it via ulCreateStringFromCopy().
///
ULExport ULString ulViewEvaluateScriptWithFrame(ULView view, ULString js_string,
                                                 ULString* exception, ULString frame);

///
/// Whether or not the View can navigate back in history.
///
ULExport bool ulViewCanGoBack(ULView view);

///
/// Whether or not the View can navigate forward in history.
///
ULExport bool ulViewCanGoForward(ULView view);

///
/// Navigate backwards in history.
///
ULExport void ulViewGoBack(ULView view);

///
/// Navigate forwards in history.
///
ULExport void ulViewGoForward(ULView view);

///
/// Navigate to an arbitrary offset in history.
///
/// @param  view    The View.
///
/// @param  offset  The number of entries to move (negative goes back, positive goes forward).
///
ULExport void ulViewGoToHistoryOffset(ULView view, int offset);

///
/// Reload current page.
///
ULExport void ulViewReload(ULView view);

///
/// Stop all page loads.
///
ULExport void ulViewStop(ULView view);

///
/// Give focus to the View.
///
/// You should call this to give visual indication that the View has input focus (changes active
/// text selection colors, for example). The page gets a window `focus` event, and so does the
/// element that had focus before ulViewUnfocus().
///
ULExport void ulViewFocus(ULView view);

///
/// Remove focus from the View.
///
/// You should call this to give visual indication that the View has lost input focus. The page
/// gets a window `blur` event.
///
/// @note  The page's focused element gets a blur event but stays focused in the document, and
///        shows focus again the next time you call ulViewFocus().
///
ULExport void ulViewUnfocus(ULView view);

///
/// Whether or not the View has focus.
///
ULExport bool ulViewHasFocus(ULView view);

///
/// Whether or not the View has an input element with visible keyboard focus (indicated by a
/// blinking caret).
///
/// You can use this to decide whether or not the View should consume keyboard input events (useful
/// in games with mixed UI and key handling).
///
/// @note  This reports text-editing focus specifically: a focused text field, text area, or
///        editable (contenteditable) region that can accept typed input. Focused elements that
///        consume keys without editing text (eg, a select or checkbox) report false, as do
///        read-only text fields, and so does everything while the View itself is unfocused
///        (see ulViewUnfocus()).
///
ULExport bool ulViewHasInputFocus(ULView view);

///
/// Fire a keyboard event.
///
/// @param  view       The View.
///
/// @param  key_event  The key event.
///
/// @note  kKeyEventType_Char events insert text into input fields, and so does a legacy
///        kKeyEventType_KeyDown that carries text. kKeyEventType_RawKeyDown never inserts text.
///
ULExport void ulViewFireKeyEvent(ULView view, ULKeyEvent key_event);

///
/// Fire a mouse event.
///
/// @param  view         The View.
///
/// @param  mouse_event  The mouse event.
///
ULExport void ulViewFireMouseEvent(ULView view, ULMouseEvent mouse_event);

///
/// Fire a scroll event.
///
/// @param  view          The View.
///
/// @param  scroll_event  The scroll event.
///
ULExport void ulViewFireScrollEvent(ULView view, ULScrollEvent scroll_event);

typedef void (*ULChangeTitleCallback)(void* user_data, ULView caller, ULString title);

///
/// Set callback for when the page title changes.
///
ULExport void ulViewSetChangeTitleCallback(ULView view, ULChangeTitleCallback callback,
                                           void* user_data,
                                           ULUserDataDestroyCallback destroy_user_data);

typedef void (*ULChangeURLCallback)(void* user_data, ULView caller, ULString url);

///
/// Set callback for when the main frame loads a new page.
///
ULExport void ulViewSetChangeURLCallback(ULView view, ULChangeURLCallback callback,
                                         void* user_data,
                                         ULUserDataDestroyCallback destroy_user_data);

typedef void (*ULChangeTooltipCallback)(void* user_data, ULView caller, ULString tooltip);

///
/// Set callback for when the tooltip under the mouse changes (the text is empty when there's no
/// tooltip).
///
/// This is called as the mouse moves over the page, with the tooltip text of the element under
/// the cursor (eg, its `title` attribute). It may repeat the same text on every move.
///
/// The library doesn't display tooltips (and neither does AppCore)-- you should draw the text
/// yourself near the cursor and hide it when the text is empty.
///
ULExport void ulViewSetChangeTooltipCallback(ULView view, ULChangeTooltipCallback callback,
                                             void* user_data,
                                             ULUserDataDestroyCallback destroy_user_data);

typedef void (*ULChangeCursorCallback)(void* user_data, ULView caller, ULCursor cursor);

///
/// Set callback for when the mouse cursor changes.
///
ULExport void ulViewSetChangeCursorCallback(ULView view, ULChangeCursorCallback callback,
                                            void* user_data,
                                            ULUserDataDestroyCallback destroy_user_data);

typedef void (*ULChangeEditableStateCallback)(void* user_data, ULView caller,
                                              ULEditableState state);

///
/// Set callback for when the editable state changes.
///
/// This happens when focus moves to or from an editable element, when the View gains or loses
/// focus, and when the focused element's `inputmode` changes. Use the state to turn an input
/// method or on-screen keyboard on or off (see ULEditableState).
///
/// @warning  The callback can be called from inside your own ulViewFocus() or ulViewUnfocus()
///           call. Don't change focus or content from the callback (wait until it returns).
///
ULExport void ulViewSetChangeEditableStateCallback(ULView view,
                                                   ULChangeEditableStateCallback callback,
                                                   void* user_data,
                                                   ULUserDataDestroyCallback destroy_user_data);

typedef void (*ULUpdateCompositionCallback)(void* user_data, ULView caller);

///
/// Set callback for after every change to the active composition.
///
/// Move the input method's candidate window from the callback (see
/// ulEditorGetCompositionCharacterBounds()).
///
ULExport void ulViewSetUpdateCompositionCallback(ULView view,
                                                 ULUpdateCompositionCallback callback,
                                                 void* user_data,
                                                 ULUserDataDestroyCallback destroy_user_data);

typedef void (*ULDiscardCompositionCallback)(void* user_data, ULView caller);

///
/// Set callback for when the page ends the active composition on its own (eg, on navigation or
/// when the selection moves away from it).
///
/// Cancel the input method's composition from the callback (on Windows, call ImmNotifyIME() with
/// CPS_CANCEL). The composition's text stays in the page as final text, so don't insert it again.
///
ULExport void ulViewSetDiscardCompositionCallback(ULView view,
                                                  ULDiscardCompositionCallback callback,
                                                  void* user_data,
                                                  ULUserDataDestroyCallback destroy_user_data);

typedef void (*ULAddConsoleMessageCallback)(void* user_data, ULView caller, ULMessageSource source,
                                            ULMessageLevel level, ULString message,
                                            unsigned int line_number, unsigned int column_number,
                                            ULString source_id);

///
/// Set callback for when the page adds a message to the console (useful for errors and
/// debugging).
///
ULExport void ulViewSetAddConsoleMessageCallback(ULView view, ULAddConsoleMessageCallback callback,
                                                 void* user_data,
                                                 ULUserDataDestroyCallback destroy_user_data);

typedef ULView (*ULCreateChildViewCallback)(void* user_data, ULView caller, ULString opener_url,
                                            ULString target_url, bool is_popup,
                                            ULIntRect popup_rect);

///
/// Set callback for when the page wants to open a new window (a link with `target="_blank"` or a
/// call to `window.open()`).
///
/// Return a new View from the callback to allow the window or NULL to block it. The library loads
/// `target_url` into the View, and you display it. `popup_rect` is the position and size the page
/// asked for in `window.open()`.
///
/// @warning  Don't destroy the View you return while its window is open.
///
ULExport void ulViewSetCreateChildViewCallback(ULView view, ULCreateChildViewCallback callback,
                                               void* user_data,
                                               ULUserDataDestroyCallback destroy_user_data);

typedef ULView (*ULCreateInspectorViewCallback)(void* user_data, ULView caller, bool is_local,
                                                ULString inspected_url);

///
/// Set callback for when the inspector needs a View to display in (after
/// ulViewCreateLocalInspectorView() or when a remote inspector connects).
///
/// Return a new View from the callback or NULL to cancel. The library loads the inspector into the
/// View, and you display it.
///
ULExport void ulViewSetCreateInspectorViewCallback(ULView view,
                                                   ULCreateInspectorViewCallback callback,
                                                   void* user_data,
                                                   ULUserDataDestroyCallback destroy_user_data);

typedef void (*ULBeginLoadingCallback)(void* user_data, ULView caller, unsigned long long frame_id,
                                       bool is_main_frame, ULString url);

///
/// Set callback for when a load starts (see "Load Events" in the file description).
///
ULExport void ulViewSetBeginLoadingCallback(ULView view, ULBeginLoadingCallback callback,
                                            void* user_data,
                                            ULUserDataDestroyCallback destroy_user_data);

typedef void (*ULFinishLoadingCallback)(void* user_data, ULView caller, unsigned long long frame_id,
                                        bool is_main_frame, ULString url);

///
/// Set callback for when a load ends (whether or not it succeeded).
///
ULExport void ulViewSetFinishLoadingCallback(ULView view, ULFinishLoadingCallback callback,
                                             void* user_data,
                                             ULUserDataDestroyCallback destroy_user_data);

typedef void (*ULFailLoadingCallback)(void* user_data, ULView caller, unsigned long long frame_id,
                                      bool is_main_frame, ULString url, ULString description,
                                      ULString error_domain, int error_code);

///
/// Set callback for when a load fails (just before the finish-loading callback).
///
/// HTTP error responses count as failures (`error_domain` is "HTTPErrorDomain" and `error_code`
/// is the HTTP status code). Canceled loads aren't reported.
///
ULExport void ulViewSetFailLoadingCallback(ULView view, ULFailLoadingCallback callback,
                                           void* user_data,
                                           ULUserDataDestroyCallback destroy_user_data);

typedef void (*ULWindowObjectReadyCallback)(void* user_data, ULView caller,
                                            unsigned long long frame_id, bool is_main_frame,
                                            ULString url);

///
/// Set callback for before a frame's scripts run.
///
/// Set up JavaScript state for the page from the callback (APIs you added with ulViewAttachJSAPI()
/// are already there). Use the DOM-ready callback for anything that needs the DOM.
///
/// @note  Only called when JavaScript is enabled. A page with no scripts may never trigger it.
///
ULExport void ulViewSetWindowObjectReadyCallback(ULView view, ULWindowObjectReadyCallback callback,
                                                 void* user_data,
                                                 ULUserDataDestroyCallback destroy_user_data);

typedef void (*ULDOMReadyCallback)(void* user_data, ULView caller, unsigned long long frame_id,
                                   bool is_main_frame, ULString url);

///
/// Set callback for when a frame's document has been parsed and the DOM is ready.
///
/// This is the best time to read or change the DOM. The page's own `DOMContentLoaded` event has
/// already fired. This is also called when JavaScript is disabled.
///
ULExport void ulViewSetDOMReadyCallback(ULView view, ULDOMReadyCallback callback, void* user_data,
                                        ULUserDataDestroyCallback destroy_user_data);

typedef void (*ULUpdateHistoryCallback)(void* user_data, ULView caller);

///
/// Set callback for when the main frame's back-forward history changes.
///
ULExport void ulViewSetUpdateHistoryCallback(ULView view, ULUpdateHistoryCallback callback,
                                             void* user_data,
                                             ULUserDataDestroyCallback destroy_user_data);

typedef void (*ULPageSettledCallback)(void* user_data, ULView caller, ULString url);

///
/// Set callback for when the page has settled and is ready to capture or display (resources have
/// loaded, initial scripts have run, and layout is stable).
///
/// This fires after the finish-loading callback once network and layout activity have stayed idle
/// for a short time (see ulConfigSetPageSettleDelay()).
///
/// @warning  Pages with constant network or layout activity (ads, animated layouts, streaming
///           video) may never settle. Use a timeout.
///
/// \parblock
/// @note  Animations that don't change layout (eg, a CSS transform or opacity transition) can
///        still be running when the page settles.
/// \endparblock
///
/// \parblock
/// @note  You must call ulUpdate(), ulRefreshDisplay(), and ulRender() from your run loop for this
///        to fire (ulCreateApp() does this for you).
/// \endparblock
///
ULExport void ulViewSetPageSettledCallback(ULView view, ULPageSettledCallback callback,
                                           void* user_data,
                                           ULUserDataDestroyCallback destroy_user_data);

///
/// Set whether or not a view should be repainted during the next call to ulRender().
///
/// @param  view         The View.
///
/// @param  needs_paint  Whether or not the View needs a repaint.
///
/// @note  The library sets this flag automatically when a repaint is due: after a
///        ulRefreshDisplay() call that produces animating or changed content, on resize, when the
///        View is shown, and when the page reacts to user input. You can also set it directly to
///        force a repaint (it still waits for the View's frame-rate cap).
///
ULExport void ulViewSetNeedsPaint(ULView view, bool needs_paint);

///
/// Whether or not a view should be painted during the next call to ulRender().
///
/// When this returns false, rendering would reproduce the previous frame, so you can skip
/// rendering or presenting the view. This is always false while the View is hidden, and while
/// its frame-rate cap holds back its next frame.
///
/// When this returns true, the next call to ulRender() repaints the view. The resulting frame
/// may still be visually identical to the previous frame.
///
/// @note  Continue calling ulRefreshDisplay() on every display refresh regardless of this flag.
///        A View whose only pending work is animation callbacks or CSS animations repaints only
///        after that call, and reports ulViewGetNeedsPaint() as true again from it.
///
ULExport bool ulViewGetNeedsPaint(ULView view);

///
/// Set whether or not this View is visible. (Default = True)
///
/// Hiding a View (passing false) stops it from being painted during ulRender and pauses its
/// requestAnimationFrame callbacks and CSS animations (they stop advancing while hidden). The
/// page's `visibilitychange` event fires with `document.visibilityState` set to "hidden".
///
/// Showing a View again (passing true) resumes its animations and forces a repaint on the next
/// call to ulRender.
///
/// @param  view     The View.
///
/// @param  visible  Whether or not the View is visible.
///
/// @note  JavaScript timers (setTimeout / setInterval) keep running while hidden unless
///        ulViewConfigSetEnableHiddenTimerThrottling() was set when the View was created.
///
ULExport void ulViewSetVisible(ULView view, bool visible);

///
/// Whether or not this View is visible.
///
ULExport bool ulViewIsVisible(ULView view);

///
/// Get the frame-rate cap (in FPS) set for the View, or 0 if none is set.
///
ULExport unsigned int ulViewGetMaxRenderFps(ULView view);

///
/// Set the maximum rate, in frames per second, at which the View advances its animations and
/// repaints. (Default = 0, unthrottled)
///
/// This caps the View's whole frame loop (requestAnimationFrame, CSS and Web animations, smooth
/// scrolling, and painting).
///
/// @param  view  The View.
///
/// @param  fps   The frame-rate cap, or 0 to remove it (the View then advances at the display's
///               refresh rate).
///
ULExport void ulViewSetMaxRenderFps(ULView view, unsigned int fps);

///
/// Set the color scheme this View reports to pages via the `prefers-color-scheme` CSS media
/// feature. (Default = kColorScheme_Auto)
///
/// Overrides the value set via ulViewConfigSetPreferredColorScheme(). A change re-evaluates
/// `prefers-color-scheme` media queries on the page. `matchMedia` change listeners fire and
/// styles recalculate, the same as an OS theme change.
///
/// An out-of-range value logs a warning and leaves the View unchanged.
///
/// @see  ulRendererSetSystemColorScheme()
///
ULExport void ulViewSetPreferredColorScheme(ULView view, ULColorScheme scheme);

///
/// Get the color scheme this View reports to pages.
///
ULExport ULColorScheme ulViewGetPreferredColorScheme(ULView view);

///
/// Create an Inspector View to inspect / debug this View locally.
///
/// This will only succeed if you have the inspector assets in your filesystem-- the inspector
/// will look for file:///inspector/Main.html when it first loads.
///
/// You must handle ulViewSetCreateInspectorViewCallback() so that the library has a View to display
/// the inspector in. This function will call the callback only if an inspector view is not
/// currently active.
///
ULExport void ulViewCreateLocalInspectorView(ULView view);

///
/// Set whether or not to display compositor debug information.
///
/// @param  view     The View.
///
/// @param  enabled  Whether or not to show the debug information.
///
/// @note  Only valid when the compositor is enabled.
///
ULExport void ulViewSetCompositorDebugInfoEnabled(ULView view, bool enabled);

///
/// Whether or not compositor debug information is enabled.
///
ULExport bool ulViewGetCompositorDebugInfoEnabled(ULView view);

/******************************************************************************
 * ViewListener: OnRequestClose
 *****************************************************************************/

typedef void (*ULRequestCloseCallback)(void* user_data, ULView caller);

///
/// Set callback for when the page asks to close (`window.close()` in a window that script
/// opened, or in one with a single page in its history).
///
ULExport void ulViewSetRequestCloseCallback(ULView view, ULRequestCloseCallback callback,
                                            void* user_data,
                                            ULUserDataDestroyCallback destroy_user_data);

/******************************************************************************
 * DownloadListener
 *****************************************************************************/

///
/// Callback for when the View needs an ID for a new download.
///
/// A download starts when the page navigates to a file it can't display (eg, a response with
/// `Content-Disposition: attachment`). Downloads are ignored until you set the download
/// callbacks.
///
/// The library doesn't write anything to disk. You save the data yourself in these callbacks
/// (called in this order):
///
/// | Callback                      | What to do                         |
/// |-------------------------------|------------------------------------|
/// | ULDownloadNextIdCallback      | Return a new ID                    |
/// | ULDownloadRequestCallback     | Return true to accept the download |
/// | ULDownloadBeginCallback       | Open a file                        |
/// | ULDownloadReceiveDataCallback | Write the data to the file         |
/// | ULDownloadFinishCallback      | Close the file                     |
/// | ULDownloadFailCallback        | Close the file and delete it       |
///
/// @return  Return a new ID (a different one on every call, eg a counter starting at 0).
///
typedef unsigned int (*ULDownloadNextIdCallback)(void* user_data, ULView caller);

///
/// Callback for when the page wants to download a file.
///
/// @return  Return true to allow the download, or false to block it.
///
typedef bool (*ULDownloadRequestCallback)(void* user_data, ULView caller, unsigned int id,
                                          ULString url);

///
/// Callback for when a download starts.
///
/// Open a file to write the data to here. `filename` is the suggested file name (from the server,
/// or the end of the URL).
///
typedef void (*ULDownloadBeginCallback)(void* user_data, ULView caller, unsigned int id,
                                        ULString url, ULString filename,
                                        long long expected_content_length);

///
/// Callback for when a download receives data (usually several times per download).
///
/// Write the data to the file here.
///
/// @param  data  The data (only valid during this call, so copy it to keep it).
///
typedef void (*ULDownloadReceiveDataCallback)(void* user_data, ULView caller, unsigned int id,
                                              ULBuffer data);

///
/// Callback for when a download finishes.
///
/// Close the file here.
///
typedef void (*ULDownloadFinishCallback)(void* user_data, ULView caller, unsigned int id);

///
/// Callback for when a download fails.
///
/// Close the file and delete it here.
///
typedef void (*ULDownloadFailCallback)(void* user_data, ULView caller, unsigned int id);

///
/// Set callback for generating unique download ids.
///
ULExport void ulViewSetDownloadNextIdCallback(ULView view, ULDownloadNextIdCallback callback,
                                              void* user_data,
                                              ULUserDataDestroyCallback destroy_user_data);

///
/// Set callback for when the View requests a download.
///
ULExport void ulViewSetDownloadRequestCallback(ULView view, ULDownloadRequestCallback callback,
                                               void* user_data,
                                               ULUserDataDestroyCallback destroy_user_data);

///
/// Set callback for when a download begins.
///
ULExport void ulViewSetDownloadBeginCallback(ULView view, ULDownloadBeginCallback callback,
                                             void* user_data,
                                             ULUserDataDestroyCallback destroy_user_data);

///
/// Set callback for when download data is received.
///
ULExport void ulViewSetDownloadReceiveDataCallback(ULView view,
                                                    ULDownloadReceiveDataCallback callback,
                                                    void* user_data,
                                                    ULUserDataDestroyCallback destroy_user_data);

///
/// Set callback for when a download finishes.
///
ULExport void ulViewSetDownloadFinishCallback(ULView view, ULDownloadFinishCallback callback,
                                              void* user_data,
                                              ULUserDataDestroyCallback destroy_user_data);

///
/// Set callback for when a download fails.
///
ULExport void ulViewSetDownloadFailCallback(ULView view, ULDownloadFailCallback callback,
                                            void* user_data,
                                            ULUserDataDestroyCallback destroy_user_data);

///
/// Cancel an active download.
///
/// No more data arrives for the download, and the callback set with
/// ulViewSetDownloadFailCallback() is called for it during a later ulUpdate().
///
/// @param  view  The View.
///
/// @param  id    The download id to cancel.
///
ULExport void ulViewCancelDownload(ULView view, unsigned int id);

/******************************************************************************
 * NetworkListener
 *****************************************************************************/

///
/// Callback for before the View sends a network request.
///
/// @return  Return true to allow the request, or false to block it.
///
/// @note  `file:` and `data:` URLs and synchronous XMLHttpRequests don't go through here.
///
/// @pre  Not available in the Free edition (the callback is never called there).
///
typedef bool (*ULNetworkRequestCallback)(void* user_data, ULView caller, ULString url);

///
/// Set callback for when the View is about to begin a network request.
///
ULExport void ulViewSetNetworkRequestCallback(ULView view, ULNetworkRequestCallback callback,
                                              void* user_data,
                                              ULUserDataDestroyCallback destroy_user_data);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ULTRALIGHT_CAPI_VIEW_H