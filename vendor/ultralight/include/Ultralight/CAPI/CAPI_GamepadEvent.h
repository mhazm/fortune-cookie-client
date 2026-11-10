///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///

///
/// @file CAPI_GamepadEvent.h
///
/// Gamepad event interface.
///
/// `#include <Ultralight/CAPI/CAPI_GamepadEvent.h>`
///
/// This file defines the C API for gamepad events.
///
/// @see ulFireGamepadEvent
///
#ifndef ULTRALIGHT_CAPI_GAMEPADEVENT_H
#define ULTRALIGHT_CAPI_GAMEPADEVENT_H

#include <Ultralight/CAPI/CAPI_Defines.h>

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
 * Gamepad Event
 *****************************************************************************/

///
/// Create a gamepad event.
///
/// @param  index  The index of the gamepad (should match ulSetGamepadDetails()).
/// @param  type   The type of gamepad event.
///
/// @return  A new ULGamepadEvent instance. You must call ulDestroyGamepadEvent() when finished.
///
ULExport ULGamepadEvent ulCreateGamepadEvent(unsigned int index, ULGamepadEventType type);

///
/// Destroy a gamepad event created by ulCreateGamepadEvent().
///
ULExport void ulDestroyGamepadEvent(ULGamepadEvent evt);

/******************************************************************************
 * Gamepad Axis Event
 *****************************************************************************/

///
/// Create a gamepad axis event.
///
/// @param  index       The index of the gamepad.
/// @param  axis_index  The index of the axis whose value has changed.
/// @param  value       The new value of the axis, normalized to [-1.0, 1.0].
///
/// @return  A new ULGamepadAxisEvent instance. You must call ulDestroyGamepadAxisEvent() when
///          finished.
///
ULExport ULGamepadAxisEvent ulCreateGamepadAxisEvent(unsigned int index, unsigned int axis_index,
                                                     double value);

///
/// Destroy a gamepad axis event created by ulCreateGamepadAxisEvent().
///
ULExport void ulDestroyGamepadAxisEvent(ULGamepadAxisEvent evt);

/******************************************************************************
 * Gamepad Button Event
 *****************************************************************************/

///
/// Create a gamepad button event.
///
/// @param  index         The index of the gamepad.
/// @param  button_index  The index of the button whose value has changed.
/// @param  value         The new value of the button, normalized to [0.0, 1.0]. Any value
///                       greater than 0.0 is considered "pressed".
///
/// @return  A new ULGamepadButtonEvent instance. You must call ulDestroyGamepadButtonEvent() when
///          finished.
///
ULExport ULGamepadButtonEvent ulCreateGamepadButtonEvent(unsigned int index,
                                                         unsigned int button_index, double value);

///
/// Destroy a gamepad button event created by ulCreateGamepadButtonEvent().
///
ULExport void ulDestroyGamepadButtonEvent(ULGamepadButtonEvent evt);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ULTRALIGHT_CAPI_GAMEPADEVENT_H