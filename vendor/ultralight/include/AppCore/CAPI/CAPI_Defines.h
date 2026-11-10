///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///

///
/// @file CAPI_Defines.h
///
/// Common declarations for the AppCore C API.
///
/// `#include <AppCore/CAPI/CAPI_Defines.h>`
///
#ifndef APPCORE_CAPI_DEFINES_H
#define APPCORE_CAPI_DEFINES_H

#include <Ultralight/CAPI.h>

#if defined(ULTRALIGHT_STATIC_BUILD)
#  define ACExport
#else
#  if defined(__WIN32__) || defined(_WIN32)
#    if defined(APPCORE_IMPLEMENTATION)
#      define ACExport __declspec(dllexport)
#    else
#      define ACExport __declspec(dllimport)
#    endif
#  else
#    define ACExport __attribute__((visibility("default")))
#  endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/// Opaque handle to a Settings object. @see  ulCreateSettings(), ulDestroySettings()
typedef struct C_Settings* ULSettings;

/// Opaque handle to an App object. @see  ulCreateApp(), ulDestroyApp()
typedef struct C_App* ULApp;

/// Opaque handle to a Window object. @see  ulCreateWindow(), ulDestroyWindow()
typedef struct C_Window* ULWindow;

/// Opaque handle to a Monitor object. @see  ulAppGetMainMonitor()
typedef struct C_Monitor* ULMonitor;

/// Opaque handle to a repeating App timer. @see  ulAppSetInterval(), ulDestroyAppTimer()
typedef struct C_AppTimer* ULAppTimer;

///
/// A rectangle in logical (DPI-independent) pixels.
///
/// The space each rect is relative to is stated per function.
///
typedef struct {
  double x;
  double y;
  double width;
  double height;
} ULLayoutRect;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // APPCORE_CAPI_DEFINES_H
