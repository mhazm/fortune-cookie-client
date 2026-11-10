///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///

///
/// @file CAPI_ThreadFactory.h
///
/// User-defined factory for creating new threads.
///
/// `#include <Ultralight/CAPI/CAPI_ThreadFactory.h>`
///
/// You can implement this interface so that the library will use your own implementation for
/// creating threads (useful for tracking thread creation, setting thread names, etc).
///
/// The factory covers the threads that run page content: script workers, the JavaScript compiler
/// and garbage collector, networking, and resource loading. The library starts a few threads
/// outside the factory (media playback and some background rendering work), and the libraries it
/// links start their own, so a factory won't account for every thread in the process.
///
/// ## Default Implementation
///
/// When no factory is defined, the library will create threads using the default platform-specific
/// thread creation functions (eg, `_beginthreadex()` on Windows, `pthread_create()` on POSIX).
///
/// @see ulPlatformSetThreadFactory()
///
#ifndef ULTRALIGHT_CAPI_THREADFACTORY_H
#define ULTRALIGHT_CAPI_THREADFACTORY_H

#include <Ultralight/CAPI/CAPI_Defines.h>

#ifdef __cplusplus
extern "C" {
#endif

///
/// Unique id of the thread, used for referencing the created thread later.
///   * on Windows this should be the thread identifier (not the HANDLE that _beginthreadex()
///     returns): the id _beginthreadex() writes through its `thrdaddr` out-parameter, or
///     GetCurrentThreadId() called on the new thread
///   * on POSIX this can be whatever unique id you want
///
typedef unsigned int ULThreadId;

///
/// Platform-specific handle
///   * on Windows this is HANDLE
///   * on POSIX this is pthread_t
///
typedef unsigned long long ULThreadHandle;

///
/// Entry point for the thread, this function should be called by the thread once it is active
/// and should be passed entry_point_data as the argument.
///
typedef void (*ULThreadEntryPoint)(void*);

///
/// Result of creating a new thread.
///
/// This struct is used to return the id and handle of the created thread.
///
typedef struct {
  ULThreadId id;         ///< The unique id of the thread. @see ULThreadId
  ULThreadHandle handle; ///< The platform-specific handle of the thread. @see ULThreadHandle
} ULCreateThreadResult;

///
/// The callback invoked when the library wants to create a new thread.
///
/// @param  user_data          A user-defined pointer, see ULThreadFactory.
///
/// @param  name               The name of the thread (can be NULL).
///
/// @param  type               The type of thread.
///
/// @param  entry_point        The entry point for the thread.
///
/// @param  entry_point_data   The data to pass to the entry point.
///
/// @param  result             The resulting id and handle of the thread creation.
///
/// @return  Returns whether or not the thread was created successfully.
///
typedef bool (*ULThreadFactoryCreateThreadCallback)(void* user_data, const char* name,
                                                    ULThreadType type,
                                                    ULThreadEntryPoint entry_point,
                                                    void* entry_point_data,
                                                    ULCreateThreadResult* result);

///
/// User-defined factory for creating new threads.
///
/// You should implement each of these callbacks, then pass an instance of this struct containing
/// your callbacks to ulPlatformSetThreadFactory().
///
typedef struct {
  ///
  /// A user-defined pointer passed to every callback (can be NULL).
  ///
  void* user_data;

  ///
  /// Called to create a new thread.
  ///
  ULThreadFactoryCreateThreadCallback create_thread;
} ULThreadFactory;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ULTRALIGHT_CAPI_THREADFACTORY_H
