///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/Defines.h>

#if UL_HAS(PROFILER)

#include <cstdint>

namespace ultralight {

///
/// User-defined profiling interface.
///
/// Implement this class to receive profiling callbacks from the library. The callbacks provide
/// structured timing data (scopes, events, counters) that can be forwarded to any profiling
/// backend -- Tracy, Optick, Unreal Insights, the built-in Perfetto trace file writer provided
/// below (CreateProfilerPerfetto()), or a custom implementation.
///
/// @pre  Requires the Pro edition or higher.
///
/// ## Usage
///
/// 1. Subclass `Profiler` and implement all pure virtual methods.
/// 2. Call `Platform::instance().set_profiler(myProfiler)` before creating the Renderer or App.
/// 3. Your profiler instance should outlive the Renderer/App.
///
/// ## Frame Timing
///
/// The library automatically calls `BeginFrame()` and `EndFrame()` around each rendering frame.
/// Implement these to measure dirty-to-display latency and detect jank.
///
/// ## Threading
///
/// All methods may be called concurrently from any thread. Implementations must be thread-safe.
///
/// @see Platform::set_profiler
///
class UExport Profiler {
public:
  virtual ~Profiler();

  ///
  /// Enter a named profiling scope.
  ///
  /// Calls are always balanced (`BeginScope` / `EndScope` pairs). Implementations should maintain
  /// a per-thread stack of active scopes.
  ///
  /// @param  name  A static string literal identifying the scope (e.g., `"Ultralight.Layout"`).
  ///               The string should be valid for the lifetime of the library.
  ///
  virtual void BeginScope(const char* name) = 0;

  ///
  /// Exit the most recently entered scope on the current thread.
  ///
  virtual void EndScope() = 0;

  ///
  /// Called when a new rendering frame begins.
  ///
  /// Called automatically by the library-- users do not call this directly.
  ///
  /// @param  frame_id  Monotonic frame counter.
  ///
  virtual void BeginFrame(uint64_t frame_id) {}

  ///
  /// Called when a rendering frame ends.
  ///
  /// Called automatically by the library-- users do not call this directly.
  ///
  virtual void EndFrame() {}

  ///
  /// Record an instant (point-in-time) event on the timeline.
  ///
  /// @param  name    A static string literal identifying the event (e.g., `"View.LoadURL"`).
  /// @param  detail  Optional context string (may be `nullptr`).
  ///
  virtual void EmitEvent(const char* name, const char* detail) = 0;

  ///
  /// Record a counter value.
  ///
  /// Called periodically with named numeric values (e.g., memory usage, object counts).
  /// Profiling tools typically render these as line or area charts.
  ///
  /// @param  name   A static string literal identifying the counter (e.g., `"Memory.VRAM"`).
  /// @param  value  The current value.
  ///
  virtual void EmitCounter(const char* name, int64_t value) = 0;

  ///
  /// Begin an async event that may span multiple frames.
  ///
  /// Unlike scopes, async events can overlap and are not bound to a single call stack.
  /// Use these for long-running operations like page loads or resource fetches.
  ///
  /// @param  name    A static string literal identifying the event type (e.g., `"View.PageLoad"`).
  /// @param  detail  Optional context string (may be `nullptr`).
  ///
  /// @return  A non-zero event ID that must be passed to `EndEvent()` to close the span.
  ///          Returning 0 is reserved as a "no event" sentinel.
  ///
  virtual uint64_t BeginEvent(const char* name, const char* detail) = 0;

  ///
  /// End a previously started async event.
  ///
  /// @param  id  The event ID returned by `BeginEvent()`.
  ///
  virtual void EndEvent(uint64_t id) = 0;
};

///
/// Create a Profiler implementation that writes Perfetto binary traces (.perfetto-trace).
///
/// The trace file is streamed to disk with bounded memory usage and is crash-safe
/// (partial traces are always valid and loadable in https://ui.perfetto.dev).
///
/// @param  path  Output file path (e.g., "trace.perfetto-trace").
///
/// @return  Owned Profiler pointer, or nullptr if unavailable for your platform or edition.
///          Caller is responsible for deletion via DestroyProfilerPerfetto().
///
UExport Profiler* CreateProfilerPerfetto(const char* path);

///
/// Destroy a Profiler created by CreateProfilerPerfetto. Safe to call with nullptr.
///
UExport void DestroyProfilerPerfetto(Profiler* profiler);

}  // namespace ultralight

#endif // UL_HAS(PROFILER)
