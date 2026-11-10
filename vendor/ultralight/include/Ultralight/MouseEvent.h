///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/Defines.h>

namespace ultralight {

///
/// Mouse event representing a change in mouse state.
///
/// \parblock
/// @note  Coordinates are logical pixels: the View's device scale is not applied to them, so
///        they run over View::width() and View::height() divided by View::device_scale().
///        Convert from your window's device pixels before firing the event.
/// \endparblock
///
/// \parblock
/// @note  Modifier keys reported to the page (eg, `shiftKey`) come from the last KeyEvent fired
///        on the View, so you should forward modifier key presses and releases as key events.
/// \endparblock
///
/// @see  View::FireMouseEvent()
///
class MouseEvent {
public:
  ///
  /// The various MouseEvent types.
  ///
  enum Type {
    ///
    /// Mouse moved type
    ///
    kType_MouseMoved,

    ///
    /// Mouse button pressed type
    ///
    kType_MouseDown,

    ///
    /// Mouse button released type
    ///
    kType_MouseUp,
  };

  ///
  /// The various mouse button types.
  ///
  enum Button {
    kButton_None = 0,
    kButton_Left,
    kButton_Middle,

    ///
    /// The right button. A right-button click also fires `contextmenu` on the page (after the
    /// release on Windows, after the press elsewhere); the library shows no menu of its own.
    ///
    kButton_Right,

    ///
    /// The first side button ("back"), eg, X1 on Windows. Reaches the page as
    /// `MouseEvent.button` 3; the library performs no history navigation of its own.
    ///
    kButton_Back,

    ///
    /// The second side button ("forward"), eg, X2 on Windows. Reaches the page as
    /// `MouseEvent.button` 4; the library performs no history navigation of its own.
    ///
    kButton_Forward,
  };

  ///
  /// The type of this MouseEvent
  ///
  Type type;

  ///
  /// The current x-position of the mouse, in logical pixels relative to the View.
  ///
  int x;

  ///
  /// The current y-position of the mouse, in logical pixels relative to the View.
  ///
  int y;

  ///
  /// The mouse button that was pressed/released, if any.
  ///
  Button button;
};

}  // namespace ultralight
