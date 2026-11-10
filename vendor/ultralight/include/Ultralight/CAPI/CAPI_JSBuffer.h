///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// @file CAPI_JSBuffer.h
///
/// Functions for sharing raw binary memory with JavaScript through ArrayBuffers and typed arrays.
///
/// `#include <Ultralight/CAPI/CAPI_JSBuffer.h>`
///
/// @note  This API is a preview and may still change after 2.0.
///
/// These functions allow native code and page script to share raw binary memory without copying.
/// You can pass native memory directly to JavaScript as an ArrayBuffer or typed array, and read
/// binary buffers created by the page.
///
/// For conventions governing handle ownership, NULL handles, threading, and error reporting across
/// the JavaScript C API, see CAPI_JSValue.h.
///
/// Share native memory with the page by wrapping a ULBuffer in an ArrayBuffer:
///
/// ```c
/// #include <stdlib.h>
///
/// static void FreePixels(void* user_data, void* data) {
///   free(data);
/// }
///
/// ULJSValue SharePixels(ULJSContext ctx, void* pixels, size_t size) {
///   ULBuffer buffer = ulCreateBuffer(pixels, size, NULL, FreePixels);
///   ULJSValue array_buffer = ulCreateJSArrayBufferFromBuffer(ctx, buffer);
///   ulDestroyBuffer(buffer); /* the ArrayBuffer keeps its own reference */
///   return array_buffer;
/// }
/// ```
///
/// ## Sharing Native Bytes
///
/// Native memory passes to JavaScript as an ArrayBuffer or typed array using a ULBuffer or an
/// explicit copy:
///
/// - **Share memory without copying.** Pass a ULBuffer to ulCreateJSArrayBufferFromBuffer() or
///   ulCreateJSTypedArrayFromBuffer().
/// - **Destroy your own buffer handle.** Calling ulDestroyBuffer() on your ULBuffer handle after
///   creating the JavaScript value doesn't affect the reference that the ArrayBuffer keeps.
/// - **Copy bytes when sharing isn't needed.** Call ulCreateJSArrayBuffer() to populate a new
///   ArrayBuffer with duplicate data.
///
/// ## Reading Bytes from the Page
///
/// Native code accesses bytes from an ArrayBuffer or typed array without copying:
///
/// - **Obtain a buffer handle.** Call ulJSArrayBufferGetBuffer() to receive an owned ULBuffer that
///   views an ArrayBuffer's bytes. You must call ulDestroyBuffer() when finished.
/// - **Read memory directly through a pointer.** Call ulJSArrayBufferGetBytes() for an ArrayBuffer,
///   or ulJSTypedArrayGetInfo() to fill a ULJSTypedArrayInfo struct with a typed array's element
///   type, pointer, and lengths.
/// - **Inspect lengths without reading bytes.** Call ulJSArrayBufferGetByteLength() or
///   ulJSTypedArrayGetLength() in CAPI_JSValue.h to check sizes while leaving the ArrayBuffer
///   detachable.
///
/// Inspect a typed array and read its elements through ULJSTypedArrayInfo:
///
/// ```c
/// float SumWeights(ULJSValue weights) {
///   float total = 0;
///   ULJSTypedArrayInfo info;
///   if (ulJSTypedArrayGetInfo(weights, &info) &&
///       info.type == kULJSTypedArrayType_Float32) {
///     const float* data = (const float*)info.data;
///     for (size_t i = 0; i < info.length; i++)
///       total += data[i];
///   }
///   return total;
/// }
/// ```
///
/// ## Detaching an ArrayBuffer
///
/// Call ulJSArrayBufferDetach() to take shared bytes back from the page early so JavaScript can no
/// longer reach the memory.
///
/// Once native code reads the bytes (by calling ulJSArrayBufferGetBuffer(),
/// ulJSArrayBufferGetBytes(), or ulJSTypedArrayGetInfo()), the ArrayBuffer can no longer be
/// detached by native code or by the page. This restriction applies even to an ArrayBuffer created
/// from your own ULBuffer. Calling ulJSArrayBufferDetach() then returns `false`.
///
/// ## Byte Lifetimes
///
/// A ULBuffer shared with the page releases its memory according to who holds it:
///
/// - **Shared buffers stay referenced until collection or detachment.** The library releases its
///   reference during a later ulUpdate() after the ArrayBuffer is garbage-collected or detached.
/// - **The destruction callback runs when the last reference is released.** That happens during a
///   later ulUpdate() if the ArrayBuffer held the last reference, or wherever native code calls
///   ulDestroyBuffer().
///
/// @warning  Bytes owned by the page are freed when the page goes away, even while native code
///           holds a ULBuffer over them. Call ulCreateBufferFromCopy() if the bytes must outlive
///           the page.
///
/// @see  ulCreateBuffer(), ulCreateBufferFromCopy(), CAPI_JSValue.h, `<Ultralight/js/Buffer.h>`
///
#ifndef ULTRALIGHT_CAPI_JSBUFFER_H
#define ULTRALIGHT_CAPI_JSBUFFER_H

#include <Ultralight/CAPI/CAPI_Buffer.h>
#include <Ultralight/CAPI/CAPI_JSValue.h>

#ifdef __cplusplus
extern "C" {
#endif

///
/// Element types for JavaScript typed arrays.
///
typedef enum {
  kULJSTypedArrayType_None = 0,  ///< The value is not a typed array.
  kULJSTypedArrayType_Int8,
  kULJSTypedArrayType_Uint8,
  kULJSTypedArrayType_Uint8Clamped,
  kULJSTypedArrayType_Int16,
  kULJSTypedArrayType_Uint16,
  kULJSTypedArrayType_Int32,
  kULJSTypedArrayType_Uint32,
  kULJSTypedArrayType_Float32,
  kULJSTypedArrayType_Float64,
  kULJSTypedArrayType_BigInt64,
  kULJSTypedArrayType_BigUint64,
} ULJSTypedArrayType;

///
/// Description of a typed array, filled by ulJSTypedArrayGetInfo().
///
typedef struct {
  ULJSTypedArrayType type; ///< The element type (kULJSTypedArrayType_None if the value is not
                           ///< a typed array).
  void* data;              ///< Pointer to the first element (NULL if the array is detached).
  size_t length;           ///< Number of elements.
  size_t byte_length;      ///< Length of the view in bytes.
  size_t byte_offset;      ///< Offset of the view within its ArrayBuffer.
} ULJSTypedArrayInfo;

/******************************************************************************
 * ArrayBuffer
 *****************************************************************************/

///
/// Whether or not a value is an ArrayBuffer (detached ones included). The ArrayBuffer can still
/// be detached.
///
/// @param  value  The value to check.
///
/// @return  Returns true if `value` is an ArrayBuffer.
///
ULExport bool ulJSValueIsArrayBuffer(ULJSValue value);

///
/// Create an ArrayBuffer holding a copy of the given bytes.
///
/// @param  ctx     The context to create the value in.
///
/// @param  bytes   The bytes to copy (may be NULL only when `length` is 0).
///
/// @param  length  The number of bytes to copy (0 creates an empty ArrayBuffer).
///
/// @return  Returns a new ULJSValue handle (NULL if the context is gone, or if `bytes` is NULL
///          and `length` isn't 0). You must call ulDestroyJSValue() when finished.
///
ULExport ULJSValue ulCreateJSArrayBuffer(ULJSContext ctx, const void* bytes, size_t length);

///
/// Create an ArrayBuffer that shares a Buffer's bytes with JavaScript without copying.
///
/// The ArrayBuffer keeps a reference to `buffer` until it's garbage collected or detached
/// (see "Byte Lifetimes" above). You still own your `buffer` handle and must
/// destroy it as usual.
///
/// @param  ctx     The context to create the value in.
///
/// @param  buffer  The Buffer to share. An empty Buffer gives an ordinary empty ArrayBuffer
///                 that keeps no reference.
///
/// @return  Returns a new ULJSValue handle (NULL if the context is gone or `buffer` is NULL).
///          You must call ulDestroyJSValue() when finished.
///
ULExport ULJSValue ulCreateJSArrayBufferFromBuffer(ULJSContext ctx, ULBuffer buffer);

///
/// Get a Buffer that views an ArrayBuffer's bytes without copying.
///
/// The Buffer is always a new one, even for an ArrayBuffer created from your own Buffer.
/// Holding it keeps the ArrayBuffer alive while the page lives.
///
/// @param  array_buffer  The ArrayBuffer.
///
/// @return  Returns a new ULBuffer (NULL if `array_buffer` isn't an ArrayBuffer or is
///          detached). An empty ArrayBuffer gives a zero-size Buffer. You must call
///          ulDestroyBuffer() when finished (from any thread).
///
/// @note  The ArrayBuffer can no longer be detached afterwards.
///
/// @warning  The bytes can be freed when the page goes away, even while you hold the Buffer (see
///           "Byte Lifetimes" above).
///
ULExport ULBuffer ulJSArrayBufferGetBuffer(ULJSValue array_buffer);

///
/// Get a pointer to an ArrayBuffer's bytes without copying.
///
/// @param  array_buffer  The ArrayBuffer.
///
/// @param  length        Where to store the byte length (may be NULL; written only on
///                       success).
///
/// @return  Returns the bytes (NULL if `array_buffer` isn't an ArrayBuffer or is detached). An
///          empty ArrayBuffer gives a non-NULL pointer and a zero length. The pointer stays
///          valid while `array_buffer` lives and the page lives (see "Byte
///          Lifetimes" above).
///
/// @note  The ArrayBuffer can no longer be detached afterwards.
///
ULExport void* ulJSArrayBufferGetBytes(ULJSValue array_buffer, size_t* length);

///
/// Detach an ArrayBuffer so JavaScript can no longer reach its bytes.
///
/// Afterwards its `byteLength` is 0, typed arrays over it have length 0 (element reads return
/// `undefined`), and creating a new typed array over it throws a TypeError. If it was created
/// from a Buffer, its reference to that Buffer is released during a later ulUpdate().
///
/// @param  array_buffer  The ArrayBuffer to detach.
///
/// @return  Returns true if the ArrayBuffer is detached on return (including one that was
///          already detached). Returns false if it can no longer be detached or
///          `array_buffer` isn't an ArrayBuffer.
///
ULExport bool ulJSArrayBufferDetach(ULJSValue array_buffer);

/******************************************************************************
 * Typed arrays
 *****************************************************************************/

///
/// Get the element type of a typed array. The ArrayBuffer can still be detached, so you can use
/// it to check a value before ulJSTypedArrayGetInfo().
///
/// @param  value  The value to check.
///
/// @return  Returns the element type (kULJSTypedArrayType_None if `value` isn't a typed array;
///          an ArrayBuffer or a DataView isn't one).
///
ULExport ULJSTypedArrayType ulJSValueGetTypedArrayType(ULJSValue value);

///
/// Create a typed array of zero-filled elements.
///
/// @param  ctx     The context to create the value in.
///
/// @param  type    The element type.
///
/// @param  length  The number of elements (not bytes).
///
/// @return  Returns a new ULJSValue handle (NULL if the context is gone or `type` is
///          kULJSTypedArrayType_None). You must call ulDestroyJSValue() when finished.
///
ULExport ULJSValue ulCreateJSTypedArray(ULJSContext ctx, ULJSTypedArrayType type,
                                        size_t length);

///
/// Create a typed array that shares a Buffer's bytes with JavaScript without copying.
///
/// Lifetime works as for ulCreateJSArrayBufferFromBuffer().
///
/// @param  ctx     The context to create the value in.
///
/// @param  type    The element type.
///
/// @param  buffer  The Buffer to share (its size must be a multiple of the element size). An
///                 empty Buffer gives an ordinary empty typed array that keeps no reference.
///
/// @return  Returns a new ULJSValue handle (NULL if the context is gone, `type` is
///          kULJSTypedArrayType_None, `buffer` is NULL, or its size isn't a multiple of the
///          element size). You must call ulDestroyJSValue() when finished.
///
ULExport ULJSValue ulCreateJSTypedArrayFromBuffer(ULJSContext ctx, ULJSTypedArrayType type,
                                                  ULBuffer buffer);

///
/// Create a typed array over part of an existing ArrayBuffer without copying. The ArrayBuffer
/// can still be detached.
///
/// @param  ctx           The context to create the value in.
///
/// @param  type          The element type.
///
/// @param  array_buffer  The ArrayBuffer to view.
///
/// @param  byte_offset   The byte offset of the first element (must be a multiple of the
///                       element size).
///
/// @param  length        The number of elements (not bytes).
///
/// @return  Returns a new ULJSValue handle (NULL if the context is gone, `type` is
///          kULJSTypedArrayType_None, `array_buffer` isn't an ArrayBuffer, `byte_offset` isn't
///          a multiple of the element size, or the range doesn't fit in `array_buffer`). You
///          must call ulDestroyJSValue() when finished.
///
/// @note  An `array_buffer` whose page is gone, or one from another page (unless both pages are
///        frames of the same top-level page with exactly the same origin), gives NULL, and the
///        second case logs a diagnostic.
///
ULExport ULJSValue ulCreateJSTypedArrayFromArrayBuffer(ULJSContext ctx, ULJSTypedArrayType type,
                                                       ULJSValue array_buffer, size_t byte_offset,
                                                       size_t length);

///
/// Describe a typed array: its element type, a pointer to its first element, and its lengths.
///
/// @param  typed_array  The typed array.
///
/// @param  info         The struct to fill (if NULL, the function returns false).
///
/// @return  Returns true if `typed_array` is a typed array and `info` was filled. A detached
///          typed array counts: it reports its type with NULL `data` and zero lengths. Returns
///          false for anything else, with `info->type` set to kULJSTypedArrayType_None.
///
/// @note  The ArrayBuffer can no longer be detached afterwards. The `data` pointer stays
///        valid while `typed_array` lives and the page lives (see "Byte Lifetimes"
///        above).
///
ULExport bool ulJSTypedArrayGetInfo(ULJSValue typed_array, ULJSTypedArrayInfo* info);

///
/// Get the ArrayBuffer a typed array views. The ArrayBuffer can still be detached.
///
/// @param  typed_array  The typed array.
///
/// @return  Returns a new ULJSValue handle (NULL if `typed_array` isn't a typed array). You must
///          call ulDestroyJSValue() when finished.
///
ULExport ULJSValue ulJSTypedArrayGetBuffer(ULJSValue typed_array);

#ifdef __cplusplus
}
#endif

#endif  // ULTRALIGHT_CAPI_JSBUFFER_H
