///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///

///
/// @file CAPI_ScrollEvent.h
///
/// Scroll event interface.
///
/// `#include <Ultralight/CAPI/CAPI_ScrollEvent.h>`
///
/// This file defines the C API for scroll events.
///
#ifndef ULTRALIGHT_CAPI_SCROLLEVENT_H
#define ULTRALIGHT_CAPI_SCROLLEVENT_H

#include <Ultralight/CAPI/CAPI_Defines.h>

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
 * Scroll Event
 *****************************************************************************/

///
/// Create a scroll event.
///
/// Positive deltas scroll up (or left), like a mouse wheel turned away from the user. Pages that
/// read the legacy `wheelDelta` properties see 120 for every 100 pixels (or every page) of delta.
/// Modifier keys reported to the page come from the last key event fired on the View.
///
/// @param  type     The scroll granularity type.
/// @param  delta_x  Horizontal scroll amount, in the unit named by `type` (logical pixels, or
///                  pages).
/// @param  delta_y  Vertical scroll amount, in the unit named by `type` (logical pixels, or
///                  pages).
/// @param  x        The x-position of the cursor when the event occurred, in logical pixels
///                  relative to the View. Pass a negative value to dispatch at the last known
///                  cursor position instead; if either coordinate is negative, both are
///                  ignored.
/// @param  y        The y-position of the cursor when the event occurred, in logical pixels
///                  relative to the View. Same sentinel rule as `x`.
///
/// @return  Returns a new ULScrollEvent instance. You must call ulDestroyScrollEvent() when
///          finished.
///
ULExport ULScrollEvent ulCreateScrollEvent(ULScrollEventType type, int delta_x, int delta_y,
                                           int x, int y);

///
/// Destroy a scroll event created by ulCreateScrollEvent().
///
ULExport void ulDestroyScrollEvent(ULScrollEvent evt);

///
/// Get the scroll granularity type of a scroll event.
///
/// @param  evt  The scroll event.
///
/// @return  Returns the scroll granularity type (see ULScrollEventType).
///
ULExport ULScrollEventType ulScrollEventGetType(ULScrollEvent evt);

///
/// Get the horizontal scroll amount.
///
/// @param  evt  The scroll event.
///
/// @return  Returns the horizontal scroll amount, in the unit named by the event's type.
///
ULExport int ulScrollEventGetDeltaX(ULScrollEvent evt);

///
/// Get the vertical scroll amount.
///
/// @param  evt  The scroll event.
///
/// @return  Returns the vertical scroll amount, in the unit named by the event's type.
///
ULExport int ulScrollEventGetDeltaY(ULScrollEvent evt);

///
/// Get the x-position of the cursor when the event occurred.
///
/// @param  evt  The scroll event.
///
/// @return  Returns the x-position, in logical pixels relative to the View, or a negative value if
///          the event dispatches at the last known cursor position.
///
ULExport int ulScrollEventGetX(ULScrollEvent evt);

///
/// Get the y-position of the cursor when the event occurred.
///
/// @param  evt  The scroll event.
///
/// @return  Returns the y-position, in logical pixels relative to the View, or a negative value if
///          the event dispatches at the last known cursor position.
///
ULExport int ulScrollEventGetY(ULScrollEvent evt);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ULTRALIGHT_CAPI_SCROLLEVENT_H