///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/Defines.h>
#include <Ultralight/DiagnosticsLevel.h>
#include <Ultralight/String.h>
#include <Ultralight/platform/Logger.h>

// X11's headers define None as a macro; FontHinting::None needs the plain identifier.
#pragma push_macro("None")
#undef None

namespace ultralight {

///
/// The winding order for front-facing triangles. (Only used when the GPU renderer is used)
///
enum class FaceWinding : uint8_t {
  ///
  /// Clockwise Winding (Direct3D, etc.)
  ///
  Clockwise,

  ///
  /// Counter-Clockwise Winding (OpenGL, etc.)
  ///
  CounterClockwise,
};

///
/// The font hinting algorithm to use when rendering glyphs.
///
/// @see  Config::font_hinting
///
enum class FontHinting : uint8_t {
  ///
  /// Lighter hinting algorithm-- glyphs are slightly fuzzier but better resemble their original
  /// shape. This is achieved by snapping glyphs to the pixel grid only vertically which better
  /// preserves inter-glyph spacing.
  ///
  Smooth,

  ///
  /// Let the font profile choose-- Config::font_profile picks the hinting that suits its
  /// appearance, which for every profile but FontProfile::Light means no hinting (Light keeps
  /// light, vertical-only hinting).
  ///
  Normal,

  ///
  /// Strongest hinting algorithm-- outputs only black/white glyphs. The result is usually
  /// unpleasant if the underlying TTF does not contain hints for this type of rendering.
  ///
  Monochrome,

  ///
  /// No hinting is performed-- fonts may be blurry at smaller font sizes.
  ///
  None,
};

///
/// The font-appearance preset that provides default values for the text rendering knobs
/// (Config::font_blend_mode, Config::font_gamma, Config::font_contrast,
/// Config::font_stem_darkening, Config::font_embolden, and the hinting behind
/// FontHinting::Normal). Knobs you set explicitly override the preset's values.
///
/// @see  Config::font_profile
///
enum class FontProfile : uint8_t {
  ///
  /// The standard appearance-- currently the same as WindowsLike.
  ///
  Default,

  ///
  /// Text with the weight and sharpness of Safari and other macOS applications.
  ///
  MacOSLike,

  ///
  /// Text with the crisp, moderate weight of mainstream browsers on Windows.
  ///
  WindowsLike,

  ///
  /// No weight compensation at all-- the thinnest appearance.
  ///
  Light,
};

///
/// How antialiased glyph edges are blended into the page.
///
/// @see  Config::font_blend_mode
///
enum class FontBlendMode : uint8_t {
  ///
  /// Use the value from the font profile.
  ///
  Profile,

  ///
  /// Gamma-corrected blending-- edges read lighter with more contrast (how browsers on Windows
  /// composite text).
  ///
  GammaCorrected,

  ///
  /// Blend in sRGB space-- edges keep their full coverage (how macOS composites text).
  ///
  SRGB,
};

///
/// The quality level for rendering effects (CSS filters, shadows, blurs, etc.).
///
/// The level trades the fidelity of large blurs and shadows for speed: lower levels process them
/// at more aggressively reduced resolutions and with a tighter blur falloff.
///
/// @see  Config::effect_quality
///
enum class EffectQuality : uint8_t {
  ///
  /// Fastest effect quality. Large blurs and shadows render at heavily reduced resolution and
  /// with a truncated falloff, so they appear visibly coarser and tighter than at higher levels.
  ///
  Low,

  ///
  /// Default effect quality-- strikes a good balance between quality and performance.
  ///
  Medium,

  ///
  /// Highest effect quality-- favors quality over performance.
  ///
  /// Only software (CPU) rendering processes very large blurs at higher working resolutions than
  /// Medium; on GPU-accelerated views blur quality is equivalent to Medium.
  ///
  High,
};

///
/// Memory vs. performance tuning for the renderer's HTML / JavaScript
/// engine heaps.
///
/// The profile governs how aggressively the library returns freed heap
/// memory to the operating system, how it balances allocation speed
/// against memory footprint, and how much backing-store memory the
/// compositor keeps per View. The effect of each profile is described on
/// the enumerator below.
///
/// The profile is read when the Renderer is created and again by each
/// View at its creation; changing Config::memory_profile afterward
/// affects only Views created later.
///
/// @see  Config::memory_profile
///
enum class MemoryProfile : uint8_t {
  ///
  /// Sensible default. Freed memory is returned to the OS on a moderate
  /// cadence and allocation favors speed. Recommended unless you have a
  /// specific reason to choose otherwise.
  ///
  Balanced,

  ///
  /// Optimize for low memory footprint at some cost to peak throughput.
  /// Freed memory is returned to the OS promptly, allocations are packed
  /// more densely, and the JavaScript engine collects garbage more
  /// eagerly. Recommended for resource-constrained targets and
  /// long-running pages where footprint matters more than raw speed.
  ///
  LowMemory,

  ///
  /// Optimize for peak throughput at the cost of a higher steady-state
  /// memory footprint. Freed memory is held longer for reuse and the
  /// JavaScript engine collects garbage less often. Recommended when
  /// performance is paramount and the host has plenty of RAM to spare.
  ///
  Performance,
};

#if UL_HAS(MEDIA)

///
/// Memory vs. performance tuning for HTML5 `<video>` and `<audio>`.
///
/// Pick the profile that matches your target's resource envelope; the
/// engine will tune internal media-pipeline sizing accordingly. The
/// effect of each profile is described on the enumerator below.
///
/// The profile is read once when each media element starts loading.
/// Changing Config::media_profile after a `<video>` or `<audio>` element
/// is already loading does not affect that element; subsequent elements
/// will pick up the new profile.
///
/// @see  Config::media_profile
///
/// @note  The library plays WebM files (`video/webm` and `audio/webm`) with VP9 video and Opus
///        audio. Other containers and codecs aren't supported.
///
enum class MediaProfile : uint8_t {
  ///
  /// Sensible default. Smooth playback and responsive seeking with
  /// bounded memory growth as more media elements load. Recommended
  /// starting point unless you have a specific reason to choose
  /// otherwise.
  ///
  Balanced,

  ///
  /// Optimise for low memory footprint at the cost of some smoothness
  /// and seek responsiveness. Recommended for resource-constrained
  /// targets and for long-running pages that may host many media
  /// elements simultaneously.
  ///
  FavorMemory,

  ///
  /// Optimise for the smoothest playback and snappiest seeking, at the
  /// cost of higher peak memory per media element. Recommended when
  /// playback quality is paramount and the host has plenty of RAM to
  /// spare.
  ///
  FavorPerformance,
};

#endif // UL_HAS(MEDIA)

///
/// Diagnostics and logging options.
///
/// @see  Config::diagnostics
///
struct DiagnosticsConfig {
  ///
  /// Whether or not the library runs in developer mode.
  ///
  /// Developer mode turns the DiagnosticsLevel::Auto levels below into DiagnosticsLevel::Warn,
  /// and lets the diagnostics environment variables (`UL_JS_DIAGNOSTICS`, `UL_DOM_DIAGNOSTICS`)
  /// override them. You should turn it on in your development builds and leave it off in the
  /// builds you ship (a release library ignores those variables when developer mode is off, so a
  /// shipped app's behavior can't be changed from its environment). Each variable takes `off`,
  /// `warn`, or `strict`.
  ///
  bool developer_mode = false;

  ///
  /// The diagnostics level for JavaScript APIs that leave their own level at
  /// DiagnosticsLevel::Auto.
  ///
  DiagnosticsLevel javascript = DiagnosticsLevel::Auto;

  ///
  /// The diagnostics level for the DOM API and data bindings.
  ///
  DiagnosticsLevel dom = DiagnosticsLevel::Auto;

  ///
  /// The least severe level the library sends to the Logger (less severe messages are dropped).
  ///
  /// Diagnostics warnings are sent at LogLevel::Warning, so a minimum of LogLevel::Error or
  /// LogLevel::Fatal hides them.
  ///
  LogLevel min_log_level = LogLevel::Info;
};

///
/// Core configuration for the renderer.
///
/// These are various configuration options that can be used to customize the behavior of the
/// library. These options can only be set once before creating the Renderer.
///
/// ## Setting the Config
/// 
/// You should create an instance of the Config struct, set its members, and then call
/// Platform::set_config() before creating the Renderer at the beginning of your
/// application's lifetime.
/// 
/// @par Example usage
/// ```
///   Config config;
///   config.user_stylesheet = "body { background: purple; }";
/// 
///   Platform::instance().set_config(config);
///   // (Setup other Platform interfaces here.)
/// 
///   auto renderer = Renderer::Create();
/// ```
///
struct UExport Config {
  ///
  /// A writable OS file path to store persistent Session data in.
  /// 
  /// This data may include cookies, cached network resources, indexed DB, etc.
  /// 
  /// @note  Files are only written to the path when using a persistent Session.
  /// 
  /// @see  Renderer::CreateSession()
  ///
  String cache_path;

  ///
  /// The relative path to the resources folder (loaded via the FileSystem API).
  /// 
  /// The library loads certain resources (SSL certs, ICU data, etc.) from the FileSystem API
  /// during runtime (eg, `file:///resources/cacert.pem`).
  /// 
  /// You can customize the relative file path to the resources folder by modifying this setting.
  /// 
  /// @see  FileSystem
  ///
  String resource_path_prefix = "resources/";

  ///
  /// The winding order for front-facing triangles.
  ///
  /// @pre  Only used when GPU rendering is enabled for the View.
  ///
  /// @see  FaceWinding
  ///
  FaceWinding face_winding = FaceWinding::CounterClockwise;

  ///
  /// The hinting algorithm to use when rendering fonts.
  ///
  /// Leave this at FontHinting::Normal to use the hinting from font_profile. Any other value
  /// pins the hinting and overrides the profile.
  ///
  /// @see  FontHinting
  ///
  FontHinting font_hinting = FontHinting::Normal;

  ///
  /// The font-appearance preset providing default values for the text rendering knobs below
  /// (font_blend_mode, font_gamma, font_contrast, font_stem_darkening, font_embolden). Knobs you
  /// set explicitly override the preset's values, so you can start from a preset and adjust
  /// individual aspects.
  ///
  /// (Default: FontProfile::Default)
  ///
  FontProfile font_profile = FontProfile::Default;

  ///
  /// How antialiased glyph edges are blended into the page.
  ///
  /// Set to FontBlendMode::Profile to use the value from font_profile.
  ///
  FontBlendMode font_blend_mode = FontBlendMode::Profile;

  ///
  /// The gamma to use when compositing font glyphs. Lower values render text heavier, higher
  /// values render it lighter with more edge contrast (Adobe and Apple prefer 1.8).
  ///
  /// Set to 0 to use the value from font_profile.
  ///
  /// (Default: 0, meaning the profile's value; the Default profile uses 2.2)
  ///
  double font_gamma = 0.0;

  ///
  /// Additional contrast enhancement applied when compositing font glyphs. Values above 0
  /// thicken the midtones of antialiased glyph edges, increasing apparent text weight at every
  /// size. The useful range is 0.0 (none) to about 1.0.
  ///
  /// Set below 0 to use the value from font_profile.
  ///
  /// (Default: -1.0, meaning the profile's value; the Default profile uses 0.3)
  ///
  double font_contrast = -1.0;

  ///
  /// Stem-darkening strength for rendered text. Boosts the coverage of antialiased glyph
  /// pixels so thin stems keep their apparent weight on light backgrounds; each preset shapes
  /// how the boost varies with glyph size. Text on dark backgrounds is unaffected.
  ///
  /// Set below 0 to use the value from font_profile.
  ///
  /// (Default: -1.0, meaning the profile's value)
  ///
  double font_stem_darkening = -1.0;

  ///
  /// Extra stroke weight applied to glyph shapes, as a fraction of the font size (eg, 0.01 widens
  /// stems by roughly 1% of the em). Applies at every size and on both light and dark backgrounds;
  /// glyph advances and layout are unaffected.
  ///
  /// Set below 0 to use the value from font_profile.
  ///
  /// (Default: -1.0, meaning the profile's value)
  ///
  double font_embolden = -1.0;

  ///
  /// Global user-defined CSS string (included before any CSS on the page).
  /// 
  /// You can use this to override default styles for various elements on the page.
  /// 
  /// @note  This is an actual string of CSS, not a file path.
  ///
  String user_stylesheet;

  ///
  /// Whether or not to continuously repaint any Views, regardless if they are dirty.
  ///
  /// This is mainly used to diagnose painting/shader issues and profile performance.
  ///
  bool force_repaint = false;

  ///
  /// Whether or not GPU-accelerated Views render vector content (path fills and strokes)
  /// analytically (Photon) instead of tessellating it to triangles. Analytic rendering is
  /// resolution-independent: edges stay crisp under any zoom or transform.
  ///
  /// Path rendering on CPU-rendered Views is unaffected. Disabling this also disables
  /// `enable_photon_text`.
  ///
  /// (Default: true)
  ///
  bool enable_photon = true;

  ///
  /// Whether or not text and color emoji also render analytically when `enable_photon` is
  /// enabled (see photon_text_min_px for the size cutoff). Ignored when `enable_photon` is
  /// disabled.
  ///
  /// This selects the text appearance for both renderers: CPU-rendered Views match the
  /// analytic look (unhinted outlines, fractional glyph placement) so output stays
  /// near-identical across renderers.
  ///
  /// (Default: true)
  ///
  bool enable_photon_text = true;

  ///
  /// The minimum on-screen glyph size, in pixels, at which text renders analytically when
  /// `enable_photon_text` is enabled; smaller glyphs use the rasterized glyph atlas. Set to 0
  /// to render all text analytically.
  ///
  /// (Default: 16)
  ///
  uint32_t photon_text_min_px = 16;

  ///
  /// Additional delay (in seconds) on top of the engine's internal page-settled detection.
  /// (Default: 0.1)
  ///
  /// Increase this if you want a little more assurance that the page has fully settled
  /// (animations finished, late-arriving network activity complete) before View::is_settled()
  /// flips and LoadListener::OnPageSettled() fires, at the cost of additional wait time.
  ///
  /// @see  LoadListener::OnPageSettled()
  ///
  double page_settle_delay = 0.1;

  ///
  /// The number of consecutive idle samples the library requires before it declares a page
  /// settled. (Default: 5)
  ///
  /// Together with page_settle_tick this sets the shortest time a page can take to settle
  /// (page_settle_cycles * page_settle_tick). Lower it to settle sooner, at the cost of
  /// tolerance for late layout or paint.
  ///
  unsigned page_settle_cycles = 5;

  ///
  /// How often (in seconds) the library samples a loading page for idleness. (Default: 0.006)
  ///
  /// @see  page_settle_cycles
  ///
  double page_settle_tick = 0.006;

  ///
  /// The interval (in seconds) at which the library automatically recycles internal caches and
  /// reclaims memory.
  ///
  /// The library performs this lightweight recycle for you from inside `Renderer::Update()` /
  /// `Renderer::Render()`. It is cheap enough to be invisible at typical frame rates, and letting
  /// the library drive it on this timer is the recommended approach.
  ///
  /// Set to `0` to disable the automatic recycler entirely, for applications that prefer to call
  /// `Renderer::Recycle()` on their own schedule.
  ///
  /// (Default: 0.5)
  ///
  double recycle_delay = 0.5;

  ///
  /// Whether the library automatically collects garbage and returns freed
  /// memory to the OS without embedder involvement.
  ///
  /// When a page builds up a large JavaScript heap and then goes quiet, that
  /// memory would otherwise stay resident until the next burst of activity or
  /// an explicit Renderer::Recycle / Renderer::PurgeMemory. With this enabled,
  /// the library collects on its own and pushes the freed memory back to the
  /// OS, so the process footprint drifts back down after heavy content:
  ///
  /// - While the application is idle (no user input, little render activity),
  ///   collection runs on a paced schedule without blocking the render thread.
  /// - While the application is busy (for example a game rendering every
  ///   frame), collection still runs concurrently whenever the heap has grown
  ///   well past its last collected size, so a HUD that allocated heavily and
  ///   then stopped does not hold its peak footprint. How eagerly this
  ///   triggers follows `memory_profile`.
  ///
  /// Reclamation only happens while Renderer::Update() is being pumped. An
  /// application that stops calling Renderer::Update() while idle gets no
  /// automatic reclamation; call Renderer::Recycle() / Renderer::PurgeMemory()
  /// explicitly in that case.
  ///
  /// Set this to false if your application manages collection itself; behavior
  /// then matches earlier releases.
  ///
  /// (Default: true)
  ///
  bool idle_gc_enabled = true;

  ///
  /// How long (in seconds) a page must be idle, with no user input, before
  /// automatic idle collection begins.
  ///
  /// Lower values reclaim sooner after the user stops interacting; higher
  /// values wait longer before assuming the page is idle. Only used when
  /// `idle_gc_enabled` is true.
  ///
  /// (Default: 2.0)
  ///
  double idle_gc_idle_time = 2.0;

  ///
  /// The maximum size of the library's memory cache in bytes. (Default: 256 MiB)
  ///
  /// @note  This is used for caching decoded images, compiled JavaScript, etc. Increasing this
  ///        may improve performance for resource-heavy applications (at the cost of higher
  ///        memory usage).
  ///
  uint32_t memory_cache_size = 256 * 1024 * 1024;

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
  uint32_t page_cache_size = 0;

  ///
  /// Memory vs. performance tuning for the renderer's HTML / JavaScript
  /// engine heaps. See the MemoryProfile enumerator descriptions for the
  /// effect of each profile.
  ///
  /// Read when the Renderer is created and by each View at its creation. (Default: Balanced)
  ///
  MemoryProfile memory_profile = MemoryProfile::Balanced;

  ///
  /// The system's physical RAM size in bytes.
  ///
  /// JavaScriptCore tries to detect the system's physical RAM size to set reasonable allocation
  /// limits. Set this to anything other than 0 to override the detected value. Size is in bytes.
  ///
  /// This can be used to force JavaScriptCore to be more conservative with its allocation strategy
  /// (at the cost of some performance).
  ///
  uint32_t override_ram_size = 0;

  ///
  /// The minimum size of large VM heaps in JavaScriptCore.
  ///
  /// Set this to a lower value to make these heaps start with a smaller initial value.
  ///
  /// Set to `0` (the default) to derive this value from `memory_profile`.
  ///
  uint32_t min_large_heap_size = 0;

  ///
  /// The minimum size of small VM heaps in JavaScriptCore.
  /// 
  /// Set this to a lower value to make these heaps start with a smaller initial value.
  ///
  uint32_t min_small_heap_size = 1 * 1024 * 1024;

  ///
  /// A soft target for the total JavaScriptCore heap size, in bytes.
  ///
  /// This is not a hard limit: the heap may grow beyond this target. When it
  /// does, the garbage collector treats the heap as under memory pressure and
  /// reclaims more aggressively to bring it back toward the target.
  ///
  /// Set to `0` (the default) to derive this value from `memory_profile`.
  ///
  uint32_t max_heap_size = 0;

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
  uint32_t num_renderer_threads = 0;

  /// 
  /// The max amount of time (in seconds) to allow repeating timers to run during each call to
  /// Renderer::Update.
  /// 
  /// The library will attempt to throttle timers if this time budget is exceeded.
  ///
  double max_update_time = 1.0 / 200.0;

  ///
  /// The alignment (in bytes) of the BitmapSurface when using the CPU renderer.
  ///
  /// The underlying bitmap associated with each BitmapSurface will have row_bytes padded to reach
  /// this alignment.
  ///
  /// Aligning the bitmap helps improve performance when using the CPU renderer. Determining the
  /// proper value to use depends on the CPU architecture and max SIMD instruction set used.
  ///
  /// We generally target the 128-bit SSE2 instruction set across most PC platforms so '16' is
  /// a safe value to use.
  ///
  /// You can set this to '0' to perform no padding (row_bytes will always be width * 4) at a
  /// slight cost to performance.
  ///
  uint32_t bitmap_alignment = 16;

  ///
  /// The quality of effects (blurs, CSS filters, SVG filters, etc.) to use when rendering.
  ///
  /// Applies to every rendering path (CSS element filters, canvas `ctx.filter`, and SVG filters)
  /// on both the CPU and GPU renderers. See EffectQuality for what each level trades.
  ///
  /// (Default: EffectQuality::Medium)
  ///
  EffectQuality effect_quality = EffectQuality::Medium;

  ///
  /// Whether to ignore SSL certificate errors when using the curl network backend.
  ///
  /// When set to true, SSL certificate verification will be disabled, allowing connections
  /// to servers with self-signed or invalid certificates. This should only be used for
  /// development/testing purposes.
  ///
  /// @warning  This option reduces security and should never be enabled in production.
  ///
  bool ignore_ssl_errors = false;

  ///
  /// Whether to validate TLS server certificates against the operating system's certificate
  /// store instead of the certificate bundle shipped with the application
  /// (resources/cacert.pem).
  ///
  /// When false (the default), HTTPS connections are verified using the bundled certificate
  /// authority list, giving identical behavior on every platform. When true, certificates
  /// trusted by the host operating system (including enterprise or user-installed roots) are
  /// honored and the bundled list is not used.
  ///
  /// Currently honored on Windows. On platforms where the OS trust store is not yet supported,
  /// this option is ignored and the bundled list is used.
  ///
  bool use_system_certificate_store = false;

  ///
  /// Whether to paint full layers when dirty instead of just the dirty subregion.
  ///
  /// When set to true and a layer has dirty regions, the full layer will be repainted
  /// instead of just the dirty rectangle intersection. This can be useful for debugging
  /// rendering issues or when dirty region tracking is causing artifacts.
  ///
  /// @note  Enabling this will impact performance as entire layers are redrawn
  ///       instead of just the changed portions.
  ///
  bool paint_full_layers = false;

  ///
  /// The interval (in milliseconds) at which the library samples and emits resource counters
  /// (memory usage, VRAM, cache sizes, etc.) via the Profiler's EmitCounter() callback.
  ///
  /// Set to 0 to disable resource counter sampling entirely -- scopes and events are unaffected.
  ///
  /// Default: 500ms.
  ///
  uint32_t resource_sample_interval = 500;

  ///
  /// Diagnostics and logging options (developer mode, diagnostics levels, and the minimum log
  /// level).
  ///
  /// @see  DiagnosticsConfig
  ///
  DiagnosticsConfig diagnostics;

#if UL_HAS(MEDIA)
  ///
  /// The memory-vs-performance profile for the HTML5 media subsystem.
  ///
  /// Picks a tradeoff between peak memory per media element and playback
  /// smoothness / seek responsiveness. The value is read once when each
  /// media element starts loading and stays fixed for that element's
  /// lifetime; later changes to `Config::media_profile` apply only to
  /// subsequently-loaded elements.
  ///
  /// @see  MediaProfile
  ///
  MediaProfile media_profile = MediaProfile::Balanced;
#endif // UL_HAS(MEDIA)
};

} // namespace ultralight

#pragma pop_macro("None")
