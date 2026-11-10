///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <AppCore/layout/Options.h>
#include <AppCore/layout/Panel.h>

#include <concepts>
#include <utility>

namespace ultralight {

///
/// Floating layer that displays panels above a window's layout.
///
/// The foreground layer hosts overlays like heads-up displays and dialogs without shifting the
/// panels beneath them. You reach it through Window::foreground().
///
/// Call AddPanel() to float a panel with a new View:
///
/// ```
/// RefPtr<Panel> hud = window->foreground()->AddPanel(
///     { .width = "200px", .height = "120px", .placement = Anchor::At(24, 24) });
/// hud->view()->LoadURL("file:///hud.html");
/// ```
///
/// ## Adding Floating Panels
///
/// A floating panel is an ordinary Panel without a parent container (see Panel). Call AdoptPanel()
/// to float a View you already have.
///
/// ## Size and Placement
///
/// Set dimensions and placement through ForegroundPanelOptions when adding a panel (see Anchor).
/// The options also configure auto-dismissal and keyboard focus behavior.
///
/// A floating panel's size and placement are fixed at creation.
///
/// To move or resize a panel, re-adopt its View with updated options:
///
/// ```
/// RefPtr<View> hud_view = hud->view();  // before Remove(), which drops it
/// window->foreground()->Remove(hud);
/// hud = window->foreground()->AdoptPanel(
///     hud_view, { .width = "200px", .height = "120px",
///                 .placement = Anchor::WindowCenter() });
/// ```
///
/// ## Stacking and Removal
///
/// Floating panels stack in the order you add them, with the newest panel on top. Call
/// Panel::BringToFront() to move a panel to the top of the layer so it paints on top and receives
/// mouse input first.
///
/// Dropping a panel handle doesn't remove it from the layer. Call Remove() to detach a floating
/// panel.
///
/// Calls on a handle stay safe and do nothing after a panel is removed or after the window closes,
/// and Panel::view() returns null (see LayoutNode).
///
/// @note  Floating panels clip to the window (use Window::CreatePopup() for menus and dropdowns
///        that extend past it).
///
/// @see  Window::foreground(), ForegroundPanelOptions, Anchor, Panel::BringToFront(),
///       Window::CreatePopup()
///
class AExport Foreground : public RefCounted {
 public:
  ///
  /// Create a floating panel with a new View (window-default view configuration).
  ///
  /// @param  options  The panel's declared options.
  ///
  /// @return  Returns the new panel (see LayoutNode if the window has closed).
  ///
  virtual RefPtr<Panel> AddPanel(const ForegroundPanelOptions& options = {}) = 0;

  ///
  /// Create a floating panel with a caller-supplied ViewConfig (the same rules as
  /// Container::AddPanel()).
  ///
  /// @param  options      The panel's declared options.
  ///
  /// @param  view_config  The configuration for the new View. The fields describing the
  ///                      hosting window are filled in from the window for you.
  ///
  /// @return  Returns the new panel (see LayoutNode if the window has closed).
  ///
  virtual RefPtr<Panel> AddPanel(const ForegroundPanelOptions& options,
                                 const ViewConfig& view_config) = 0;

  ///
  /// Create a floating panel that adopts an existing View (including a View created through
  /// a Session, or one that survived Remove()).
  ///
  /// The panel takes a reference to the View and resizes it to the panel's bounds.
  ///
  /// @param  view     The View to host.
  ///
  /// @param  options  The panel's declared options.
  ///
  /// @return  Returns the new panel (see LayoutNode if the window has closed).
  ///
  /// @note  The panel replaces the View's editor listener with its own (see Panel).
  ///
  virtual RefPtr<Panel> AdoptPanel(RefPtr<View> view,
                                   const ForegroundPanelOptions& options = {}) = 0;

  ///
  /// Adopt an existing View (shorthand for AdoptPanel()).
  ///
  /// @param  view     The View to host.
  ///
  /// @param  options  The panel's declared options.
  ///
  /// @return  Returns the new panel (see LayoutNode if the window has closed).
  ///
  template <typename ViewRef>
    requires std::convertible_to<ViewRef&&, RefPtr<View>>
  RefPtr<Panel> AddPanel(ViewRef&& view, const ForegroundPanelOptions& options = {}) {
    return AdoptPanel(RefPtr<View>(std::forward<ViewRef>(view)), options);
  }

  ///
  /// Remove a floating panel from the foreground layer.
  ///
  /// Removal drops the panel's reference to its View. A View the application holds a RefPtr
  /// to survives and can be adopted elsewhere. For what removed handles do afterwards, see
  /// LayoutNode.
  ///
  /// @param  panel  The floating panel to remove. A panel that is not one of this window's
  ///                floating panels is a no-op with a warning.
  ///
  /// @return  Returns whether or not the panel was one of this window's floating panels
  ///          and was removed.
  ///
  virtual bool Remove(RefPtr<Panel> panel) = 0;

  ///
  /// Get the number of floating panels (hidden panels included).
  ///
  virtual int panel_count() const = 0;

  ///
  /// Get the floating panel at an index in current z-order, bottom-most first.
  ///
  /// @param  index  The zero-based index in z-order.
  ///
  /// @return  Returns the panel, or null when the index is out of range.
  ///
  virtual RefPtr<Panel> panel_at(int index) const = 0;

  ///
  /// Find a floating panel by key.
  ///
  /// @param  key  The key to look for.
  ///
  /// @return  Returns the matching panel, or null when nothing matches. With duplicate keys
  ///          the most recently created match wins.
  ///
  virtual RefPtr<Panel> FindPanel(const String& key) = 0;

 protected:
  virtual ~Foreground();
};

}  // namespace ultralight
