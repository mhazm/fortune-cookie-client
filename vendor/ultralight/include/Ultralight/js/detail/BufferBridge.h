///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// Implementation detail of `<Ultralight/js/Buffer.h>`; include that header instead.
/// The element-type mapping behind js::TypedArray and the RefPtr<Buffer> bridge onto the
/// waist's ULBuffer channel.
///
/// \cond INTERNAL
#pragma once
#include <Ultralight/Buffer.h>
#include <Ultralight/CAPI/CAPI_JSBuffer.h>
#include <Ultralight/RefPtr.h>
#include <Ultralight/js/Value.h>

#include <cstdint>
#include <type_traits>
#include <utility>

namespace ultralight {
namespace js {
namespace detail {

// Maps a C++ element type to the JavaScript typed-array element type that views it. char is
// deliberately absent (text is owned by the string conversions), as is std::byte.
template <typename T>
inline constexpr ULJSTypedArrayType kTypedArrayTypeOf
    = std::is_same_v<T, int8_t>        ? kULJSTypedArrayType_Int8
    : std::is_same_v<T, uint8_t>       ? kULJSTypedArrayType_Uint8
    : std::is_same_v<T, int16_t>       ? kULJSTypedArrayType_Int16
    : std::is_same_v<T, uint16_t>      ? kULJSTypedArrayType_Uint16
    : std::is_same_v<T, int32_t>       ? kULJSTypedArrayType_Int32
    : std::is_same_v<T, uint32_t>      ? kULJSTypedArrayType_Uint32
    : std::is_same_v<T, float>         ? kULJSTypedArrayType_Float32
    : std::is_same_v<T, double>        ? kULJSTypedArrayType_Float64
    : std::is_same_v<T, int64_t>       ? kULJSTypedArrayType_BigInt64
    : std::is_same_v<T, uint64_t>      ? kULJSTypedArrayType_BigUint64
                                       : kULJSTypedArrayType_None;

template <typename T>
constexpr const char* TypedArrayNameOf() {
  constexpr ULJSTypedArrayType type = kTypedArrayTypeOf<T>;
  if constexpr (type == kULJSTypedArrayType_Int8)
    return "Int8Array";
  else if constexpr (type == kULJSTypedArrayType_Uint8)
    return "Uint8Array";
  else if constexpr (type == kULJSTypedArrayType_Int16)
    return "Int16Array";
  else if constexpr (type == kULJSTypedArrayType_Uint16)
    return "Uint16Array";
  else if constexpr (type == kULJSTypedArrayType_Int32)
    return "Int32Array";
  else if constexpr (type == kULJSTypedArrayType_Uint32)
    return "Uint32Array";
  else if constexpr (type == kULJSTypedArrayType_Float32)
    return "Float32Array";
  else if constexpr (type == kULJSTypedArrayType_Float64)
    return "Float64Array";
  else if constexpr (type == kULJSTypedArrayType_BigInt64)
    return "BigInt64Array";
  else if constexpr (type == kULJSTypedArrayType_BigUint64)
    return "BigUint64Array";
  else
    return "TypedArray";
}

// The ULJS schema-format element token ({"typedarray": "float32"}), lowercase by contract
// (TypedArrayNameOf() above is TypeError-message text, not a schema expression).
template <typename T>
constexpr const char* TypedArrayElementToken() {
  constexpr ULJSTypedArrayType type = kTypedArrayTypeOf<T>;
  if constexpr (type == kULJSTypedArrayType_Int8)
    return "int8";
  else if constexpr (type == kULJSTypedArrayType_Uint8)
    return "uint8";
  else if constexpr (type == kULJSTypedArrayType_Int16)
    return "int16";
  else if constexpr (type == kULJSTypedArrayType_Uint16)
    return "uint16";
  else if constexpr (type == kULJSTypedArrayType_Int32)
    return "int32";
  else if constexpr (type == kULJSTypedArrayType_Uint32)
    return "uint32";
  else if constexpr (type == kULJSTypedArrayType_Float32)
    return "float32";
  else if constexpr (type == kULJSTypedArrayType_Float64)
    return "float64";
  else if constexpr (type == kULJSTypedArrayType_BigInt64)
    return "bigint64";
  else if constexpr (type == kULJSTypedArrayType_BigUint64)
    return "biguint64";
  else
    return "uint8";
}

// Whether an incoming typed array's element type satisfies a view of element type `want`.
// Uint8Clamped is layout-identical to Uint8 and converts to uint8_t views.
constexpr bool ElementAccepts(ULJSTypedArrayType have, ULJSTypedArrayType want) {
  return have == want
      || (want == kULJSTypedArrayType_Uint8 && have == kULJSTypedArrayType_Uint8Clamped);
}

inline void ReleaseBufferStake(void* user_data, void* /*data*/) {
  delete static_cast<RefPtr<ultralight::Buffer>*>(user_data);
}

inline void DestroyULBufferHandle(void* user_data, void* /*data*/) {
  ulDestroyBuffer(static_cast<ULBuffer>(user_data));
}

// Bridges a RefPtr<Buffer> onto the waist's ULBuffer channel zero-copy: a transient C-level
// Buffer views the same bytes and holds a stake on `buffer`, released (deferred, on the
// Renderer's thread) when JavaScript lets go of the backing store.
template <typename MakeFn>
ULJSValue WrapBufferThrough(ULJSContext ctx, const RefPtr<ultralight::Buffer>& buffer,
                            MakeFn&& make) {
  if (!buffer)
    return ulCreateJSValueNull(ctx);
  if (!buffer->data() || buffer->size() == 0) {
    // An empty buffer has no bytes to share, so no stake is taken (a Buffer wrapping empty
    // bytes never runs its destruction callback, which would strand one); the waist returns
    // an independent empty buffer object.
    ULBuffer handle = ulCreateBuffer(nullptr, 0, nullptr, nullptr);
    ULJSValue result = make(handle);
    ulDestroyBuffer(handle);
    return result;
  }
  auto* stake = new RefPtr<ultralight::Buffer>(buffer);
  ULBuffer handle
      = ulCreateBuffer((*stake)->data(), (*stake)->size(), stake, &ReleaseBufferStake);
  ULJSValue result = make(handle);
  ulDestroyBuffer(handle);
  return result;
}

// Wraps a waist-owned ULBuffer as a RefPtr<Buffer> through the public C surface: the
// returned Buffer views the handle's bytes and destroys the handle when released.
inline RefPtr<ultralight::Buffer> AdoptULBuffer(ULBuffer handle) {
  if (!handle)
    return nullptr;
  RefPtr<ultralight::Buffer> result = ultralight::Buffer::Create(
      ulBufferGetData(handle), ulBufferGetSize(handle), handle, &DestroyULBufferHandle);
  if (!result)
    ulDestroyBuffer(handle);
  return result;
}

}  // namespace detail
}  // namespace js
}  // namespace ultralight
/// \endcond
