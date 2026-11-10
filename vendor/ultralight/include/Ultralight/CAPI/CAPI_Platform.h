///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///

///
/// @file CAPI_Platform.h
/// 
/// Global platform singleton, manages user-defined platform handlers.
///
/// `#include <Ultralight/CAPI/CAPI_Platform.h>`
///
/// The library uses the Platform API for most platform-specific operations (eg, file access,
/// clipboard, font loading, GPU access, pixel buffer transport, etc.).
///
/// ## Motivation
///
/// Ultralight is designed to work in as many platforms and environments as possible. To achieve
/// this, we've factored out most platform-specific code into a set of interfaces that you can
/// implement and set on the Platform singleton.
///
/// ## Default Implementations
///
/// We provide a number of default implementations for desktop platforms (eg, Windows, macOS, Linux)
/// for you when you call ulCreateApp(). These implementations ship as Zlib-licensed source in
/// the SDK's `platform` folder, we recommend using their source code as a starting point for your
/// own implementations.
///
/// ## Required Handlers
///
/// When using ulCreateRenderer() directly, you'll need to provide your own implementations for
/// ULFileSystem and ULFontLoader at a minimum.
/// 
/// @par Overview of which platform handlers are required / optional / provided:
/// 
/// |                     | ulCreateRenderer() | ulCreateApp() |
/// |---------------------|--------------------|---------------|
/// | ULFileSystem        | **Required**       | *Provided*    |
/// | ULFontLoader        | **Required**       | *Provided*    |
/// | ULClipboard         |  *Optional*        | *Provided*    |
/// | ULGPUDriver         |  *Optional*        | *Provided*    |
/// | ULLogger            |  *Optional*        | *Provided*    |
/// | ULSurfaceDefinition |  *Provided*        | *Provided*    |
/// | ULThreadFactory     |  *Optional*        | *Optional*    |
/// | ULAudioOutput       |  *Optional*        | *Provided*    |
/// | ULProfiler          |  *Optional*        | *Optional*    |
///
/// ULAudioOutput and ULProfiler require the Pro edition or higher.
///
/// ## Setting Handlers
///
/// Each ulPlatformSet*() function copies the struct you pass it, so the struct can be a temporary.
/// The function pointers in it must stay valid until the Renderer or App is destroyed.
///
/// Set each handler once, before calling ulCreateRenderer() or ulCreateApp(). A later call
/// replaces a handler that the library may still be using.
///
#ifndef ULTRALIGHT_CAPI_PLATFORM_H
#define ULTRALIGHT_CAPI_PLATFORM_H

#include <Ultralight/CAPI/CAPI_Defines.h>
#include <Ultralight/CAPI/CAPI_Logger.h>
#include <Ultralight/CAPI/CAPI_FileSystem.h>
#include <Ultralight/CAPI/CAPI_FontLoader.h>
#include <Ultralight/CAPI/CAPI_Surface.h>
#include <Ultralight/CAPI/CAPI_GPUDriver.h>
#include <Ultralight/CAPI/CAPI_Clipboard.h>
#include <Ultralight/CAPI/CAPI_ThreadFactory.h>
#include <Ultralight/CAPI/CAPI_AudioOutput.h>
#include <Ultralight/CAPI/CAPI_Profiler.h>

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
 * Platform
 *****************************************************************************/

///
/// Set a custom Logger implementation.
///
/// This is used to log debug messages to the console or to a log file.
///
/// You should call this before ulCreateRenderer() or ulCreateApp().
///
/// \parblock
/// @note  ulCreateApp() will use the default logger if you never call this.
/// \endparblock
///
/// \parblock
/// @note  If you're not using ulCreateApp(), (eg, using ulCreateRenderer()) you can still use the
///        default logger by calling ulEnableDefaultLogger() (@see  <AppCore/CAPI.h>)
/// \endparblock
///
ULExport void ulPlatformSetLogger(ULLogger logger);

///
/// Set a custom FileSystem implementation.
///
/// The library uses this to load all file URLs (eg, <file:///page.html>).
///
/// You can provide the library with your own FileSystem implementation so that file assets are
/// loaded from your own pipeline.
///
/// You should call this before ulCreateRenderer() or ulCreateApp().
/// 
/// @warning  This is required to be defined before calling ulCreateRenderer()
///
/// \parblock
/// @note  ulCreateApp() will use the default platform file system if you never call this.
/// \endparblock
///
/// \parblock
/// @note  If you're not using ulCreateApp(), (eg, using ulCreateRenderer()) you can still use the
///        default platform file system by calling ulEnablePlatformFileSystem()'
///        (@see  <AppCore/CAPI.h>)
/// \endparblock
///
ULExport void ulPlatformSetFileSystem(ULFileSystem file_system);

///
/// Set a custom FontLoader implementation.
/// 
/// The library uses this to load all system fonts.
///
/// Every operating system has its own library of installed system fonts. The FontLoader interface
/// is used to lookup these fonts and fetch the actual font data (raw TTF/OTF file data) for a given
/// given font description.
/// 
/// You should call this before ulCreateRenderer() or ulCreateApp().
/// 
/// @warning  This is required to be defined before calling ulCreateRenderer()
/// 
/// \parblock
/// @note  ulCreateApp() will use the default platform font loader if you never call this.
/// \endparblock
///
/// \parblock
/// @note  If you're not using ulCreateApp(), (eg, using ulCreateRenderer()) you can still use the
///        default platform font loader by calling ulEnablePlatformFontLoader()'
///        (@see  <AppCore/CAPI.h>)
/// \endparblock
///
ULExport void ulPlatformSetFontLoader(ULFontLoader font_loader);

///
/// Set a custom Surface implementation.
///
/// This can be used to wrap a platform-specific GPU texture, Windows DIB, macOS CGImage, or any
/// other pixel buffer target for display on screen.
///
/// By default, the library uses a bitmap surface for all surfaces but you can override this by
/// providing your own surface definition here.
///
/// You should call this before ulCreateRenderer() or ulCreateApp().
///
ULExport void ulPlatformSetSurfaceDefinition(ULSurfaceDefinition surface_definition);

///
/// Set a custom GPUDriver implementation.
///
/// This should be used if you are using ulCreateRenderer() (which does not provide its own
/// GPUDriver implementation) and want Views rendered on the GPU. A View renders on the GPU when
/// you create it with ulViewConfigSetIsAccelerated() set to true.
///
/// The GPUDriver interface is used by the library to dispatch GPU calls to your native GPU context
/// (eg, D3D11, Metal, OpenGL, Vulkan, etc.) There are Zlib-licensed reference
/// implementations for this interface in the SDK's `platform` folder.
///
/// You should call this before ulCreateRenderer().
///
ULExport void ulPlatformSetGPUDriver(ULGPUDriver gpu_driver);

///
/// Set a custom Clipboard implementation.
///
/// This should be used if you are using ulCreateRenderer() (which does not provide its own
/// clipboard implementation).
///
/// The Clipboard interface is used by the library to make calls to the system's native clipboard
/// (eg, cut, copy, paste).
///
/// You should call this before ulCreateRenderer().
///
ULExport void ulPlatformSetClipboard(ULClipboard clipboard);

///
/// Set a custom ThreadFactory implementation.
///
/// This can be used to provide a platform-specific thread creation implementation for the library
/// to use when creating threads (useful for tracking thread creation, setting thread names, etc).
///
/// You should call this before ulCreateRenderer() or ulCreateApp().
///
/// @note  The library creates threads using the default platform-specific thread creation
///        functions if you never call this.
///
ULExport void ulPlatformSetThreadFactory(ULThreadFactory thread_factory);

#if UL_HAS(MEDIA)
///
/// Set a custom AudioOutput implementation.
///
/// The library uses this to play decoded audio for `<video>` and `<audio>` elements. ulCreateApp()
/// provides one for you unless you've set your own first. If you are using ulCreateRenderer() and
/// don't set one, media plays without sound.
///
/// You should call this before ulCreateRenderer() or ulCreateApp(). The callbacks must remain
/// valid until after the Renderer/App is destroyed.
///
/// @pre  Requires the Pro edition or higher.
///
ULExport void ulPlatformSetAudioOutput(ULAudioOutput audio_output);
#endif // UL_HAS(MEDIA)

#if UL_HAS(PROFILER)
///
/// Set a custom Profiler implementation.
///
/// When set, the library emits structured timing data (scopes, events, counters) at key
/// internal boundaries that can be forwarded to any profiling tool or trace format.
///
/// You should call this before ulCreateRenderer() or ulCreateApp(). The profiler callbacks
/// must remain valid until after the Renderer/App is destroyed.
///
/// @pre  Requires the Pro edition or higher.
///
ULExport void ulPlatformSetProfiler(ULProfiler profiler);
#endif // UL_HAS(PROFILER)

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ULTRALIGHT_CAPI_PLATFORM_H