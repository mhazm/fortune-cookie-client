///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///

///
/// @file CAPI_Clipboard.h
///
/// User-defined clipboard interface.
///
/// `#include <Ultralight/CAPI/CAPI_Clipboard.h>`
///
/// The library uses this to read and write data to the system's clipboard, as whole
/// multi-format payloads (ULClipboardData: an ordered set of (type, value) entries keyed by a
/// MIME type or an application-defined type string).
///
/// The library currently reads and writes these well-known types: `text/plain`, `text/html`,
/// and `text/uri-list`. Your implementation should map each type it supports to the matching OS
/// clipboard format and simply skip types it doesn't recognize.
///
/// @note  The library only calls the ULClipboard callbacks on the thread you created the Renderer
///        or App on, so your implementation doesn't need to be safe to call from more than one
///        thread.
///
/// @see ulPlatformSetClipboard()
///
#ifndef ULTRALIGHT_CAPI_CLIPBOARD_H
#define ULTRALIGHT_CAPI_CLIPBOARD_H

#include <Ultralight/CAPI/CAPI_Defines.h>

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
 * ClipboardData
 *****************************************************************************/

///
/// Create an empty clipboard payload.
///
/// @return  Returns a payload you own; call ulDestroyClipboardData() when finished (unless
///          you return it from a ULClipboardReadCallback, where the library consumes it).
///
ULExport ULClipboardData ulCreateClipboardData(void);

///
/// Create a clipboard payload holding a single `text/plain` entry.
///
/// @return  Returns a payload you own; call ulDestroyClipboardData() when finished (unless
///          you return it from a ULClipboardReadCallback, where the library consumes it).
///
ULExport ULClipboardData ulCreateClipboardDataFromText(ULString text);

///
/// Create another owned reference to an existing clipboard payload.
///
/// You can use this to keep a payload that was passed to you as a borrowed handle (eg, the
/// payload passed to your ULClipboardWriteCallback) beyond the call that provided it.
///
/// @return  Returns a new owned reference; call ulDestroyClipboardData() when finished.
///
ULExport ULClipboardData ulCreateClipboardDataRef(ULClipboardData data);

///
/// Destroy a clipboard payload (you should only destroy payloads you have explicitly created
/// via the functions above).
///
ULExport void ulDestroyClipboardData(ULClipboardData data);

///
/// Set the text value for a type, replacing any existing entry with the same type.
///
/// Use bare MIME types (`text/plain`, not `text/plain;charset=utf-8`)-- the library normalizes
/// charset-suffixed spellings at its own boundaries.
///
ULExport void ulClipboardDataSetText(ULClipboardData data, ULString type, ULString text);

///
/// Set the bytes value for a type, replacing any existing entry with the same type.
///
/// @note  The payload retains `bytes`; you can still destroy your ULBuffer afterwards.
///
ULExport void ulClipboardDataSetBytes(ULClipboardData data, ULString type, ULBuffer bytes);

///
/// Get the text value for a type.
///
/// @return  Returns a new string, or NULL when there is no entry for `type` or its value isn't
///          text. Destroy the returned string via ulDestroyString() when done.
///
ULExport ULString ulClipboardDataGetText(ULClipboardData data, ULString type);

///
/// Get the bytes value for a type.
///
/// @return  Returns a new buffer, or NULL when there is no entry for `type` or its value isn't
///          bytes. Destroy the returned buffer via ulDestroyBuffer() when done.
///
ULExport ULBuffer ulClipboardDataGetBytes(ULClipboardData data, ULString type);

///
/// Whether or not the entry for a type holds text.
///
ULExport bool ulClipboardDataIsText(ULClipboardData data, ULString type);

///
/// Whether or not an entry exists for a type.
///
ULExport bool ulClipboardDataHas(ULClipboardData data, ULString type);

///
/// Remove the entry for a type (does nothing if no such entry exists).
///
ULExport void ulClipboardDataRemove(ULClipboardData data, ULString type);

///
/// Get the number of entries.
///
ULExport size_t ulClipboardDataGetSize(ULClipboardData data);

///
/// Get the type of the entry at a certain index, in insertion order.
///
/// @return  Returns a new string, or NULL when `index` is out of range. Destroy the returned
///          string via ulDestroyString() when done.
///
ULExport ULString ulClipboardDataGetTypeAt(ULClipboardData data, size_t index);

///
/// Get the URL of the document this payload came from (can be empty).
///
/// @return  Returns a new string. Destroy the returned string via ulDestroyString() when done.
///
ULExport ULString ulClipboardDataGetSourceURL(ULClipboardData data);

///
/// Set the URL of the document this payload came from.
///
/// The library sets this when copying page content; on Windows it becomes the HTML clipboard
/// format's SourceURL.
///
ULExport void ulClipboardDataSetSourceURL(ULClipboardData data, ULString url);

/******************************************************************************
 * Clipboard
 *****************************************************************************/

///
/// The callback invoked when the library wants to clear the system's clipboard.
///
typedef void (*ULClipboardClearCallback)(void* user_data);

///
/// The callback invoked when the library wants to read from the system's clipboard.
///
/// You should return a payload you created via ulCreateClipboardData() holding one entry per
/// format you can read (at minimum the `text/plain` entry, if the clipboard holds text). The
/// library takes ownership of the result and will call ulDestroyClipboardData() when done.
///
/// You can return NULL for an empty clipboard.
///
typedef ULClipboardData (*ULClipboardReadCallback)(void* user_data);

///
/// The callback invoked when the library wants to write a payload to the system's clipboard,
/// replacing its previous contents.
///
/// You should write all of the payload's entries in one atomic clipboard transaction (on
/// Windows: one OpenClipboard / EmptyClipboard / SetClipboardData per entry / CloseClipboard
/// cycle), skipping any types you don't support.
///
/// @note  `data` is borrowed: valid only until the callback returns. Use
///        ulCreateClipboardDataRef() to keep it beyond that (eg, for delayed rendering).
///
typedef void (*ULClipboardWriteCallback)(void* user_data, ULClipboardData data);

///
/// User-defined clipboard interface.
///
/// You should implement each of these callbacks, then pass an instance of this struct containing
/// your callbacks to ulPlatformSetClipboard().
///
typedef struct {
  ///
  /// A user-defined pointer passed to every callback (can be NULL).
  ///
  void* user_data;

  ///
  /// Called to clear the system's clipboard.
  ///
  ULClipboardClearCallback clear;

  ///
  /// Called to read the system clipboard's current contents.
  ///
  ULClipboardReadCallback read;

  ///
  /// Called to write a payload to the system's clipboard.
  ///
  ULClipboardWriteCallback write;
} ULClipboard;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ULTRALIGHT_CAPI_CLIPBOARD_H
