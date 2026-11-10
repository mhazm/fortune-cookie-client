///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/Geometry.h>
#include <Ultralight/RefPtr.h>

// X11's headers define None as a macro, which would break the None enumerators.
#pragma push_macro("None")
#undef None

namespace ultralight {

class Panel;

///
/// A window corner.
///
/// @see  Anchor::WindowCorner()
///
enum class AnchorCorner : uint8_t { TopLeft = 0, TopRight, BottomLeft, BottomRight };

///
/// Cross-axis alignment of a floating panel against its anchor rect.
///
/// @see  Anchor::Align()
///
enum class AnchorAlign : uint8_t { Start = 0, Center, End };

///
/// Fit behavior when an anchored floating panel does not fit on its preferred side.
///
/// @see  Anchor::Fit()
///
enum class AnchorFit : uint8_t { None = 0, Flip };

///
/// Placement of a floating panel in a window.
///
/// An Anchor determines where a floating panel sits within an AppCore window. You pass it through
/// ForegroundPanelOptions when adding a panel to the window's foreground layer, positioning
/// overlays like toasts or dropdown menus.
///
/// This example floats a toast in the bottom-right corner of the window, nudged inward from the
/// edge:
///
/// ```
/// RefPtr<Panel> toast = window->foreground()->AddPanel(
///     { .width = "320px", .height = "80px",
///       .placement = Anchor::WindowCorner(AnchorCorner::BottomRight)
///                        .Offset(-16, -16) });
/// ```
///
/// ## Placement Forms
///
/// Static factory methods define where a panel appears:
///
/// | Form                   | Placement                                           |
/// |------------------------|-----------------------------------------------------|
/// | Anchor::At()           | Positions the panel at absolute window coordinates. |
/// | Anchor::WindowCorner() | Snaps the panel to a window corner.                 |
/// | Anchor::WindowCenter() | Centers the panel within the window.                |
/// | Anchor::Fill()         | Places the panel at the window origin.              |
/// | Anchor::Below()        | Hangs the panel under a rectangle in another panel. |
///
/// Panels positioned with Anchor::WindowCorner() or Anchor::WindowCenter() follow the window as it
/// resizes.
///
/// Calling Offset() on any form nudges the panel by horizontal and vertical offsets in logical
/// pixels, applied after the form places the panel.
///
/// ## Anchoring Below an Element
///
/// Anchor::Below() takes a target panel and a rectangle in that panel's local logical pixels, which
/// match the page's CSS pixels-- so bounding rectangles from dom::Element::getBoundingClientRect()
/// pass straight through without conversion.
///
/// The library updates the placement every frame from the target panel's current geometry, so the
/// floating panel follows the target as it moves.
///
/// Two chained methods refine placement under Anchor::Below():
///
/// - **Align() sets cross-axis alignment against the rectangle.** Left edges align by default, or
///   you can center the panel or align its right edge.
/// - **Fit() handles panels that don't fit below the rectangle.** AnchorFit::Flip places the panel
///   above the rectangle instead.
///
/// This example positions a dropdown below a button from another panel, flipping it upward if space
/// runs out:
///
/// ```
/// dom::DOMRect box = button.getBoundingClientRect();
/// RefPtr<Panel> dropdown = window->foreground()->AddPanel(
///     { .width = "240px", .height = "180px",
///       .placement = Anchor::Below(toolbar, Rect::FromXYWH(box.x, box.y,
///                                                          box.width, box.height))
///                        .Fit(AnchorFit::Flip) });
/// ```
///
/// @note  A floating panel's placement is fixed once the panel exists. See Foreground for how to
///        move an existing panel.
///
/// @see  ForegroundPanelOptions::placement, Foreground::AddPanel(),
///       dom::Element::getBoundingClientRect()
///
class Anchor {
 public:
  ///
  /// The placement form.
  ///
  enum class Kind : uint8_t {
    Origin = 0,    ///< Window origin (Fill() with the default 100% sizes).
    At,            ///< Absolute window-space logical px.
    WindowCorner,  ///< A window corner, following the window.
    WindowCenter,  ///< Centered in the window, following the window.
    Below,         ///< Under an anchor rect in a target panel.
  };

  ///
  /// Create the default placement (the window origin).
  ///
  Anchor() = default;

  ///
  /// Place the panel's top-left corner at an absolute position in the window.
  ///
  /// @param  x  The x-position, in logical px from the window's left edge.
  ///
  /// @param  y  The y-position, in logical px from the window's top edge.
  ///
  static Anchor At(double x, double y) {
    Anchor anchor;
    anchor.kind_ = Kind::At;
    anchor.x_ = x;
    anchor.y_ = y;
    return anchor;
  }

  ///
  /// Snap the panel's matching corner to a corner of the window.
  ///
  /// The panel follows the window as it resizes. You can nudge it with Offset().
  ///
  /// @param  corner  The window corner to snap to.
  ///
  static Anchor WindowCorner(AnchorCorner corner) {
    Anchor anchor;
    anchor.kind_ = Kind::WindowCorner;
    anchor.corner_ = corner;
    return anchor;
  }

  ///
  /// Center the panel in the window. The panel follows the window as it resizes.
  ///
  static Anchor WindowCenter() {
    Anchor anchor;
    anchor.kind_ = Kind::WindowCenter;
    return anchor;
  }

  ///
  /// Place the panel at the window origin, the named form of the default placement.
  ///
  /// With the default `100%` sizes this fills the window.
  ///
  static Anchor Fill() { return Anchor(); }

  ///
  /// Place the panel below an anchor rectangle in a target panel (eg, a dropdown below its box).
  ///
  /// The placement updates every frame from the target's current geometry.
  ///
  /// You can refine the placement with Align() and Fit().
  ///
  /// @param  target  The panel the anchor rectangle belongs to.
  ///
  /// @param  rect    The anchor rectangle, in the target panel's local logical pixels (which
  ///                 equal the page's CSS pixels).
  ///
  /// \parblock
  /// @note  Anchoring one floating panel below another can lag the target by a frame while the
  ///        target moves.
  /// \endparblock
  ///
  /// \parblock
  /// @note  If the target panel is removed or has not been laid out yet, the panel stays where
  ///        it was last placed.
  /// \endparblock
  ///
  static Anchor Below(RefPtr<Panel> target, const Rect& rect) {
    Anchor anchor;
    anchor.kind_ = Kind::Below;
    anchor.target_ = target;
    anchor.rect_ = rect;
    return anchor;
  }

  ///
  /// Nudge the placement by an offset, applied after the placement form resolves (every form,
  /// At() and the default origin included).
  ///
  /// @param  dx  The horizontal nudge, in logical px.
  ///
  /// @param  dy  The vertical nudge, in logical px.
  ///
  /// @return  Returns a copy of this Anchor with the offset applied.
  ///
  [[nodiscard]] Anchor Offset(double dx, double dy) const {
    Anchor anchor = *this;
    anchor.offset_x_ = dx;
    anchor.offset_y_ = dy;
    return anchor;
  }

  ///
  /// Align the panel against the anchor rect's cross axis. (Default: Start)
  ///
  /// Under Below(), Start aligns the left edges and End aligns the right edges.
  ///
  /// @param  alignment  The cross-axis alignment to use.
  ///
  /// @return  Returns a copy of this Anchor with the alignment applied.
  ///
  [[nodiscard]] Anchor Align(AnchorAlign alignment) const {
    Anchor anchor = *this;
    anchor.align_ = alignment;
    return anchor;
  }

  ///
  /// Choose the fit behavior when the panel does not fit on its preferred side. (Default: None)
  ///
  /// Under Below(), Flip places the panel above the rect instead.
  ///
  /// @param  fit  The fit behavior to use.
  ///
  /// @return  Returns a copy of this Anchor with the fit behavior applied.
  ///
  [[nodiscard]] Anchor Fit(AnchorFit fit) const {
    Anchor anchor = *this;
    anchor.fit_ = fit;
    return anchor;
  }

  ///
  /// The placement form.
  ///
  Kind kind() const { return kind_; }

  ///
  /// The window corner (WindowCorner() forms).
  ///
  AnchorCorner corner() const { return corner_; }

  ///
  /// The cross-axis alignment (Below() forms).
  ///
  AnchorAlign alignment() const { return align_; }

  ///
  /// The fit behavior (Below() forms).
  ///
  AnchorFit fit_mode() const { return fit_; }

  ///
  /// The absolute x-position, in logical px (At() forms).
  ///
  double x() const { return x_; }

  ///
  /// The absolute y-position, in logical px (At() forms).
  ///
  double y() const { return y_; }

  ///
  /// The x-offset applied after the placement form resolves, in logical px.
  ///
  double offset_x() const { return offset_x_; }

  ///
  /// The y-offset applied after the placement form resolves, in logical px.
  ///
  double offset_y() const { return offset_y_; }

  ///
  /// The target panel (Below() forms, null otherwise).
  ///
  RefPtr<Panel> target() const { return target_; }

  ///
  /// The anchor rect, in the target panel's local logical px (Below() forms).
  ///
  const Rect& rect() const { return rect_; }

 private:
  Kind kind_ = Kind::Origin;
  AnchorCorner corner_ = AnchorCorner::TopLeft;
  AnchorAlign align_ = AnchorAlign::Start;
  AnchorFit fit_ = AnchorFit::None;
  double x_ = 0.0;
  double y_ = 0.0;
  double offset_x_ = 0.0;
  double offset_y_ = 0.0;
  RefPtr<Panel> target_;
  Rect rect_ = Rect::MakeEmpty();
};

}  // namespace ultralight

#pragma pop_macro("None")
