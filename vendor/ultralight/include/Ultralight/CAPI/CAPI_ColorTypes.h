///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// @file CAPI_ColorTypes.h
///
/// The ULColor value type and its state enums.
///
/// `#include <Ultralight/CAPI/CAPI_ColorTypes.h>`
///
/// This header carries only the ULColor value type and its state enums; the color
/// functions live in `<Ultralight/CAPI/CAPI_Color.h>`.
///
#ifndef ULTRALIGHT_CAPI_COLORTYPES_H
#define ULTRALIGHT_CAPI_COLORTYPES_H

///
/// The color space of a ULColor's channels.
///
typedef enum {
  kULColorSpace_SRGB = 0,  ///< sRGB (the only value currently produced or accepted).
} ULColorSpace;

///
/// State bits carried in ULColor::flags.
///
typedef enum {
  ///
  /// The color was explicitly set. A zeroed ULColor lacks this bit and means "unset: the
  /// receiving field's default applies"; explicit transparent black carries it.
  ///
  kULColorFlags_Set = 1 << 0,

  ///
  /// The color came from a string that failed to parse. Invalid colors are inert-- whatever
  /// you pass one to ignores it and logs a warning.
  ///
  kULColorFlags_Invalid = 1 << 1,
} ULColorFlags;

///
/// A color value. Zero-initialized means unset (distinguishable from explicit transparent
/// black, which carries kULColorFlags_Set).
///
typedef struct {
  float r;               ///< Red, 0..1, in the space named by `space`.
  float g;               ///< Green, 0..1.
  float b;               ///< Blue, 0..1.
  float a;               ///< Alpha, 0..1, linear (1 is fully opaque).
  unsigned char space;   ///< A ULColorSpace value.
  unsigned char flags;   ///< A ULColorFlags combination.
} ULColor;

#endif  // ULTRALIGHT_CAPI_COLORTYPES_H
