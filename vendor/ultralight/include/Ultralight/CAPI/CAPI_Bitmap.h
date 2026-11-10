///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///

///
/// @file CAPI_Bitmap.h
///
/// A container for pixel data.
///
/// `#include <Ultralight/CAPI/CAPI_Bitmap.h>`
///
/// The bitmap class is used to store pixel data in a variety of formats. It intelligently manages
/// the lifetime of the pixel buffer and serializes access to it while the bitmap owns the pixels.
///
/// ## Thread Safety
///
/// ulBitmapLockPixels() and ulBitmapUnlockPixels() take the bitmap's internal lock only when the
/// bitmap owns its pixel buffer, ie. one created by a creation function that allocates, or one
/// that copied the pixels you passed in. A bitmap that wraps memory you still own-- created with
/// `should_copy = false`-- does no locking at all, so you must serialize access to that buffer
/// yourself.
///
/// ## Accessing Pixel Data
///
/// To access the pixel data, you must first lock the pixels using ulBitmapLockPixels(). This will
/// return a pointer to the pixel buffer. An example follows:
///
/// ```
/// void* pixels = ulBitmapLockPixels(bitmap);
/// if (pixels) {
///   // Zero out the pixel buffer
///   memset(pixels, 0, ulBitmapGetSize(bitmap));
/// }
///
/// // Unlock the pixels when you're done.
/// ulBitmapUnlockPixels(bitmap);
/// ```
///
#ifndef ULTRALIGHT_CAPI_BITMAP_H
#define ULTRALIGHT_CAPI_BITMAP_H

#include <Ultralight/CAPI/CAPI_Defines.h>

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
 * Bitmap Format
 *****************************************************************************/

///
/// Block geometry of a pixel format.
///
/// Every format stores its pixels in blocks: a 1x1 block for an uncompressed format, and a 4x4
/// block for a block-compressed (BCn) format. The storage of an image follows from the block size:
///
/// ```
/// row_pitch    = ceil(width / block_width) * block_bytes
/// surface_size = row_pitch * ceil(height / block_height)
/// ```
///
/// For a 1x1 block this reduces to `width * block_bytes` and `row_pitch * height`.
///
/// @see  ulGetBitmapFormatInfo()
///
typedef struct {
  unsigned int block_width;  ///< Width of one block, in pixels (1 for uncompressed, 4 for BCn).
  unsigned int block_height; ///< Height of one block, in pixels (1 for uncompressed, 4 for BCn).
  unsigned int block_bytes;  ///< Size of one block, in bytes (the bytes per pixel for a 1x1 block).
} ULBitmapFormatInfo;

///
/// Get the block geometry for a given pixel format.
///
/// @param  format  The pixel format to query.
///
/// @return  Returns the block width, block height, and block size in bytes for the format.
///
ULExport ULBitmapFormatInfo ulGetBitmapFormatInfo(ULBitmapFormat format);

///
/// Whether or not a given pixel format is block-compressed (a BCn format).
///
/// @param  format  The pixel format to query.
///
/// @return  Returns true for kBitmapFormat_BC1_UNORM, kBitmapFormat_BC2_UNORM,
///          kBitmapFormat_BC3_UNORM, and kBitmapFormat_BC7_UNORM.
///
ULExport bool ulIsCompressedFormat(ULBitmapFormat format);

/******************************************************************************
 * Bitmap
 *****************************************************************************/

///
/// Create an empty bitmap. No pixels will be allocated.
///
/// @return  Returns a new bitmap instance. You must call ulDestroyBitmap() when finished.
///
ULExport ULBitmap ulCreateEmptyBitmap(void);

///
/// Create bitmap with certain dimensions and pixel format. Pixels will be allocated but not
/// initialized.
///
/// @param  width   The width in pixels.
///
/// @param  height  The height in pixels.
///
/// @param  format  The pixel format to use.
///
/// @return  Returns a new bitmap instance. You must call ulDestroyBitmap() when finished.
///
ULExport ULBitmap ulCreateBitmap(unsigned int width, unsigned int height, ULBitmapFormat format);

///
/// Create bitmap with certain dimensions, pixel format, and row byte alignment.
///
/// @param  width      The width in pixels.
///
/// @param  height     The height in pixels.
///
/// @param  format     The pixel format to use.
///
/// @param  alignment  The alignment in bytes. Row bytes will be padded to a multiple of this value.
///
/// @return  Returns a new bitmap instance. You must call ulDestroyBitmap() when finished.
///
ULExport ULBitmap ulCreateBitmapAligned(unsigned int width, unsigned int height,
                                        ULBitmapFormat format, unsigned int alignment);

///
/// Create a bitmap from an existing pixel buffer.
///
/// @param  width        The width in pixels.
///
/// @param  height       The height in pixels.
///
/// @param  format       The pixel format to use.
///
/// @param  row_bytes    The number of bytes between each row of pixels. This should be at least
///                      width * bytes_per_pixel, or for a block-compressed format, the width in
///                      4x4 blocks times the block size (see ulGetBitmapFormatInfo()).
///
/// @param  pixels       Pointer to the raw pixel buffer.
///
/// @param  size         Size of the raw pixel buffer in bytes.
///
/// @param  should_copy  Whether or not a copy should be made of the pixels. If this is false, the
///                      returned bitmap will reference the raw pixels directly and you must keep
///                      the buffer alive until the bitmap is destroyed.
///
/// @return  Returns a new bitmap instance. You must call ulDestroyBitmap() when finished.
///
ULExport ULBitmap ulCreateBitmapFromPixels(unsigned int width, unsigned int height,
                                           ULBitmapFormat format, unsigned int row_bytes,
                                           const void* pixels, size_t size, bool should_copy);

///
/// Create a bitmap from a deep copy of another bitmap.
///
/// @param  existing_bitmap  The bitmap to copy from.
///
/// @return  Returns a new bitmap instance. You must call ulDestroyBitmap() when finished.
///
ULExport ULBitmap ulCreateBitmapFromCopy(ULBitmap existing_bitmap);

///
/// Destroy a bitmap.
///
/// You should only destroy bitmaps you have explicitly created via one of the creation functions
/// above.
///
/// @param  bitmap  The bitmap to destroy.
///
ULExport void ulDestroyBitmap(ULBitmap bitmap);

///
/// Get the width in pixels.
///
ULExport unsigned int ulBitmapGetWidth(ULBitmap bitmap);

///
/// Get the height in pixels.
///
ULExport unsigned int ulBitmapGetHeight(ULBitmap bitmap);

///
/// Get the bounds as a ULIntRect.
///
ULExport ULIntRect ulBitmapGetBounds(ULBitmap bitmap);

///
/// Get the pixel format.
///
ULExport ULBitmapFormat ulBitmapGetFormat(ULBitmap bitmap);

///
/// Get the bytes per pixel.
///
ULExport unsigned int ulBitmapGetBpp(ULBitmap bitmap);

///
/// Get the number of bytes per row.
///
ULExport unsigned int ulBitmapGetRowBytes(ULBitmap bitmap);

///
/// Get the size in bytes of the underlying pixel buffer.
///
ULExport size_t ulBitmapGetSize(ULBitmap bitmap);

///
/// Whether or not this bitmap owns its own pixel buffer.
///
ULExport bool ulBitmapOwnsPixels(ULBitmap bitmap);

///
/// Whether or not this bitmap uses a block-compressed (BCn) format.
///
/// @note  Compressed bitmaps store 4x4 pixel blocks and don't support per-pixel operations.
///        ulBitmapErase(), ulBitmapDrawBitmap(), ulBitmapResample(), and the channel/alpha
///        conversions are no-ops or fail on a compressed bitmap.
///
ULExport bool ulBitmapIsCompressed(ULBitmap bitmap);

///
/// Get the maximum supported width or height, in pixels.
///
/// The bitmap creation functions return NULL if either dimension exceeds this limit.
///
ULExport unsigned int ulBitmapMaxDimension(void);

///
/// Lock the pixel buffer for reading/writing.
///
/// @return  Returns a pointer to the pixel buffer.
///
ULExport void* ulBitmapLockPixels(ULBitmap bitmap);

///
/// Unlock pixels after locking.
///
ULExport void ulBitmapUnlockPixels(ULBitmap bitmap);

///
/// Get the raw pixel buffer.
///
/// @note  You should only call this if the pixels are already locked.
///
ULExport void* ulBitmapRawPixels(ULBitmap bitmap);

///
/// Whether or not this bitmap is empty.
///
ULExport bool ulBitmapIsEmpty(ULBitmap bitmap);

///
/// Reset bitmap pixels to 0.
///
ULExport void ulBitmapErase(ULBitmap bitmap);

///
/// Assign another bitmap to this one.
///
/// @param  bitmap  The destination bitmap.
///
/// @param  source  The source bitmap to copy from.
///
/// @note  The source pixels are deep-copied in every case but one: a source that wraps memory you
///        still own (created with `should_copy = false`) is aliased instead, so that buffer must
///        outlive the destination bitmap.
///
ULExport void ulBitmapSet(ULBitmap bitmap, ULBitmap source);

///
/// Draw another bitmap to this bitmap.
///
/// @note  Formats do not need to match. Bitmap formats will be converted to one another
///        automatically.
///
/// @param  bitmap      The destination bitmap.
///
/// @param  src_rect    The source rectangle, relative to the source bitmap.
///
/// @param  dest_rect   The destination rectangle, relative to this bitmap.
///
/// @param  src         The source bitmap to draw.
///
/// @param  pad_repeat  Whether or not to pad the drawn bitmap by one pixel of repeated edge pixels.
///
/// @return  Returns whether or not the operation succeeded (this can fail if the src_rect and/or
///          dest_rect are invalid).
///
ULExport bool ulBitmapDrawBitmap(ULBitmap bitmap, ULIntRect src_rect, ULIntRect dest_rect,
                                 ULBitmap src, bool pad_repeat);

///
/// Write bitmap to a PNG on disk.
///
/// @param  bitmap  The bitmap to write.
///
/// @param  path    The file path to write to.
///
/// @return  Returns whether or not the operation succeeded.
///
/// @note  This automatically converts BGRA to RGBA and premultiplied alpha to straight alpha.
///        Use ulBitmapWritePNGEx() if you need to control these conversions.
///
ULExport bool ulBitmapWritePNG(ULBitmap bitmap, const char* path);

///
/// Write bitmap to a PNG on disk with explicit conversion control.
///
/// @param  bitmap  The bitmap to write.
///
/// @param  path    The file path to write to.
///
/// @param  convert_to_rgba  The PNG format expects RGBA format but the bitmap is stored as BGRA,
///                          set this to true to perform the conversion automatically.
///
/// @param  convert_to_straight_alpha  The PNG format expects semi-transparent values to be stored
///                                    as straight alpha instead of premultiplied alpha, set this
///                                    to true to perform the conversion automatically.
///
/// @return  Returns whether or not the operation succeeded.
///
ULExport bool ulBitmapWritePNGEx(ULBitmap bitmap, const char* path, bool convert_to_rgba,
                                 bool convert_to_straight_alpha);

///
/// Encode this bitmap as a PNG image and return the encoded bytes in a buffer.
///
/// @param  bitmap  The bitmap to encode.
///
/// @return  Returns a buffer containing the PNG-encoded bytes, or NULL on failure. You must call
///          ulDestroyBuffer() on the returned buffer when finished.
///
/// @note  This automatically converts BGRA to RGBA and premultiplied alpha to straight alpha.
///        Use ulBitmapEncodePNGEx() if you need to control these conversions.
///
ULExport ULBuffer ulBitmapEncodePNG(ULBitmap bitmap);

///
/// Encode this bitmap as a PNG image with explicit conversion control and return the encoded
/// bytes in a buffer.
///
/// @param  bitmap  The bitmap to encode.
///
/// @param  convert_to_rgba  The PNG format expects RGBA format but the bitmap is stored as BGRA,
///                          set this to true to perform the conversion automatically.
///
/// @param  convert_to_straight_alpha  The PNG format expects semi-transparent values to be stored
///                                    as straight alpha instead of premultiplied alpha, set this
///                                    to true to perform the conversion automatically.
///
/// @return  Returns a buffer containing the PNG-encoded bytes, or NULL on failure. You must call
///          ulDestroyBuffer() on the returned buffer when finished.
///
ULExport ULBuffer ulBitmapEncodePNGEx(ULBitmap bitmap, bool convert_to_rgba,
                                      bool convert_to_straight_alpha);

///
/// Make a resized copy of this bitmap by writing to a pre-allocated destination bitmap.
///
/// @param  bitmap        The source bitmap.
///
/// @param  destination   The destination bitmap, its width and height determine the output size.
///
/// @param  high_quality  Whether or not a high quality resampling will be used during the resize.
///                       (Otherwise, just uses fast nearest-neighbor sampling)
///
/// @return  Returns whether or not the operation succeeded. This operation is only valid if both
///          formats are kBitmapFormat_BGRA8_UNORM_SRGB and the source and destination are
///          non-empty.
///
ULExport bool ulBitmapResample(ULBitmap bitmap, ULBitmap destination, bool high_quality);

///
/// Convert a BGRA bitmap from premultiplied alpha to straight alpha.
///
/// @note  Only valid if the bitmap format is kBitmapFormat_BGRA8_UNORM_SRGB.
///
ULExport void ulBitmapConvertToStraightAlpha(ULBitmap bitmap);

///
/// Convert a BGRA bitmap from straight alpha to premultiplied alpha.
///
/// @note  Only valid if the bitmap format is kBitmapFormat_BGRA8_UNORM_SRGB.
///
ULExport void ulBitmapConvertToPremultipliedAlpha(ULBitmap bitmap);

///
/// This converts a BGRA bitmap to RGBA bitmap and vice-versa by swapping the red and blue channels.
///
ULExport void ulBitmapSwapRedBlueChannels(ULBitmap bitmap);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ULTRALIGHT_CAPI_BITMAP_H