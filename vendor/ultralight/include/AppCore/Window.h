///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include "Defines.h"
#include "Layout.h"
#include <Ultralight/Color.h>
#include <Ultralight/Geometry.h>
#include <Ultralight/RefPtr.h>
#include <Ultralight/Listener.h>
#include <Ultralight/Bitmap.h>
#include <Ultralight/KeyEvent.h>
#include <Ultralight/MouseEvent.h>
#include <Ultralight/ScrollEvent.h>
#include <Ultralight/RenderTarget.h>

// X11's headers define None as a macro, which would break the None enumerators.
#pragma push_macro("None")
#undef None

namespace ultralight {

class LayoutEngine;
class Monitor;
class Surface;
class View;
class Window;

///
/// The native backdrop materials a window can use. @see  Window::SetBackdrop()
///
/// Choose the material that matches what your window is (a main window, a menu, or an
/// overlay). The OS decides how each one looks.
///
enum class BackdropMaterial : uint8_t {
  None    = 0,  ///< No material.
  Window  = 1,  ///< For a main window.
  Popup   = 2,  ///< For menus, dropdowns, and other flyouts.
  Overlay = 3,  ///< For heads-up panels and on-screen displays (eg, a volume indicator).
};

///
/// Whether a backdrop material shows its active or inactive look.
/// @see  Window::SetBackdropState()
///
enum class BackdropState : uint8_t {
  FollowWindow = 0,  ///< Match the window: the active look while the window is active.
  Active       = 1,  ///< Always show the active look.
  Inactive     = 2,  ///< Always show the inactive look.
};

///
/// The look of a backdrop material. @see  Window::SetBackdrop()
///
enum class BackdropVariant : uint8_t {
  Auto    = 0,  ///< The platform's usual look for the material.
  Frosted = 1,  ///< Translucent: a live blur of what's behind the window.
  Matte   = 2,  ///< Opaque: tinted by the desktop wallpaper and theme.
};

///
/// The light or dark look of a backdrop material. @see  Window::SetBackdrop()
///
enum class BackdropTheme : uint8_t {
  Auto  = 0,  ///< Follow the OS theme.
  Light = 1,  ///< Always use the light look.
  Dark  = 2,  ///< Always use the dark look.
};

///
/// The style of an opaque BackdropMaterial::Window. @see  Window::SetBackdrop()
///
/// Only Windows has more than one: most windows use the standard style, and Windows' own
/// tabbed apps (eg, File Explorer) use a more colorful one. Other platforms ignore this.
///
enum class BackdropRendition : uint8_t {
  Auto     = 0,  ///< The platform default (Standard).
  Standard = 1,  ///< The style most windows use.
  Vivid    = 2,  ///< The more colorful style, with more of the wallpaper's color.
};

///
/// Optional settings for a backdrop material. @see  Window::SetBackdrop()
///
struct BackdropOptions {
  ///
  /// The material's look. If the platform can't render it, the library uses the nearest
  /// look it can.
  ///
  BackdropVariant variant = BackdropVariant::Auto;

  ///
  /// The material's light or dark look.
  ///
  /// @note  On Windows and macOS, this also sets the light or dark look of the window's
  ///        title bar.
  ///
  BackdropTheme theme = BackdropTheme::Auto;

  ///
  /// The color of the fallback fill, shown where no live material can render (and on
  /// Windows, as the material's inactive look).
  ///
  /// Leave it unset to use a color from the theme. The alpha channel is ignored, and the
  /// window background color still draws over it.
  ///
  Color fallback_color;

  ///
  /// The style of an opaque BackdropMaterial::Window (see BackdropRendition).
  ///
  BackdropRendition rendition = BackdropRendition::Auto;

  friend constexpr bool operator==(const BackdropOptions&, const BackdropOptions&) = default;
};

///
/// The window's current display state. @see  Window::is_maximized(), Window::is_minimized(),
/// WindowListener::OnWindowStateChanged()
///
enum class WindowState : uint8_t {
  Normal    = 0,  ///< Neither minimized nor maximized.
  Minimized = 1,
  Maximized = 2,
};

///
/// The window's corner style. @see  Window::SetCornerStyle()
///
/// By default, framed windows, custom-chrome windows, and popups get rounded corners, and
/// other transparent windows get square ones. Linux leaves corners to the window manager.
///
/// @note  On windows that draw their own frame (custom-chrome and transparent windows), the
///        system shadow comes with the rounded corners: squaring them removes the shadow too.
///
enum class CornerStyle : uint8_t {
  Default = 0,  ///< The platform's usual corners for this kind of window.
  Rounded = 1,  ///< Rounded corners.
  Square  = 2,  ///< Square corners.
};

///
/// Interface for all Window-related events. @see  Window::set_listener()
///
class WindowListener {
public:
  virtual ~WindowListener() {}

  ///
  /// Called when the Window is closed.
  ///
  /// @param  window  The window that was closed.
  ///
  virtual void OnClose(ultralight::Window* window) { }

  ///
  /// Called when the Window is resized.
  ///
  /// @param  window  The window that was resized.
  ///
  /// @param  width   The new width (in logical pixels).
  ///
  /// @param  height  The new height (in logical pixels).
  ///
  virtual void OnResize(ultralight::Window* window, double width, double height) { }

  ///
  /// Called when the window becomes normal, minimized, or maximized, however the change was
  /// made (eg, Window::Maximize(), the caption buttons, or a snap gesture).
  ///
  /// If you draw your own window chrome, you can swap the maximize/restore icon here.
  ///
  /// @param  window  The window whose state changed.
  ///
  /// @param  state   The state the window is now in.
  ///
  /// @note  This fires after OnResize(), so the window's new size is already set.
  ///
  virtual void OnWindowStateChanged(ultralight::Window* window, WindowState state) { }

  ///
  /// Called when the window becomes, or stops being, the active window (the one the user is
  /// working in).
  ///
  /// If your window chrome dims while the window is inactive, you can restyle it here.
  ///
  /// @param  window  The window whose activation changed.
  ///
  /// @param  active  Whether or not the window is now active.
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
  virtual void OnActivationChanged(ultralight::Window* window, bool active) { }

  ///
  /// Called when a hidden window is shown, by Window::Show() or when a
  /// Window::ShowWhenReady() wait ends.
  ///
  /// You can start work here that only needs to run while the window is on screen (eg,
  /// animations or a live data feed).
  ///
  /// @param  window  The window that was shown.
  ///
  /// @note  This doesn't fire for a window created visible, or for a Show() call on a window
  ///        that is already visible.
  ///
  virtual void OnShow(ultralight::Window* window) { }

  ///
  /// Called when a visible window is hidden, by Window::Hide() or by the library (see
  /// OnDismiss()).
  ///
  /// @param  window  The window that was hidden.
  ///
  /// \parblock
  /// @note  Minimizing or closing the window doesn't fire this (see OnWindowStateChanged()
  ///        and OnClose()).
  /// \endparblock
  ///
  /// \parblock
  /// @note  The popups this window owns are hidden first, so their OnHide() fires before
  ///        this window's.
  /// \endparblock
  ///
  virtual void OnHide(ultralight::Window* window) { }

  ///
  /// Called when the window receives a key event, before any page does.
  ///
  /// @param  window  The window that received the event.
  ///
  /// @param  evt     Details for the event.
  ///
  /// @return  Return false to consume the event and prevent it from propagating further.
  ///
  virtual bool OnKeyEvent(ultralight::Window* window, const ultralight::KeyEvent& evt) {
    return true;
  }

  ///
  /// Called when the window receives a mouse event, before any page does.
  ///
  /// @param  window  The window that received the event.
  ///
  /// @param  evt     Details for the event.
  ///
  /// @return  Return false to consume the event and prevent it from propagating further.
  ///
  virtual bool OnMouseEvent(ultralight::Window* window, const ultralight::MouseEvent& evt) {
    return true;
  }

  ///
  /// Called when the window receives a scroll event, before any page does.
  ///
  /// @param  window  The window that received the event.
  ///
  /// @param  evt     Details for the event.
  ///
  /// @return  Return false to consume the event and prevent it from propagating further.
  ///
  virtual bool OnScrollEvent(ultralight::Window* window, const ultralight::ScrollEvent& evt) {
    return true;
  }

  ///
  /// Called after the window background is drawn, before any View is drawn.
  ///
  /// You can use this to draw custom content beneath every View. When you do:
  ///
  ///  - draw through the GPUDriver interface, into the window's render buffer (on
  ///    CPU-rendered windows, draw on the native window surface instead)
  ///  - don't count on any GPU state you set lasting past the callback
  ///  - don't present the frame
  ///
  /// To animate your content, call Window::Invalidate() to schedule the next frame.
  ///
  /// @param  window  The window being drawn.
  ///
  /// @note  On Windows, drawing from this callback has no effect on transparent CPU-rendered
  ///        windows (their pixels come from an internal surface you can't draw on).
  ///
  virtual void OnClear(ultralight::Window* window) { }

  ///
  /// Called after every View is drawn, before the frame is presented.
  ///
  /// You can use this to draw custom content above every View. The rules and the note on
  /// OnClear() apply here too.
  ///
  /// @param  window  The window being drawn.
  ///
  virtual void OnPaint(ultralight::Window* window) { }

  ///
  /// Called when the backdrop material in effect changes after Window::SetBackdrop() (eg,
  /// the user turns off transparency effects, or the material's look changes).
  ///
  /// @param  window     The window whose backdrop changed.
  ///
  /// @param  effective  The material now in effect (BackdropMaterial::None when none
  ///                    renders). If it's the same as before, the look changed instead:
  ///                    read Window::effective_backdrop_variant().
  ///
  /// @note  This doesn't fire for the initial result: read Window::effective_backdrop()
  ///        right after Window::SetBackdrop() for that.
  ///
  virtual void OnBackdropChanged(ultralight::Window* window, BackdropMaterial effective) { }

  ///
  /// Called when the library hides this window for you: through its auto-dismiss policy
  /// (see Window::SetDismiss()), or because the window that owns it was hidden.
  ///
  /// This never fires for the window you hide yourself. It fires after OnHide().
  ///
  /// @param  window  The window being dismissed.
  ///
  /// @note  Hiding a popup yourself still fires this for the popups it owns, since the
  ///        library hides those for you.
  ///
  virtual void OnDismiss(ultralight::Window* window) { }
};

///
/// Window creation flags (combine with `|`). @see  Window::Create()
///
enum class WindowFlags : uint16_t {
  None         = 0,
  Borderless   = 1 << 0,  ///< No frame or title bar.
  Titled       = 1 << 1,  ///< A title bar.
  Resizable    = 1 << 2,  ///< The user can resize the window by dragging its edges.
  Maximizable  = 1 << 3,  ///< Minimize and maximize buttons.
  Hidden       = 1 << 4,  ///< Start hidden (call Window::Show() to show it).

  ///
  /// Per-pixel transparency: anything the window leaves transparent shows what's behind it.
  /// You can only set this when you create the window.
  ///
  /// The View and the page need to be transparent too (pages draw on an opaque white
  /// background by default). See the Transparent Windows section in the Window class
  /// overview for the full setup.
  ///
  /// \parblock
  /// @note  On Windows, transparent CPU-rendered windows are always borderless (the frame
  ///        flags are ignored with a warning), and drawing from WindowListener::OnClear()
  ///        or WindowListener::OnPaint() has no effect on them.
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
  ///        Square corners remove it (see Window::SetCornerStyle()).
  /// \endparblock
  ///
  Transparent  = 1 << 5,

  ///
  /// Marks a popup window. Window::CreatePopup() sets this for you (Window::Create()
  /// ignores it with a warning).
  ///
  /// Popups are always borderless and transparent, and ignore CustomChrome.
  ///
  Popup        = 1 << 6,

  ///
  /// Draw the title bar and caption buttons yourself, as page content. The window keeps
  /// every native frame behavior (resizing, snapping, the system shadow). See the Custom
  /// Chrome section in the Window class overview for the full setup.
  ///
  /// You don't need Titled with this. It's ignored (with a warning) together with
  /// Borderless, and on popup and fullscreen windows. Window::SetCaptionColor() has no
  /// effect, since there's no system title bar to color.
  ///
  /// \parblock
  /// @note  On macOS, the native window buttons stay and float over your content; use
  ///        Window::window_control_bounds() to leave room for them. Right-clicking a
  ///        caption region sends the click to the page (on Windows it opens the system
  ///        window menu).
  /// \endparblock
  ///
  /// \parblock
  /// @note  On Linux, this needs X11 or XWayland (native Wayland isn't supported), and
  ///        there's no system shadow.
  /// \endparblock
  ///
  CustomChrome = 1 << 7,
};

/// \cond ignore
consteval void ul_enable_bitmask_operators(WindowFlags);
/// \endcond

///
/// What a hit-test region does. @see  Window::SetHitTestRegions()
///
enum class HitRegionRole : uint8_t {
  ///
  /// A draggable title-bar area. Dragging moves the window, and double-clicking does what
  /// double-clicking a title bar does on the platform (usually maximize/restore). The page
  /// under it gets no mouse input.
  ///
  Caption  = 0,

  Minimize = 1,  ///< Your minimize button.

  ///
  /// Your maximize/restore button. On Windows 11, hovering it also shows the snap layouts
  /// flyout.
  ///
  Maximize = 2,

  Close    = 3,  ///< Your close button.
};

///
/// An area of a custom-chrome window and what it does. @see  Window::SetHitTestRegions()
///
struct HitTestRegion {
  HitRegionRole role;  ///< What the region does.
  Rect rect;           ///< The region's bounds (in logical pixels, window content coordinates).
};

///
/// The edge or corner to resize a window from. @see  Window::BeginDragResize()
///
enum class ResizeEdge : uint8_t {
  Left = 0,
  Top,
  Right,
  Bottom,
  TopLeft,
  TopRight,
  BottomLeft,
  BottomRight,
};

///
/// The window actions available to the user (combine with `|`).
/// @see  Window::SetWindowButtons()
///
enum class WindowButtons : uint8_t {
  None     = 0,
  Minimize = 1 << 0,
  Maximize = 1 << 1,
  Close    = 1 << 2,
  All      = 0x7,
};

/// \cond ignore
consteval void ul_enable_bitmask_operators(WindowButtons);
/// \endcond


///
/// A native OS window that displays web content.
///
/// A window holds one or more panels, each showing a View, and passes mouse, keyboard, and
/// scroll input to them. It can also use native materials, draw its own title bar, and show
/// popups for menus and dropdowns.
///
/// ## Creating a Window
///
/// Use Create():
///
/// ```
/// auto window = Window::Create(monitor, 1024, 768, false, WindowFlags::Titled);
/// ```
///
/// ## Setting a WindowListener
///
/// To receive callbacks for window-related events, set a WindowListener:
///
/// ```
/// class MyWindowListener : public WindowListener {
///   virtual void OnClose(Window* window) override {
///     printf("Window closed!\n");
///   }
/// };
///
/// auto listener = new MyWindowListener();
/// window->set_listener(listener);
/// ```
///
/// ## Displaying Web Content
///
/// A window's content is a layout of panels, each showing a View. Panels tile the window
/// (see layout()), and floating panels can sit above them (see foreground()). A bare
/// AddPanel() fills the whole window with one page:
///
/// ```
/// window->AddPanel()->view()->LoadURL("file:///app.html");
/// ```
///
/// ## Coordinate Systems
///
/// The OS may apply a device scale to a monitor or window (eg, 2x on a Retina display).
///
/// Sizes and positions in this API are in logical pixels (independent of the device scale),
/// unless the name says device (eg, device_width()). The two are related by scale():
///
/// ```
/// device_pixels = round(logical_pixels * scale)
/// ```
///
/// Use LogicalToDevice() and DeviceToLogical() to convert between them.
///
/// Positions inside the window (mouse events, the layout, hit-test regions) are in window
/// content coordinates: logical pixels from the top-left of the window's content area.
///
/// ## Popup Windows
///
/// Use CreatePopup() to show menus, dropdowns, and other short-lived content in their own
/// OS window. Clicking a popup doesn't take focus away from your main window, a popup can
/// extend past its owner's edges, and the library dismisses it the way the OS dismisses a
/// native menu (see SetDismiss()).
///
/// ```
/// // Show a context menu at (x, y) within the owner's content area:
/// auto menu = Window::CreatePopup(window, x, y, 220, 320);
/// menu->SetBackdrop(BackdropMaterial::Popup);
///
/// ViewConfig config;
/// config.is_transparent = true;
/// menu->AddPanel({}, config)->view()->LoadURL("file:///menu.html");
/// ```
///
/// Use a popup for anything that may extend past the window (most menus and dropdowns),
/// and a floating panel (see foreground()) for anything that stays inside it (a toast, an
/// in-window dialog).
///
/// ## Dialogs
///
/// For a native message box, use ShowMessageBox() in `<AppCore/Dialogs.h>`. For a dialog
/// drawn with your own HTML, use a floating panel (see foreground()) with
/// `FocusPolicy::Grab`, so it takes keyboard focus when shown.
///
/// ## Transparent Windows
///
/// A window created with WindowFlags::Transparent shows what's behind it wherever you leave
/// it transparent. You can use this for custom window shapes (eg, soft edges or irregular
/// outlines).
///
/// Three layers need to be transparent for the window to be see-through:
///
///  1. The window: create it with WindowFlags::Transparent.
///  2. The View: set `is_transparent` in the panel's ViewConfig (pages draw on an opaque
///     white background by default).
///  3. The page: `html, body { background: transparent; }`.
///
/// ```
/// auto window = Window::Create(monitor, 400, 300, false,
///                              WindowFlags::Borderless | WindowFlags::Transparent);
///
/// ViewConfig config;
/// config.is_transparent = true;
/// window->AddPanel({}, config)->view()->LoadURL("file:///shaped.html");
/// ```
///
/// A transparent window has no background color until you set one (see
/// SetBackgroundColor()).
///
/// ## Backdrop Materials
///
/// A window can show a native backdrop material behind its content (the blurred,
/// theme-aware materials the OS uses for its own windows). Request one with SetBackdrop(),
/// and tint it with a translucent background color:
///
/// ```
/// auto window = Window::Create(monitor, 1024, 768, false,
///                              WindowFlags::Titled | WindowFlags::Resizable);
/// window->SetBackdrop(BackdropMaterial::Window);
/// window->SetBackgroundColor("rgba(32, 32, 32, 0.5)");
/// ```
///
/// The material shows wherever your content is transparent, so the View and the page need
/// to be transparent (steps 2 and 3 in Transparent Windows above). If the platform can't
/// render the material, the library uses the closest look it can (see SetBackdrop()).
///
/// ## Custom Chrome
///
/// To draw the title bar and caption buttons yourself (in HTML and CSS, like the rest of
/// your UI), create the window with WindowFlags::CustomChrome. Your content covers the
/// whole window, and the OS keeps its frame behaviors:
///
///  - the system shadow, rounded corners, and window animations
///  - the resize edges
///  - snap gestures and the system window menu
///
/// To build one:
///
///  1. Create the window with WindowFlags::CustomChrome.
///  2. Draw the title bar and caption buttons as ordinary page content.
///  3. Mark which areas drag the window and which act as caption buttons, either with
///     SetHitTestRegions() or from the page with the `app-region` CSS property
///     (`app-region: drag` / `no-drag`).
///
/// ```
/// auto window = Window::Create(monitor, 1024, 768, false,
///     WindowFlags::CustomChrome | WindowFlags::Resizable | WindowFlags::Maximizable);
///
/// // The top 40px drags the window; your close button sits on the right.
/// HitTestRegion regions[] = {
///   { HitRegionRole::Caption, Rect::FromXYWH(0, 0, window->width(), 40) },
///   { HitRegionRole::Close,   Rect::FromXYWH(window->width() - 46, 0, 46, 40) },
/// };
/// window->SetHitTestRegions(regions, 2);
/// ```
///
/// Use SetWindowButtons() to choose which window actions are available. To restyle your
/// chrome as the window changes, use WindowListener::OnWindowStateChanged() and
/// WindowListener::OnActivationChanged().
///
/// ## Text Input
///
/// The window handles the OS input method for you, so Chinese, Japanese, and Korean input
/// works in your pages without any code of your own. If you want to drive something else
/// from the same signal (eg, an on-screen keyboard), use OnEditableStateChange().
///
/// @note  Password fields keep the input method off. Moving focus away during a composition
///        commits the typed text on macOS and cancels it on Windows, matching each
///        platform's own apps.
///
class AExport Window : public RefCounted {
public:
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
    /// @param  window_flags  How the window looks and behaves (see WindowFlags).
    ///
    /// @return  Returns a ref-pointer to a new Window instance.
    ///
    /// @note  The window is shown right away unless you pass WindowFlags::Hidden (call Show()
    ///        to show it later).
    ///
    static RefPtr<Window> Create(Monitor* monitor, double width, double height,
        bool fullscreen, WindowFlags window_flags);

    ///
    /// Create a popup window owned by another window.
    ///
    /// Popups are borderless, transparent windows that stay above their owner and don't
    /// appear in the taskbar. Clicking one doesn't take focus away from the owner, and the
    /// library dismisses it like a native menu (see SetDismiss()). Use them for menus,
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
    /// @param  window_flags  Extra window flags (eg, WindowFlags::Hidden for a popup you
    ///                       create once and show when needed).
    ///
    /// @return  Returns a ref-pointer to a new Window instance, or null if `owner` is null.
    ///
    /// \parblock
    /// @note  Popups always position relative to their owner's content area (this includes
    ///        MoveTo(), x(), and y()), so you can place one at an element's position in the
    ///        owner's page without converting it. The position stays correct when the
    ///        owner's device scale changes.
    /// \endparblock
    ///
    /// \parblock
    /// @note  When the owner closes, its popups close with it (WindowListener::OnClose()
    ///        fires for each). The popup handle stays valid, but Show() and MoveTo() then
    ///        only log a warning, so create reusable popups on a window that outlives them.
    /// \endparblock
    ///
    /// \parblock
    /// @note  Popups get rounded corners and a drop shadow by default. For a sharp,
    ///        shadow-free popup, call SetCornerStyle() with CornerStyle::Square.
    /// \endparblock
    ///
    /// @see  SetBackdrop()
    ///
    static RefPtr<Window> CreatePopup(RefPtr<Window> owner, double x, double y, double width,
        double height, WindowFlags window_flags = WindowFlags::None);

    ///
    /// Set a WindowListener to receive callbacks for window-related events.
    ///
    /// @param  listener  A user-defined WindowListener implementation, ownership remains with
    ///                   the caller. Pass a nullptr to remove the current listener.
    ///
    virtual void set_listener(WindowListener* listener) = 0;

    ///
    /// Get the WindowListener (can be nullptr).
    ///
    virtual WindowListener* listener() = 0;

    ///
    /// Get the window width (in logical pixels).
    ///
    virtual double width() const = 0;

    ///
    /// Get the window height (in logical pixels).
    ///
    virtual double height() const = 0;

    ///
    /// Get the window width (in device pixels).
    ///
    virtual uint32_t device_width() const = 0;

    ///
    /// Get the window height (in device pixels).
    ///
    virtual uint32_t device_height() const = 0;

    ///
    /// Move the window to a new position on the screen.
    ///
    /// @param  x  The new x-position (in logical pixels).
    ///
    /// @param  y  The new y-position (in logical pixels).
    ///
    /// @note  Positions are global screen coordinates (one space across all monitors). Popups
    ///        position relative to their owner's content area instead (see CreatePopup()).
    ///
    virtual void MoveTo(double x, double y) = 0;

    ///
    /// Move the window to the center of the screen.
    ///
    /// @note  Popups ignore this with a warning (they position relative to their owner).
    ///
    virtual void MoveToCenter() = 0;

    ///
    /// Get the x-position of the window (in logical pixels). @see  MoveTo()
    ///
    virtual double x() const = 0;

    ///
    /// Get the y-position of the window (in logical pixels). @see  MoveTo()
    ///
    virtual double y() const = 0;

    ///
    /// Whether or not the window is fullscreen.
    ///
    virtual bool is_fullscreen() const = 0;

    ///
    /// Whether or not the window renders on the GPU.
    ///
    virtual bool is_accelerated() const = 0;

    ///
    /// Get the ID of the render buffer the window draws into (0 if the window doesn't
    /// render on the GPU).
    ///
    /// You can use this with the GPUDriver interface to draw into the window (see
    /// WindowListener::OnClear()).
    ///
    virtual uint32_t render_buffer_id() const = 0;

    ///
    /// Get the window's device scale (eg, 2.0 on a Retina display).
    ///
    virtual double scale() const = 0;

    ///
    /// Set the window title.
    ///
    /// @param  title  The new title, as a UTF-8 string.
    ///
    /// @note  Custom-chrome windows don't draw a system title bar, but the OS still shows the
    ///        title elsewhere (eg, the taskbar and window switcher).
    ///
    virtual void SetTitle(const char* title) = 0;

    ///
    /// Set the cursor.
    ///
    /// @param  cursor  The cursor to show over the window.
    ///
    /// @note  The library sets the cursor for web content automatically (link hands, the text
    ///        I-beam, CSS cursor rules), so you don't need to call this for pages. A cursor
    ///        you set here lasts until the mouse next moves over a page.
    ///
    virtual void SetCursor(ultralight::Cursor cursor) = 0;

    ///
    /// Set the window icon (shown in the title bar and taskbar).
    ///
    /// @param  icon  A 32-bit BGRA bitmap with straight (unpremultiplied) alpha. Pass a nullptr
    ///               to restore the default icon (Settings::app_icon when set, else the
    ///               platform's standard icon).
    ///
    /// @note  macOS windows don't show an icon (the Dock icon comes from Settings::app_icon or
    ///        your app bundle), so this does nothing there.
    ///
    virtual void SetIcon(RefPtr<Bitmap> icon) = 0;

    ///
    /// Show the window (if it was hidden).
    ///
    /// @note  On macOS, showing a minimized window briefly brings it on screen (and takes
    ///        focus) before it minimizes to the Dock. On Windows it appears minimized directly.
    ///
    virtual void Show() = 0;

    ///
    /// Hide the window.
    ///
    /// @note  Hiding a window also hides the popups it owns.
    ///
    virtual void Hide() = 0;

    ///
    /// Show the window once its pages have settled.
    ///
    /// Call this instead of Show() on a window created hidden (WindowFlags::Hidden),
    /// after adding panels and starting their page loads. This avoids displaying a blank or
    /// half-loaded first frame.
    ///
    /// While waiting, the window stays hidden-- its pages continue to load, run scripts,
    /// and lay out.
    ///
    /// The window shows once every page in a visible panel has settled
    /// (LoadListener::OnPageSettled()).
    ///
    /// A panel with no page loaded does not delay showing the window.
    ///
    /// If the pages have not settled when the timeout elapses, the window shows anyway.
    ///
    /// @param  timeout_seconds  The maximum time to wait, in seconds. Pass 0 to show the window
    ///                          immediately.
    ///
    /// \parblock
    /// @note  Calling Show() or Hide() before the window shows cancels the wait--
    ///        Show() shows the window immediately, while Hide() keeps it hidden.
    /// \endparblock
    ///
    /// \parblock
    /// @note  If the window is already visible, this call does nothing.
    /// \endparblock
    ///
    void ShowWhenReady(double timeout_seconds = 0.5);

    ///
    /// Whether or not the window is visible (not hidden).
    ///
    virtual bool is_visible() const = 0;

    ///
    /// Whether or not the window is maximized.
    ///
    virtual bool is_maximized() const = 0;

    ///
    /// Whether or not the window is minimized.
    ///
    virtual bool is_minimized() const = 0;

    ///
    /// Maximize the window. Popups ignore this with a warning.
    ///
    /// @note  On macOS, maximizing a window that isn't resizable changes is_maximized() (and
    ///        fires WindowListener::OnWindowStateChanged()) without changing the window's
    ///        size, since the OS only zooms resizable windows. On Windows the window resizes
    ///        either way.
    ///
    virtual void Maximize() = 0;

    ///
    /// Minimize the window. Popups ignore this with a warning.
    ///
    virtual void Minimize() = 0;

    ///
    /// Restore the window from the minimized or maximized state. Popups ignore this with a
    /// warning.
    ///
    virtual void Restore() = 0;

    ///
    /// Redraw the window on an upcoming frame, even if no View has changed.
    ///
    /// You can use this to animate content you draw from WindowListener::OnClear() or
    /// WindowListener::OnPaint().
    ///
    void Invalidate();

    ///
    /// Set the window background color.
    ///
    /// The background is drawn beneath the window's content each frame and shows wherever no
    /// View covers the window.
    ///
    /// An unset color (the default) means:
    ///
    /// | Window                      | Unset background                                   |
    /// |-----------------------------|----------------------------------------------------|
    /// | Plain opaque                | Opaque white                                       |
    /// | Transparent                 | Nothing: the window is see-through                 |
    /// | Backdrop material in effect | Nothing: the material shows untinted               |
    ///
    /// A color you set always draws. On transparent windows and over a backdrop material its
    /// alpha is used (over a material, it acts as the material's tint). On plain opaque
    /// windows the alpha is ignored.
    ///
    /// @param  color  The new background color. Pass an unset color to go back to the
    ///                default; an invalid color is ignored with a warning.
    ///
    /// @note  On Windows and macOS, while a backdrop material is in effect, setting a
    ///        background color also stops the material from following later OS light/dark
    ///        theme changes, so the colors you picked keep matching it.
    ///
    void SetBackgroundColor(const Color& color);

    ///
    /// Request a native backdrop material for this window. (Default: BackdropMaterial::None)
    ///
    /// ```
    /// // A main window with the native window material, tinted by a translucent dark
    /// // background:
    /// window->SetBackdrop(BackdropMaterial::Window);
    /// window->SetBackgroundColor("rgba(32, 32, 32, 0.5)");
    /// ```
    ///
    /// Not every platform, OS version, or window can render a live material. When one can't,
    /// the library uses the nearest look it can, down to a solid fallback fill in the theme's
    /// color. You can read back what's showing:
    ///
    /// | What shows                   | effective_backdrop()   | effective_backdrop_variant() |
    /// |------------------------------|------------------------|------------------------------|
    /// | A live translucent material  | the requested material | BackdropVariant::Frosted     |
    /// | An opaque wallpaper material | the requested material | BackdropVariant::Matte       |
    /// | The fallback fill            | the requested material | BackdropVariant::Matte       |
    /// | Nothing                      | BackdropMaterial::None | BackdropVariant::Auto        |
    ///
    /// On Windows and macOS, CPU-rendered and fullscreen windows can't show a material at all
    /// and report BackdropMaterial::None. Linux always shows at least the fallback fill.
    ///
    /// The material shows untinted until you set a background color. To tint it, set a
    /// translucent one: the lower the alpha, the more of the material shows through.
    /// @see  SetBackgroundColor()
    ///
    /// @param  material  The material to request (BackdropMaterial::None removes it).
    ///
    /// @param  options   Optional settings: the look, the light/dark theme, the fallback
    ///                   color, and the style (see BackdropOptions).
    ///
    /// @note  To react when what's showing changes later, listen for
    ///        WindowListener::OnBackdropChanged().
    ///
    void SetBackdrop(BackdropMaterial material, const BackdropOptions& options = {});

    ///
    /// Get the backdrop material you requested. @see  SetBackdrop()
    ///
    BackdropMaterial backdrop() const;

    ///
    /// Get the backdrop options you requested. @see  SetBackdrop()
    ///
    BackdropOptions backdrop_options() const;

    ///
    /// Get the backdrop material in effect: the requested material while anything shows (a
    /// live material or the fallback fill), else BackdropMaterial::None.
    ///
    /// @see  SetBackdrop()
    ///
    BackdropMaterial effective_backdrop() const;

    ///
    /// Get the look of the backdrop material in effect (BackdropVariant::Auto when none is).
    ///
    /// @see  SetBackdrop()
    ///
    BackdropVariant effective_backdrop_variant() const;

    ///
    /// Set whether the backdrop material shows its active or inactive look.
    ///
    /// By default the material follows the window, like native windows do. Popups always
    /// show the active look by default, since a popup is never the active window.
    ///
    /// @param  state  The look to show.
    ///
    /// @note  Linux ignores this. So do titled windows on newer Windows 11 builds, where the
    ///        OS draws the material itself and controls its inactive look.
    ///
    void SetBackdropState(BackdropState state);

    ///
    /// Get the backdrop look you requested with SetBackdropState() (not necessarily what the
    /// OS is showing).
    ///
    BackdropState backdrop_state() const;

    ///
    /// Set the title bar color.
    ///
    /// The title text color is picked automatically for contrast. This only works on
    /// Windows 11; other platforms, and windows without a system title bar (eg, custom-chrome
    /// windows), ignore it.
    ///
    /// @param  color  The new title bar color. Pass an unset color to go back to the system
    ///                color; an invalid color is ignored with a warning.
    ///
    /// @note  A set title bar color covers any backdrop material there; unset lets the
    ///        material show through the title bar.
    ///
    void SetCaptionColor(const Color& color);

    ///
    /// Mark which areas of your custom chrome drag the window or act as caption buttons.
    ///
    /// Only custom-chrome windows use this; other windows ignore it with a warning. See the
    /// Custom Chrome section in the class overview for an example.
    ///
    /// Regions are in window content coordinates. They don't move or scale with the window,
    /// so set them again whenever your chrome's layout changes (WindowListener::OnResize()
    /// is a good place).
    ///
    ///  - Caption regions act like a real title bar: dragging moves the window,
    ///    double-clicking maximizes or restores it, and the page under them gets no mouse
    ///    input. On Windows, right-clicking one opens the system window menu.
    ///  - Button regions still send hover to the page (so your chrome can style `:hover`),
    ///    but a click performs the window action. Style pressed states from hover or the
    ///    window state callbacks.
    ///
    /// @param  regions  The full list of regions, replacing any previous list (pass a nullptr
    ///                  to clear it).
    ///
    /// @param  count    The number of entries in `regions` (0 clears the list).
    ///
    /// @note  Where regions overlap, later entries win. The resize edges always win over
    ///        regions, and a button region for an action you turned off with
    ///        SetWindowButtons() is ignored. Pages can also mark drag areas with the
    ///        `app-region` CSS property; regions you set here win where the two overlap.
    ///
    void SetHitTestRegions(const HitTestRegion* regions, size_t count);

    ///
    /// Set which window actions are available to the user.
    ///
    /// This covers every way to trigger an action: native and custom caption buttons,
    /// double-clicking the title bar, the system window menu, and OS shortcuts and snap
    /// gestures. By default Close is available, and Minimize and Maximize are available when
    /// the window was created with WindowFlags::Maximizable.
    ///
    /// @param  buttons  The actions to make available.
    ///
    /// @note  Popups ignore this with a warning.
    ///
    void SetWindowButtons(WindowButtons buttons);

    ///
    /// Get the window actions available to the user. @see  SetWindowButtons()
    ///
    WindowButtons window_buttons() const;

    ///
    /// Start moving the window with the mouse, the way dragging a title bar does.
    ///
    /// Call this while handling a left mouse-button press (eg, from
    /// WindowListener::OnMouseEvent()). The OS then moves the window until the button is
    /// released, and any press in progress on the page is canceled.
    ///
    /// @return  Returns whether or not the move started (false if the left button isn't
    ///          down or the window is fullscreen).
    ///
    virtual bool BeginDragMove() { return false; }

    ///
    /// Start resizing the window from an edge or corner with the mouse.
    ///
    /// Call this while handling a left mouse-button press. macOS doesn't support this (the OS
    /// provides its own resize edges).
    ///
    /// @param  edge  The edge or corner to resize from.
    ///
    /// @return  Returns whether or not the resize started (false on macOS, if the left button
    ///          isn't down, or if the window is fullscreen, maximized, or not resizable).
    ///
    virtual bool BeginDragResize(ResizeEdge edge) { return false; }

    ///
    /// Get the area the native window buttons cover (in window content coordinates), so you
    /// can leave room for them.
    ///
    /// Only macOS custom-chrome windows have these buttons over their content (the traffic
    /// lights). Other windows, and macOS windows in fullscreen (where the buttons hide), get
    /// an empty rect.
    ///
    /// The area changes with SetWindowControlInset(), device scale changes, and fullscreen,
    /// so read it again after calling SetWindowControlInset() and in
    /// WindowListener::OnResize().
    ///
    virtual Rect window_control_bounds() const { return Rect::MakeEmpty(); }

    ///
    /// Move the native window buttons on a macOS custom-chrome window (other windows ignore
    /// this).
    ///
    /// @param  x  The offset (in logical pixels) from the leading edge of the content area
    ///            (mirrored automatically in right-to-left layouts).
    ///
    /// @param  y  The offset (in logical pixels) from the top of the content area.
    ///
    /// @note  The offset positions the buttons' frames, which extend a little past the
    ///        visible circles.
    ///
    /// @see  window_control_bounds()
    ///
    void SetWindowControlInset(double x, double y);

    ///
    /// Set the window's corner style (see CornerStyle). (Default: CornerStyle::Default)
    ///
    /// On transparent windows the library rounds the window's content to match.
    ///
    /// @param  style  The corner style to use.
    ///
    /// @note  On macOS this only affects transparent windows; other windows keep the OS's
    ///        own corners and shadow.
    ///
    void SetCornerStyle(CornerStyle style);

    ///
    /// Get the window's corner style. @see  SetCornerStyle()
    ///
    CornerStyle corner_style() const;

    ///
    /// Set whether a popup can take keyboard focus. (Default: false)
    ///
    /// Popups normally never take focus: clicking one leaves focus with the owner, and key
    /// presses keep going to the owner, the way native menus work. Turn this on for popups
    /// with text fields.
    ///
    /// @param  accepts  Whether or not the popup can take keyboard focus.
    ///
    /// @note  Only popups use this; other windows ignore it.
    ///
    void SetAcceptsKeyInput(bool accepts);

    ///
    /// Whether or not a popup can take keyboard focus. @see  SetAcceptsKeyInput()
    ///
    bool accepts_key_input() const;

    ///
    /// Set whether the library dismisses this popup automatically. (Default: Dismiss::Auto
    /// for popups)
    ///
    /// With Dismiss::Auto, the library hides the popup when the OS would dismiss a native
    /// menu:
    ///
    ///  - a click in the owner, outside the popup
    ///  - a click on the owner's title bar or border (where the platform reports these)
    ///  - the owner starting a move or resize, scrolling, or becoming inactive
    ///  - Esc while the owner has focus (a popup that takes key input also closes on Esc
    ///    while it has focus; see SetAcceptsKeyInput())
    ///
    /// Dismissing hides the popup but doesn't destroy it, so you can show it again.
    /// WindowListener::OnDismiss() fires each time.
    ///
    /// @param  dismiss  Whether or not to dismiss the popup automatically.
    ///
    /// \parblock
    /// @note  Only popups use this; other windows ignore it.
    /// \endparblock
    ///
    /// \parblock
    /// @note  The library checks for dismissal before WindowListener sees the event, so
    ///        consuming the event doesn't stop the popup from being dismissed.
    /// \endparblock
    ///
    void SetDismiss(Dismiss dismiss);

    ///
    /// Get the auto-dismiss setting. @see  SetDismiss()
    ///
    Dismiss dismiss() const;

    ///
    /// Get the window's root layout container (a column that fills the window).
    ///
    /// Build the window's content by adding panels and nested containers to it:
    ///
    /// ```
    /// auto body = window->layout()->AddRow({ .key = "body", .resizable = true });
    /// body->AddPanel({ .key = "sidebar", .size = "240px" });
    /// body->AddPanel({ .key = "content" });
    /// ```
    ///
    RefPtr<Container> layout();

    ///
    /// Get the window's foreground layer, the floating panels displayed above the window's
    /// layout (eg, a toast, an in-window dialog, or a command palette).
    ///
    /// ```
    /// auto toast = window->foreground()->AddPanel({ .width = "320px", .height = "80px",
    ///   .placement = Anchor::WindowCorner(AnchorCorner::BottomRight).Offset(-16, -16),
    ///   .focus = FocusPolicy::None });
    /// ```
    ///
    /// Floating panels clip to the window. For menus and dropdowns that need to extend
    /// outside it, use CreatePopup() instead.
    ///
    /// @see  Foreground
    ///
    RefPtr<Foreground> foreground();

    ///
    /// Configure the window's root container and get it.
    ///
    /// The root is always a column that fills the window, so the sizing fields and `fixed`
    /// in `root_options` are ignored; key, resizable, gap, padding, and hidden apply.
    ///
    /// Each call applies the whole options struct (an unset length or empty key resets that
    /// field). To add children without reconfiguring the root, use layout()->Build().
    ///
    /// @param  root_options  The options to apply to the root container.
    ///
    /// @return  Returns the root container.
    ///
    RefPtr<Container> BuildLayout(const ContainerOptions& root_options);

    ///
    /// Configure the root container and add builder elements to it (see
    /// `<AppCore/layout/Builder.h>`).
    ///
    /// This **appends**, so calling it twice adds the children twice. To start over, call
    /// layout()->RemoveAll() first.
    ///
    /// ```
    /// window->BuildLayout({},
    ///   panel({ .key = "toolbar", .size = "44px", .fixed = true }, &toolbar),
    ///   row({ .key = "body", .resizable = true },
    ///     panel({ .key = "sidebar", .size = "240px" }, &sidebar),
    ///     panel({ .key = "content" }, &content)));
    /// ```
    ///
    /// @param  root_options  The options to apply to the root container.
    ///
    /// @param  children      The builder elements to add to the root.
    ///
    /// @return  Returns the root container (so you can chain calls).
    ///
    template <typename... Children>
      requires(LayoutBuildable<Children> && ...)
    RefPtr<Container> BuildLayout(const ContainerOptions& root_options, Children... children) {
      RefPtr<Container> root = BuildLayout(root_options);
      (children.ApplyTo(*root), ...);
      return root;
    }

    ///
    /// Add a panel to the window's root container (shorthand for layout()->AddPanel()).
    ///
    /// A bare AddPanel() fills the whole window:
    ///
    /// ```
    /// window->AddPanel()->view()->LoadURL("file:///app.html");
    /// ```
    ///
    /// @param  options  The panel's layout options.
    ///
    /// @return  Returns a ref-pointer to the new Panel.
    ///
    RefPtr<Panel> AddPanel(const PanelOptions& options = {});

    ///
    /// Add a panel with a caller-supplied ViewConfig to the window's root container.
    ///
    /// @param  options      The panel's layout options.
    ///
    /// @param  view_config  Configuration details for the panel's View.
    ///
    /// @return  Returns a ref-pointer to the new Panel.
    ///
    /// @see  Container::AddPanel()
    ///
    RefPtr<Panel> AddPanel(const PanelOptions& options, const ViewConfig& view_config);

    ///
    /// Get the ViewConfig this window uses for a bare AddPanel().
    ///
    /// The window fills in what it or the app's Settings decide, and leaves the rest at the
    /// defaults:
    ///
    ///  - `initial_device_scale` (the window's scale)
    ///  - `display_id` (the window's display)
    ///  - `is_accelerated` (whether the window composites on the GPU)
    ///  - `match_native_editing_behavior` (from Settings::match_native_editing_behavior)
    ///  - the `font_family_*` picks for the host OS (while Settings::auto_font_families is
    ///    set)
    ///
    /// You can use this as a starting point for a View you create yourself:
    ///
    /// ```
    /// ViewConfig config = window->default_view_config();
    /// config.enable_javascript = false;
    /// window->AdoptPanel(renderer->CreateView(w, h, config, nullptr));
    /// ```
    ///
    /// @return  Returns a copy of the current values (a later device scale change won't
    ///          update it).
    ///
    ViewConfig default_view_config() const;

    ///
    /// Add a panel adopting an existing View to the window's root container.
    ///
    /// @param  view     The View the panel will host.
    ///
    /// @param  options  The panel's layout options.
    ///
    /// @return  Returns a ref-pointer to the new Panel.
    ///
    /// @see  Container::AdoptPanel()
    ///
    RefPtr<Panel> AdoptPanel(RefPtr<View> view, const PanelOptions& options = {});

    ///
    /// Add a panel that hosts an existing View (same as AdoptPanel()).
    ///
    /// @param  view     The View the panel will host.
    ///
    /// @param  options  The panel's layout options.
    ///
    /// @return  Returns a ref-pointer to the new Panel.
    ///
    template <typename ViewRef>
      requires std::convertible_to<ViewRef&&, RefPtr<View>>
    RefPtr<Panel> AddPanel(ViewRef&& view, const PanelOptions& options = {}) {
      return AdoptPanel(RefPtr<View>(std::forward<ViewRef>(view)), options);
    }

    ///
    /// Set the default divider style for every resizable container in this window (see
    /// `<AppCore/layout/DividerStyle.h>`).
    ///
    /// Fields you leave unset keep the library's defaults, and Container::SetDividerStyle()
    /// overrides them per container. The change takes effect on the next layout.
    ///
    /// @param  style  The divider style to use as this window's default.
    ///
    void SetDividerStyle(const DividerStyle& style);

    ///
    /// Find a panel by key anywhere in the window: the tiled layout first, then the
    /// foreground layer.
    ///
    /// @param  key  The panel key to look for.
    ///
    /// @return  Returns a ref-pointer to the matching Panel, or null if no panel in this
    ///          window has that key.
    ///
    RefPtr<Panel> FindPanel(const String& key);

    ///
    /// Get the panel holding keyboard focus (null when none does).
    ///
    RefPtr<Panel> focused_panel() const;

    ///
    /// Take keyboard focus away from every panel.
    ///
    /// As with View::Unfocus(), the page's focused element gets a blur event but stays
    /// focused in the document, and shows focus again the next time you call Panel::Focus().
    ///
    /// @note  If a panel had focus, the window's focus-change callback fires with a null panel.
    ///
    /// @see  Panel::Focus(), OnFocusChange()
    ///
    void ClearFocus();

    ///
    /// Register a callback fired when the window's focused panel changes (the new panel may
    /// be null).
    ///
    /// @param  callback           The callback to fire.
    ///
    /// @param  user_data          Pointer to user-defined data, passed back to the callback.
    ///
    /// @param  destroy_user_data  Called once when the registration ends, so you can release
    ///                            `user_data`.
    ///
    /// @return  Returns a LayoutCallback controlling how long the callback stays
    ///          registered.
    ///
    /// @note  Callbacks must not let exceptions propagate.
    ///
    [[nodiscard]] LayoutCallback OnFocusChange(LayoutFocusCallback callback, void* user_data,
                                               LayoutDestroyUserDataCallback destroy_user_data);

    ///
    /// Register a focus-change callback (invocable form).
    ///
    /// @param  callback  The callback to fire. It may take the newly focused panel as
    ///                   `Panel*` (null when none), or take nothing at all.
    ///
    /// @return  Returns a LayoutCallback controlling how long the callback stays
    ///          registered.
    ///
    template <typename Callback>
      requires std::invocable<Callback&, Panel*> || std::invocable<Callback&>
    [[nodiscard]] LayoutCallback OnFocusChange(Callback callback) {
      auto* holder = new Callback(std::move(callback));
      LayoutFocusCallback trampoline = [](void* user_data, Window*, Panel* focused) {
        auto& cb = *static_cast<Callback*>(user_data);
        if constexpr (std::invocable<Callback&, Panel*>)
          cb(focused);
        else
          cb();
      };
      return OnFocusChange(trampoline, holder,
                           [](void* user_data) { delete static_cast<Callback*>(user_data); });
    }

    ///
    /// Register a callback fired when the focused panel's editable state changes.
    ///
    /// This is the window-level form of EditorListener::OnChangeEditableState(): one callback
    /// reports the focused panel's state, so a multi-panel app doesn't need to listen on
    /// each View (see EditableState in `<Ultralight/Editor.h>`). You can use it to show and
    /// hide an on-screen keyboard, for example.
    ///
    /// The callback fires when:
    ///
    ///  - keyboard focus moves to or from an editable element in the focused panel
    ///  - the focused element's `inputmode` or `enterkeyhint` changes
    ///  - the focused panel's View gains or loses focus
    ///  - focus moves to a panel with a different state (removing the focused panel reports
    ///    a default, nothing-editable state)
    ///
    /// @param  callback           The callback to fire.
    ///
    /// @param  user_data          Pointer to user-defined data, passed back to the callback.
    ///
    /// @param  destroy_user_data  Called once when the registration ends, so you can release
    ///                            `user_data`.
    ///
    /// @return  Returns a LayoutCallback controlling how long the callback stays
    ///          registered.
    ///
    /// \parblock
    /// @note  The window uses each panel's editor listener to track this (see
    ///        View::set_editor_listener()). If you replace it on a hosted View, that View's
    ///        changes stop reaching this callback and the window's input method handling.
    /// \endparblock
    ///
    /// \parblock
    /// @note  Callbacks must not let exceptions propagate.
    /// \endparblock
    ///
    [[nodiscard]] LayoutCallback OnEditableStateChange(
        LayoutEditableStateCallback callback, void* user_data,
        LayoutDestroyUserDataCallback destroy_user_data);

    ///
    /// Register an editable-state callback (invocable form).
    ///
    /// @param  callback  The callback to fire. It may take the focused panel and state as
    ///                   `(Panel*, const EditableState&)`, or just the state.
    ///
    /// @return  Returns a LayoutCallback controlling how long the callback stays
    ///          registered.
    ///
    template <typename Callback>
      requires std::invocable<Callback&, Panel*, const EditableState&>
                   || std::invocable<Callback&, const EditableState&>
    [[nodiscard]] LayoutCallback OnEditableStateChange(Callback callback) {
      auto* holder = new Callback(std::move(callback));
      LayoutEditableStateCallback trampoline = [](void* user_data, Window*, Panel* panel,
                                                  const EditableState& state) {
        auto& cb = *static_cast<Callback*>(user_data);
        if constexpr (std::invocable<Callback&, Panel*, const EditableState&>)
          cb(panel, state);
        else
          cb(state);
      };
      return OnEditableStateChange(
          trampoline, holder, [](void* user_data) { delete static_cast<Callback*>(user_data); });
    }

    ///
    /// Close the window.
    ///
    /// WindowListener::OnClose() fires, the popups this window owns close with it, and the
    /// window's panels and layout handles stop working. Calling this again does nothing.
    ///
    virtual void Close() = 0;

    ///
    /// Convert logical pixels to device pixels using the window's device scale (rounded to
    /// the nearest pixel).
    ///
    int LogicalToDevice(double val) const;

    ///
    /// Convert device pixels to logical pixels using the window's device scale.
    ///
    double DeviceToLogical(int val) const;

    ///
    /// Copy a CPU-rendered surface into the window. The library calls this for you to present
    /// CPU-rendered panels; you don't need to call it.
    ///
    virtual void DrawSurface(int x, int y, Surface* surface) {}

    ///
    /// Capture a screenshot of the window.
    ///
    /// @return  Returns a Bitmap with the window's pixels in BGRA8_UNORM_SRGB format, or an
    ///          empty ref-pointer if the capture fails.
    ///
    /// @note  This captures what was last shown on screen, so anything drawn since the last
    ///        frame isn't included.
    ///
    virtual RefPtr<Bitmap> TakeScreenshot() { return RefPtr<Bitmap>(); }

#if UL_HAS(TESTING)
    /// \cond ignore
    virtual RefPtr<Bitmap> ReadbackPixels(RefPtr<View> view) { return RefPtr<Bitmap>(); }
    virtual RefPtr<Bitmap> ReadbackRenderTarget(const RenderTarget& rt) { return RefPtr<Bitmap>(); }
    virtual void FireKeyEvent(const ultralight::KeyEvent& evt) { }
    virtual void FireMouseEvent(const ultralight::MouseEvent& evt) { }
    virtual void FireScrollEvent(const ultralight::ScrollEvent& evt) { }
    virtual bool NeedsRepaint() { return false; }
    virtual void SetGeometryForTesting(double width, double height, double scale) { }
    virtual bool UsesCompositionForTesting() { return false; }
    virtual ultralight::Cursor CursorForTesting() const { return ultralight::kCursor_Pointer; }
    int HitTestChromeRegionForTesting(double x, double y);
    /// \endcond
#endif

    ///
    /// Get the native window handle: an `HWND` on Windows, an `NSWindow*` on macOS, and a
    /// `GLFWwindow*` on Linux.
    ///
    virtual void* native_handle() const = 0;

    ///
    /// Show frame statistics (eg, FPS and frame times) after the window title. You can't turn
    /// this off again.
    ///
    virtual void EnableFrameStatistics() {}

protected:
  /// \cond ignore
  virtual ~Window();
  virtual bool platform_always_uses_cpu_renderer() const = 0;
  virtual LayoutEngine* layout_engine() const = 0;

  ///
  /// Fill the current damage region with a color, excluding the given covered rects (in
  /// pixels). Only called for windows composited by the CPU. On transparent windows with
  /// no background the fill is fully transparent (clears to see-through).
  ///
  virtual void FillBackground(const Color& color, const IntRect* covered_rects,
                              size_t covered_count) {}

  ///
  /// Fill one rect (in pixels) with an opaque color. Only called for windows composited by
  /// the CPU, between the content blits of a paint cycle (divider chrome).
  ///
  virtual void FillRect(const Color& color, const IntRect& rect) {}

  ///
  /// Platform application hooks for the shared window-chrome state (stored on the engine).
  /// Each applies the given state to the native window and, for the backdrop, reports the
  /// material it applied back through the engine.
  ///
  virtual void PlatformSetBackdrop(BackdropMaterial material, BackdropState state) {}
  virtual void PlatformSetCornerStyle(CornerStyle style) {}
  virtual void PlatformSetAcceptsKeyInput(bool accepts) {}
  virtual void PlatformSetCaptionColor(const Color& color) {}
  virtual void PlatformSetBackgroundColor(const Color& color) {}
  virtual void PlatformBeginPendingShow() {}
  virtual void PlatformSetWindowButtons(WindowButtons buttons) {}
  virtual void PlatformSetWindowControlInset(double x, double y) {}

  ///
  /// Platform notification hooks for the focused panel's editing state (fired by the
  /// engine alongside the public OnEditableStateChange stream). These drive the native
  /// input-method integration (IME association, candidate-window placement, composition
  /// cancellation).
  ///
  virtual void PlatformOnEditableStateChange(const EditableState& state) {}
  virtual void PlatformOnCompositionUpdated() {}
  virtual void PlatformOnCompositionDiscarded() {}

  friend class LayoutEngine;
  friend class ViewHost;
  /// \endcond
};

}  // namespace ultralight

#pragma pop_macro("None")
