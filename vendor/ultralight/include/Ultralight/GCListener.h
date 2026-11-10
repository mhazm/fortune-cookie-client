///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/Defines.h>

#if UL_HAS(GC_LISTENER)

#include <cstdint>

namespace ultralight {

class Renderer;

///
/// How much compiled JavaScript code a collection discards.
///
/// Compiled code keeps some JavaScript objects alive, so discarding it lets a collection free more
/// memory. Pages then run slower for a while as their code is compiled again.
///
enum class CodeDropMode : uint8_t {
  Nothing,  ///< Keep all compiled code.
  Linked,   ///< Discard compiled code but keep parsed scripts (pages recompile but don't
            ///< re-parse).
  All,      ///< Discard compiled code and parsed scripts (pages re-parse and recompile).
};

///
/// A snapshot of the JavaScript heap's state (see Renderer::gc_status()).
///
struct GCInfo {
  ///
  /// How much reclaimable memory has built up, from 0 (none) to 1 (a collection is overdue).
  ///
  double pressure;

  ///
  /// Milliseconds since the last full collection (0 before the first one).
  ///
  double time_since_last_collect_ms;

  ///
  /// The JavaScript heap's in-use size in bytes (it drops when a collection frees memory).
  ///
  uint64_t heap_bytes;
};

///
/// User-defined interface to observe and control the Renderer's JavaScript garbage collections.
///
/// The Renderer collects JavaScript garbage continuously in the background on other threads, so
/// most collection work never touches your thread. Occasionally it needs a larger collection that
/// runs on the thread that calls Renderer::Update() and pauses it while it runs.
///
/// Your application knows better than the engine when a pause won't be noticed (eg, a loading
/// screen or a quiet moment in your update loop). So before running one of these collections, the
/// engine asks through OnRequestGC(). You can let it run right away (the default), run it later
/// with Renderer::CollectNow(), or skip it.
///
/// Attach a listener with Renderer::set_gc_listener(). Callbacks run during Renderer::Update() on
/// the thread that calls it.
///
/// | Callback            | When                                                |
/// |---------------------|-----------------------------------------------------|
/// | OnRequestGC()       | The engine wants to run a collection on your thread |
/// | OnPressureChanged() | Reclaimable memory rises past a threshold           |
/// | OnCollectComplete() | A collection on your thread finished                |
///
/// Background collections aren't reported here.
///
/// ## Choosing When to Collect
///
/// Override OnRequestGC() to skip the collection, then run it yourself later with
/// Renderer::CollectNow():
///
/// ```
/// class MyGCScheduler : public GCListener {
///  public:
///   void OnRequestGC(Renderer* caller, const GCInfo& info, CodeDropMode mode) override {
///     // Don't collect now. Remember the request for later.
///     pending_ = true;
///     mode_ = mode;
///   }
///
///   // Call this from your run loop when a short pause won't be noticed.
///   void RunPendingGC(Renderer* renderer) {
///     if (pending_) {
///       renderer->CollectNow(mode_);
///       pending_ = false;
///     }
///   }
///
///  private:
///   bool pending_ = false;
///   CodeDropMode mode_ = CodeDropMode::Nothing;
/// };
/// ```
///
/// @warning  Don't detach or destroy the listener from inside one of its callbacks.
///
/// @pre  Requires the Pro edition or higher.
///
class UExport GCListener {
 public:
  virtual ~GCListener() = default;

  ///
  /// Called when the engine wants to run a collection on your thread.
  ///
  /// The engine asks when there's been no input for a while (see Config::idle_gc_enabled). One
  /// idle period can bring two requests: a lighter one first, then, if the app stays idle, one
  /// that drops more compiled code. A request you don't act on isn't offered again until new
  /// input ends the idle period.
  ///
  /// The default implementation collects right away (Renderer::CollectNow() with `mode`). Override
  /// it to collect later or not at all.
  ///
  /// @param  caller  The Renderer to collect on.
  ///
  /// @param  info    The current heap state.
  ///
  /// @param  mode    How much compiled code the engine recommends discarding (pass it to
  ///                 Renderer::CollectNow()).
  ///
  /// @note  Call Renderer::CollectNow() on this same thread.
  ///
  virtual void OnRequestGC(Renderer* caller, const GCInfo& info, CodeDropMode mode);

  ///
  /// Called when reclaimable memory rises past a threshold (see GCInfo::pressure).
  ///
  /// You can use this to decide when to reclaim memory yourself (eg, with Renderer::CollectNow()).
  /// It fires again only after the pressure drops and rises again.
  ///
  /// @param  info  The current heap state.
  ///
  virtual void OnPressureChanged(const GCInfo& info) { }

  ///
  /// Called after a collection on your thread finishes.
  ///
  /// This covers the engine's own collections and the ones you start with Renderer::CollectNow(),
  /// Renderer::Recycle(), or Renderer::PurgeMemory(). It's called during the next
  /// Renderer::Update().
  ///
  /// @param  info             The heap state when this is called.
  ///
  /// @param  code             How much compiled code the collection was asked to discard.
  ///
  /// @param  duration_ms      How long the collection took in milliseconds.
  ///
  /// @param  bytes_reclaimed  How many bytes the collection freed.
  ///
  virtual void OnCollectComplete(const GCInfo& info, CodeDropMode code,
                                 double duration_ms, uint64_t bytes_reclaimed) { }
};

} // namespace ultralight

#endif // UL_HAS(GC_LISTENER)
