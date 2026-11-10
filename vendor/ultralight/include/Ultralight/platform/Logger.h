///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/Defines.h>
#include <Ultralight/String.h>

namespace ultralight {

///
/// The severity of a log message, from most to least severe.
///
/// @see  DiagnosticsConfig::min_log_level
///
enum class LogLevel : uint8_t {
  ///
  /// The application can't continue (eg, a failed device or startup, missing ICU data, or a
  /// required platform interface that wasn't set).
  ///
  Fatal,

  ///
  /// A failure the library recovers from (eg, a resource that failed to load).
  ///
  Error,

  ///
  /// A problem or diagnostic that isn't a failure (eg, a misused API call).
  ///
  Warning,

  ///
  /// Status information.
  ///
  Info,

  ///
  /// Verbose detail (not sent unless you lower DiagnosticsConfig::min_log_level to Debug).
  ///
  Debug,
};

///
/// User-defined logging interface.
///
/// The library uses this to report failures, warnings, and status messages.
///
/// This is intended to be implemented by users and defined before creating the Renderer.
///
/// Each message starts with a tag in brackets for the part of the library it came from (eg,
/// `[net] Failed to load cacert.pem ...`). The library doesn't send messages below
/// DiagnosticsConfig::min_log_level.
///
/// @note  The library calls LogMessage() from any of its threads, and more than one can be inside
///        it at the same time, so your implementation **must** serialize its writes.
///
/// @see  Platform::set_logger()
///
class UExport Logger {
public:
  virtual ~Logger();

  ///
  /// Called when the library wants to display a log message.
  ///
  /// @param  log_level  The severity of the message.
  ///
  /// @param  message    The message text.
  ///
  virtual void LogMessage(LogLevel log_level, const String& message) = 0;
};

}  // namespace ultralight
