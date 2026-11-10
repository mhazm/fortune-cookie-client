///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include "Defines.h"

namespace ultralight {

///
/// A platform-specific monitor.
///
/// ## Coordinate Systems
///
/// A monitor may have a device scale applied by the OS (for example, a Retina display on macOS
/// may have a 2x or 3x DPI scale).
///
/// Unadorned dimensions in this API are logical pixels (device-scale independent). The device_*
/// accessors report physical device pixels. The two are related by scale():
///
/// ```
/// device_pixels = round(logical_pixels * scale)
/// ```
///
class AExport Monitor {
public:
  virtual ~Monitor() {}

  ///
  /// Get the unique display ID of the monitor.
  ///
  /// The renderer uses this ID to identify which monitor a View is on (ViewConfig::display_id).
  ///
  /// The App tracks the refresh of each monitor it enumerates and automatically calls
  /// Renderer::RefreshDisplay() with this ID to drive animation in the corresponding View%s.
  ///
  /// @see  App::monitor()
  ///
  virtual uint32_t display_id() const = 0;

  ///
  /// Get the DPI scale (1.0 = 100%).
  ///
  virtual double scale() const = 0;

  ///
  /// Get the width of the monitor (in logical pixels).
  ///
  virtual double width() const = 0;

  ///
  /// Get the height of the monitor (in logical pixels).
  ///
  virtual double height() const = 0;

  ///
  /// Get the width of the monitor (in device pixels).
  ///
  virtual uint32_t device_width() const = 0;

  ///
  /// Get the height of the monitor (in device pixels).
  ///
  virtual uint32_t device_height() const = 0;

  ///
  /// Get the refresh rate of the monitor (in Hz).
  ///
  /// This is the nominal rate of the monitor's current display mode (60 when the rate is
  /// unavailable). A variable-refresh display reports its maximum rate.
  ///
  virtual uint32_t refresh_rate() const = 0;
};

}  // namespace ultralight
