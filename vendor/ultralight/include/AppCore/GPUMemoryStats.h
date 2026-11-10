///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include "Defines.h"
#include <stdint.h>

namespace ultralight {

///
/// GPU memory statistics reported by the App's GPU driver.
///
/// Filled by App::GetGPUMemoryStats(). The per-category fields describe GPU
/// resources the library has allocated through the active GPU backend; the
/// process fields describe the video memory usage of the whole process as
/// reported by the operating system (including allocations made outside the
/// library).
///
/// How byte figures are computed depends on the active backend: backends
/// that track allocations directly report exact allocation sizes, while
/// others compute sizes from resource dimensions and formats, which may
/// slightly understate the true driver allocation (internal alignment and
/// padding are not visible).
///
/// @note  This structure may grow over time; new fields are appended and
///        zero-initialized, so an unset field always reads back as zero.
///
struct AExport GPUMemoryStats {
  ///
  /// Bytes allocated for sampled textures (images, glyph atlases, and other
  /// engine textures), not counting render targets.
  ///
  uint64_t texture_bytes = 0;

  ///
  /// Number of sampled textures currently allocated.
  ///
  uint32_t texture_count = 0;

  ///
  /// Bytes allocated for render targets, including multisample storage and
  /// any single-sample resolve textures.
  ///
  uint64_t render_target_bytes = 0;

  ///
  /// Number of render-target textures currently allocated.
  ///
  uint32_t render_target_count = 0;

  ///
  /// Bytes allocated for geometry (vertex and index buffers).
  ///
  uint64_t geometry_bytes = 0;

  ///
  /// Number of geometry buffers currently allocated.
  ///
  uint32_t geometry_count = 0;

  ///
  /// Bytes allocated for window back-buffers.
  ///
  uint64_t swap_chain_bytes = 0;

  ///
  /// Number of swap chains currently active.
  ///
  uint32_t swap_chain_count = 0;

  ///
  /// Bytes allocated for transient per-frame memory (upload staging and
  /// constant buffers).
  ///
  uint64_t transient_bytes = 0;

  ///
  /// Total bytes of GPU heap memory committed by the backend's allocator.
  ///
  /// Only reported by backends that sub-allocate resources from larger
  /// heaps; zero elsewhere. May exceed the sum of the tracked categories due
  /// to heap block granularity.
  ///
  uint64_t allocator_committed_bytes = 0;

  ///
  /// Current video memory usage of this process, as reported by the
  /// operating system. Covers the whole process, including allocations made
  /// outside the library. Zero if the OS query is unavailable.
  ///
  uint64_t process_usage_bytes = 0;

  ///
  /// Video memory budget for this process, as reported by the operating
  /// system. Usage above this value may cause the OS to throttle or evict
  /// allocations. Zero if the OS query is unavailable.
  ///
  uint64_t process_budget_bytes = 0;

  ///
  /// Dedicated video memory of the active display adapter, in bytes.
  ///
  uint64_t adapter_video_memory_bytes = 0;

  ///
  /// Human-readable name of the active display adapter (UTF-8,
  /// null-terminated, truncated if necessary).
  ///
  char adapter_description[128] = {};
};

}  // namespace ultralight
