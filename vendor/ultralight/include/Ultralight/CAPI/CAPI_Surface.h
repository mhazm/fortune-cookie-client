///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///

///
/// @file CAPI_Surface.h
///
/// User-defined pixel buffer surface. 
///
/// `#include <Ultralight/CAPI/CAPI_Surface.h>`
///
/// The library uses this to store pixel data when rendering Views on the CPU (see
/// ulViewIsAccelerated()).
///
/// You can provide the library with your own Surface implementation to reduce the latency of
/// displaying pixels in your application (Views will be drawn directly to a block of memory
/// controlled by you).
///
/// When a View is rendered on the CPU, you can retrieve the backing Surface via ulViewGetSurface().
/// After each paint the Surface lists the rectangles the library changed
/// (ulSurfaceGetDirtyRectCount(), ulSurfaceGetDirtyRect()); ulSurfaceGetDirtyBounds() is their
/// union.
///
/// @pre  This is automatically managed for you when using ulCreateApp(), if you want to override
///       ULSurfaceDefinition, you'll need to use ulCreateRenderer() instead.
///
/// ## Default Implementation
///
/// A default Surface implementation, BitmapSurface, is automatically provided by the library when
/// you call ulCreateRenderer() without defining a custom ULSurfaceDefinition.
///
/// You should cast the ULSurface to a ULBitmapSurface and call ulBitmapSurfaceGetBitmap() to access
/// the underlying Bitmap.
///
/// ## Setting the Surface Implementation
///
/// To define your own implementation, you should implement the ULSurfaceDefinition callbacks,
/// and then pass an instance of ULSurfaceDefinition containing your callbacks to 
/// ulPlatformSetSurfaceDefinition() before calling ulCreateRenderer().
///
#ifndef ULTRALIGHT_CAPI_SURFACE_H
#define ULTRALIGHT_CAPI_SURFACE_H

#include <Ultralight/CAPI/CAPI_Defines.h>

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
 * Surface
 *****************************************************************************/

///
/// Width (in pixels).
///
ULExport unsigned int ulSurfaceGetWidth(ULSurface surface);

///
/// Height (in pixels).
///
ULExport unsigned int ulSurfaceGetHeight(ULSurface surface);

///
/// Number of bytes between rows (usually width * 4)
///
ULExport unsigned int ulSurfaceGetRowBytes(ULSurface surface);

///
/// Size in bytes.
///
ULExport size_t ulSurfaceGetSize(ULSurface surface);

///
/// Lock the pixel buffer and get a pointer to the beginning of the data for reading/writing.
///
/// Native pixel format is premultiplied BGRA 32-bit (8 bits per channel).
///
ULExport void* ulSurfaceLockPixels(ULSurface surface);

///
/// Unlock the pixel buffer.
///
ULExport void ulSurfaceUnlockPixels(ULSurface surface);

///
/// Resize the pixel buffer to a certain width and height (both in pixels).
///
/// This should never be called while pixels are locked.
///
ULExport void ulSurfaceResize(ULSurface surface, unsigned int width, unsigned int height);

///
/// Set the dirty bounds to a certain value.
///
/// This is called after the Renderer paints to an area of the pixel buffer. (The new value will be
/// joined with the existing dirty_bounds())
///
ULExport void ulSurfaceSetDirtyBounds(ULSurface surface, ULIntRect bounds);

///
/// Get the dirty bounds.
///
/// This value can be used to determine which portion of the pixel buffer has been updated since the
/// last call to ulSurfaceClearDirtyBounds().
///
/// The general algorithm to determine if a Surface needs display is:
/// ```
///   if (!ulIntRectIsEmpty(ulSurfaceGetDirtyBounds(surface))) {
///       // Surface pixels are dirty and needs display.
///       // Cast Surface to native Surface and use it here (pseudo code)
///       DisplaySurface(surface);
///
///       // Once you're done, clear the dirty bounds:
///       ulSurfaceClearDirtyBounds(surface);
///   }
/// ```
///
ULExport ULIntRect ulSurfaceGetDirtyBounds(ULSurface surface);

///
/// Clear the dirty bounds.
///
/// You should call this after you're done displaying the Surface.
///
ULExport void ulSurfaceClearDirtyBounds(ULSurface surface);

///
/// Shift a rectangle of pixels by a certain offset (see ULSurfaceDefinitionScrollCallback for the
/// rules).
///
/// @param  surface  The surface handle.
///
/// @param  rect     The rectangle to shift (in pixels).
///
/// @param  dx       The horizontal shift, in pixels (positive moves pixels to the right).
///
/// @param  dy       The vertical shift, in pixels (positive moves pixels down).
///
/// @return  Returns the scroll callback's answer, or false for the default surface.
///
ULExport bool ulSurfaceScroll(ULSurface surface, ULIntRect rect, int dx, int dy);

///
/// Shift a rectangle of 32-bit pixels in place.
///
/// Use this from your scroll callback to move the pixels of `rect` by `dx` and `dy` through the
/// pointer you returned from the lock callback; overlapping rows and columns are handled for you.
///
/// @param  pixels     Pointer to the first pixel of the buffer.
///
/// @param  row_bytes  Number of bytes between rows (usually width * 4).
///
/// @param  rect       The rectangle to shift (in pixels).
///
/// @param  dx         The horizontal shift, in pixels (positive moves pixels to the right).
///
/// @param  dy         The vertical shift, in pixels (positive moves pixels down).
///
ULExport void ulSurfaceShiftPixels(void* pixels, unsigned int row_bytes, ULIntRect rect, int dx,
                                   int dy);

///
/// Get the number of dirty rectangles (0 when nothing has been painted since the last clear).
///
/// @param  surface  The surface handle.
///
/// @return  Returns the number of dirty rectangles.
///
ULExport unsigned int ulSurfaceGetDirtyRectCount(ULSurface surface);

///
/// Get a dirty rectangle by index.
///
/// Each lies inside the dirty bounds; the dirty bounds are their union, so you can copy each one
/// instead of the union to move fewer pixels.
///
/// @param  surface  The surface handle.
///
/// @param  index    The index of the rectangle (less than ulSurfaceGetDirtyRectCount()).
///
/// @return  Returns the rectangle, or an empty rectangle for an index out of range.
///
ULExport ULIntRect ulSurfaceGetDirtyRect(ULSurface surface, unsigned int index);

///
/// Get the underlying user data pointer (this is only valid if you have set a custom surface
/// implementation via ulPlatformSetSurfaceDefinition).
///
/// This will return nullptr if this surface is the default ULBitmapSurface.
///
ULExport void* ulSurfaceGetUserData(ULSurface surface);

/******************************************************************************
 * BitmapSurface
 *****************************************************************************/

///
/// Get the underlying Bitmap from the default Surface.
///
/// @note  Do not call ulDestroyBitmap() on the returned value, it is owned by the surface.
///
ULExport ULBitmap ulBitmapSurfaceGetBitmap(ULBitmapSurface surface);

/******************************************************************************
 * Surface Definition
 *****************************************************************************/

///
/// The callback invoked when a Surface is created.
///
/// @param  width  The width in pixels.
/// @param  height  The height in pixels.
///
/// @return  Return a pointer to user-defined data for the instance. This user data pointer will be
///          passed to all other callbacks when operating on the instance.
///
typedef void* (*ULSurfaceDefinitionCreateCallback)(unsigned int width, unsigned int height);

///
/// The callback invoked when a Surface is destroyed.
///
/// @param  user_data  User data pointer uniquely identifying the surface.
///
typedef void (*ULSurfaceDefinitionDestroyCallback)(void* user_data);

///
/// The callback invoked when a Surface's width (in pixels) is requested.
///
/// @param  user_data  User data pointer uniquely identifying the surface.
///
typedef unsigned int (*ULSurfaceDefinitionGetWidthCallback)(void* user_data);

///
/// The callback invoked when a Surface's height (in pixels) is requested.
///
/// @param  user_data  User data pointer uniquely identifying the surface.
///
typedef unsigned int (*ULSurfaceDefinitionGetHeightCallback)(void* user_data);

///
/// The callback invoked when a Surface's row bytes is requested.
///
/// @note  This value is also known as "stride". Usually width * 4.
///
/// @param  user_data  User data pointer uniquely identifying the surface.
///
typedef unsigned int (*ULSurfaceDefinitionGetRowBytesCallback)(void* user_data);

///
/// The callback invoked when a Surface's size (in bytes) is requested.
///
/// @param  user_data  User data pointer uniquely identifying the surface.
///
typedef size_t (*ULSurfaceDefinitionGetSizeCallback)(void* user_data);

///
/// The callback invoked when a Surface's pixel buffer is requested to be locked for reading/writing
/// (should return a pointer to locked bytes).
///
/// @param  user_data  User data pointer uniquely identifying the surface.
///
typedef void* (*ULSurfaceDefinitionLockPixelsCallback)(void* user_data);

///
/// The callback invoked when a Surface's pixel buffer is requested to be unlocked after previously
/// being locked.
///
/// @param  user_data  User data pointer uniquely identifying the surface.
///
typedef void (*ULSurfaceDefinitionUnlockPixelsCallback)(void* user_data);

///
/// The callback invoked when a Surface is requested to be resized to a certain width/height.
///
/// @param  user_data  User data pointer uniquely identifying the surface.
///
/// @param  width      Width in pixels.
///
/// @param  height     Height in pixels.
///
typedef void (*ULSurfaceDefinitionResizeCallback)(void* user_data, unsigned int width,
                                                  unsigned int height);

///
/// The callback invoked when a Surface is asked to shift a rectangle of pixels by an offset.
///
/// The library calls this during ulRender() when a page scrolls, moving already painted pixels
/// instead of repainting them. Afterward, the library repaints only the exposed strip.
///
/// The Surface pixel buffer is the source the library paints into. The destination is wherever
/// you copy those pixels to display them (eg, a GPU texture or a window). This callback always
/// shifts the source.
///
/// Most implementations do this with one call to ulSurfaceShiftPixels().
///
/// @param  user_data  User data pointer uniquely identifying the surface.
///
/// @param  rect       The rectangle to shift (in pixels). This always lies inside the surface, and
///                    the shift is smaller than the rectangle on each axis.
///
/// @param  dx         The horizontal shift, in pixels (positive moves pixels to the right).
///
/// @param  dy         The vertical shift, in pixels (positive moves pixels down).
///
/// @return  Return false in most cases. This means the destination was not shifted. The
///          library then adds `rect` to the dirty bounds so you copy the moved pixels from the
///          source to the destination again. Return true only in the rare case where you
///          shifted the destination by the same offset yourself (eg, you scrolled your
///          window or copied within your GPU texture)-- the library then reports only the
///          newly exposed strip as dirty.
///
/// \parblock
/// @note  This is called while pixels are locked (between the lock and unlock callbacks). Shift
///        through the pointer you returned from the lock callback-- do not lock the buffer again.
/// \endparblock
///
/// \parblock
/// @note  Rows may be padded. Step rows by the row bytes, never by width * 4.
/// \endparblock
///
typedef bool (*ULSurfaceDefinitionScrollCallback)(void* user_data, ULIntRect rect, int dx,
                                                   int dy);

///
/// User-defined surface interface.
///
/// You should implement each of these callbacks, then pass an instance of this struct containing
/// your callbacks to ulPlatformSetSurfaceDefinition().
///
/// Zero-initialize the struct (`ULSurfaceDefinition def = {0};`) so a callback you leave unset
/// reads as NULL.
///
typedef struct {
  /// Callback invoked when a Surface is created.
  ULSurfaceDefinitionCreateCallback create;
  /// Callback invoked when a Surface is destroyed.
  ULSurfaceDefinitionDestroyCallback destroy;
  /// Callback invoked to get the Surface width in pixels.
  ULSurfaceDefinitionGetWidthCallback get_width;
  /// Callback invoked to get the Surface height in pixels.
  ULSurfaceDefinitionGetHeightCallback get_height;
  /// Callback invoked to get the number of bytes per row.
  ULSurfaceDefinitionGetRowBytesCallback get_row_bytes;
  /// Callback invoked to get the Surface size in bytes.
  ULSurfaceDefinitionGetSizeCallback get_size;
  /// Callback invoked to lock the pixel buffer for reading/writing.
  ULSurfaceDefinitionLockPixelsCallback lock_pixels;
  /// Callback invoked to unlock the pixel buffer.
  ULSurfaceDefinitionUnlockPixelsCallback unlock_pixels;
  /// Callback invoked to resize the pixel buffer.
  ULSurfaceDefinitionResizeCallback resize;
  /// Callback invoked to shift a rectangle of pixels (may be NULL, the library then shifts the
  /// locked buffer itself and reports the whole rectangle dirty).
  ULSurfaceDefinitionScrollCallback scroll;
} ULSurfaceDefinition;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ULTRALIGHT_CAPI_SURFACE_H