///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// Implementation detail of the `<Ultralight/dom/data/*.h>` headers; include those
/// instead. Everything here lives in ultralight::dom::data::detail.
///
/// RecordOps is filled by the library and called from code compiled into the embedder's
/// binary, so its layout is ABI even though it is not a documented API: fields are only
/// ever appended.
///
/// The slot-record surface the Snapshot/Model views ride: one function table per bound
/// type table, implemented by the engine's record (and by test doubles). C-shaped on
/// purpose: the views are header-only and must reach the record without linking C++
/// symbols; only ABI-stable currencies cross (String by pointer, StyleValue and Color by
/// value).
/// Slot indices are the entry ordinals of the owning type's schema, in declaration order.
///
/// \cond INTERNAL
#pragma once
#include <Ultralight/String.h>
#include <Ultralight/Color.h>
#include <Ultralight/dom/StyleValue.h>

#include <cstdint>

namespace ultralight {
namespace dom {
namespace data {
namespace detail {

struct RecordOps {
  void* state = nullptr;

  // Leaf reads: the slot's current value (last committed, plus this pass's earlier
  // writes).
  bool (*get_bool)(void* state, uint32_t slot) = nullptr;
  int64_t (*get_int64)(void* state, uint32_t slot) = nullptr;
  double (*get_double)(void* state, uint32_t slot) = nullptr;
  void (*get_string)(void* state, uint32_t slot, String* out) = nullptr;
  StyleValue (*get_style)(void* state, uint32_t slot) = nullptr;

  // Leaf staging writes. The record owns the equality cutoff: a write of an unchanged
  // value must not mark the slot dirty.
  void (*set_bool)(void* state, uint32_t slot, bool value) = nullptr;
  void (*set_int64)(void* state, uint32_t slot, int64_t value) = nullptr;
  void (*set_double)(void* state, uint32_t slot, double value) = nullptr;
  void (*set_string)(void* state, uint32_t slot, const String* value) = nullptr;
  void (*set_style)(void* state, uint32_t slot, StyleValue value) = nullptr;

  // Nullable sub-objects: presence gates the child subtree (absent means the whole
  // subtree is absent; the child table's contents are irrelevant while absent).
  void (*set_present)(void* state, uint32_t slot, bool present) = nullptr;

  // Nested object tables.
  RecordOps* (*child)(void* state, uint32_t slot) = nullptr;

  // Keyed/index list publish protocol: begin with the row count, address each row in
  // display order (by declared key for keyed lists, by ordinal otherwise), end to commit
  // the row set. The returned row table is walked like any object table.
  void (*list_begin)(void* state, uint32_t slot, uint64_t count) = nullptr;
  RecordOps* (*list_row)(void* state, uint32_t slot, uint64_t ordinal) = nullptr;
  RecordOps* (*list_row_key_int64)(void* state, uint32_t slot, uint64_t ordinal,
                                   int64_t key) = nullptr;
  RecordOps* (*list_row_key_string)(void* state, uint32_t slot, uint64_t ordinal,
                                    const String* key) = nullptr;
  void (*list_end)(void* state, uint32_t slot) = nullptr;

  // Color leaf access. Appended after the v1 fields (like the descriptor table, the
  // struct grows at the end only, so an older embedder's view stays layout-identical).
  Color (*get_color)(void* state, uint32_t slot) = nullptr;
  void (*set_color)(void* state, uint32_t slot, Color value) = nullptr;
};

}  // namespace detail
}  // namespace data
}  // namespace dom
}  // namespace ultralight
/// \endcond
