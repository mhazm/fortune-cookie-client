///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <AppCore/layout/Anchor.h>
#include <AppCore/layout/Length.h>
#include <Ultralight/RefPtr.h>
#include <Ultralight/String.h>

#include <cstdint>
#include <utility>

// X11's headers define None as a macro, which would break the None enumerators.
#pragma push_macro("None")
#undef None

namespace ultralight {

class LayoutNode;

///
/// Auto-dismiss policy for floating panels and popup windows.
///
/// @see  ForegroundPanelOptions::dismiss, Window::SetDismiss()
///
enum class Dismiss : uint8_t {
  Manual = 0,  ///< Hides only when you hide it (a popup window also hides when its owner hides).
  Auto   = 1,  ///< The library also hides it the way the OS closes a native menu.
};

///
/// Keyboard focus policy for a floating panel.
///
/// @note  Hiding a floating panel that holds focus releases focus. No panel holds focus until the
///        user clicks one or you call Panel::Focus().
///
enum class FocusPolicy : uint8_t {
  Auto = 0,  ///< Takes focus when clicked, like any panel. Showing the panel never takes focus.
  Grab = 1,  ///< Takes keyboard focus when created (unless hidden) and each time it is shown,
             ///< even from inside a click handler (dropdowns, palettes, in-window dialogs).
  None = 2,  ///< Never takes keyboard focus. Calling Panel::Focus() on it is ignored with a
             ///< warning (toasts, HUDs).
};

///
/// Configuration options for a floating panel.
///
/// Pass ForegroundPanelOptions to Foreground::AddPanel() or Foreground::AdoptPanel() to configure
/// an overlay that floats above a window's tiled layout. A floating panel displays content such as
/// a dialog or dropdown menu without shifting the layout beneath it.
///
/// Passing empty options creates a panel that fills the entire window.
///
/// This creates a centered dialog that closes like a native menu and takes keyboard focus:
///
/// ```
/// RefPtr<Panel> dialog = window->foreground()->AddPanel(
///     { .key = "confirm", .width = "400px", .height = "240px",
///       .placement = Anchor::WindowCenter(), .dismiss = Dismiss::Auto,
///       .focus = FocusPolicy::Grab });
/// dialog->view()->LoadURL("file:///confirm.html");
/// ```
///
/// ## Size and Placement
///
/// Set `width` and `height` using logical pixels (eg, `"400px"`) or a percentage of the window (eg,
/// `"50%"`).
///
/// The `placement` option sets where the panel sits in the window (see Anchor), defaulting to the
/// window origin.
///
/// @note  A floating panel's size and placement are fixed at creation. To move or resize a panel,
///        keep a reference to its View with Panel::view() before calling Foreground::Remove()--
///        Panel::view() returns null once the panel is removed. Pass that View to
///        Foreground::AdoptPanel() with updated options so the page doesn't reload.
///
/// ## Dismissal and Focus
///
/// By default, a floating panel uses `Dismiss::Manual` and stays open until you hide it. Set
/// `dismiss = Dismiss::Auto` to close the panel automatically like a native menu (see Dismiss).
/// Call Panel::OnDismiss() to respond when the library dismisses the panel automatically.
///
/// By default, a floating panel uses `FocusPolicy::Auto` and takes keyboard focus only when
/// clicked. Set `focus = FocusPolicy::Grab` for dialogs and dropdown menus that need focus
/// immediately upon appearing (see FocusPolicy).
///
/// ## Starting Hidden
///
/// Set `hidden = true` to create the panel hidden, then call Panel::Show() once its page has
/// loaded.
///
/// @see  Foreground::AddPanel(), Anchor, Dismiss, FocusPolicy, Panel::OnDismiss()
///
struct ForegroundPanelOptions {
  ///
  /// Optional identity for lookup and diagnostics. Empty means unkeyed.
  ///
  String key;

  ///
  /// The panel's width, in logical px or as a percent of the window's width. Unset means `100%`.
  ///
  /// @note  A flex factor (`fr`) is not meaningful here and is treated as unset with a warning.
  ///
  Size width;

  ///
  /// The panel's height, in logical px or as a percent of the window's height. Unset means
  /// `100%`.
  ///
  /// @note  A flex factor (`fr`) is not meaningful here and is treated as unset with a warning.
  ///
  Size height;

  ///
  /// Where the panel sits in the window (see Anchor). The default is the window origin, which
  /// with the default sizes gives a full-window panel.
  ///
  Anchor placement;

  ///
  /// The auto-dismiss policy.
  ///
  /// Auto hides the panel the way native menus close, ie. when:
  ///
  ///  - a press lands outside it
  ///  - Esc is pressed
  ///  - the user moves, resizes, minimizes, maximizes, or deactivates the window (or
  ///    presses its title bar or borders)
  ///
  /// Panel::OnDismiss() fires each time. Scrolling never dismisses a floating panel, and Manual
  /// panels hide only when you hide them.
  ///
  /// @note  A press on another floating panel overlapping this one's rect still counts as
  ///        inside (the outside test is against this panel's own rect).
  ///
  Dismiss dismiss = Dismiss::Manual;

  ///
  /// The keyboard-focus policy (see FocusPolicy).
  ///
  FocusPolicy focus = FocusPolicy::Auto;

  ///
  /// Whether or not the panel starts hidden (create hidden, then Show() once its content
  /// has loaded).
  ///
  bool hidden = false;
};

///
/// Configuration options for a panel in a row or column.
///
/// Pass PanelOptions to Container::AddPanel() or Window::AddPanel() when adding a panel to a window
/// layout. It declares how the panel shares space with sibling nodes along its parent container's
/// axis.
///
/// The following example creates a resizable row with a sized sidebar and a default content panel:
///
/// ```
/// RefPtr<Container> body = window->layout()->AddRow({ .resizable = true });
/// RefPtr<Panel> sidebar =
///     body->AddPanel({ .key = "sidebar", .size = "240px", .min_size = "160px" });
/// RefPtr<Panel> content = body->AddPanel();  // one flex share
/// ```
///
/// ## Defaults and Sizing
///
/// Every field is optional. Passing an empty `{}` gives an unkeyed panel that takes one flex share
/// (`1fr`).
///
/// For the supported size units, see Size and Length.
///
/// ## Panel Keys
///
/// Setting `key` assigns a string identifier to the panel for diagnostics and retrieval. You can
/// look up the panel later by passing its key to Window::FindPanel() or Container::Find().
///
/// @see  Container::AddPanel(), Window::AddPanel(), Size, Length, ContainerOptions
///
struct PanelOptions {
  ///
  /// Optional identity for lookup (Find()) and diagnostics. Empty means unkeyed, which is fully
  /// supported.
  ///
  /// @note  Duplicate keys log a warning, and Find() returns the most recently created match.
  ///
  String key;

  ///
  /// The panel's size along its container's axis. Unset means one flex share (`1fr`). For
  /// what each unit means, see Size.
  ///
  Size size;

  ///
  /// Minimum size constraint. Percentages are relative to the container's content box (inside its
  /// padding). Unset means no minimum.
  ///
  Length min_size;

  ///
  /// Maximum size constraint. Percentages are relative to the container's content box (inside its
  /// padding). Unset means no maximum.
  ///
  Length max_size;

  ///
  /// Whether or not the panel is fixed (the user cannot resize it by dragging a divider).
  ///
  bool fixed = false;

  ///
  /// Whether or not the panel is created hidden. A hidden panel keeps its declared size and
  /// shows later without a layout flash.
  ///
  bool hidden = false;
};

///
/// Configuration options for a row or column container.
///
/// You pass ContainerOptions to Container::AddRow() or Container::AddColumn() when adding a
/// container to a window's layout tree. These options configure the container's own size within its
/// parent, alongside spacing and divider behavior for its direct children.
///
/// This creates a resizable row with child spacing and outer padding:
///
/// ```
/// RefPtr<Container> body = window->layout()->AddRow(
///     { .key = "body", .resizable = true, .gap = 4, .padding = 8 });
/// ```
///
/// ## Options Shared with Panels
///
/// Because a container is also a layout child, it shares its sizing, constraints, and visibility
/// options with PanelOptions. These fields govern how the container behaves within its parent
/// container (see PanelOptions).
///
/// ## Dividers and Spacing
///
/// Settings on the container and its children control interactive divider behavior between panes:
///
/// - **A container's `resizable` option enables dividers between all direct children.** Dragging a
///   divider redistributes space between the panes on either side.
/// - **A child node's `fixed` option preserves that child's size.** Divider drags on either side
///   can't resize a fixed pane.
///
/// To configure divider thickness and colors, call Container::SetDividerStyle() or
/// Window::SetDividerStyle() (see DividerStyle).
///
/// Container options configure empty space around and between direct children:
///
/// - **The `gap` option separates adjacent direct children along the layout axis.** Unset gap
///   leaves no space between children.
/// - **The `padding` option insets direct children from all four container edges.** Unset padding
///   places children flush against the container boundary.
///
/// Both properties accept only logical pixels. Percentage units are ignored with a warning.
///
/// @see  Container::AddRow(), Container::AddColumn(), Window::BuildLayout(), PanelOptions,
///       DividerStyle
///
struct ContainerOptions {
  ///
  /// Optional identity for lookup (Find()) and diagnostics. Empty means unkeyed.
  ///
  String key;

  ///
  /// The container's size along its parent's axis. Unset means one flex share (`1fr`).
  ///
  Size size;

  ///
  /// Minimum size constraint. Percentages are relative to the parent's content box (inside its
  /// padding). Unset means no minimum.
  ///
  Length min_size;

  ///
  /// Maximum size constraint. Percentages are relative to the parent's content box (inside its
  /// padding). Unset means no maximum.
  ///
  Length max_size;

  ///
  /// Whether or not the container is fixed (the user cannot resize it by dragging a divider).
  ///
  bool fixed = false;

  ///
  /// Whether or not the user can resize this container's children by dragging the dividers
  /// between them.
  ///
  bool resizable = false;

  ///
  /// The gap between adjacent children (logical px only; other units are ignored with a
  /// warning). Unset means no gap.
  ///
  Length gap;

  ///
  /// Padding between the container's edges and its children, applied on all four sides
  /// (logical px only). Unset means no padding.
  ///
  Length padding;

  ///
  /// Whether or not the container is created hidden (its whole subtree is hidden with it).
  ///
  bool hidden = false;
};

///
/// Position relative to an existing sibling in a container.
///
/// An InsertPosition places a node before or after an existing sibling instead of appending to the
/// end of the container. You pass it when adding panels or rearranging children.
///
/// The helper functions Before() and After() position nodes around an existing child:
///
/// ```
/// RefPtr<Panel> toolbar = window->layout()->AddPanel(
///     { .key = "toolbar", .size = "44px" }, Before(content));
/// body->Move(sidebar, After(content));  // put the sidebar on the right
/// ```
///
/// ## Relative Placement
///
/// Container methods accept helper functions or default construction:
///
/// - **Before() places the child immediately before an anchor sibling.**
/// - **After() places the child immediately after an anchor sibling.**
/// - **A default-constructed position appends the child at the end of the container.** This applies
///   whenever you omit the position argument or construct one with no anchor.
///
/// When the anchor sibling isn't a direct child of the container, the library logs a warning and
/// appends the node at the end.
///
/// @note  The anchor member here is an existing sibling LayoutNode within the container. Floating
///        panels use the Anchor class instead to position overlays in window space.
///
/// @see  Before(), After(), Container::AddPanel(), Container::Move()
///
struct InsertPosition {
  ///
  /// The sibling the position is relative to. Null means append at the end.
  ///
  RefPtr<LayoutNode> anchor;

  ///
  /// Whether or not the position is after the anchor (Before() when false).
  ///
  bool after = false;
};

///
/// Create an insertion position immediately before a sibling.
///
/// @param  anchor  The sibling to insert before.
///
inline InsertPosition Before(RefPtr<LayoutNode> anchor) { return { std::move(anchor), false }; }

///
/// Create an insertion position immediately after a sibling.
///
/// @param  anchor  The sibling to insert after.
///
inline InsertPosition After(RefPtr<LayoutNode> anchor) { return { std::move(anchor), true }; }

}  // namespace ultralight

#pragma pop_macro("None")
