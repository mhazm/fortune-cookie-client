///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <AppCore/layout/LayoutNode.h>
#include <Ultralight/View.h>

namespace ultralight {

///
/// A layout node that hosts a single web view.
///
/// Panel%s are the leaf nodes of an AppCore layout tree and the elements of its floating layer.
/// Every panel hosts a single View, sizing the view to match its layout bounds and routing user
/// input to the page.
///
/// This example adds a panel to a window, loads a page into its view, and gives the panel keyboard
/// focus:
///
/// ```
/// RefPtr<Panel> content = window->AddPanel({ .key = "content" });
/// content->view()->LoadURL("file:///app.html");
/// content->Focus();
/// ```
///
/// ## Hosting a View
///
/// Call view() to access the hosted View.
///
/// You can create a panel with a new View or adopt an existing View:
///
/// - **New views come from AddPanel().** Calling Container::AddPanel(), Window::AddPanel(), or
///   Foreground::AddPanel() creates a panel alongside a fresh View configured for the parent
///   window.
/// - **Existing views are adopted into layout.** Calling Container::AdoptPanel() or
///   Foreground::AdoptPanel() creates a panel that hosts a View you created yourself, such as a
///   View created through a Session or one that survived removal from another panel.
///
/// @warning  Never replace the editor listener on a panel's View. Replacing it disables the
///           window's input method support and stops Window::OnEditableStateChange() from firing
///           for that panel.
///
/// ## Keyboard Focus
///
/// A window directs keyboard events to one panel at a time:
///
/// - **Clicking a panel or calling Focus() grants keyboard focus.**
/// - **Never call View::Focus() on a panel's View.** Focusing the view directly bypasses the window
///   and prevents it from routing keyboard input to the active panel.
/// - **The window tracks the active panel.** Call Window::focused_panel() to retrieve the focused
///   panel, or listen for focus transitions with Window::OnFocusChange().
///
/// @note  BringToFront() and OnDismiss() apply to floating panels only.
///
/// @see  LayoutNode, Container::AddPanel(), Container::AdoptPanel(), Foreground,
///       Window::focused_panel(), Window::OnFocusChange()
///
class AExport Panel : public LayoutNode {
 public:
  ///
  /// Get the hosted View.
  ///
  /// @return  Returns the hosted View, or null after the panel is removed or its window
  ///          closes.
  ///
  /// @note  For what happens to a View you hold after removal, see LayoutNode.
  ///
  virtual RefPtr<View> view() = 0;

  ///
  /// Grant this panel exclusive keyboard focus.
  ///
  /// You should always focus a panel through this method (or a user click) rather than
  /// calling View::Focus() directly, otherwise the panel's window won't know where to route
  /// keyboard input.
  ///
  /// @note  Focusing a hidden panel (or one inside a hidden container) is ignored with a
  ///        warning. So is focusing a floating panel created with FocusPolicy::None, which
  ///        never takes keyboard focus.
  ///
  virtual void Focus() = 0;

  ///
  /// Bring this floating panel to the top of the foreground layer so it paints on top of other
  /// floating panels and receives mouse input first.
  ///
  /// @note  This method only applies to floating panels. A tiled panel composites in tree order,
  ///        so calling BringToFront() on it does nothing and logs a warning.
  ///
  virtual void BringToFront() = 0;

  ///
  /// Register a callback fired when the library dismisses this panel.
  ///
  /// The library dismisses a panel when an auto-dismiss trigger fires on a floating panel
  /// (see ForegroundPanelOptions). An application-initiated Hide() does not fire it.
  ///
  /// @param  callback           The function to call. It receives `user_data` and this panel.
  ///
  /// @param  user_data          Passed back to the callback on every fire (can be nullptr).
  ///
  /// @param  destroy_user_data  Called once when the registration drops `user_data` (can be
  ///                            nullptr).
  ///
  /// @return  Returns a LayoutCallback controlling how long the callback stays registered.
  ///
  /// @note  Callbacks must not let exceptions propagate.
  ///
  [[nodiscard]] virtual LayoutCallback OnDismiss(
      LayoutPanelCallback callback, void* user_data,
      LayoutDestroyUserDataCallback destroy_user_data) = 0;

  ///
  /// Register a dismiss callback (invocable form).
  ///
  /// @param  callback  Any invocable. It may take the panel as `Panel&`, or nothing at all.
  ///
  /// @return  Returns a LayoutCallback controlling how long the callback stays registered.
  ///
  template <typename Callback>
    requires std::invocable<Callback&, Panel&> || std::invocable<Callback&>
  [[nodiscard]] LayoutCallback OnDismiss(Callback callback) {
    auto* holder = new Callback(std::move(callback));
    LayoutPanelCallback trampoline = [](void* user_data, Panel* panel) {
      auto& cb = *static_cast<Callback*>(user_data);
      if constexpr (std::invocable<Callback&, Panel&>)
        cb(*panel);
      else
        cb();
    };
    return OnDismiss(trampoline, holder,
                     [](void* user_data) { delete static_cast<Callback*>(user_data); });
  }

 protected:
  virtual ~Panel();
};

}  // namespace ultralight
