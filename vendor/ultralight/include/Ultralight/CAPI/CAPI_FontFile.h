///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///

///
/// @file CAPI_FontFile.h
///
/// Font file interface.
///
/// `#include <Ultralight/CAPI/CAPI_FontFile.h>`
///
/// The font file interface represents a font file: either on-disk path or in-memory file contents.
///
/// @see ULFontLoader
///
#ifndef ULTRALIGHT_CAPI_FONTFILE_H
#define ULTRALIGHT_CAPI_FONTFILE_H

#include <Ultralight/CAPI/CAPI_Defines.h>
#include <Ultralight/CAPI/CAPI_String.h>
#include <Ultralight/CAPI/CAPI_Buffer.h>

#ifdef __cplusplus
extern "C" {
#endif

///
/// Create a font file from an on-disk file path.
///
/// @param  file_path  The path to the font file on disk. The file should already exist.
///
/// @return  A new font file instance. You must call ulDestroyFontFile() when finished.
///
ULExport ULFontFile ulFontFileCreateFromFilePath(ULString file_path);

///
/// Create a font file from an in-memory buffer.
///
/// @param  buffer  A buffer containing the font file data (TTF/OTF).
///
/// @return  A new font file instance. You must call ulDestroyFontFile() when finished.
///
/// @note  The font file keeps its own reference to the buffer, so you still call
///        ulDestroyBuffer() on `buffer` when you're done with it (right after this call is fine).
///
ULExport ULFontFile ulFontFileCreateFromBuffer(ULBuffer buffer);

///
/// Create a font file from an on-disk file path, naming the face within a font collection.
///
/// @param  file_path   The path to the font file on disk. The file should already exist.
///
/// @param  face_index  The index of the face to use when the file is a font collection (eg, a
///                     `.ttc` holding several families). Pass -1 to let the library choose the
///                     face that best matches the requested weight and style.
///
/// @return  Returns a new font file instance. You must call ulDestroyFontFile() when finished.
///
ULExport ULFontFile ulFontFileCreateFromFilePathWithFaceIndex(ULString file_path, int face_index);

///
/// Create a font file from an in-memory buffer, naming the face within a font collection.
///
/// @param  buffer      A buffer containing the font file data (TTF/OTF, or a collection).
///
/// @param  face_index  The index of the face to use when the data is a font collection. Pass -1
///                     to let the library choose the face that best matches the requested weight
///                     and style.
///
/// @return  Returns a new font file instance. You must call ulDestroyFontFile() when finished.
///
/// @note  The font file keeps its own reference to the buffer, so you still call
///        ulDestroyBuffer() on `buffer` when you're done with it (right after this call is fine).
///
ULExport ULFontFile ulFontFileCreateFromBufferWithFaceIndex(ULBuffer buffer, int face_index);

///
/// Destroy a font file previously created with ulFontFileCreateFromFilePath() or
/// ulFontFileCreateFromBuffer().
///
/// @param  font_file  The font file to destroy.
///
ULExport void ulDestroyFontFile(ULFontFile font_file);

///
/// Whether or not this font file was created from an in-memory buffer.
///
ULExport bool ulFontFileIsInMemory(ULFontFile font_file);

///
/// Get the file path for this font file (if created from a file path).
///
/// @return  The file path, or an empty string if created from a buffer. Do not destroy the
///          returned string. It stays valid until the next call to ulFontFileGetFilePath() on the
///          same thread (for any font file); copy it via ulCreateStringFromCopy() to keep it.
///
ULExport ULString ulFontFileGetFilePath(ULFontFile font_file);

///
/// Get the in-memory buffer for this font file (if created from a buffer).
///
/// @return  The buffer, or NULL if created from a file path. Do not destroy the returned buffer.
///          It stays valid until the next call to ulFontFileGetBuffer() on the same thread (for
///          any font file).
///
ULExport ULBuffer ulFontFileGetBuffer(ULFontFile font_file);

///
/// Get the index of the face within a font collection, or -1 when the library chooses the face.
///
ULExport int ulFontFileGetFaceIndex(ULFontFile font_file);

///
/// Get the unique hash for this font file.
///
ULExport unsigned int ulFontFileGetHash(ULFontFile font_file);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ULTRALIGHT_CAPI_FONTFILE_H