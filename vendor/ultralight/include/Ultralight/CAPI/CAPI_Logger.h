///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///

///
/// @file CAPI_Logger.h
///
/// User-defined logging interface.
///
/// `#include <Ultralight/CAPI/CAPI_Logger.h>`
///
/// The library uses this to report failures, warnings, and status messages.
///
/// This is intended to be implemented by users and defined before creating the Renderer.
///
/// Each message starts with a tag in brackets for the part of the library it came from (eg,
/// `[net] Failed to load cacert.pem ...`). The library doesn't send messages below the minimum
/// log level (see ulConfigSetMinLogLevel()).
///
/// @note  The library invokes your callback from any of its threads, and more than one call can
///        be in progress at the same time, so your implementation **must** serialize its writes.
///
/// @see  ulPlatformSetLogger()
///
#ifndef ULTRALIGHT_CAPI_LOGGER_H
#define ULTRALIGHT_CAPI_LOGGER_H

#include <Ultralight/CAPI/CAPI_Defines.h>

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
 * Logger
 *****************************************************************************/

///
/// The severity of a log message, from most to least severe.
///
/// @see  ulConfigSetMinLogLevel()
///
typedef enum {
  ///
  /// The application can't continue (eg, a failed device or startup, missing ICU data, or a
  /// required platform interface that wasn't set).
  ///
  kLogLevel_Fatal = 0,

  ///
  /// A failure the library recovers from (eg, a resource that failed to load).
  ///
  kLogLevel_Error,

  ///
  /// A problem or diagnostic that isn't a failure (eg, a misused API call).
  ///
  kLogLevel_Warning,

  ///
  /// Status information.
  ///
  kLogLevel_Info,

  ///
  /// Verbose detail (not sent unless you lower the minimum log level to kLogLevel_Debug with
  /// ulConfigSetMinLogLevel()).
  ///
  kLogLevel_Debug,
} ULLogLevel;

///
/// The callback invoked when the library wants to print a message to the log.
///
/// @param  log_level  The severity level of the log message.
///
/// @param  message    The log message string. Valid only for the duration of this callback.
///
typedef void (*ULLoggerLogMessageCallback)(ULLogLevel log_level, ULString message);

///
/// User-defined logging interface.
///
/// You should implement the callback, then pass an instance of this struct containing your
/// callback to ulPlatformSetLogger(). The library copies the struct. A NULL `log_message` is
/// allowed (messages are then dropped).
///
typedef struct {
  ULLoggerLogMessageCallback log_message;
} ULLogger;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ULTRALIGHT_CAPI_LOGGER_H