///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/Defines.h>

namespace ultralight {

///
/// Scroll event representing a change in scroll state.
///
/// Positive deltas scroll up (or left), like a mouse wheel turned away from the user. Pages that
/// read the legacy `wheelDelta` properties see 120 for every 100 pixels (or every page) of delta.
///
/// @note  Modifier keys reported to the page (eg, `ctrlKey`) come from the last KeyEvent fired
///        on the View.
///
/// @see  View::FireScrollEvent()
///
class ScrollEvent {
 public:
  ///
  /// The scroll event granularity type
  ///
  enum Type {
    kType_ScrollByPixel, ///< The delta value will be interpreted as number of pixels to scroll.
    kType_ScrollByPage,  ///< The delta value will be interpreted as number of pages to scroll.
  };

  ///
  /// Scroll granularity type
  ///
  Type type;

  ///
  /// Horizontal scroll amount, in the unit named by `type` (logical pixels, or pages).
  ///
  int delta_x;

  ///
  /// Vertical scroll amount, in the unit named by `type` (logical pixels, or pages).
  ///
  int delta_y;

  ///
  /// The x-position of the cursor when the event occurred, in logical pixels relative to the
  /// View. Leave negative to dispatch at the last known cursor position instead; if either
  /// coordinate is negative, both are ignored.
  ///
  int x = -1;

  ///
  /// The y-position of the cursor when the event occurred, in logical pixels relative to the
  /// View. Same sentinel rule as `x`.
  ///
  int y = -1;
};

} // namespace ultralight
