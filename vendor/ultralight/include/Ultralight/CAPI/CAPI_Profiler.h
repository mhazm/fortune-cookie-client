///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///

///
/// @file CAPI_Profiler.h
///
/// User-defined profiling interface.
///
/// `#include <Ultralight/CAPI/CAPI_Profiler.h>`
///
/// When set, the library emits structured timing data (scopes, events, counters) at key
/// internal boundaries that can be forwarded to any profiling tool or trace format.
///
/// This is intended to be implemented by users and set before creating the Renderer.
/// The profiler must outlive the Renderer/App.
///
/// @see ulPlatformSetProfiler()
///
#ifndef ULTRALIGHT_CAPI_PROFILER_H
#define ULTRALIGHT_CAPI_PROFILER_H

#include <Ultralight/CAPI/CAPI_Defines.h>

#if UL_HAS(PROFILER)

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
 * Profiler
 *****************************************************************************/

///
/// The callback invoked when the library enters a named profiling scope.
///
/// @param  name  Static string literal identifying the scope (e.g., "Ultralight.Layout").
///               Pointer is stable for the lifetime of the library.
///
typedef void (*ULProfilerBeginScopeCallback)(const char* name);

///
/// The callback invoked when the library exits the most recently entered scope.
///
typedef void (*ULProfilerEndScopeCallback)(void);

///
/// The callback invoked when a new rendering frame begins.
/// Called automatically by the library.
///
/// @param  frame_id  Monotonic frame counter.
///
typedef void (*ULProfilerBeginFrameCallback)(uint64_t frame_id);

///
/// The callback invoked when a rendering frame ends.
/// Called automatically by the library.
///
typedef void (*ULProfilerEndFrameCallback)(void);

///
/// The callback invoked for instant (point-in-time) annotations.
///
/// @param  name    Static string literal identifying the event.
/// @param  detail  Optional detail string (may be NULL).
///
typedef void (*ULProfilerEmitEventCallback)(const char* name, const char* detail);

///
/// The callback invoked for counter/gauge updates.
///
/// @param  name   Static string literal identifying the counter.
/// @param  value  Current value of the counter.
///
typedef void (*ULProfilerEmitCounterCallback)(const char* name, int64_t value);

///
/// The callback invoked when an async event begins.
///
/// @param  name    Static string literal identifying the event type.
/// @param  detail  Optional detail string (may be NULL).
///
/// @return  Implementation-defined event ID (passed to end_event to close). Must be non-zero.
///
typedef uint64_t (*ULProfilerBeginEventCallback)(const char* name, const char* detail);

///
/// The callback invoked when an async event ends.
///
/// @param  id  The event ID returned by begin_event.
///
typedef void (*ULProfilerEndEventCallback)(uint64_t id);

typedef struct {
  ULProfilerBeginScopeCallback begin_scope;
  ULProfilerEndScopeCallback end_scope;
  ULProfilerBeginFrameCallback begin_frame;   ///< May be NULL.
  ULProfilerEndFrameCallback end_frame;       ///< May be NULL.
  ULProfilerEmitEventCallback emit_event;
  ULProfilerEmitCounterCallback emit_counter;
  ULProfilerBeginEventCallback begin_event;
  ULProfilerEndEventCallback end_event;
} ULProfiler;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // UL_HAS(PROFILER)

#endif // ULTRALIGHT_CAPI_PROFILER_H
