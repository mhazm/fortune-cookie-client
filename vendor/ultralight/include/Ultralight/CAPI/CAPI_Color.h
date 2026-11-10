///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// @file CAPI_Color.h
///
/// An RGBA color value in a certain color space (with CSS parsing helpers).
///
/// `#include <Ultralight/CAPI/CAPI_Color.h>`
///
/// C mirror of the C++ Color class (see `<Ultralight/Color.h>` for the full semantics). ULColor
/// is a plain value struct: a zero-initialized ULColor is unset, distinguishable from explicit
/// transparent black (which carries the kULColorFlags_Set bit). An unset color passed to any
/// setter or descriptor field means the built-in default.
///
#ifndef ULTRALIGHT_CAPI_COLOR_H
#define ULTRALIGHT_CAPI_COLOR_H

#include <Ultralight/CAPI/CAPI_ColorTypes.h>
#include <Ultralight/CAPI/CAPI_Defines.h>

#ifdef __cplusplus
extern "C" {
#endif

///
/// Parse a CSS color string at runtime: the full CSS color grammar (named colors, hex,
/// rgb()/hsl()/hwb()/lab()/oklch()/color(), `transparent`). Wide-gamut input resolves to sRGB.
///
/// @param  css  A null-terminated UTF-8 CSS color string. Passing NULL yields an invalid
///              color, exactly as an unparsable string does.
///
/// @return  Returns the parsed color (kULColorFlags_Set) on success, or a color carrying
///          kULColorFlags_Invalid (and not Set) when the string does not parse.
///
ULExport ULColor ulColorParse(const char* css);

///
/// Serialize a color as CSS hex text: lowercase `#rrggbb`, or `#rrggbbaa` when the alpha
/// channel is not fully opaque. Float channels quantize to 8-bit here (the hex wire form is
/// 8-bit), so ulColorParse() of the result round-trips to the quantized channels.
///
/// @return  Returns the hex text, or an empty string when the color is unset or invalid.
///          You must call ulDestroyString() when finished.
///
/// @see  ulColorParse()
///
ULExport ULString ulColorToHexString(ULColor color);

///
/// Create a set sRGB color from float channels (0..1; alpha 1 is fully opaque).
///
static inline ULColor ulColorRGBA(float r, float g, float b, float a) {
  ULColor color;
  color.r = r;
  color.g = g;
  color.b = b;
  color.a = a;
  color.space = (unsigned char)kULColorSpace_SRGB;
  color.flags = (unsigned char)kULColorFlags_Set;
  return color;
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // ULTRALIGHT_CAPI_COLOR_H
