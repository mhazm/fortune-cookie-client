///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once

namespace ultralight {
namespace dom {

///
/// A rectangle in CSS pixels, relative to the viewport (DOMRect).
///
/// The coordinates are in the same space as event client positions and
/// Document::elementFromPoint().
///
/// \parblock
/// @note  For an element in a subframe, the viewport is that frame's own viewport.
/// \endparblock
///
/// \parblock
/// @note  CSS pixels are device pixels divided by the View's device scale.
/// \endparblock
///
/// @see  Element::getBoundingClientRect(), Range::getBoundingClientRect()
///
struct DOMRect {
  double x = 0;       ///< The horizontal position.
  double y = 0;       ///< The vertical position.
  double width = 0;   ///< The width (can be negative).
  double height = 0;  ///< The height (can be negative).

  ///
  /// Get the left edge (left). This is `x` unless the width is negative.
  ///
  double left() const { return width >= 0 ? x : x + width; }

  ///
  /// Get the top edge (top). This is `y` unless the height is negative.
  ///
  double top() const { return height >= 0 ? y : y + height; }

  ///
  /// Get the right edge (right).
  ///
  double right() const { return width >= 0 ? x + width : x; }

  ///
  /// Get the bottom edge (bottom).
  ///
  double bottom() const { return height >= 0 ? y + height : y; }
};

}  // namespace dom
}  // namespace ultralight
