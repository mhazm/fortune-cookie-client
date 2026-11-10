///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///

///
/// @file CAPI_Config.h
///
/// Core configuration for the renderer.
///
/// `#include <Ultralight/CAPI/CAPI_Config.h>`
///
/// These are various configuration options that can be used to customize the behavior of the
/// library. These options can only be set once before creating the Renderer.
///
#ifndef ULTRALIGHT_CAPI_CONFIG_H
#define ULTRALIGHT_CAPI_CONFIG_H

#include <Ultralight/CAPI/CAPI_Defines.h>
#include <Ultralight/CAPI/CAPI_Logger.h>

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
 * Config
 *****************************************************************************/

///
/// How much checking and warning the library does for mistakes made through one of its APIs.
///
/// Warnings go to the Logger at kLogLevel_Warning, tagged with the API they came from (eg,
/// `[js]`, `[dom]`, `[data]`), and to the page's console when a page is involved.
///
/// @see  ulConfigSetJavaScriptDiagnostics(), ulConfigSetDOMDiagnostics()
///
typedef enum {
  ///
  /// No per-operation warnings (a mistake the library only finds once, like an invalid setup
  /// call, still warns).
  ///
  kULDiagnosticsLevel_Off = 0,

  ///
  /// Development warnings about each mistake as it happens.
  ///
  kULDiagnosticsLevel_Warn = 1,

  ///
  /// Everything kULDiagnosticsLevel_Warn reports, plus stricter checks (eg, a lossy argument
  /// conversion in a JavaScript binding becomes a TypeError, and every DOM call on a NULL handle
  /// warns).
  ///
  kULDiagnosticsLevel_Strict = 2,

  ///
  /// Warn when developer mode is on, Off when it's off (see ulConfigSetDeveloperMode()).
  ///
  kULDiagnosticsLevel_Auto = 3,
} ULDiagnosticsLevel;

///
/// Create config with default values (see <Ultralight/platform/Config.h>).
///
/// @return  Returns a new ULConfig instance. You must call ulDestroyConfig() when finished.
///
ULExport ULConfig ulCreateConfig(void);

///
/// Destroy a ULConfig instance created by ulCreateConfig().
///
/// @param  config  The config to destroy (can be NULL).
///
ULExport void ulDestroyConfig(ULConfig config);

///
/// A writable OS file path to store persistent Session data in.
///
/// This data may include cookies, cached network resources, indexed DB, etc.
///
/// @note  Files are only written to the path when using a persistent Session.
///
ULExport void ulConfigSetCachePath(ULConfig config, ULString cache_path);

///
/// The relative path to the resources folder (loaded via the FileSystem API).
///
/// The library loads certain resources (SSL certs, ICU data, etc.) from the FileSystem API
/// during runtime (eg, `file:///resources/cacert.pem`).
///
/// You can customize the relative file path to the resources folder by modifying this setting.
///
/// (Default = "resources/")
///
ULExport void ulConfigSetResourcePathPrefix(ULConfig config, ULString resource_path_prefix);

///
/// The winding order for front-facing triangles.
///
/// @pre  Only used when GPU rendering is enabled for the View.
///
/// (Default = kFaceWinding_CounterClockwise)
///
ULExport void ulConfigSetFaceWinding(ULConfig config, ULFaceWinding winding);

///
/// The hinting algorithm to use when rendering fonts. (Default = kFontHinting_Normal)
/// 
/// @see  ULFontHinting
///
ULExport void ulConfigSetFontHinting(ULConfig config, ULFontHinting font_hinting);

///
/// The font-appearance preset providing default values for the text rendering knobs (font
/// gamma, font contrast, font stem darkening, font embolden). Knobs you set explicitly override
/// the preset's values. (Default = kFontProfile_Default)
///
/// @see  ULFontProfile
///
ULExport void ulConfigSetFontProfile(ULConfig config, ULFontProfile font_profile);

///
/// Set how antialiased glyph edges are blended into the page (Default = kFontBlendMode_Profile,
/// meaning the font profile's value).
///
/// @see  ULFontBlendMode
///
ULExport void ulConfigSetFontBlendMode(ULConfig config, ULFontBlendMode font_blend_mode);

///
/// The gamma to use when compositing font glyphs, change this value to adjust contrast (Adobe and
/// Apple prefer 1.8, others may prefer 2.2). Set to 0 to use the value from the font profile.
/// (Default = 0, meaning the profile's value; the default profile uses 2.2)
///
ULExport void ulConfigSetFontGamma(ULConfig config, double font_gamma);

///
/// Additional contrast enhancement applied when compositing font glyphs. Values above 0 thicken
/// the midtones of antialiased glyph edges, increasing apparent text weight at every size. The
/// useful range is 0.0 (none) to about 1.0. Set below 0 to use the value from the font profile.
/// (Default = -1.0, meaning the profile's value; the default profile uses 0.3)
///
ULExport void ulConfigSetFontContrast(ULConfig config, double font_contrast);

///
/// Stem-darkening strength for rendered text. Boosts the coverage of antialiased glyph pixels
/// so thin stems keep their apparent weight on light backgrounds; each preset shapes how the
/// boost varies with glyph size. Text on dark backgrounds is unaffected. Set below 0 to use the
/// value from the font profile. (Default = -1.0, meaning the profile's value)
///
ULExport void ulConfigSetFontStemDarkening(ULConfig config, double font_stem_darkening);

///
/// Extra stroke weight applied to glyph shapes, as a fraction of the font size (for example,
/// 0.01 widens stems by roughly 1% of the em). Applies uniformly at every size and on both
/// light and dark backgrounds; glyph advances and layout are unaffected. Set below 0 to use the
/// value from the font profile. (Default = -1.0, meaning the profile's value)
///
ULExport void ulConfigSetFontEmbolden(ULConfig config, double font_embolden);

///
/// Global user-defined CSS string (included before any CSS on the page).
///
/// You can use this to override default styles for various elements on the page.
///
/// @note  This is an actual string of CSS, not a file path.
///
ULExport void ulConfigSetUserStylesheet(ULConfig config, ULString css_string);

///
/// Whether or not to continuously repaint any Views, regardless if they are dirty.
///
/// This is mainly used to diagnose painting/shader issues and profile performance.
/// 
/// (Default = False)
///
ULExport void ulConfigSetForceRepaint(ULConfig config, bool enabled);

///
/// Whether or not GPU-accelerated Views render vector content (path fills and strokes)
/// analytically instead of tessellating it to triangles. Analytic rendering is
/// resolution-independent: edges stay crisp under any zoom or transform.
///
/// Path rendering on CPU-rendered Views is unaffected. Disabling this also disables analytic
/// text (see ulConfigSetEnablePhotonText).
///
/// (Default = True)
///
ULExport void ulConfigSetEnablePhoton(ULConfig config, bool enabled);

///
/// Whether or not text and color emoji also render analytically when analytic vector rendering
/// is enabled (see ulConfigSetPhotonTextMinPx for the size cutoff). Ignored when analytic vector
/// rendering is disabled.
///
/// This selects the text appearance for both renderers: CPU-rendered Views match the analytic
/// look (unhinted outlines, fractional glyph placement) so output stays near-identical across
/// renderers.
///
/// (Default = True)
///
ULExport void ulConfigSetEnablePhotonText(ULConfig config, bool enabled);

///
/// The minimum on-screen glyph size, in pixels, at which text renders analytically when analytic
/// text is enabled; smaller glyphs use the rasterized glyph atlas. Set to 0 to render all text
/// analytically. (Default = 16)
///
ULExport void ulConfigSetPhotonTextMinPx(ULConfig config, unsigned int min_px);

///
/// Additional delay (in seconds) on top of the engine's internal page-settled detection.
///
/// Increase this if you want a little more assurance that the page has fully settled (animations
/// finished, late-arriving network activity complete) before ulViewIsSettled() flips and the
/// page-settled callback fires, at the cost of additional wait time.
///
/// (Default = 0.1)
///
/// @see  ulViewSetPageSettledCallback()
///
ULExport void ulConfigSetPageSettleDelay(ULConfig config, double delay);

///
/// The number of consecutive idle samples the library requires before it declares a page settled.
///
/// Together with the settle tick this sets the shortest time a page can take to settle (cycles *
/// tick). Lower it to settle sooner, at the cost of tolerance for late layout or paint.
///
/// (Default = 5)
///
/// @see  ulConfigSetPageSettleTick()
///
ULExport void ulConfigSetPageSettleCycles(ULConfig config, unsigned int cycles);

///
/// How often (in seconds) the library samples a loading page for idleness. (Default = 0.006)
///
/// @see  ulConfigSetPageSettleCycles()
///
ULExport void ulConfigSetPageSettleTick(ULConfig config, double tick);

///
/// The interval (in seconds) at which the library automatically recycles internal caches and
/// reclaims memory.
///
/// The library performs this lightweight recycle for you from inside ulUpdate() / ulRender(). It
/// is cheap enough to be invisible at typical frame rates, and letting the library drive it on
/// this timer is the recommended approach.
///
/// Set to 0 to disable the automatic recycler entirely, for applications that prefer to call
/// ulRecycle() on their own schedule.
///
/// (Default = 0.5)
///
ULExport void ulConfigSetRecycleDelay(ULConfig config, double delay);

///
/// Whether the library automatically collects garbage and returns freed memory to the OS without
/// embedder involvement.
///
/// When a page builds up a large JavaScript heap and then goes quiet, that memory would otherwise
/// stay resident until the next burst of activity or an explicit ulRecycle() / ulPurgeMemory().
/// With this enabled, the library collects on its own and pushes the freed memory back to the OS,
/// so the process footprint drifts back down after heavy content.
///
/// @note  Reclamation only happens while ulUpdate() is being pumped. An application that stops
///        calling ulUpdate() while idle gets no automatic reclamation; call ulRecycle() /
///        ulPurgeMemory() explicitly in that case.
///
/// (Default = True)
///
ULExport void ulConfigSetIdleGCEnabled(ULConfig config, bool enabled);

///
/// How long (in seconds) a page must be idle, with no user input, before automatic idle
/// collection begins.
///
/// Lower values reclaim sooner after the user stops interacting; higher values wait longer before
/// assuming the page is idle. Only used when idle GC is enabled.
///
/// (Default = 2.0)
///
/// @see  ulConfigSetIdleGCEnabled()
///
ULExport void ulConfigSetIdleGCIdleTime(ULConfig config, double idle_time);

///
/// The size of the library's memory cache in bytes.
///
/// @note  You should increase this if you anticipate handling pages with large resources, Safari
///        typically uses 128+ MiB for its cache.
///
/// (Default = 256 * 1024 * 1024)
///
ULExport void ulConfigSetMemoryCacheSize(ULConfig config, unsigned int size);

///
/// The number of pages to keep in the cache. (Default: 0, none)
///
/// @note
/// \parblock
///
/// Safari typically caches about 5 pages and maintains an on-disk cache to support typical
/// web-browsing activities.
///
/// If you increase this, you should probably increase the memory cache size as well.
///
/// \endparblock
///
/// (Default = 0)
///
ULExport void ulConfigSetPageCacheSize(ULConfig config, unsigned int size);

///
/// Memory vs. performance tuning for the renderer's HTML / JavaScript engine heaps. See the
/// ULMemoryProfile enumerators for the effect of each profile.
///
/// Read once when the Renderer is created. (Default = kMemoryProfile_Balanced)
///
/// @see  ULMemoryProfile
///
ULExport void ulConfigSetMemoryProfile(ULConfig config, ULMemoryProfile memory_profile);

///
/// The system's physical RAM size in bytes.
///
/// JavaScriptCore tries to detect the system's physical RAM size to set reasonable allocation
/// limits. Set this to anything other than 0 to override the detected value. Size is in bytes.
///
/// This can be used to force JavaScriptCore to be more conservative with its allocation strategy
/// (at the cost of some performance).
///
ULExport void ulConfigSetOverrideRAMSize(ULConfig config, unsigned int size);

///
/// The minimum size of large VM heaps in JavaScriptCore.
///
/// Set this to a lower value to make these heaps start with a smaller initial value.
///
/// Set to 0 to derive this value from the renderer's memory profile.
///
/// (Default = 0)
///
ULExport void ulConfigSetMinLargeHeapSize(ULConfig config, unsigned int size);

///
/// The minimum size of small VM heaps in JavaScriptCore.
///
/// Set this to a lower value to make these heaps start with a smaller initial value.
///
/// (Default = 1 * 1024 * 1024)
/// 
ULExport void ulConfigSetMinSmallHeapSize(ULConfig config, unsigned int size);

///
/// A soft target for the total JavaScriptCore heap size, in bytes.
///
/// This is not a hard limit: the heap may grow beyond this target. When it does, the garbage
/// collector treats the heap as under memory pressure and reclaims more aggressively to bring it
/// back toward the target.
///
/// Set to 0 to derive this value from the memory profile.
///
/// (Default = 0)
///
/// @see  ulConfigSetMemoryProfile()
///
ULExport void ulConfigSetMaxHeapSize(ULConfig config, unsigned int size);

///
/// The number of threads to use in the Renderer (for parallel painting on the CPU, etc.).
///
/// You can set this to a certain number to limit the number of threads to spawn.
///
/// @note
/// \parblock
///
/// If this value is 0, the number of threads will be determined at runtime using the following
/// formula:
///
///   ```
///   max(PhysicalProcessorCount() - 1, 1)
///   ```
///
/// \endparblock
///
ULExport void ulConfigSetNumRendererThreads(ULConfig config, unsigned int num_renderer_threads);

///
/// The max amount of time (in seconds) to allow repeating timers to run during each call to
/// Renderer::Update.
///
/// The library will attempt to throttle timers if this time budget is exceeded.
/// 
/// (Default = 1.0 / 200.0)
///
ULExport void ulConfigSetMaxUpdateTime(ULConfig config, double max_update_time);

///
/// The alignment (in bytes) of the BitmapSurface when using the CPU renderer.
///
/// The underlying bitmap associated with each BitmapSurface will have row_bytes padded to reach
/// this alignment.
///
/// Aligning the bitmap helps improve performance when using the CPU renderer. Determining the
/// proper value to use depends on the CPU architecture and max SIMD instruction set used.
///
/// We generally target the 128-bit SSE2 instruction set across most PC platforms so '16' is a safe
/// value to use.
///
/// You can set this to '0' to perform no padding (row_bytes will always be width * 4) at a slight
/// cost to performance.
///
/// (Default = 16)
///
ULExport void ulConfigSetBitmapAlignment(ULConfig config, unsigned int bitmap_alignment);

///
/// The quality of effects (blurs, CSS filters, SVG filters, etc.) to use when rendering.
///
/// (Default = kEffectQuality_Medium)
///
/// @see  ULEffectQuality
///
ULExport void ulConfigSetEffectQuality(ULConfig config, ULEffectQuality quality);

///
/// Whether to ignore SSL certificate errors when using the built-in network backend.
///
/// When set to true, SSL certificate verification will be disabled, allowing connections
/// to servers with self-signed or invalid certificates.
///
/// @warning  This reduces security and should never be enabled in production.
///
/// (Default = false)
///
ULExport void ulConfigSetIgnoreSSLErrors(ULConfig config, bool enabled);

///
/// Whether to validate TLS server certificates against the operating system's certificate
/// store instead of the certificate bundle shipped with the application (resources/cacert.pem).
///
/// When true, certificates trusted by the host operating system (including enterprise or
/// user-installed roots) are honored and the bundled list is not used.
///
/// Currently honored on Windows. On platforms where the OS trust store is not yet supported,
/// this option is ignored and the bundled list is used.
///
/// (Default = false)
///
ULExport void ulConfigSetUseSystemCertificateStore(ULConfig config, bool enabled);

///
/// Whether to paint full layers when dirty instead of just the dirty subregion.
///
/// This can be useful for debugging rendering issues or when dirty region tracking
/// is causing artifacts.
///
/// @note  Enabling this will impact performance as entire layers are redrawn.
///
/// (Default = false)
///
ULExport void ulConfigSetPaintFullLayers(ULConfig config, bool enabled);

///
/// The interval (in milliseconds) at which the library samples and emits resource counters
/// (memory usage, VRAM, cache sizes, etc.) via the Profiler's EmitCounter() callback.
///
/// Set to 0 to disable resource counter sampling entirely; scopes and events are unaffected.
///
/// (Default = 500)
///
ULExport void ulConfigSetResourceSampleInterval(ULConfig config, unsigned int interval_ms);

///
/// Set whether or not the library runs in developer mode (Default = false).
///
/// Developer mode turns the kULDiagnosticsLevel_Auto levels into kULDiagnosticsLevel_Warn, and
/// lets the diagnostics environment variables (`UL_JS_DIAGNOSTICS`, `UL_DOM_DIAGNOSTICS`) override
/// them. You should turn it on in your development builds and leave it off in the builds you ship
/// (a release library ignores those variables when developer mode is off, so a shipped app's
/// behavior can't be changed from its environment). Each variable takes `off`, `warn`, or
/// `strict`.
///
/// @param  config          The config.
///
/// @param  developer_mode  Whether or not to run in developer mode.
///
ULExport void ulConfigSetDeveloperMode(ULConfig config, bool developer_mode);

///
/// Set the diagnostics level for JavaScript APIs that leave their own level at the default
/// (Default = kULDiagnosticsLevel_Auto).
///
/// @param  config  The config.
///
/// @param  level   The diagnostics level.
///
ULExport void ulConfigSetJavaScriptDiagnostics(ULConfig config, ULDiagnosticsLevel level);

///
/// Set the diagnostics level for the DOM API and data bindings
/// (Default = kULDiagnosticsLevel_Auto).
///
/// @param  config  The config.
///
/// @param  level   The diagnostics level.
///
ULExport void ulConfigSetDOMDiagnostics(ULConfig config, ULDiagnosticsLevel level);

///
/// Set the least severe level the library sends to the Logger; less severe messages are dropped
/// (Default = kLogLevel_Info).
///
/// Diagnostics warnings are sent at kLogLevel_Warning, so a minimum of kLogLevel_Error or
/// kLogLevel_Fatal hides them.
///
/// @param  config  The config.
///
/// @param  level   The minimum log level.
///
ULExport void ulConfigSetMinLogLevel(ULConfig config, ULLogLevel level);

#if UL_HAS(MEDIA)
///
/// The memory-vs-performance profile for the HTML5 media subsystem.
///
/// Picks a tradeoff between peak memory per media element and playback smoothness / seek
/// responsiveness. The value is read once when each media element starts loading and stays fixed
/// for that element's lifetime; later changes apply only to subsequently-loaded elements.
///
/// (Default = kMediaProfile_Balanced)
///
/// @pre  Requires the Pro edition or higher.
///
/// @see  ULMediaProfile
///
ULExport void ulConfigSetMediaProfile(ULConfig config, ULMediaProfile media_profile);
#endif // UL_HAS(MEDIA)

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ULTRALIGHT_CAPI_CONFIG_H