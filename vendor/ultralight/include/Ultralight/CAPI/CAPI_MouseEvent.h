///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///

///
/// @file CAPI_MouseEvent.h
///
/// Mouse event interface.
///
/// `#include <Ultralight/CAPI/CAPI_MouseEvent.h>`
///
/// This file defines the C API for mouse events.
///
#ifndef ULTRALIGHT_CAPI_MOUSEEVENT_H
#define ULTRALIGHT_CAPI_MOUSEEVENT_H

#include <Ultralight/CAPI/CAPI_Defines.h>

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
 * Mouse Event
 *****************************************************************************/

///
/// Create a mouse event.
///
/// Modifier keys reported to the page come from the last key event fired on the View, so you
/// should forward modifier key presses and releases as key events. A right-button click also
/// fires `contextmenu` on the page (after the release on Windows, after the press elsewhere); the
/// library shows no menu of its own.
///
/// @param  type    The type of mouse event.
/// @param  x       The current x-position of the mouse, in logical pixels relative to the View.
/// @param  y       The current y-position of the mouse, in logical pixels relative to the View.
/// @param  button  The mouse button that was pressed/released, if any.
///
/// @return  Returns a new ULMouseEvent instance. You must call ulDestroyMouseEvent() when finished.
///
ULExport ULMouseEvent ulCreateMouseEvent(ULMouseEventType type, int x, int y, ULMouseButton button);

///
/// Destroy a mouse event created by ulCreateMouseEvent().
///
ULExport void ulDestroyMouseEvent(ULMouseEvent evt);

///
/// Get the type of a mouse event.
///
/// @param  evt  The mouse event.
///
/// @return  Returns the type of mouse event (see ULMouseEventType).
///
ULExport ULMouseEventType ulMouseEventGetType(ULMouseEvent evt);

///
/// Get the x-position of the mouse.
///
/// @param  evt  The mouse event.
///
/// @return  Returns the x-position, in logical pixels relative to the View.
///
ULExport int ulMouseEventGetX(ULMouseEvent evt);

///
/// Get the y-position of the mouse.
///
/// @param  evt  The mouse event.
///
/// @return  Returns the y-position, in logical pixels relative to the View.
///
ULExport int ulMouseEventGetY(ULMouseEvent evt);

///
/// Get the mouse button that was pressed or released.
///
/// @param  evt  The mouse event.
///
/// @return  Returns the button, or kMouseButton_None if no button changed state.
///
ULExport ULMouseButton ulMouseEventGetButton(ULMouseEvent evt);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ULTRALIGHT_CAPI_MOUSEEVENT_H