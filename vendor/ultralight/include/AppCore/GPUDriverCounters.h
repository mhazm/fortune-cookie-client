///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include "Defines.h"
#include <Ultralight/Defines.h>
#include <stdint.h>

#if UL_HAS(TESTING)

namespace ultralight {

/// \cond ignore
struct AExport GPUDriverCounters {
  uint64_t frames = 0;
  uint64_t frame_permit_wait_us = 0;
  uint64_t command_buffers_committed = 0;
  uint64_t command_buffers_completed = 0;
  uint64_t render_passes = 0;
  uint64_t render_passes_window = 0;
  uint64_t render_passes_clear = 0;
  uint64_t blit_encoders = 0;
  uint64_t draws = 0;
  uint64_t draws_dropped = 0;
  uint64_t clears_requested = 0;
  uint64_t clears_folded = 0;
  uint64_t clears_pending = 0;
  uint64_t textures_created = 0;
  uint64_t textures_destroyed = 0;
  uint64_t render_targets_created = 0;
  uint64_t render_targets_destroyed = 0;
  uint64_t texture_uploads = 0;
  uint64_t texture_upload_bytes = 0;
  uint64_t texture_uploads_partial = 0;
  uint64_t staging_buffers_reused = 0;
  uint64_t staging_buffers_allocated = 0;
  uint64_t staging_buffers_discarded = 0;
  uint64_t staging_pool_free_bytes = 0;
  uint64_t geometry_created = 0;
  uint64_t geometry_destroyed = 0;
  uint64_t geometry_updates = 0;
  uint64_t geometry_upload_bytes = 0;
  uint64_t geometry_buffers_allocated = 0;
  uint64_t drawables_acquired = 0;
  uint64_t drawables_missed = 0;
  uint64_t frames_presented = 0;
  uint64_t presented_frame_copies = 0;
  uint64_t presented_frame_textures_created = 0;
  uint64_t pipeline_states_created = 0;
  uint64_t readbacks = 0;
  uint64_t gpu_busy_us = 0;
  uint64_t gpu_timed_frames = 0;
};
/// \endcond

}  // namespace ultralight

#endif  // UL_HAS(TESTING)
