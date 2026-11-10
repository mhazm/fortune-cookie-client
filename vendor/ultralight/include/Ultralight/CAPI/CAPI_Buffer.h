///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///

///
/// @file CAPI_Buffer.h
///
/// A fixed-size container for raw byte data.
///
/// `#include <Ultralight/CAPI/CAPI_Buffer.h>`
///
/// ULBuffer is an opaque handle representing a raw data buffer in Ultralight. It intelligently
/// manages the lifetime of the data and can optionally call a user-supplied callback to deallocate
/// the data when the Buffer is destroyed.
///
#ifndef ULTRALIGHT_CAPI_BUFFER_H
#define ULTRALIGHT_CAPI_BUFFER_H

#include <Ultralight/CAPI/CAPI_Defines.h>

#ifdef __cplusplus
extern "C" {
#endif

///
/// Callback invoked when a Buffer created via ulCreateBuffer() is destroyed.
/// Use this to deallocate any user-managed data associated with the Buffer.
///
/// @param  user_data  The user data pointer that was passed to ulCreateBuffer().
///
/// @param  data       The raw data pointer that was passed to ulCreateBuffer().
///
typedef void (*ulDestroyBufferCallback)(void* user_data, void* data);

///
/// Create a Buffer from existing, user-owned data without any copies. An optional, user-supplied
/// callback will be called to deallocate data upon destruction.
///
/// @param  data                  A pointer to the data.
///
/// @param  size                  Size of the data in bytes.
///
/// @param  user_data             Optional user data that will be passed to destruction_callback
///                               when the returned Buffer is destroyed.
///
/// @param  destruction_callback  Optional callback that will be called upon destruction. Pass a
///                               null pointer if you don't want to be informed of destruction.
///
/// @return  A new ULBuffer instance. You must call ulDestroyBuffer() when finished.
///
ULExport ULBuffer ulCreateBuffer(void* data, size_t size, void* user_data,
                                 ulDestroyBufferCallback destruction_callback);

///
/// Create a Buffer from existing data, a deep copy of data will be made.
///
/// @param  data  A pointer to the data to copy.
///
/// @param  size  Size of the data in bytes.
///
/// @return  A new ULBuffer instance. You must call ulDestroyBuffer() when finished.
///
ULExport ULBuffer ulCreateBufferFromCopy(const void* data, size_t size);

///
/// Destroy a buffer previously created with ulCreateBuffer() or ulCreateBufferFromCopy().
///
/// @param  buffer  The buffer to destroy.
///
ULExport void ulDestroyBuffer(ULBuffer buffer);

///
/// Get a pointer to the raw byte data.
///
ULExport void* ulBufferGetData(ULBuffer buffer);

///
/// Get the size in bytes.
///
ULExport size_t ulBufferGetSize(ULBuffer buffer);

///
/// Get the user data associated with this Buffer, if any.
///
ULExport void* ulBufferGetUserData(ULBuffer buffer);

///
/// Check whether this Buffer owns its own data (Buffer was created via ulCreateBufferFromCopy).
/// If this is false, Buffer will call the user-supplied destruction callback to deallocate data
/// when this Buffer instance is destroyed.
///
ULExport bool ulBufferOwnsData(ULBuffer buffer);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ULTRALIGHT_CAPI_BUFFER_H