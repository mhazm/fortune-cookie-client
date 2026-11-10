///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///

///
/// @file CAPI_Defines.h
///
/// Various defines and utility functions for the C API.
///
/// `#include <Ultralight/CAPI/CAPI_Defines.h>`
///
/// This file contains various defines, structures, and utility functions for the C API.
///
/// ## Text with a Length
///
/// A function that takes UTF-8 text as a pointer and a byte length reads exactly `length` bytes,
/// so the text doesn't need a terminating NUL and can contain NUL characters. A length of 0 always
/// means an empty string. To pass a NUL-terminated C string, pass `strlen(text)` as the length.
///
#ifndef ULTRALIGHT_CAPI_DEFINES_H
#define ULTRALIGHT_CAPI_DEFINES_H

#include <stddef.h>
#include <stdint.h>
#include <Ultralight/Config.h>
#include <JavaScriptCore/JavaScript.h>
#ifdef __OBJC__
#import <AppKit/NSEvent.h>
#endif

#if defined(ULTRALIGHT_STATIC_BUILD)
#define ULExport
#else
#if defined(__WIN32__) || defined(_WIN32)
#if defined(ULTRALIGHT_IMPLEMENTATION)
#define ULExport __declspec(dllexport)
#else
#define ULExport __declspec(dllimport)
#endif
#else
#define ULExport __attribute__((visibility("default")))
#endif
#endif

///
/// Marks a C API function as deprecated; calls to it produce a compiler warning with `msg`.
///
#if defined(_MSC_VER)
#define UL_DEPRECATED(msg) __declspec(deprecated(msg))
#elif defined(__GNUC__) || defined(__clang__)
#define UL_DEPRECATED(msg) __attribute__((deprecated(msg)))
#else
#define UL_DEPRECATED(msg)
#endif

#if defined(__WIN32__) || defined(_WIN32)
#define _thread_local __declspec(thread)
#ifndef _NATIVE_WCHAR_T_DEFINED
#define DISABLE_NATIVE_WCHAR_T
typedef unsigned short ULChar16;
#else
typedef wchar_t ULChar16;
#endif
#else
#define _thread_local __thread
typedef unsigned short ULChar16;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/// Opaque handle to a Config object. @see  ulCreateConfig, ulDestroyConfig
typedef struct C_Config* ULConfig;

/// Opaque handle to a Renderer object. @see  ulCreateRenderer, ulDestroyRenderer
typedef struct C_Renderer* ULRenderer;

/// Opaque handle to a Session object. @see  ulCreateSession, ulDestroySession
typedef struct C_Session* ULSession;

/// Opaque handle to a ViewConfig object. @see  ulCreateViewConfig, ulDestroyViewConfig
typedef struct C_ViewConfig* ULViewConfig;

/// Opaque handle to a View object. @see  ulCreateView, ulDestroyView
typedef struct C_View* ULView;

/// Opaque handle to a View's Editor object. @see  ulViewGetEditor
typedef struct C_Editor* ULEditor;

/// Opaque handle to a Bitmap object. @see  ulCreateBitmap, ulDestroyBitmap
typedef struct C_Bitmap* ULBitmap;

/// Opaque handle to a String object. @see  ulCreateString, ulDestroyString
typedef struct C_String* ULString;

/// Opaque handle to a Buffer object. @see  ulCreateBuffer, ulDestroyBuffer
typedef struct C_Buffer* ULBuffer;

typedef struct C_ClipboardData* ULClipboardData;

/// Opaque handle to a KeyEvent object. @see  ulCreateKeyEvent, ulDestroyKeyEvent
typedef struct C_KeyEvent* ULKeyEvent;

/// Opaque handle to a MouseEvent object. @see  ulCreateMouseEvent, ulDestroyMouseEvent
typedef struct C_MouseEvent* ULMouseEvent;

/// Opaque handle to a ScrollEvent object. @see  ulCreateScrollEvent, ulDestroyScrollEvent
typedef struct C_ScrollEvent* ULScrollEvent;

/// Opaque handle to a GamepadEvent object. @see  ulCreateGamepadEvent, ulDestroyGamepadEvent
typedef struct C_GamepadEvent* ULGamepadEvent;

/// Opaque handle to a GamepadAxisEvent object. @see  ulCreateGamepadAxisEvent, ulDestroyGamepadAxisEvent
typedef struct C_GamepadAxisEvent* ULGamepadAxisEvent;

/// Opaque handle to a GamepadButtonEvent object. @see  ulCreateGamepadButtonEvent, ulDestroyGamepadButtonEvent
typedef struct C_GamepadButtonEvent* ULGamepadButtonEvent;

/// Opaque handle to a Surface object. @see  ULSurfaceDefinition
typedef struct C_Surface* ULSurface;

/// Opaque handle to a BitmapSurface (the default Surface implementation). @see  ulBitmapSurfaceGetBitmap
typedef struct C_Surface* ULBitmapSurface;

/// Opaque handle to a FontFile object. @see  ulFontFileCreateFromFilePath, ulDestroyFontFile
typedef struct C_FontFile* ULFontFile;

/// Opaque handle to an ImageSource object. @see  ulCreateImageSourceFromTexture, ulDestroyImageSource
typedef struct C_ImageSource* ULImageSource;

///
/// Callback invoked exactly once when the library finally drops a piece of user data. For a
/// callback slot that is when the callback is replaced or its owner is destroyed; for a posted
/// task it is after the task has run, or without the task running if it is dropped first. Never
/// invoked while the callback or task itself is executing. May be NULL.
///
typedef void (*ULUserDataDestroyCallback)(void* user_data);

///
/// A page color scheme, as reported to pages via the `prefers-color-scheme` CSS media feature.
///
/// @see  ulViewConfigSetPreferredColorScheme, ulViewSetPreferredColorScheme,
///      ulRendererSetSystemColorScheme
///
typedef enum {
  kColorScheme_Auto = 0,   ///< Follow the system color scheme set on the Renderer.
  kColorScheme_Light = 1,  ///< Always report a light color scheme.
  kColorScheme_Dark = 2,   ///< Always report a dark color scheme.
} ULColorScheme;

///
/// Policy governing script-initiated clipboard reads.
///
/// @see  ulViewConfigSetClipboardReadPolicy
///
typedef enum {
  kClipboardReadPolicy_Deny = 0,                ///< Refuse every script-initiated clipboard read.
  kClipboardReadPolicy_AllowForAppContent = 1,  ///< Grant reads from application content only.
  kClipboardReadPolicy_Allow = 2,               ///< Grant reads from any page.
} ULClipboardReadPolicy;

typedef enum {
  ///
  /// Alpha channel only, 8-bits per pixel.
  ///
  /// Encoding: 8-bits per channel, unsigned normalized.
  ///
  /// Color-space: Linear (no gamma), alpha-coverage only.
  ///
  kBitmapFormat_A8_UNORM,

  ///
  /// Blue Green Red Alpha channels, 32-bits per pixel.
  ///
  /// Encoding: 8-bits per channel, unsigned normalized.
  ///
  /// Color-space: sRGB gamma with premultiplied linear alpha channel.
  ///
  kBitmapFormat_BGRA8_UNORM_SRGB,

  ///
  /// Red Green channels, 16-bits per pixel.
  ///
  /// Encoding: 8-bits per channel, unsigned normalized.
  ///
  /// Color-space: Linear (no gamma).
  ///
  /// Not PNG-encodable.
  ///
  kBitmapFormat_RG8_UNORM,

  ///
  /// BC1 block-compressed color (4x4 blocks, 8 bytes/block). Optional 1-bit alpha.
  ///
  /// Color-space: sRGB gamma with optional alpha.
  ///
  kBitmapFormat_BC1_UNORM,

  ///
  /// BC2 block-compressed color (4x4 blocks, 16 bytes/block). Explicit 4-bit alpha.
  ///
  /// Color-space: sRGB gamma with premultiplied alpha.
  ///
  kBitmapFormat_BC2_UNORM,

  ///
  /// BC3 block-compressed color (4x4 blocks, 16 bytes/block). Interpolated alpha.
  ///
  /// Color-space: sRGB gamma with premultiplied alpha.
  ///
  kBitmapFormat_BC3_UNORM,

  ///
  /// BC7 block-compressed color (4x4 blocks, 16 bytes/block). High-quality RGB(A).
  ///
  /// Color-space: sRGB gamma with premultiplied alpha.
  ///
  kBitmapFormat_BC7_UNORM,

  ///
  /// Four half-precision float channels (16 bits per channel, 64 bits per pixel).
  ///
  /// This is a raw data format: a GPU driver should create these as sampleable textures without
  /// filtering or mipmaps and upload rows exactly as stored.
  ///
  kBitmapFormat_RGBA16F,

  ///
  /// Four unsigned integer channels (16 bits per channel, 64 bits per pixel).
  ///
  /// This is a raw data format: a GPU driver should create these as sampleable textures without
  /// filtering or mipmaps and upload rows exactly as stored.
  ///
  kBitmapFormat_RGBA16UI,

  ///
  /// Four float channels (32 bits per channel, 128 bits per pixel).
  ///
  /// This is a raw data format: a GPU driver should create these as sampleable textures without
  /// filtering or mipmaps and upload rows exactly as stored.
  ///
  kBitmapFormat_RGBA32F
} ULBitmapFormat;

typedef enum {
  ///
  /// Key-Down event type. This type combines a key press with its text input: it triggers editing
  /// shortcuts (eg, Ctrl+C for copy) and, when the event carries text, also inserts that text.
  ///
  /// @warning  You should probably use kKeyEventType_RawKeyDown instead (followed by a
  ///           kKeyEventType_Char event for any text). This type is only here for historic
  ///           compatibility with WebCore's key event types.
  ///
  kKeyEventType_KeyDown,

  ///
  /// Key-Up event type. Use this when a physical key is released.
  ///
  kKeyEventType_KeyUp,

  ///
  /// Raw Key-Down type. Use this when a physical key is pressed.
  ///
  kKeyEventType_RawKeyDown,

  ///
  /// Character input event type. Use this when the OS generates text from
  /// a physical key being pressed (eg, WM_CHAR on Windows).
  ///
  kKeyEventType_Char,
} ULKeyEventType;

///
/// The various MouseEvent types.
///
/// @see  ulCreateMouseEvent
///
typedef enum {
  /// Mouse moved type
  kMouseEventType_MouseMoved,

  /// Mouse button pressed type
  kMouseEventType_MouseDown,

  /// Mouse button released type
  kMouseEventType_MouseUp,
} ULMouseEventType;

///
/// The various mouse button types.
///
/// @see  ulCreateMouseEvent
///
typedef enum {
  kMouseButton_None = 0, ///< No button
  kMouseButton_Left,     ///< Left mouse button
  kMouseButton_Middle,   ///< Middle mouse button
  kMouseButton_Right,    ///< Right mouse button
  kMouseButton_Back,     ///< First side button ("back"); reaches the page as button 3.
  kMouseButton_Forward,  ///< Second side button ("forward"); reaches the page as button 4.
} ULMouseButton;

///
/// The scroll event granularity type.
///
/// @see  ulCreateScrollEvent
///
typedef enum {
  kScrollEventType_ScrollByPixel, ///< Delta value is interpreted as number of pixels to scroll.
  kScrollEventType_ScrollByPage,  ///< Delta value is interpreted as number of pages to scroll.
} ULScrollEventType;

///
/// The various GamepadEvent types.
///
/// @see  ulCreateGamepadEvent
///
typedef enum {
  ///
  /// This event type should be fired when a gamepad is connected.
  ///
  /// @note  You will need to previously declare the gamepad, its index, and details about
  ///        its axis and button layout via ulSetGamepadDetails() prior to calling
  ///        ulFireGamepadEvent().
  ///
  kGamepadEventType_Connected,

  /// This event type should be fired when a gamepad is disconnected.
  kGamepadEventType_Disconnected,
} ULGamepadEventType;

///
/// The winding order for front-facing triangles.
///
/// @see  ulConfigSetFaceWinding
///
typedef enum {
  kFaceWinding_Clockwise,        ///< Clockwise winding (default for Ultralight)
  kFaceWinding_CounterClockwise, ///< Counter-clockwise winding
} ULFaceWinding;

typedef enum {
  ///
  /// Lighter hinting algorithm-- glyphs are slightly fuzzier but better
  /// resemble their original shape. This is achieved by snapping glyphs to the
  /// pixel grid only vertically which better preserves inter-glyph spacing.
  ///
  kFontHinting_Smooth,

  ///
  /// Default hinting algorithm-- offers a good balance between sharpness and
  /// shape at smaller font sizes.
  ///
  kFontHinting_Normal,

  ///
  /// Strongest hinting algorithm-- outputs only black/white glyphs. The result
  /// is usually unpleasant if the underlying TTF does not contain hints for
  /// this type of rendering.
  ///
  kFontHinting_Monochrome,

  ///
  /// No hinting is performed-- fonts may be blurry at smaller font sizes.
  ///
  kFontHinting_None,
} ULFontHinting;

///
/// The font-appearance preset that provides default values for the text rendering knobs.
///
/// @see  ulConfigSetFontProfile
///
typedef enum {
  ///
  /// The standard appearance-- currently the same as kFontProfile_WindowsLike.
  ///
  kFontProfile_Default,

  ///
  /// Text with the weight and sharpness of Safari and other macOS applications.
  ///
  kFontProfile_MacOSLike,

  ///
  /// Text with the crisp, moderate weight of mainstream browsers on Windows.
  ///
  kFontProfile_WindowsLike,

  ///
  /// No weight compensation at all-- the thinnest appearance.
  ///
  kFontProfile_Light,
} ULFontProfile;

///
/// How antialiased glyph edges are blended into the page.
///
/// @see  ulConfigSetFontBlendMode
///
typedef enum {
  ///
  /// Use the value from the font profile.
  ///
  kFontBlendMode_Profile,

  ///
  /// Gamma-corrected blending-- edges read lighter with more contrast (how browsers on Windows
  /// composite text).
  ///
  kFontBlendMode_GammaCorrected,

  ///
  /// Blend in sRGB space-- edges keep their full coverage (how macOS composites text).
  ///
  kFontBlendMode_SRGB,
} ULFontBlendMode;

///
/// The quality level for rendering effects (CSS filters, shadows, blurs, etc.).
///
/// @see  ulConfigSetEffectQuality
///
typedef enum {
  ///
  /// Fastest effect quality-- uses the lowest quality effects (half-resolution, fewer passes, etc.)
  ///
  kEffectQuality_Low,

  ///
  /// Default effect quality-- strikes a good balance between quality and performance.
  ///
  kEffectQuality_Medium,

  ///
  /// Highest effect quality-- favors quality over performance.
  ///
  kEffectQuality_High,
} ULEffectQuality;

///
/// Memory vs. performance tuning for the renderer's HTML / JavaScript engine heaps.
///
/// The profile governs how aggressively the library returns freed heap memory to the operating
/// system and how it balances allocation speed against memory footprint.
///
/// @see  ulConfigSetMemoryProfile
///
typedef enum {
  ///
  /// Sensible default. Freed memory is returned to the OS on a moderate cadence and allocation
  /// favors speed. Recommended unless you have a specific reason to choose otherwise.
  ///
  kMemoryProfile_Balanced,

  ///
  /// Optimize for low memory footprint at some cost to peak throughput. Freed memory is returned
  /// to the OS promptly, allocations are packed more densely, and the JavaScript engine collects
  /// garbage more eagerly.
  ///
  kMemoryProfile_LowMemory,

  ///
  /// Optimize for peak throughput at the cost of a higher steady-state memory footprint. Freed
  /// memory is held longer for reuse and the JavaScript engine collects garbage less often.
  ///
  kMemoryProfile_Performance,
} ULMemoryProfile;

#if UL_HAS(MEDIA)
///
/// Memory vs. performance tuning for HTML5 `<video>` and `<audio>`.
///
/// Pick the profile that matches your target's resource envelope; the engine will tune internal
/// media-pipeline sizing accordingly.
///
/// @see  ulConfigSetMediaProfile
///
typedef enum {
  ///
  /// Sensible default. Smooth playback and responsive seeking with bounded memory growth as more
  /// media elements load.
  ///
  kMediaProfile_Balanced,

  ///
  /// Optimize for low memory footprint at the cost of some smoothness and seek responsiveness.
  ///
  kMediaProfile_FavorMemory,

  ///
  /// Optimize for the smoothest playback and snappiest seeking, at the cost of higher peak memory
  /// per media element.
  ///
  kMediaProfile_FavorPerformance,
} ULMediaProfile;
#endif // UL_HAS(MEDIA)

///
/// The type of thread, you can choose to optionally handle these for better performance.
///
/// @see  ULThreadFactoryCreateThreadCallback
///
typedef enum {
  kThreadType_Unknown = 0,
  kThreadType_JavaScript,
  kThreadType_Compiler,
  kThreadType_GarbageCollection,
  kThreadType_Network,
  kThreadType_Graphics,
  kThreadType_Audio,
} ULThreadType;

///
/// Keyboard modifier flags, used with ulCreateKeyEvent.
///
/// These can be combined with bitwise OR.
///
typedef enum {
  /// Whether or not an ALT key is down
  kKeyEventModifier_AltKey = 1 << 0,

  /// Whether or not a Control key is down
  kKeyEventModifier_CtrlKey = 1 << 1,

  /// Whether or not a meta key (Command-key on Mac, Windows-key on Win) is down
  kKeyEventModifier_MetaKey = 1 << 2,

  /// Whether or not a Shift key is down
  kKeyEventModifier_ShiftKey = 1 << 3,
} ULKeyEventModifiers;

///
/// Float rectangle defined by left, top, right, and bottom coordinates.
///
typedef struct {
  float left;
  float top;
  float right;
  float bottom;
} ULRect;

///
/// Integer rectangle defined by left, top, right, and bottom coordinates.
///
typedef struct {
  int left;
  int top;
  int right;
  int bottom;
} ULIntRect;

///
/// Offscreen render target, used when rendering Views via the GPU renderer.
///
/// When a View is rendered via the GPU renderer (see ulViewIsAccelerated()), it will be rendered to
/// an offscreen render target (ulViewGetRenderTarget()) that you can display in your application.
///
/// This is intended to be used with a custom ULGPUDriver implementation in a game or similar 
/// application (ulPlatformSetGPUDriver()).
///
typedef struct {
  /// Whether this target is empty (null texture).
  bool is_empty;

  /// The viewport width (in device coordinates).
  unsigned int width;

  /// The viewport height (in device coordinates).
  unsigned int height;

  /// The GPUDriver-specific texture ID (you should bind the texture using your custom GPUDriver
  /// implementation before drawing a quad).
  unsigned int texture_id;

  /// The texture width (in pixels). This may be padded.
  unsigned int texture_width;

  /// The texture height (in pixels). This may be padded.
  unsigned int texture_height;

  /// The pixel format of the texture.
  ULBitmapFormat texture_format;

  /// UV coordinates of the texture (this is needed because the texture may be padded).
  ULRect uv_coords;

  /// The GPUDriver-specific render buffer ID.
  unsigned int render_buffer_id;
} ULRenderTarget;

/******************************************************************************
 * Version
 *****************************************************************************/

///
/// Get the version string of the library in MAJOR.MINOR.PATCH format.
///
ULExport const char* ulVersionString(void);

///
/// Get the numeric major version of the library.
///
ULExport unsigned int ulVersionMajor(void);

///
/// Get the numeric minor version of the library.
///
ULExport unsigned int ulVersionMinor(void);

///
/// Get the numeric patch version of the library.
///
ULExport unsigned int ulVersionPatch(void);

///
/// Get the full WebKit version string.
///
ULExport const char* ulWebKitVersionString(void);

///
/// Get the numeric major version of the WebKit engine.
///
ULExport unsigned int ulWebKitVersionMajor(void);

///
/// Get the numeric minor version of the WebKit engine.
///
ULExport unsigned int ulWebKitVersionMinor(void);

///
/// Get the numeric tiny version of the WebKit engine.
///
ULExport unsigned int ulWebKitVersionTiny(void);

///
/// Get the numeric micro version of the WebKit engine.
///
ULExport unsigned int ulWebKitVersionMicro(void);

///
/// Get the numeric nano version of the WebKit engine.
///
ULExport unsigned int ulWebKitVersionNano(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ULTRALIGHT_CAPI_DEFINES_H