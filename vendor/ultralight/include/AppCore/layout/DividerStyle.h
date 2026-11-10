///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <AppCore/Defines.h>
#include <Ultralight/Color.h>

namespace ultralight {

///
/// Line thickness, colors, and grab width for draggable pane dividers.
///
/// Resizable Container%s place draggable dividers between direct children so users can adjust split
/// panes in a window. DividerStyle controls the appearance of those dividers and the size of their
/// pointer hit target.
///
/// Pass a style to Window::SetDividerStyle() to format dividers across the window:
///
/// ```
/// window->SetDividerStyle({ .visual_thickness = 1, .color = "#3c3c3c" });
/// ```
///
/// ## Window and Container Styles
///
/// You can configure dividers across the window or on an individual container:
///
/// - **Window::SetDividerStyle() sets the baseline for all dividers in the window.** Any field left
///   unset (a zero width or an unset Color) falls back to the built-in defaults.
/// - **Container::SetDividerStyle() overrides the window style for dividers between that
///   container's direct children.** Any field left unset falls back to the window's style, while
///   passing a default-constructed DividerStyle clears the override.
///
/// Set an override on a container to change specific properties:
///
/// ```
/// RefPtr<Container> body =
///     window->layout()->AddRow({ .key = "body", .resizable = true });
/// body->SetDividerStyle({ .hover_color = "#0078d4" });  // the rest comes from the window
/// ```
///
/// @note  Dividers paint no line by default (an unset `color` lets the window background show
///        through).
///
/// @see  Window::SetDividerStyle(), Container::SetDividerStyle(), ContainerOptions::resizable
///
struct DividerStyle {
  ///
  /// The width of the divider's pointer target, in logical px. (Default: 6)
  ///
  double hit_width = 0.0;

  ///
  /// The thickness of the painted divider line, in logical px. (Default: 1)
  ///
  double visual_thickness = 0.0;

  ///
  /// The color of the divider line. Unset means no line is painted and the window background
  /// shows through.
  ///
  Color color;

  ///
  /// The hover highlight color, painted over the divider while the pointer rests on it or drags
  /// it. (Default: a half-opacity gray)
  ///
  Color hover_color;
};

}  // namespace ultralight
