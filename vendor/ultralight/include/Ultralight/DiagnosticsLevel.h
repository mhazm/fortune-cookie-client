///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <cstdint>

namespace ultralight {

///
/// How much checking and warning the library does for mistakes made through one of its APIs.
///
/// Warnings go to the Logger at LogLevel::Warning, tagged with the API they came from (eg,
/// `[js]`, `[dom]`, `[data]`), and to the page's console when a page is involved.
///
/// @see  DiagnosticsConfig
///
enum class DiagnosticsLevel : uint8_t {
  ///
  /// No per-operation warnings (a mistake the library only finds once, like an invalid setup
  /// call, still warns).
  ///
  Off = 0,

  ///
  /// Development warnings about each mistake as it happens.
  ///
  Warn,

  ///
  /// Everything Warn reports, plus stricter checks (eg, a lossy argument conversion in a
  /// JavaScript binding becomes a TypeError, and every DOM call on an empty handle warns).
  ///
  Strict,

  ///
  /// Warn when DiagnosticsConfig::developer_mode is on, Off when it's off.
  ///
  Auto,
};

}  // namespace ultralight
