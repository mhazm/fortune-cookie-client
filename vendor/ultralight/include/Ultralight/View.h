///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/Defines.h>
#include <Ultralight/RefPtr.h>
#include <Ultralight/Color.h>
#include <Ultralight/Editor.h>
#include <Ultralight/KeyEvent.h>
#include <Ultralight/JavaScript.h>
#include <Ultralight/MouseEvent.h>
#include <Ultralight/ScrollEvent.h>
#include <Ultralight/GamepadEvent.h>
#include <Ultralight/RenderTarget.h>
#include <Ultralight/Bitmap.h>
#include <Ultralight/Listener.h>
#include <Ultralight/platform/Surface.h>

///
/// Opaque handle to a JavaScript execution context (one page's script world in one View).
///
/// Declared in full in `<Ultralight/CAPI/CAPI_JSValue.h>`.
///
typedef struct C_JSContext* ULJSContext;

///
/// Opaque handle to a set of native JavaScript bindings.
///
/// Declared in full in `<Ultralight/CAPI/CAPI_JSAPI.h>`.
///
typedef struct C_JSAPI* ULJSAPI;

///
/// Opaque handle to a page's DOM document.
///
/// Declared in full in `<Ultralight/CAPI/CAPI_DOMDocument.h>`.
///
typedef struct C_DOMDocument* ULDOMDocument;

///
/// Opaque handle to a set of DOM event listeners and DOM-ready hooks.
///
/// Declared in full in `<Ultralight/CAPI/CAPI_DOMTriggers.h>`.
///
typedef struct C_DOMTriggers* ULDOMTriggers;

///
/// Opaque handle to a data-binding context.
///
/// Declared in full in `<Ultralight/CAPI/CAPI_DOMData.h>`.
///
typedef struct C_DOMDataContext* ULDOMDataContext;

///
/// The details an API injection filter decides on.
///
/// Declared in full in `<Ultralight/CAPI/CAPI_JSAPI.h>`.
///
typedef struct ULJSAPIInjectionRequest ULJSAPIInjectionRequest;

///
/// The details a DOM triggers injection filter decides on.
///
/// Declared in full in `<Ultralight/CAPI/CAPI_DOMTriggers.h>`.
///
typedef struct ULDOMTriggersInjectionRequest ULDOMTriggersInjectionRequest;

namespace ultralight {

///
/// A page color scheme, as reported to pages via the `prefers-color-scheme` CSS media feature.
///
/// @see  ViewConfig::preferred_color_scheme
/// @see  View::set_preferred_color_scheme()
/// @see  Renderer::set_system_color_scheme()
///
enum class ColorScheme : uint8_t {
  Auto = 0,   ///< Follow the system color scheme set on the Renderer.
  Light = 1,  ///< Always report a light color scheme.
  Dark = 2,   ///< Always report a dark color scheme.
};

///
/// Which pages may read the clipboard from script.
///
/// @see  ViewConfig::clipboard_read_policy
///
enum class ClipboardReadPolicy : uint8_t {
  Deny = 0,                ///< Refuse every script-initiated clipboard read.
  AllowForAppContent = 1,  ///< Grant reads from your application's own content only.
  Allow = 2,               ///< Grant reads from any page.
};

///
/// View-specific configuration settings.
///
/// @see  Renderer::CreateView()
///
struct UExport ViewConfig {
  
  ///
  /// A user-generated id for the display (monitor, TV, or screen) that this View will be shown on.
  /// 
  /// Animations are driven based on the physical refresh rate of the display. Multiple Views can
  /// share the same display.
  /// 
  /// @note  This is automatically managed for you when App::Create() is used and the View is
  ///        hosted in a Window. A View you create yourself in an App should use the id of the
  ///        Monitor it is shown on (Monitor::display_id()).
  /// 
  /// @see  Renderer::RefreshDisplay(), Renderer::SetDisplayRefreshRate()
  /// 
  uint32_t display_id = 0;

  ///
  /// Whether to render using the GPU renderer (accelerated) or the CPU renderer (unaccelerated).
  ///
  /// When true, the View renders to an offscreen GPU texture using the GPU driver set with
  /// Platform::set_gpu_driver(). You can get the texture's details with View::render_target().
  ///
  /// When false (the default), the View renders to an offscreen pixel buffer using the
  /// multithreaded CPU renderer. You can provide this pixel buffer yourself-- see
  /// Platform::set_surface_factory() and View::surface().
  ///
  /// \parblock
  /// @note  You must set a GPU driver before creating an accelerated View (the process exits
  ///        with an error otherwise).
  /// \endparblock
  ///
  /// \parblock
  /// @note  This is automatically managed for you when App::Create() is used.
  /// \endparblock
  ///
  bool is_accelerated = false;

  ///
  /// The initial device scale, ie. the amount to scale page units to screen pixels. This should
  /// be set to the scaling factor of the device that the View is displayed on.
  ///
  /// \parblock
  /// @note  1.0 is equal to 100% zoom (no scaling), 2.0 is equal to 200% zoom (2x scaling).
  /// \endparblock
  ///
  /// \parblock
  /// @note  This is automatically managed for you when App::Create() is used.
  /// \endparblock
  ///
  double initial_device_scale = 1.0;

  ///
  /// Whether or not this View should support transparency.
  ///
  /// The page needs a transparent background too:
  ///
  /// ```
  /// html, body { background: transparent; }
  /// ```
  ///
  bool is_transparent = false;

  ///
  /// The base color the page renders on before any page styling applies.
  ///
  /// This is the color visible while a page loads and wherever the page itself paints nothing. Dark
  /// applications should set a dark base so pages never flash white during load. An unset color
  /// (the default) means opaque white.
  ///
  /// @note  `is_transparent` takes precedence. A transparent View keeps a fully transparent base.
  ///        On an opaque View a translucent color is composited over white, so the View itself
  ///        stays opaque.
  ///
  Color background_color;

  ///
  /// The color scheme this View reports to pages via the `prefers-color-scheme` CSS media feature.
  ///
  /// Auto (the default) follows the system scheme set via Renderer::set_system_color_scheme(),
  /// which is Light until you (or App) set a different one. Light or Dark pins the scheme
  /// for this View regardless of the system value.
  ///
  /// \parblock
  /// @note  The scheme is a signal to the page, which styles itself via media queries. Built-in UI
  ///        (form controls, scrollbars, the default white canvas) keeps its light appearance under
  ///        a dark scheme.
  /// \endparblock
  ///
  /// \parblock
  /// @note  This is automatically managed for you when App::Create() is used (the OS setting
  ///        is fed to the Renderer and tracked as it changes).
  /// \endparblock
  ///
  /// @see  View::set_preferred_color_scheme()
  ///
  ColorScheme preferred_color_scheme = ColorScheme::Auto;

  ///
  /// Whether or not the View should initially have input focus.
  ///
  /// @see  View::Focus()
  ///
  bool initial_focus = true;

  ///
  /// Whether or not images should be enabled.
  ///
  bool enable_images = true;

  ///
  /// Whether or not JavaScript should be enabled.
  ///
  bool enable_javascript = true;
  
  ///
  /// Whether or not compositing should be enabled.
  ///
  /// When enabled, certain content (eg, 3D transforms, `will-change`, animated transforms or
  /// opacity, and video) paints into separate composited layers, making transform and opacity
  /// changes cheap to animate.
  ///
  bool enable_compositor = true;

  ///
  /// Whether or not to enable extra compositor debug information, specifically:
  ///  - Visualize compositor layers and tile boundaries
  ///  - Display repaint counters for each layer
  ///
  /// @note  Only valid when the compositor is enabled.
  ///
  bool enable_compositor_debug_info = false;

  ///
  /// Whether or not the HTML5 Canvas Filters API (`CanvasRenderingContext2D.filter`) is enabled.
  ///
  /// @note  Reference filters (`url(#x)`) render unfiltered. The filter string is still readable
  ///        for feature detection.
  ///
  bool enable_canvas_filters = true;

  ///
  /// Whether or not text editing should follow the host OS's native conventions instead of
  /// the library's cross-platform behavior.
  ///
  /// When false (the default), text selection and editing behave identically on every
  /// platform (a directionless selection is promoted to forward, the convention Windows
  /// browsers use). Set this to true for desktop applications that should feel native to
  /// each platform (eg, non-directional selections and Mac-style shift-click extension on
  /// macOS).
  ///
  /// @note  App turns this on for the Views it creates for a bare AddPanel() (see
  ///        Settings::match_native_editing_behavior). A ViewConfig you pass yourself keeps
  ///        this value.
  ///
  bool match_native_editing_behavior = false;

  ///
  /// Policy governing script-initiated clipboard reads (eg, a page's own Paste button calling
  /// `document.execCommand('paste')`).
  ///
  /// AllowForAppContent (the default) grants requests from your application's own content (local
  /// `file:///` pages and pages loaded from application-supplied data, eg, View::LoadHTML()) and
  /// refuses them from other pages. Allow grants requests from any page. Deny refuses them all.
  ///
  /// \parblock
  /// @note  The library only considers requests made during a user gesture such as a click or key
  ///        press. Script running outside a gesture is always refused.
  /// \endparblock
  ///
  /// \parblock
  /// @note  Pastes the user performs directly (Ctrl+V, or Editor::Execute() with
  ///        EditorCommand::Paste) are never affected by this policy.
  /// \endparblock
  ///
  ClipboardReadPolicy clipboard_read_policy = ClipboardReadPolicy::AllowForAppContent;

  ///
  /// Whether to throttle JavaScript timers (setTimeout / setInterval) while this View is hidden.
  ///
  /// When false (the default), timers keep running at their normal rate while the View is hidden,
  /// so background logic continues to tick.
  ///
  /// When true, repeating timers in a hidden View are aligned to roughly one-second boundaries (so
  /// a fast interval effectively drops to about 1 Hz), reducing CPU usage for off-screen Views.
  /// Timers return to their normal rate once the View is shown again.
  ///
  /// @note  This does not affect requestAnimationFrame, which is always suspended while a View is
  ///        hidden regardless of this setting.
  ///
  /// @see  View::set_visible()
  ///
  bool enable_hidden_timer_throttling = false;

  ///
  /// The maximum rate, in frames per second, at which this View advances its animations and
  /// repaints.
  ///
  /// This caps the View's whole frame loop: requestAnimationFrame callbacks, CSS and Web
  /// animations, smooth scrolling, and painting all advance at no more than this rate. Use it to
  /// spend less CPU and GPU on a View that is on-screen but not the focus of attention (a secondary
  /// panel, a minimap, a paused menu behind a modal).
  ///
  /// A value of 0 (the default) leaves the View unthrottled, advancing at the display's refresh
  /// rate, the same as a normal browser tab.
  ///
  /// \parblock
  /// @note  Page content changes (from script, the DOM API, or data bindings) also wait for the
  ///        next capped frame. Input events, a resize, and showing the View paint right away.
  /// \endparblock
  ///
  /// \parblock
  /// @note  This is independent of View::set_visible() and enable_hidden_timer_throttling. Hiding a
  ///        View stops it entirely. This setting slows a View that remains visible.
  /// \endparblock
  ///
  /// \parblock
  /// @note  The View is always capped at your edition's maximum frame rate.
  /// \endparblock
  ///
  uint32_t max_render_fps = 0;

  ///
  /// Whether or not a script can open a window with `window.open()` without a user gesture.
  ///
  /// When false (the default), Ultralight behaves like a typical browser: a `window.open()` call
  /// succeeds only while the page is handling a user gesture, such as from a click handler. A call
  /// made on its own (on page load, or from a timer) is blocked as a popup. Set this to true to
  /// let scripts call `window.open()` at any time. The same gesture rule applies to a form
  /// submission that targets a new window.
  ///
  /// @note  Even when this is allowed, a window is only created if your
  ///        ViewListener::OnCreateChildView() handler returns one; return nullptr there to block
  ///        it.
  ///
  bool javascript_can_open_windows_automatically = false;

  ///
  /// Whether or not pages loaded from `file:///` URLs can access content from any origin.
  ///
  /// When true (the default), a `file:///` page can read other frames' documents and make
  /// requests to any origin without cross-origin checks. Set this to false to give `file:///`
  /// pages the same cross-origin rules as web pages (they stay same-origin with other
  /// `file:///` pages).
  ///
  bool allow_universal_access_from_file_urls = true;

  ///
  /// The default font family (used for text that doesn't set one).
  ///
  /// @note  App replaces these font_family_* defaults with ones that match the host OS for the
  ///        Views it creates for a bare AddPanel() (see Settings::auto_font_families).
  ///
  String font_family_standard = "Times New Roman";

  ///
  /// The default monospace font family (eg, for `pre` and `code`).
  ///
  String font_family_fixed = "Courier New";

  ///
  /// The default font family for the CSS `serif` generic.
  ///
  String font_family_serif = "Times New Roman";

  ///
  /// The default font family for the CSS `sans-serif` generic.
  ///
  String font_family_sans_serif = "Arial";

  ///
  /// The default font family for the CSS `cursive` generic.
  ///
  String font_family_cursive = "Comic Sans MS";

  ///
  /// The default font family for the CSS `fantasy` generic.
  ///
  String font_family_fantasy = "Impact";

  ///
  /// The default font family for the CSS `-webkit-pictograph` generic. Leave it empty to use
  /// font_family_standard.
  ///
  String font_family_pictograph = "";

  ///
  /// The default font size, in pixels.
  ///
  uint32_t font_size_default = 16;

  ///
  /// The default font size for monospace text (eg, `pre` and `code`), in pixels.
  ///
  uint32_t font_size_fixed = 13;

  ///
  /// Custom user-agent string. You can use this to override the default user-agent string.
  ///
  /// @pre  Not available in the Free edition (the default user-agent string is always used).
  ///
  String user_agent = ULTRALIGHT_USER_AGENT;
};

///
/// Web-page container rendered to an offscreen surface.
/// 
/// The View class is responsible for loading and rendering web-pages to an offscreen surface. It
/// is completely isolated from the OS windowing system, you must forward all input events to it
/// from your application.
///
/// ## Creating a View
///
/// You can create a View using Renderer::CreateView():
///
/// ```
///  // Create a ViewConfig with the desired settings
///  ViewConfig view_config;
///  
///  // Create a View, 500 by 500 pixels in size, using the default Session
///  RefPtr<View> view = renderer->CreateView(500, 500, view_config, nullptr);
/// ```
///
/// @note  When using App::Create(), the library creates a View for you when you add a panel to
///        a window (see `<AppCore/Layout.h>`).
///
/// ## Loading Content into a View
///
/// You can load content asynchronously into a View using View::LoadURL().
///
/// ```
///  // Load a URL into the View
///  view->LoadURL("https://en.wikipedia.org/wiki/Main_Page");
/// ```
///
/// ### Local File URLs
///
/// Local file URLs (eg, `file:///page.html`) will be loaded via FileSystem. You can provide your
/// own FileSystem implementation so these files can be loaded from your application's resources.
///
/// ### Displaying Views in Your Application
///
/// Views are rendered either to a pixel-buffer (View::surface()) or a GPU texture
/// (View::render_target()) depending on whether CPU or GPU rendering is used (see
/// ViewConfig::is_accelerated).
///
/// You can use the Surface or RenderTarget to display the View in your application.
///
/// ```
///  // Get the Surface for the View (assuming CPU rendering)
///  Surface* surface = view->surface();
///
///  // Check if the Surface is dirty (pixels have changed)
///  if (!surface->dirty_bounds().IsEmpty()) {
///    // Cast to the default Surface implementation (BitmapSurface) and get
///    // the underlying Bitmap.
///    RefPtr<Bitmap> bitmap = static_cast<BitmapSurface*>(surface)->bitmap();
///
///    // Use the bitmap pixels here...
///
///    // Clear the dirty bounds after you're done displaying the pixels
///    surface->ClearDirtyBounds();
///  }
/// ```
///
/// ## Input Events
///
/// You must forward all input events to the View from your application. This includes keyboard,
/// mouse, and scroll events.
///
/// ```
///  // Forward a mouse-move event to the View
///  MouseEvent evt;
///  evt.type = MouseEvent::kType_MouseMoved;
///  evt.x = 100;
///  evt.y = 100;
///  evt.button = MouseEvent::kButton_None;
///  view->FireMouseEvent(evt);
/// ```
///
/// @note  The View API is not thread-safe, all calls must be made on the same thread that the
///        Renderer or App was created on (AttachDOMDataContext() and DetachDOMDataContext() are
///        the exceptions).
///
class UExport View : public RefCounted {
 public:
  ///
  /// Get the URL of the current page loaded into this View, if any.
  ///
  virtual String url() = 0;

  ///
  /// Get the title of the current page loaded into this View, if any.
  ///
  virtual String title() = 0;

  ///
  /// Get the width of the View, in pixels.
  ///
  virtual uint32_t width() const = 0;

  ///
  /// Get the height of the View, in pixels.
  ///
  virtual uint32_t height() const = 0;

  ///
  /// Get the display id of the View.
  /// 
  /// @see  ViewConfig::display_id, Renderer::RefreshDisplay()
  /// 
  virtual uint32_t display_id() const = 0;

  ///
  /// Set the display id of the View.
  ///
  /// You should call this when the View moves to another display.
  ///
  /// @param  id  The id of the new display (see ViewConfig::display_id).
  ///
  /// @note  This is automatically managed for you for Views hosted in an AppCore Window.
  ///
  virtual void set_display_id(uint32_t id) = 0;

  ///
  /// Get the device scale, ie. the amount to scale page units to screen pixels.
  /// 
  /// For example, a value of 1.0 is equivalent to 100% zoom. A value of 2.0 is 200% zoom.
  /// 
  virtual double device_scale() const = 0;

  ///
  /// Set the device scale.
  ///
  /// @param  scale  The new device scale (see device_scale()).
  ///
  /// @note  To change the View's size at the same time, use Resize(width, height, device_scale)
  ///        instead, which applies both in one pass.
  ///
  virtual void set_device_scale(double scale) = 0;

  ///
  /// Whether or not the View is GPU-accelerated. If this is false, the page will be rendered
  /// via the CPU renderer.
  /// 
  virtual bool is_accelerated() const = 0;

  ///
  /// Whether or not the View supports transparent backgrounds.
  ///
  virtual bool is_transparent() const = 0;

  ///
  /// Check if the main frame of the page is currently loading.
  ///
  virtual bool is_loading() = 0;

  ///
  /// Check if the page has settled after loading.
  ///
  /// This is true once the page's network and layout activity have gone idle following the main
  /// frame's onload event. It is the latched, queryable form of the LoadListener::OnPageSettled()
  /// event, flipping true when that event fires and resetting to false on each new navigation.
  ///
  /// @note  Settling covers network and layout activity only. See LoadListener::OnPageSettled() for
  ///        what settling does not promise and for the run-loop calls the library needs in order to
  ///        observe it.
  ///
  virtual bool is_settled() = 0;

  ///
  /// Get the RenderTarget for the View.
  ///
  /// @pre  Only valid if this View is using the GPU renderer (see ViewConfig::is_accelerated).
  ///
  /// @note  You can use this with your GPUDriver implementation to bind and display the
  ///        corresponding texture in your application.
  ///
  virtual RenderTarget render_target() = 0;

  ///
  /// Get the Surface for the View (native pixel buffer that the CPU renderer draws into).
  ///
  /// @pre  Only valid if the View uses the CPU renderer (see ViewConfig::is_accelerated). This
  ///       returns nullptr if the View uses the GPU renderer.
  ///
  /// @note  The default Surface is BitmapSurface, but you can provide your own Surface
  ///        implementation with Platform::set_surface_factory().
  ///
  virtual Surface* surface() = 0;

  ///
  /// Load a raw string of HTML, the View will navigate to it as a new page.
  ///
  /// @param  html  The raw HTML string to load.
  ///
  /// @param  url   An optional URL for this load (to make it appear as if we loaded this HTML
  ///               from a certain URL). Can be used for resolving relative URLs and cross-origin
  ///               rules.
  ///
  /// @param  add_to_history  Whether or not this load should be added to the session's history
  ///                         (eg, the back/forward list).
  ///
  virtual void LoadHTML(const String& html, const String& url = "", bool add_to_history = false)
      = 0;

  ///
  /// Load a URL, the View will navigate to it as a new page.
  ///
  /// @param  url  The URL to load.
  ///
  /// @note  You can use file URLs (eg, `file:///page.html`), but you must provide your own
  ///        FileSystem if you aren't using AppCore.
  ///
  /// @see  Platform::set_file_system()
  ///
  virtual void LoadURL(const String& url) = 0;

  ///
  /// Resize View to a certain size.
  ///
  /// @param  width   The new width, in pixels.
  ///
  /// @param  height  The new height, in pixels.
  ///
  virtual void Resize(uint32_t width, uint32_t height) = 0;

  ///
  /// Resize View to a certain size and apply a new device scale in the same pass.
  ///
  /// Prefer this over separate set_device_scale() and Resize() calls when both change at
  /// once (for example, when a window moves to a display with a different DPI): applying
  /// them together resizes the page once instead of twice.
  ///
  /// @param  width         The new width, in pixels.
  ///
  /// @param  height        The new height, in pixels.
  ///
  /// @param  device_scale  The new device scale (see set_device_scale()).
  ///
  virtual void Resize(uint32_t width, uint32_t height, double device_scale) = 0;

  ///
  /// Acquire the page's JSContext for use with the JavaScriptCore API.
  ///
  /// @param  frame  The name of the frame to access. Pass an empty string (default) for the main
  ///                frame, or the name attribute of one of the main frame's iframes.
  ///
  /// @return  Returns a RefPtr to the JSContext for the frame, or nullptr if the frame doesn't
  ///          exist or can't run scripts (eg, when ViewConfig::enable_javascript is false).
  ///
  /// \parblock
  /// @note  You can use the underlying JSContextRef with the JavaScriptCore C API to marshal
  ///        C/C++ objects to and from JavaScript, bind callbacks, and call JS functions directly.
  /// \endparblock
  ///
  /// \parblock
  /// @note  The JSContextRef is reset on each page navigation. You should set up your
  ///        JavaScript state in LoadListener::OnWindowObjectReady() or
  ///        LoadListener::OnDOMReady().
  /// \endparblock
  ///
  /// \parblock
  /// @note  This locks the JavaScript VM for the current thread until the returned JSContext's
  ///        ref-count drops to zero. The lock is recursive, so you can call this more than once.
  /// \endparblock
  ///
  virtual RefPtr<JSContext> LockJSContext(const String& frame = "") = 0;

  ///
  /// Get a handle to the internal JavaScriptCore VM.
  ///
  /// @param  frame  The name of the frame to access. Pass an empty string (default) for the main
  ///                frame, or the name attribute of one of the main frame's iframes.
  ///
  /// @return  Returns a pointer to the VM, or nullptr if the frame doesn't exist or can't run
  ///          scripts. Every frame and View shares one VM.
  ///
  virtual void* JavaScriptVM(const String& frame = "") = 0;

  ///
  /// Evaluate a string of JavaScript and return the result as a String.
  ///
  /// @param  script     The JavaScript to evaluate.
  ///
  /// @param  exception  Receives the exception message if the script throws, or an empty string
  ///                    if it doesn't. Pass a nullptr if you don't care about exceptions.
  ///
  /// @param  frame      The name of the frame to evaluate the script in. Pass an empty string
  ///                    (default) for the main frame, or the name attribute of one of the main
  ///                    frame's iframes.
  ///
  /// @return  Returns the result converted to a String (eg, `undefined` becomes "undefined"), or
  ///          an empty string if the frame doesn't exist or can't run scripts.
  ///
  /// \parblock
  /// @note  You don't need to lock the JS context, this does it for you.
  /// \endparblock
  ///
  /// \parblock
  /// @note  For typed access to the result, use the ultralight::js layer instead: acquire the
  ///        page's context with js::Context (see `<Ultralight/JS.h>`) and call
  ///        Context::Evaluate(), which returns a js::Result holding the completion value or
  ///        the script's exception rather than a flattened string.
  /// \endparblock
  ///
  /// \parblock
  /// @note  For raw JavaScriptCore access, lock the JS context and call JSEvaluateScript() in
  ///        the JavaScriptCore C API.
  /// \endparblock
  ///
  /// @see  <JavaScriptCore/JSBase.h>
  ///
  virtual String EvaluateScript(const String& script, String* exception = nullptr, const String& frame = "") = 0;

  ///
  /// Get a ULJS handle to the main frame's JavaScript context.
  ///
  /// Unlike LockJSContext(), the returned handle never keeps the page alive. When the page
  /// navigates away or the View is destroyed, the handle stops working and operations on it fail
  /// safely. Use it with the functions in `<Ultralight/CAPI/CAPI_JSValue.h>`. Most C++ code uses
  /// js::Context instead (see `<Ultralight/JS.h>`), which wraps this handle.
  ///
  /// @return  Returns a new ULJSContext handle for the current page, or NULL if the main frame
  ///          can't run scripts (eg, when ViewConfig::enable_javascript is false, or the document
  ///          is sandboxed against scripts). You must call ulDestroyJSContext() when finished.
  ///
  /// @note  Each page gets a fresh context. After a navigation, call this again to obtain a
  ///        handle to the new page's context. LoadListener::OnWindowObjectReady() and
  ///        LoadListener::OnDOMReady() are good acquisition points.
  ///
  virtual ULJSContext GetJSContext() = 0;

  ///
  /// Get a ULDOM handle to the current page's document (main frame).
  ///
  /// The document is available even when JavaScript is disabled (ViewConfig::enable_javascript is
  /// false). Most C++ code uses dom::Document instead (see `<Ultralight/DOM.h>`).
  ///
  /// @return  Returns the document (NULL if the main frame has none). You must call
  ///          ulDestroyDOMDocument() when finished.
  ///
  /// @note  The handle stops working when the page goes away, so get it again for each page (eg, in
  ///        LoadListener::OnDOMReady()). Before your page loads, and while a new page is loading,
  ///        this returns the previous document.
  ///
  virtual ULDOMDocument GetDOMDocument() = 0;

  ///
  /// Callback that decides whether an attached set of DOM listeners is applied to a loading
  /// page.
  ///
  /// This is called on the Renderer's thread after the origin rules have been evaluated. See
  /// SetDOMTriggersInjectionFilter() and `<Ultralight/CAPI/CAPI_DOMTriggers.h>`. Most C++ code
  /// uses dom::SetInjectionFilter() instead.
  ///
  /// @param  user_data  The user data passed to SetDOMTriggersInjectionFilter().
  ///
  /// @param  request    The set of DOM listeners, the page's origin, and the verdict of the
  ///                    origin rules (see ULDOMTriggersInjectionRequest). It's owned by the
  ///                    library and valid only during the callback. Its `view` member is NULL
  ///                    here (it's set only for a filter set with
  ///                    ulViewSetDOMTriggersInjectionFilter()).
  ///
  /// @return  Return true to apply the listeners to this page, or false to skip it. The return
  ///          value is final: it can override the origin rules either way.
  ///
  using DOMTriggersInjectionFilter = bool (*)(void* user_data,
                                              const ULDOMTriggersInjectionRequest* request);

  ///
  /// Attach a set of DOM listeners to this View.
  ///
  /// A set of DOM listeners (see `<Ultralight/CAPI/CAPI_DOMTriggers.h>`) holds event listeners
  /// matched by CSS selector and DOM-ready hooks. Once attached, the set is added to the current
  /// page (if its document has finished parsing) and to every page the View loads afterward, so
  /// you don't need to register again after a navigation. This works with or without JavaScript
  /// enabled. Most C++ code uses dom::Triggers::AttachTo() instead.
  ///
  /// ```
  /// ULDOMTriggers triggers = ulCreateDOMTriggers();
  /// ulDOMTriggersOn(triggers, "#save", "click", kULDOMEventFlags_None, OnSave, ctx, nullptr);
  /// view->AttachDOMTriggers(triggers);
  /// // #save is clickable on this page and on every page the View navigates to.
  /// ```
  ///
  /// The View keeps the set attached while an owning handle to it exists (see
  /// ulDestroyDOMTriggers()). Attaching it again changes the flags and origin rules for later
  /// pages (a page that already has the listeners keeps them).
  ///
  /// @param  triggers          The listeners to attach.
  ///
  /// @param  flags             A logically ORed set of ULDOMTriggersAttachFlags. Unknown bits
  ///                           fail the attach.
  ///
  /// @param  origin_rules      An array of `scheme://host[:port]` origin patterns for the pages
  ///                           that get the set, or nullptr for the default policy (local pages
  ///                           plus your application's own content). Rules replace the default.
  ///                           See ultralight::OriginRules for the rule syntax.
  ///
  /// @param  num_origin_rules  The number of entries in `origin_rules`.
  ///
  /// @return  Returns true on success, or false if `triggers` is NULL, a flag is unknown, or a
  ///          rule failed to parse (nothing changes then, and a logged warning says why).
  ///
  /// \parblock
  /// @note  The origin policy is shared with AttachJSAPI(): same rule syntax, same default,
  ///        evaluated against the page's security origin.
  /// \endparblock
  ///
  /// \parblock
  /// @note  Without kULDOMTriggersAttachFlags_AllFrames, only the main frame gets the set.
  /// \endparblock
  ///
  /// \parblock
  /// @note  A page restored from the back-forward cache (Config::page_cache_size > 0) gets the
  ///        listeners again before its `pageshow` event. Its DOM-ready hooks don't run again, so
  ///        register a restore hook with ulDOMTriggersOnRestore() for work you need on every
  ///        restore.
  /// \endparblock
  ///
  virtual bool AttachDOMTriggers(ULDOMTriggers triggers, unsigned flags = 0,
                                 const char* const* origin_rules = nullptr,
                                 size_t num_origin_rules = 0) = 0;

  ///
  /// Detach a set of DOM listeners from this View.
  ///
  /// The listeners stop right away (they're removed from the current page), and no later page
  /// gets them.
  ///
  /// @param  triggers   The listeners to detach.
  ///
  virtual void DetachDOMTriggers(ULDOMTriggers triggers) = 0;

  ///
  /// Set a filter that decides whether an attached set of DOM listeners is applied to a page.
  ///
  /// The filter is consulted after the origin rules, for every attached set and page.
  ///
  /// @param  filter             The filter callback. Pass nullptr to remove the current filter.
  ///
  /// @param  user_data          User data passed through to the filter on every call.
  ///
  /// @param  destroy_user_data  Callback invoked exactly once to destroy `user_data` after the
  ///                            filter is replaced or the View is destroyed (may be nullptr).
  ///
  /// @see  dom::SetInjectionFilter()
  ///
  virtual void SetDOMTriggersInjectionFilter(DOMTriggersInjectionFilter filter,
                                             void* user_data,
                                             void (*destroy_user_data)(void*)) = 0;

  ///
  /// Attach a data-binding context to this View.
  ///
  /// Once attached, pages loaded into this View compile their binding markup against the
  /// context's published models and stay updated as the context publishes new data (see
  /// `<Ultralight/dom/data/Context.h>`).
  ///
  /// One context may attach to any number of View%s (one publish serves them all). The View
  /// keeps the context attached while an owning handle to it exists (see
  /// ulDestroyDOMDataContext()). Attaching an already-attached context updates its flags and
  /// origin rules.
  ///
  /// @param  context           The context to attach.
  ///
  /// @param  flags             A logically ORed set of ULDOMDataContextAttachFlags (none are
  ///                           defined yet, so pass kULDOMDataContextAttachFlags_None). Unknown
  ///                           bits fail the attach.
  ///
  /// @param  origin_rules      An array of `scheme://host[:port]` origin patterns for the pages
  ///                           that compile the context's bindings, or nullptr for the default
  ///                           policy (local pages plus your application's own content). See
  ///                           ultralight::OriginRules for the rule syntax.
  ///
  /// @param  num_origin_rules  The number of entries in `origin_rules`.
  ///
  /// @return  Returns true on success, or false if `context` is NULL, a flag is unknown, or a
  ///          rule failed to parse (nothing changes then, and a logged warning says why).
  ///
  /// \parblock
  /// @note  The origin policy is shared with AttachJSAPI(): same rule syntax, same default,
  ///        evaluated against each page's security origin. A page that isn't allowed compiles
  ///        no binding markup: it shows its authored content, and none of its controls stage
  ///        changes or fire actions. A data context has no injection filter, so the origin
  ///        rules alone decide which pages get it.
  /// \endparblock
  ///
  /// \parblock
  /// @note  Safe to call from any thread, including the context's home thread (origin rules
  ///        parse on the calling thread). Unlike most View methods this only records intent, the
  ///        attachment takes effect on the Renderer's thread during the next update.
  /// \endparblock
  ///
  virtual bool AttachDOMDataContext(ULDOMDataContext context, unsigned flags = 0,
                                    const char* const* origin_rules = nullptr,
                                    size_t num_origin_rules = 0) = 0;

  ///
  /// Detach a data-binding context from this View.
  ///
  /// Its pages stop receiving updates, and the page's binding markup recompiles against the
  /// remaining attached contexts.
  ///
  /// @param  context  The context to detach.
  ///
  /// @note  Safe to call from any thread. The attachment is removed immediately, and the page
  ///        recompiles on the Renderer's thread during its next update.
  ///
  virtual void DetachDOMDataContext(ULDOMDataContext context) = 0;

  ///
  /// Callback that decides whether an attached API's bindings are added to a loading page.
  ///
  /// This is called on the Renderer's thread after the origin rules have been evaluated. See
  /// SetJSAPIInjectionFilter() and `<Ultralight/CAPI/CAPI_JSAPI.h>`. Most C++ code uses
  /// js::SetInjectionFilter() instead.
  ///
  /// @param  user_data  The user data passed to SetJSAPIInjectionFilter().
  ///
  /// @param  request    The API, the page's origin, and the verdict of the origin rules (see
  ///                    ULJSAPIInjectionRequest). It's owned by the library and valid only
  ///                    during the callback. Its `view` member is NULL here (it's set only for a
  ///                    filter set with ulViewSetJSAPIInjectionFilter()).
  ///
  /// @return  Return true to add the bindings to this page, or false to withhold them. The
  ///          return value is final: it can override the origin rules either way.
  ///
  using JSAPIInjectionFilter = bool (*)(void* user_data, const ULJSAPIInjectionRequest* request);

  ///
  /// Attach a JavaScript API to this View.
  ///
  /// A ULJSAPI (see `<Ultralight/CAPI/CAPI_JSAPI.h>`) is a set of native bindings under a global
  /// namespace such as `myApp`.
  ///
  /// Once attached, the bindings are added to the current page (if any) and to every page the
  /// View navigates to afterwards, so you don't need to register them again after a navigation:
  ///
  /// ```
  /// ULJSAPI api = ulCreateJSAPI("myApp");
  /// ulJSAPIBindFunction(api, "greet", OnGreet, nullptr, nullptr);
  /// view->AttachJSAPI(api);
  /// // Page script on every admitted page:  myApp.greet()
  /// ```
  ///
  /// The View keeps the API attached while an owning handle to it exists (see
  /// ulDestroyJSAPI()). Attaching it again updates its flags and origin rules.
  ///
  /// @param  api               The API to attach.
  ///
  /// @param  flags             A logically ORed set of ULJSAPIAttachFlags. Unknown bits fail
  ///                           the attach.
  ///
  /// @param  origin_rules      An array of `scheme://host[:port]` origin patterns for the pages
  ///                           that get the bindings (eg, `https://*.mygame.com`), or nullptr
  ///                           for the default policy (local pages plus your application's own
  ///                           content, ie. pages loaded with LoadHTML()). See
  ///                           ultralight::OriginRules for the rule syntax.
  ///
  /// @param  num_origin_rules  The number of entries in `origin_rules`.
  ///
  /// @return  Returns true on success, or false if `api` is NULL, a flag is unknown, or a rule
  ///          failed to parse (nothing changes then, and a logged warning says why).
  ///
  /// \parblock
  /// @note  APIs that share a namespace prefix (eg, `myApp.fs` and `myApp.net`) should all be
  ///        attached before the page loads. An API attached later can't add to a namespace the
  ///        page already has, so its bindings appear after the page's next navigation instead
  ///        (see ulViewAttachJSAPI()).
  /// \endparblock
  ///
  /// \parblock
  /// @note  Without kULJSAPIAttachFlags_AllFrames, the bindings are added to the main frame
  ///        only.
  /// \endparblock
  ///
  /// \parblock
  /// @note  A page restored from the back-forward cache (Config::page_cache_size > 0) resumes
  ///        with working bindings: the API objects it kept (and any function references its
  ///        scripts held) become callable again, and native events reach the page. Page-side
  ///        event subscriptions made with the `on` mixin do **not** survive the round trip, so
  ///        page scripts should re-subscribe in a `pageshow` handler.
  /// \endparblock
  ///
  virtual bool AttachJSAPI(ULJSAPI api, unsigned flags = 0,
                           const char* const* origin_rules = nullptr,
                           size_t num_origin_rules = 0) = 0;

  ///
  /// Detach a JavaScript API from this View.
  ///
  /// Functions and objects already added to the current page remain until the next navigation,
  /// but event delivery to this View stops immediately.
  ///
  /// @param  api  The API to detach.
  ///
  virtual void DetachJSAPI(ULJSAPI api) = 0;

  ///
  /// Set a filter that decides whether an attached API's bindings are added to a page.
  ///
  /// The filter is consulted after the origin rules, for every attached API and page.
  ///
  /// @param  filter             The filter callback. Pass nullptr to remove the current filter.
  ///
  /// @param  user_data          User data passed through to the filter on every call.
  ///
  /// @param  destroy_user_data  Callback invoked exactly once to destroy `user_data` after the
  ///                            filter is replaced or the View is destroyed (may be nullptr).
  ///
  /// @see  js::SetInjectionFilter()
  ///
  virtual void SetJSAPIInjectionFilter(JSAPIInjectionFilter filter, void* user_data,
                                       void (*destroy_user_data)(void*)) = 0;

  ///
  /// Whether or not the View can navigate back in history.
  ///
  virtual bool CanGoBack() = 0;

  ///
  /// Whether or not the View can navigate forward in history.
  ///
  virtual bool CanGoForward() = 0;

  ///
  /// Navigate backwards in history.
  ///
  virtual void GoBack() = 0;

  ///
  /// Navigate forwards in history.
  ///
  virtual void GoForward() = 0;

  ///
  /// Navigate to an arbitrary offset in history.
  ///
  /// @param  offset  The number of entries to move (negative goes back, positive goes forward).
  ///
  virtual void GoToHistoryOffset(int offset) = 0;

  ///
  /// Reload current page.
  ///
  virtual void Reload() = 0;

  ///
  /// Stop all page loads.
  ///
  virtual void Stop() = 0;

  ///
  /// Give focus to the View.
  ///
  /// You should call this to give visual indication that the View has input focus (changes active
  /// text selection colors, for example). The page gets a window `focus` event, and so does the
  /// element that had focus before Unfocus().
  ///
  virtual void Focus() = 0;

  ///
  /// Remove focus from the View.
  ///
  /// You should call this to give visual indication that the View has lost input focus. The page
  /// gets a window `blur` event.
  ///
  /// @note  The page's focused element gets a blur event but stays focused in the document, and
  ///        shows focus again the next time you call Focus().
  ///
  virtual void Unfocus() = 0;

  ///
  /// Whether or not the View has focus.
  ///
  virtual bool HasFocus() = 0;

  ///
  /// Whether or not the View has an input element with visible keyboard focus (indicated by a
  /// blinking caret).
  ///
  /// You can use this to decide whether or not the View should consume keyboard input events
  /// (useful in games with mixed UI and key handling).
  ///
  /// @note  This reports text-editing focus specifically: a focused text field, text area, or
  ///        editable (contenteditable) region that can accept typed input. Focused elements that
  ///        consume keys without editing text (eg, a select or checkbox) report false, as do
  ///        read-only text fields, and so does everything while the View itself is unfocused
  ///        (see Unfocus()).
  ///
  virtual bool HasInputFocus() = 0;

  ///
  /// Get the Editor for the View (executes editing commands against the focused frame).
  ///
  /// You can use this to drive text editing natively: caret motion, selection, deletion,
  /// clipboard operations, undo/redo, and typed text insertion (the commands a native Edit menu or
  /// keybinding table needs).
  ///
  /// @return  Returns the View's Editor instance (owned by the View, this is never NULL).
  ///
  /// @see  Editor
  ///
  virtual Editor* editor() = 0;

  ///
  /// Fire a keyboard event.
  ///
  /// @param  evt  The key event.
  ///
  /// @note  KeyEvent::kType_Char events insert text into input fields, and so does a legacy
  ///        KeyEvent::kType_KeyDown that carries text. KeyEvent::kType_RawKeyDown never inserts
  ///        text.
  ///
  virtual void FireKeyEvent(const KeyEvent& evt) = 0;

  ///
  /// Fire a mouse event.
  ///
  /// @param  evt  The mouse event.
  ///
  virtual void FireMouseEvent(const MouseEvent& evt) = 0;

  ///
  /// Fire a scroll event.
  ///
  /// @param  evt  The scroll event.
  ///
  virtual void FireScrollEvent(const ScrollEvent& evt) = 0;

  ///
  /// Set a ViewListener to receive callbacks for View-related events.
  ///
  /// @param  listener  A user-defined ViewListener implementation, ownership remains with the
  ///                   caller. Pass a nullptr to remove the current listener.
  ///
  virtual void set_view_listener(ViewListener* listener) = 0;

  ///
  /// Get the active ViewListener (can be nullptr).
  ///
  virtual ViewListener* view_listener() const = 0;

  ///
  /// Set a LoadListener to receive callbacks for Load-related events.
  ///
  /// @param  listener  A user-defined LoadListener implementation, ownership remains with the
  ///                   caller. Pass a nullptr to remove the current listener.
  ///
  virtual void set_load_listener(LoadListener* listener) = 0;

  ///
  /// Get the active LoadListener (can be nullptr).
  ///
  virtual LoadListener* load_listener() const = 0;

  ///
  /// Set a DownloadListener to receive callbacks for download-related events.
  ///
  /// @param  listener  A user-defined DownloadListener implementation, ownership remains with the
  ///                   caller. Pass a nullptr to remove the current listener.
  ///
  virtual void set_download_listener(DownloadListener* listener) = 0;

  ///
  /// Get the active DownloadListener (can be nullptr).
  ///
  virtual DownloadListener* download_listener() const = 0;

  ///
  /// Cancel an active download.
  ///
  /// No more data arrives for the download, and DownloadListener::OnFailDownload() is called for
  /// it during a later Renderer::Update().
  ///
  /// @param  id  The id of the download to cancel (see DownloadListener).
  ///
  virtual void CancelDownload(DownloadId id) = 0;

  ///
  /// Set an EditorListener to receive callbacks for editing-related events.
  ///
  /// @param  listener  A user-defined EditorListener implementation, ownership remains with the
  ///                   caller. Pass a nullptr to remove the current listener.
  ///
  /// @note  An AppCore panel uses its View's editor listener for input method support and
  ///        Window::OnEditableStateChange(). Replacing it on a hosted View turns both off for
  ///        that View.
  ///
  virtual void set_editor_listener(EditorListener* listener) = 0;

  ///
  /// Get the active EditorListener (can be nullptr).
  ///
  virtual EditorListener* editor_listener() const = 0;

  ///
  /// Set a NetworkListener to receive callbacks for network-related events.
  ///
  /// @param  listener  A user-defined NetworkListener implementation, ownership remains with the
  ///                   caller. Pass a nullptr to remove the current listener.
  ///
  /// @pre  Not available in the Free edition (the listener is never called there).
  ///
  virtual void set_network_listener(NetworkListener* listener) = 0;

  ///
  /// Get the active NetworkListener (can be nullptr).
  ///
  virtual NetworkListener* network_listener() const = 0;

  ///
  /// Set whether or not this View should be repainted during the next call to Renderer::Render().
  ///
  /// @param  needs_paint  Whether or not the View needs a repaint.
  ///
  /// @note  The library sets this flag automatically when a repaint is due: after a
  ///        Renderer::RefreshDisplay() call that produces animating or changed content, on
  ///        resize, when the View is shown, and when the page reacts to user input. You can also
  ///        set it directly to force a repaint (it still waits for max_render_fps()).
  ///
  virtual void set_needs_paint(bool needs_paint) = 0;

  ///
  /// Whether or not this View should be repainted during the next call to Renderer::Render().
  ///
  /// When this returns false, rendering would reproduce the previous frame, so you can skip
  /// rendering or presenting the View. This is always false while the View is hidden, and while
  /// max_render_fps() holds back its next frame.
  ///
  /// When this returns true, the next call to Renderer::Render() repaints the View. The resulting
  /// frame may still be visually identical to the previous frame.
  ///
  /// @note  Continue calling Renderer::RefreshDisplay() on every display refresh regardless of
  ///        this flag. A View whose only pending work is animation callbacks or CSS animations
  ///        only requests a repaint after that call.
  ///
  virtual bool needs_paint() const = 0;

  ///
  /// Create an Inspector View to inspect / debug this View locally.
  /// 
  /// This will only succeed if you have the inspector assets in your filesystem-- the inspector
  /// will look for file:///inspector/Main.html when it first loads.
  /// 
  /// You must handle ViewListener::OnCreateInspectorView() so that the library has a View to
  /// display the inspector in. This function will call this event only if an inspector view is
  /// not currently active.
  ///
  virtual void CreateLocalInspectorView() = 0;

  ///
  /// Set whether or not to display compositor debug information, specifically:
  ///  - Visualize compositor layers and tile boundaries
  ///  - Display repaint counters for each layer
  ///
  /// @param  enable  Whether or not to show the debug information.
  ///
  /// @note  This is only valid when the compositor is enabled.
  ///
  /// @see  ViewConfig::enable_compositor
  ///
  virtual void set_compositor_debug_info_enabled(bool enable) = 0;

  ///
  /// Whether or not compositor debug information is enabled.
  ///
  virtual bool compositor_debug_info_enabled() const = 0;

  ///
  /// Set whether or not this View is visible.
  ///
  /// Hiding a View (passing false) stops it from being painted during Renderer::Render and pauses
  /// its rendering-driven work: requestAnimationFrame callbacks and CSS animations stop advancing
  /// while hidden, and the page's `visibilitychange` event fires with `document.visibilityState`
  /// set to "hidden".
  ///
  /// Showing a View again (passing true) resumes its animations and forces a repaint on the next
  /// call to Renderer::Render.
  ///
  /// @param  visible  Whether or not the View is visible.
  ///
  /// \parblock
  /// @note  Views are visible by default.
  /// \endparblock
  ///
  /// \parblock
  /// @note  JavaScript timers (setTimeout / setInterval) keep running while hidden unless
  ///        ViewConfig::enable_hidden_timer_throttling was set when the View was created.
  /// \endparblock
  ///
  virtual void set_visible(bool visible) = 0;

  ///
  /// Whether or not this View is visible.
  ///
  /// @see  set_visible()
  ///
  virtual bool is_visible() const = 0;

  ///
  /// Set the maximum rate, in frames per second, at which this View advances its animations and
  /// repaints.
  ///
  /// @param  fps  The frame-rate cap, or 0 to remove it (the View then advances at the
  ///              display's refresh rate).
  ///
  /// @see  ViewConfig::max_render_fps
  ///
  virtual void set_max_render_fps(uint32_t fps) = 0;

  ///
  /// Get the frame-rate cap set for this View, or 0 if none is set.
  ///
  /// @see  set_max_render_fps()
  ///
  virtual uint32_t max_render_fps() const = 0;

  ///
  /// Set the color scheme this View reports to pages via the `prefers-color-scheme` CSS
  /// media feature.
  ///
  /// Overrides ViewConfig::preferred_color_scheme for this View. A change re-evaluates
  /// `prefers-color-scheme` media queries on the page, firing `matchMedia` change listeners and
  /// recalculating styles, the same as an OS theme change.
  ///
  /// @param  scheme  The new scheme. Auto follows the system value set via
  ///                 Renderer::set_system_color_scheme().
  ///
  virtual void set_preferred_color_scheme(ColorScheme scheme) = 0;

  ///
  /// The color scheme this View reports to pages.
  ///
  /// @see  set_preferred_color_scheme()
  ///
  virtual ColorScheme preferred_color_scheme() const = 0;

#if UL_HAS(TESTING)
  /// \cond ignore
  virtual String RenderTreeAsString(uint32_t flags = 0) = 0;
  virtual String PageSource() = 0;
  virtual String DumpAsText(bool dump_child_frames = true) = 0;
  virtual uint32_t damage_verify_mismatches() const = 0;
  virtual IntRect damage_verify_last_bounds() const = 0;
  virtual RenderTarget damage_verify_render_target() const = 0;
  /// \endcond
#endif

 protected:
  virtual ~View();
};

} // namespace ultralight
