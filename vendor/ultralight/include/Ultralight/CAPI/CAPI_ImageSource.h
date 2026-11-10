///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///

///
/// @file CAPI_ImageSource.h
///
/// User-defined image source to display custom images on a web-page.
///
/// `#include <Ultralight/CAPI/CAPI_ImageSource.h>`
///
/// This API allows you to composite your own images into a web-page. This is useful for displaying
/// in-game textures, external image assets, or other custom content.
///
/// ## ImageSource File Format
///
/// To use an ImageSource, you must first create an `.imgsrc` file containing a string identifying
/// the image source. This string will be used to lookup the ImageSource from ImageSourceProvider
/// when it is loaded on a web-page.
///
/// The file format is as follows:
///
/// ```
/// IMGSRC-V1
/// <identifier>
/// ```
///
/// You can use the `.imgsrc` file anywhere in your web-page that typically accepts an image URL.
/// For example:
///
/// ```html
/// <img src="my_custom_image.imgsrc" />
/// ```
///
/// ## Creating from a GPU Texture
///
/// To composite your own GPU texture on a web-page, you should first reserve a texture ID from
/// ULGPUDriver::next_texture_id and then create an ImageSource from that texture ID. Next, you
/// should register the ImageSource with ImageSourceProvider using the identifier from the `.imgsrc`
/// file.
///
/// When the image element is drawn on the web-page, the library will draw geometry using the
/// specified texture ID and UV coordinates. You should bind your own texture when the specified
/// texture ID is used.
///
/// If the GPU renderer is not enabled for the View or pixel data is needed for other purposes, the
/// library will sample the backing bitmap instead.
///
/// ## Creating from a Bitmap
///
/// To composite your own bitmap on a web-page, you should create an ImageSource from a Bitmap.
/// Next, you should register the ImageSource with ImageSourceProvider using the identifier from
/// the `.imgsrc` file.
///
/// When the image element is drawn on the web-page, the library will sample this bitmap directly.
///
/// ## Invalidating Images
///
/// If you modify the texture or bitmap after creating the ImageSource, you should call
/// ulImageSourceInvalidate() to notify the library that the image should be redrawn.
///
#ifndef ULTRALIGHT_CAPI_IMAGESOURCE_H
#define ULTRALIGHT_CAPI_IMAGESOURCE_H

#include <Ultralight/CAPI/CAPI_Defines.h>

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
 * ImageSource
 *****************************************************************************/

///
/// Create an image source from a GPU texture with optional backing bitmap.
///
/// @param  width  The width of the image in pixels (used for layout).
///
/// @param  height  The height of the image in pixels (used for layout).
///
/// @param  texture_id  The GPU texture identifier to bind when drawing the quad for this image.
///                     This should be non-zero and obtained from ULGPUDriver::next_texture_id.
///
/// @param  texture_uv  The UV coordinates of the texture.
///
/// @param  bitmap  Optional backing bitmap for this image source. This is used when drawing
///                 the image using the CPU renderer or when pixel data is needed for other
///                 purposes. You should update this bitmap when the texture changes.
///
/// @return  A new image source instance. You must call ulDestroyImageSource() when finished.
///
ULExport ULImageSource ulCreateImageSourceFromTexture(unsigned int width, unsigned int height,
                                                      unsigned int texture_id, ULRect texture_uv,
                                                      ULBitmap bitmap);

///
/// Create an image source from a bitmap.
///
/// @param  bitmap  The backing bitmap for this image source.
///
/// @return  A new image source instance. You must call ulDestroyImageSource() when finished.
///
ULExport ULImageSource ulCreateImageSourceFromBitmap(ULBitmap bitmap);

///
/// Destroy an image source previously created with ulCreateImageSourceFromTexture() or
/// ulCreateImageSourceFromBitmap().
///
/// @param  image_source  The image source to destroy.
///
ULExport void ulDestroyImageSource(ULImageSource image_source);

///
/// Invalidate the image source, notifying the library that the image has changed
/// and should be redrawn.
///
ULExport void ulImageSourceInvalidate(ULImageSource image_source);

///
/// Get the width of the image source in pixels.
///
ULExport unsigned int ulImageSourceGetWidth(ULImageSource image_source);

///
/// Get the height of the image source in pixels.
///
ULExport unsigned int ulImageSourceGetHeight(ULImageSource image_source);

///
/// Get the GPU texture identifier for this image source.
///
/// @return  The texture ID, or 0 if the image source was created from a bitmap.
///
ULExport unsigned int ulImageSourceGetTextureId(ULImageSource image_source);

///
/// Get the UV coordinates of the texture for this image source.
///
ULExport ULRect ulImageSourceGetTextureUV(ULImageSource image_source);

///
/// Get the backing bitmap for this image source.
///
/// @return  The bitmap, or NULL if no bitmap was provided. Do not destroy the returned bitmap.
///          It stays valid until the next call to ulImageSourceGetBitmap() on the same thread
///          (for any image source), even if the image source is destroyed first.
///
ULExport ULBitmap ulImageSourceGetBitmap(ULImageSource image_source);

/******************************************************************************
 * ImageSourceProvider
 *****************************************************************************/

///
/// Add an image source to the provider.
///
/// @param  id  The identifier of the image source.
///
/// @param  image_source  The image source to add.
///
ULExport void ulImageSourceProviderAddImageSource(ULString id, ULImageSource image_source);

///
/// Remove an image source from the provider.
///
/// @param  id  The identifier of the image source.
///
ULExport void ulImageSourceProviderRemoveImageSource(ULString id);

///
/// Get an image source from the provider by its identifier.
///
/// @param  id  The identifier of the image source.
///
/// @return  The image source, or NULL if not found. Do not destroy the returned image source.
///          It stays valid until the next call to ulImageSourceProviderGetImageSource() on the
///          same thread, even if the image source is removed from the provider first.
///
ULExport ULImageSource ulImageSourceProviderGetImageSource(ULString id);

#ifdef __cplusplus
}
#endif

#endif // ULTRALIGHT_CAPI_IMAGESOURCE_H