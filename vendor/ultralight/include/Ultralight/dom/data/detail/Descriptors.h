///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// Implementation detail of the `<Ultralight/dom/data/*.h>` headers; include those
/// instead. Everything here lives in ultralight::dom::data::detail.
///
/// The descriptor tables: the compiled, C-shaped form of a described type's schema that
/// crosses the library boundary. A table is compiled into the embedder's binary and read
/// by the library, so these layouts are ABI even though they are not a documented API:
/// a layout change bumps kDescriptorFormatVersion, and the library refuses to bind a
/// table of any other version. Tools read schemas through the schema JSON and the C
/// ulDOMDataTypeGet* functions instead.
///
/// \cond INTERNAL
#pragma once
#include <Ultralight/dom/data/Model.h>
#include <Ultralight/dom/data/TypeTraits.h>
#include <Ultralight/dom/data/detail/EntryInfo.h>

#include <string>

namespace ultralight {
namespace dom {
namespace data {
namespace detail {

struct DescriptorKV {
  const char* key;
  const char* value;
};

struct TypeDescriptor;

// Every reader and validator takes the entry's kind; at most the one matching `kind` is
// set. Readers are set for fields and Vars with an accessor; validators for fields with a
// Validate. A compiled table links children through the nullary `child` (the indirection
// breaks self-referential cycles); a runtime-built table sets `child_table` instead. Reach
// a child only through ChildTableOf().
struct EntryDescriptor {
  const char* name;
  uint8_t kind;      // ValueKind (an enum field is a String).
  uint8_t flags;     // kEntryFlags_* bits.
  uint8_t key_kind;  // A keyed list's key ValueKind, else kDescriptorNoKeyKind.
  uint16_t slot;     // The entry's index in its table's `entries`.

  const TypeDescriptor* (*child)();
  const TypeDescriptor* child_table;

  bool (*read_bool)(const void* instance);
  int64_t (*read_int64)(const void* instance);
  double (*read_double)(const void* instance);
  void (*read_string)(const void* instance, String* out);
  StyleValue (*read_style)(const void* instance);
  Color (*read_color)(const void* instance);

  bool (*validate_bool)(bool* value, RecordOps* published);
  bool (*validate_int64)(int64_t* value, RecordOps* published);
  bool (*validate_double)(double* value, RecordOps* published);
  bool (*validate_string)(String* value, RecordOps* published);
  bool (*validate_color)(Color* value, RecordOps* published);

  // An enum field has one pair keyed "enum" whose value joins the enumerator names with
  // commas.
  const DescriptorKV* kv;
  uint32_t kv_count;
};

// A table and everything it points to live for the lifetime of the program (for compiled
// tables: while the declaring module stays loaded).
struct TypeDescriptor {
  uint32_t format_version;  // Must equal kDescriptorFormatVersion.
  const char* type_name;    // Unqualified; "" when unavailable.
  uint32_t entry_count;
  const EntryDescriptor* entries;  // In schema order.
};

inline constexpr uint32_t kDescriptorFormatVersion = 5;

inline constexpr uint8_t kDescriptorNoKeyKind = 0xFF;

// Descriptor-only flag bits above the public EntryFlags. Never part of the C introspection
// flags or the schema JSON.
inline constexpr uint8_t kEntryFlags_Float32 = 1 << 6;  // The field's C++ type is float.
inline constexpr uint8_t kPublicEntryFlagsMask = kEntryFlags_Float32 - 1;

template <typename T>
const TypeDescriptor* Descriptors();

}  // namespace detail
}  // namespace data
}  // namespace dom
}  // namespace ultralight

// Descriptor lowering machinery (read/validator thunks, per-type table storage). Definitions
// must follow the table declarations above and precede Descriptors() below.
#include <Ultralight/dom/data/detail/DescriptorsImpl.h>

namespace ultralight {
namespace dom {
namespace data {
namespace detail {

// The table of a described type, built at compile time from its schema (never null).
// Safe to call from any thread.
template <typename T>
const TypeDescriptor* Descriptors() {
  return &kTypeDescriptor<std::remove_cvref_t<T>>;
}

}  // namespace detail
}  // namespace data
}  // namespace dom
}  // namespace ultralight
/// \endcond
