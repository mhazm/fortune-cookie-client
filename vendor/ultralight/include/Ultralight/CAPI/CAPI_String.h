///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///

///
/// @file CAPI_String.h
///
/// Unicode string container (natively UTF-8).
///
/// `#include <Ultralight/CAPI/CAPI_String.h>`
///
/// ULString is an opaque handle representing a Unicode string in Ultralight. It can be created
/// from a variety of string types (ASCII, UTF-8, UTF-16) and accessed as a null-terminated UTF-8
/// buffer.
///
#ifndef ULTRALIGHT_CAPI_STRING_H
#define ULTRALIGHT_CAPI_STRING_H

#include <Ultralight/CAPI/CAPI_Defines.h>

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
 * String
 *****************************************************************************/

///
/// Create string from a null-terminated UTF-8 C-string.
///
/// @param  str  A null-terminated UTF-8 C-string.
///
/// @return  A new ULString instance. You must call ulDestroyString() when finished.
///
ULExport ULString ulCreateString(const char* str);

///
/// Create string from UTF-8 buffer with explicit length.
///
/// @param  str  Pointer to a UTF-8 encoded buffer (does not need to be null-terminated).
///
/// @param  len  Length of the buffer in bytes.
///
/// @return  A new ULString instance. You must call ulDestroyString() when finished.
///
ULExport ULString ulCreateStringUTF8(const char* str, size_t len);

///
/// Create string from UTF-16 buffer with explicit length.
///
/// @param  str  Pointer to a UTF-16 encoded buffer.
///
/// @param  len  Length of the buffer in characters (not bytes).
///
/// @return  A new ULString instance. You must call ulDestroyString() when finished.
///
ULExport ULString ulCreateStringUTF16(const ULChar16* str, size_t len);

///
/// Create string from copy of existing string.
///
/// @param  str  The string to copy.
///
/// @return  A new ULString instance. You must call ulDestroyString() when finished.
///
ULExport ULString ulCreateStringFromCopy(ULString str);

///
/// Destroy a string previously created with ulCreateString(), ulCreateStringUTF8(),
/// ulCreateStringUTF16(), or ulCreateStringFromCopy().
///
/// @param  str  The string to destroy.
///
ULExport void ulDestroyString(ULString str);

///
/// Get native UTF-8 buffer data (always null-terminated).
///
ULExport char* ulStringGetData(ULString str);

///
/// Get length (in bytes) of the UTF-8 buffer data, not including null terminator.
///
ULExport size_t ulStringGetLength(ULString str);

///
/// Whether this string is empty or not.
///
ULExport bool ulStringIsEmpty(ULString str);

///
/// Replaces the contents of 'str' with the contents of 'new_str'
///
ULExport void ulStringAssignString(ULString str, ULString new_str);

///
/// Replaces the contents of 'str' with the contents of a C-string.
///
ULExport void ulStringAssignCString(ULString str, const char* c_str);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ULTRALIGHT_CAPI_STRING_H