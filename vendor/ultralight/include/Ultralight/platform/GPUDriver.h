///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///

// clang-format off
#pragma once
#if defined(_MSC_VER)
#pragma warning(disable : 4251)
#endif
#include <Ultralight/Defines.h>
#include <Ultralight/Geometry.h>
#include <Ultralight/Matrix.h>
#include <Ultralight/Bitmap.h>

namespace ultralight {

/// \cond ignore
///   This pragma pack(push, 1) command is important! 
///   GPU structs should not be padded with any bytes.
/// \endcond
#pragma pack(push, 1)

///
/// Render buffer description.
///
/// This structure describes a render buffer that can be used as a target for drawing commands.
///
/// @see GPUDriver::CreateRenderBuffer.
///
struct UExport RenderBuffer {
  uint32_t texture_id;     ///< The backing texture for this RenderBuffer
  uint32_t width;          ///< The width of the RenderBuffer texture
  uint32_t height;         ///< The height of the RenderBuffer texture
  bool has_stencil_buffer; ///< Currently unused, always false.
  bool has_depth_buffer;   ///< Currently unused, always false.
};

///
/// Vertex layout for path vertices.
///
/// This struct is the in-memory layout for each path vertex (useful for synthesizing or modifying
/// your own vertex data).
///
struct Vertex_2f_4ub_2f {
  float pos[2];           ///< Vertex position (x, y) in pixels.
  unsigned char color[4]; ///< Vertex color (RGBA, 8 bits per channel).
  float obj[2];           ///< Object-space coordinates (used for anti-aliasing).
};

///
/// Vertex layout for quad vertices.
///
/// This struct is the in-memory layout for each quad vertex (useful for synthesizing or modifying
/// your own vertex data).
///
struct Vertex_2f_4ub_2f_2f_28f {
  float pos[2];           ///< Vertex position (x, y) in pixels.
  unsigned char color[4]; ///< Vertex color (RGBA, 8 bits per channel).
  float tex[2];           ///< Texture coordinates (u, v).
  float obj[2];           ///< Object-space coordinates (used for anti-aliasing).
  float data0[4];         ///< Per-vertex shader data (shader-specific).
  float data1[4];         ///< Per-vertex shader data (shader-specific).
  float data2[4];         ///< Per-vertex shader data (shader-specific).
  float data3[4];         ///< Per-vertex shader data (shader-specific).
  float data4[4];         ///< Per-vertex shader data (shader-specific).
  float data5[4];         ///< Per-vertex shader data (shader-specific).
  float data6[4];         ///< Per-vertex shader data (shader-specific).
};

///
/// Vertex layout for instance vertices.
///
/// This struct is the in-memory layout for each instance vertex (used for instanced cell
/// rendering, see ShaderType::FillPhotonGrid).
///
struct Vertex_2f_2ui {
  float pos[2];         ///< Vertex position (x, y) in pixels.
  uint32_t header_addr; ///< Cell header texel address in the index data page.
  uint32_t path_slot;   ///< Per-path constant-table slot (low bits) plus flags.
};

///
/// Vertex buffer formats.
///
/// This enumeration describes the format of a vertex buffer.
///
/// @note  Identifiers start with an underscore due to C++ naming rules.
///
/// @see VertexBuffer
///
enum class VertexBufferFormat : uint8_t {
  _2f_4ub_2f,        ///< Vertex_2f_4ub_2f (used for path rendering)
  _2f_4ub_2f_2f_28f, ///< Vertex_2f_4ub_2f_2f_28f (used for quad rendering)
  _2f_2ui,           ///< Vertex_2f_2ui (used for instanced cell rendering)
};

///
/// Vertex buffer description.
///
/// @see GPUDriver::CreateGeometry
///
struct UExport VertexBuffer {
  VertexBufferFormat format; ///< The format of the vertex buffer.
  uint32_t size;             ///< The size of the vertex buffer in bytes.
  uint8_t* data;             ///< The raw vertex buffer data.
};

///
/// Vertex index type.
///
typedef uint32_t IndexType;

///
/// Index buffer description.
///
/// This structure describes an index buffer that can be used to index into a vertex buffer.
///
/// @note The index buffer is a simple array of IndexType values.
///
/// @see GPUDriver::CreateGeometry
///
struct UExport IndexBuffer {
  uint32_t size; ///< The size of the index buffer in bytes.
  uint8_t* data; ///< The raw index buffer data.
};

///
/// Shader program types.
///
/// Each of these correspond to a vertex/pixel shader pair. Stock shaders for every program ship in
/// the SDK's `platform/shaders` folder, in a compiled or generated form for each backend (DXBC,
/// SPIR-V, MSL, and GLSL).
///
/// A conforming GPUDriver implements every program in this enumeration; the library does not
/// query for per-program support.
///
/// @see GPUState::shader_type
///
enum class ShaderType : uint8_t {
  Fill,     ///< Shader program for filling quad geometry.
  FillPath, ///< Shader program for filling tessellated path geometry.
  FilterBasic, ///< Shader program for basic CSS/SVG filters.
  FilterBlur,  ///< Shader program for blur CSS/SVG filters.
  FilterDropShadow, ///< Shader program for drop-shadow CSS/SVG filters.

  ///
  /// Shader program for analytic vector rendering (Photon). Uses the quad
  /// vertex layout.
  ///
  /// Photon draws carry data pages in the GPUState texture slots and read
  /// them with texel loads only. Bind each slot **unfiltered, no mipmaps**,
  /// at the register the shipped shader source declares:
  ///
  /// | GPUState slot  | Carries                          | Format   | Shader binding        |
  /// |----------------|----------------------------------|----------|-----------------------|
  /// | `texture_1_id` | Curve page                       | RGBA16F  | `Texture0` (t0)       |
  /// | `texture_2_id` | (never set on Photon draws)      |          |                       |
  /// | `texture_3_id` | Constants / glyph LUT / ramp     | RGBA32F  | `Texture2` (t2)       |
  /// | `texture_4_id` | Index/header page                | RGBA16UI | `IndexTexture` (t3)   |
  ///
  /// The index page has its own slot and register because it needs an
  /// integer (`uint4`) texture view; it must never share a binding point
  /// with the float views other programs use.
  ///
  /// Every driver must support this program alongside the others; there is
  /// no capability query for it.
  ///
  FillPhoton,

  ///
  /// Shader program for grid-based analytic vector rendering (Photon). Uses
  /// the Vertex_2f_2ui instance layout.
  ///
  /// Binds the same data pages, slots, and registers as FillPhoton, with one
  /// addition: the **vertex** shader also reads the texture slots, so bind
  /// them to both shader stages for this program.
  ///
  /// Every driver must support this program alongside the others.
  ///
  FillPhotonGrid,
};

///
/// Blend factors that define how source and destination colors are weighted during blending.
///
/// These factors control how much the source color (from the pixel shader) and destination color
/// (from the render buffer) contribute to the final blended result.
///
/// For each color channel, the blend equation combines colors as:
///   Result = (SourceColor * SourceFactor) [op] (DestColor * DestFactor)
/// where [op] is defined by BlendEquation.
///
enum class BlendFactor : uint8_t {
  Zero,               ///< Factor of (0, 0, 0, 0) - completely removes this color's contribution
  One,                ///< Factor of (1, 1, 1, 1) - uses this color at full intensity

  /// Source-based factors (use the incoming pixel shader output)
  SrcColor,           ///< Factor of (Rs, Gs, Bs, As) - multiply by source color components
  InvSrcColor,        ///< Factor of (1-Rs, 1-Gs, 1-Bs, 1-As) - multiply by inverse source color
  SrcAlpha,           ///< Factor of (As, As, As, As) - multiply all channels by source alpha
  InvSrcAlpha,        ///< Factor of (1-As, 1-As, 1-As, 1-As) - multiply by inverse source alpha

  /// Destination-based factors (use the existing render buffer color)
  DestColor,          ///< Factor of (Rd, Gd, Bd, Ad) - multiply by destination color components
  InvDestColor,       ///< Factor of (1-Rd, 1-Gd, 1-Bd, 1-Ad) - multiply by inverse dest color
  DestAlpha,          ///< Factor of (Ad, Ad, Ad, Ad) - multiply all channels by destination alpha
  InvDestAlpha,       ///< Factor of (1-Ad, 1-Ad, 1-Ad, 1-Ad) - multiply by inverse dest alpha

  /// Special factors
  SrcAlphaSaturate    ///< Factor of (f, f, f, 1) where f = min(As, 1-Ad). Useful for additive
                      ///< blending that doesn't oversaturate the destination alpha channel.
};

///
/// Mathematical operations for combining source and destination colors during blending.
///
/// This defines how the GPU combines the weighted source and destination colors after they've
/// been multiplied by their respective blend factors.
///
enum class BlendEquation : uint8_t {
  Add,                ///< Result = (Src * SrcFactor) + (Dest * DestFactor)
                      ///< Standard additive blending - most common operation

  Subtract,           ///< Result = (Src * SrcFactor) - (Dest * DestFactor)
                      ///< Subtractive blending - source reduces destination

  RevSubtract,        ///< Result = (Dest * DestFactor) - (Src * SrcFactor)
                      ///< Reverse subtractive blending - destination reduces source

  Min,                ///< Result = min(Src, Dest) - takes darker/smaller value
                      ///< Note: Ignores blend factors, compares colors directly

  Max                 ///< Result = max(Src, Dest) - takes lighter/larger value
                      ///< Note: Ignores blend factors, compares colors directly
};

///
/// The state of the GPU for a given draw command.
///
/// This structure describes the current state of the GPU for a given draw command.
///
/// @see Command::gpu_state
///
struct UExport GPUState {
  /// Viewport width in pixels
  uint32_t viewport_width;

  /// Viewport height in pixels
  uint32_t viewport_height;

  /// Transform matrix-- you should multiply this with the screen-space orthographic projection
  /// matrix then pass to the vertex shader.
  Matrix4x4 transform;

  /// Whether or not we should enable texturing for the current draw command.
  bool enable_texturing;

  /// Whether or not we should enable blending for the current draw command. If blending is
  /// disabled, any drawn pixels should overwrite existing. This is mainly used so we can modify
  /// alpha values of the RenderBuffer during scissored clears.
  bool enable_blend;

  /// The blend factor for the source color (pixel shader output).
  /// This factor is multiplied with the source color before the blend equation is applied.
  /// The library sets it on every draw command (One for normal premultiplied-alpha drawing).
  /// Ignore it when enable_blend is false.
  BlendFactor blend_src_factor;

  /// The blend factor for the destination color (render buffer).
  /// This factor is multiplied with the destination color before the blend equation is applied.
  /// The library sets it on every draw command (InvSrcAlpha for normal premultiplied-alpha
  /// drawing). Ignore it when enable_blend is false.
  BlendFactor blend_dst_factor;

  /// The mathematical equation used to combine the source and destination colors.
  /// The library sets it on every draw command (Add for normal drawing). Ignore it when
  /// enable_blend is false.
  BlendEquation blend_equation;

  /// The vertex/pixel shader program pair to use for the current draw command.
  ShaderType shader_type;

  /// The render buffer to use for the current draw command.
  uint32_t render_buffer_id;

  /// The texture id to bind to slot #1. (Will be 0 if none)
  uint32_t texture_1_id;

  /// The texture id to bind to slot #2. (Will be 0 if none)
  uint32_t texture_2_id;

  /// The texture id to bind to slot #3. (Will be 0 if none)
  uint32_t texture_3_id;

  /// The texture id to bind to slot #4. (Will be 0 if none)
  ///
  /// Set only on Photon draws: carries the RGBA16UI index/header page, which
  /// needs an integer texture view. The slot table on ShaderType::FillPhoton
  /// has the full binding contract.
  uint32_t texture_4_id;

  /// The uniform integers (passed to the pixel shader via uniforms).
  int32_t uniform_integer[8];

  /// The uniform scalars (passed to the pixel shader via uniforms).
  float uniform_scalar[8];

  /// The uniform vectors (passed to the pixel shader via uniforms).
  vec4 uniform_vector[8];

  /// The clip size (passed to the pixel shader via uniforms).
  uint8_t clip_size;

  /// The clip stack (passed to the pixel shader via uniforms).
  Matrix4x4 clip[8];

  /// Whether or not scissor testing should be used for the current draw command.
  bool enable_scissor;

  /// The scissor rect to use for scissor testing (units in pixels)
  IntRect scissor_rect;
};

///
/// The types of commands.
///
/// This enumeration describes the type of command to execute on the GPU.
///
/// @see Command
///
enum class CommandType : uint8_t {
  ClearRenderBuffer, ///< Clear the specified render buffer.
  DrawGeometry,      ///< Draw the specified geometry to the specified render buffer.

  ///
  /// Submit pending GPU work before executing the next command.
  ///
  /// Emitted before the library draws into a render buffer whose texture was
  /// sampled by earlier draws in the same command list. What to do:
  ///
  /// - **Direct3D 12 / Metal / Vulkan:** usually a no-op; your per-resource
  ///   barriers or transitions already order these accesses. Verify that
  ///   ordering covers clears as well as draws.
  /// - **Direct3D 11 / OpenGL:** submit pending work (`Flush()` /
  ///   `glFlush()`). Some drivers otherwise reorder the pending sampling
  ///   draws against the upcoming clear and draws, producing intermittent
  ///   dropouts in the sampled output.
  ///
  /// @note  No fields of GPUState are meaningful on a Flush command.
  ///
  Flush,
};

///
/// A command to execute on the GPU.
///
/// This structure describes a command to be executed on the GPU.
///
/// Commands are dispatched to the GPU driver asynchronously via GPUDriver::UpdateCommandList(),
/// the GPU driver should consume these commands and execute them at an appropriate time.
///
/// @see CommandList
///
struct UExport Command {
  CommandType command_type; ///< The type of command to dispatch.
  GPUState gpu_state;       ///< The current GPU state.
  uint32_t geometry_id;     ///< The geometry ID to bind. (used with CommandType::DrawGeometry)
  uint32_t indices_count;   ///< The number of indices.   (used with CommandType::DrawGeometry)  
  uint32_t indices_offset;  ///< The index to start from. (used with CommandType::DrawGeometry)
};

///
/// List of commands to execute on the GPU.
///
/// @see GPUDriver::UpdateCommandList
///
struct UExport CommandList {
  uint32_t size;     ///< The number of commands in the list.
  Command* commands; ///< The raw command list data.
};

#pragma pack(pop)

///
/// Optional GPU compressed-texture format families.
///
/// Each value is a single bit within GPUDeviceCaps::compressed_formats. A driver
/// sets a bit to advertise that it can accept a Bitmap in any of that family's
/// formats in CreateTexture() and sample it directly (the GPU decompresses the
/// blocks in hardware on read).
///
/// @note  These are bit flags and are append-only: never renumber or reuse an
///        existing value. Treat any bit you do not recognize as unsupported.
///
enum GPUCompressedFormat : uint32_t {
  /// BC1, BC2, and BC3 (also known as DXT1 / DXT3 / DXT5).
  kGPUCompressedFormat_BC1_BC3 = 1 << 0,

  /// BC7.
  kGPUCompressedFormat_BC7 = 1 << 1,
};

///
/// Optional capabilities reported by a GPUDriver.
///
/// The engine zero-initializes this structure and passes it to
/// GPUDriver::GetDeviceCaps(), which fills in the capabilities the driver
/// supports. Any field a driver leaves untouched reads back as zero (meaning
/// "not supported"), so an unset capability always degrades safely.
///
/// @note  This structure may grow over time; new fields are appended. Because
///        the engine owns and zero-initializes it, a driver compiled against an
///        older definition simply leaves any newer fields at their default.
///
struct GPUDeviceCaps {
  ///
  /// Bitmask of supported compressed-texture families (see GPUCompressedFormat).
  /// Zero means no block-compressed formats are supported; such textures are
  /// decompressed before upload.
  ///
  uint32_t compressed_formats;

  ///
  /// Nonzero if this driver can allocate multisampled (antialiased) render
  /// targets (see kGPUTextureFlag_Antialiased). Zero (the default) means it only
  /// creates single-sample targets.
  ///
  uint32_t supports_msaa;

  ///
  /// The largest width or height, in pixels, of a texture or render target this
  /// driver can create. The engine will not ask you to create one whose width or
  /// height exceeds this value.
  ///
  /// @note  This is a limit, not a capability flag: zero (the default) means the
  ///        driver does not report a limit, and the engine substitutes a
  ///        conservative built-in default rather than treating it as "no
  ///        textures allowed".
  ///
  uint32_t max_texture_dimension;

  ///
  /// The sample count this driver uses for multisampled render targets (see
  /// supports_msaa), reported so the engine can account their memory.
  ///
  /// @note  This is a limit, not a capability flag: zero (the default) means the
  ///        driver does not report a count, and the engine substitutes a
  ///        conservative built-in default when supports_msaa is set. Ignored
  ///        when supports_msaa is zero.
  ///
  uint32_t max_msaa_samples;

  ///
  /// The largest size, in bytes, of a single vertex or index buffer the engine
  /// will build for this driver. A path whose tessellated geometry would exceed
  /// this is dropped rather than uploaded, so the driver never receives an
  /// oversized CreateGeometry / UpdateGeometry.
  ///
  /// @note  This is a limit, not a capability flag: zero (the default) means the
  ///        driver does not report a limit, and the engine substitutes a
  ///        conservative built-in default rather than treating it as "no
  ///        geometry allowed".
  ///
  uint32_t max_geometry_size;

  ///
  /// Nonzero if redrawing part of a large render target (a View's render buffer) costs about
  /// what the drawn area costs on this device, so the library redraws only the region of the
  /// View that changed each frame. Zero (the default) means the library clears and redraws the
  /// whole View target every composite, the right answer on tiled or bandwidth-bound hardware
  /// where loading the previous contents costs as much as a full redraw.
  ///
  /// @note  This is a capability flag (like supports_msaa), not a limit: zero is the safe
  ///        default. It only governs the View's render target. Small partial redraws of the
  ///        library's tile render buffers happen on every driver regardless of this flag (the
  ///        library requires load-and-store behavior on those targets), and sampling an
  ///        existing render texture is always required.
  ///
  uint32_t supports_partial_redraw;
};

///
/// Flags passed to GPUDriver::CreateTexture() that tell your driver how a texture will be used.
/// They are combined with bitwise OR; test the bits your driver supports.
///
enum GPUTextureFlags : uint32_t {
  ///
  /// The texture is a render target (the backing texture for a RenderBuffer). Create it so you can
  /// both render into and sample from it. The supplied bitmap is empty (dimensions and format
  /// only), so there's nothing to upload at texture creation.
  ///
  /// Most render targets are BGRA (BitmapFormat::BGRA8_UNORM_SRGB). A render target may also be
  /// single-channel (BitmapFormat::A8_UNORM): back it with a renderable one-channel format (R8 on
  /// modern graphics APIs) so that sampling it returns the stored value in the RED channel. Do not
  /// apply the alpha swizzle that sampled (non-render-target) A8 textures traditionally carry; the
  /// library's shaders read a single-channel render target's red channel directly. Single-channel
  /// render targets are only ever drawn with blending disabled and are never multisampled.
  ///
  kGPUTextureFlag_RenderTarget = 1 << 0,

  ///
  /// The render target should be multisampled (antialiased), resolved to a single-sample texture
  /// when sampled. You will only receive this flag if your driver reports
  /// GPUDeviceCaps::supports_msaa from GetDeviceCaps().
  ///
  kGPUTextureFlag_Antialiased = 1 << 1,

  ///
  /// The texture is uploaded once and never changes. UpdateTexture() is never called on it, so
  /// you may use immutable/static storage and need not keep it CPU-writable.
  ///
  kGPUTextureFlag_Immutable = 1 << 2,
};

///
/// User-defined GPU driver interface.
/// 
/// The library uses this to optionally render Views on the GPU (see ViewConfig::is_accelerated).
///
/// You can provide the library with your own GPU driver implementation so that all rendering is
/// performed using an existing GPU context (useful for game engines).
///
/// When a View is rendered on the GPU, you can retrieve the backing texture ID via
/// View::render_target().
/// 
/// ## Default Implementation
///
/// A platform-specific implementation of GPUDriver is provided for you when you call App::Create(),
/// (currently D3D11, Metal, and OpenGL). We recommend using these classes as a starting point for
/// your own implementation (they ship as Zlib-licensed source in the SDK's `platform`
/// folder).
///
/// ## Setting the GPU Driver
///
/// When using Renderer::Create(), you can provide your own implementation of this
/// class via Platform::set_gpu_driver().
///
/// ## State Synchronization
///
/// During each call to Renderer::Render(), the library will update the state of the GPU driver
/// (textures, render buffers, geometry, command lists, etc.) to match the current state of the
/// library.
///
/// ### Detecting State Changes
///
/// The library will call BeginSynchronize() before any state is updated and EndSynchronize() after
/// all state is updated. All `Create` / `Update` / `Destroy` calls will be made between these two
/// calls.
///
/// This allows the GPU driver implementation to prepare the GPU for any state changes.
/// 
/// ## Drawing
///
/// All drawing is done via command lists (UpdateCommandList()) to allow asynchronous execution
/// of commands on the GPU.
///
/// The library will dispatch a list of commands to the GPU driver during state synchronization. The
/// GPU driver implementation should periodically consume the command list and execute the commands
/// at an appropriate time.
///
/// @see Platform::set_gpu_driver()
///
class UExport GPUDriver {
 public:
  virtual ~GPUDriver();

  ///
  /// Called before any state (eg, CreateTexture(), UpdateTexture(), DestroyTexture(), etc.) is
  /// updated during a call to Renderer::Render().
  ///
  /// This is a good time to prepare the GPU for any state updates.
  ///
  virtual void BeginSynchronize() = 0;

  ///
  /// Called after all state has been updated during a call to Renderer::Render().
  ///
  virtual void EndSynchronize() = 0;

  ///
  /// Get the next available texture ID.
  ///
  /// This is used to generate a unique texture ID for each texture created by the library. The
  /// GPU driver implementation is responsible for mapping these IDs to a native ID.
  ///
  /// @note Numbering should start at 1, 0 is reserved for "no texture".
  ///
  /// @return Returns the next available texture ID.
  ///
  virtual uint32_t NextTextureId() = 0;

  ///
  /// Create a texture with a certain ID and bitmap.
  ///
  /// @param texture_id  The texture ID to use for the new texture.
  ///
  /// @param bitmap      The bitmap to initialize the texture with. For a render
  ///                    target (`flags` includes kGPUTextureFlag_RenderTarget) this
  ///                    holds the dimensions and format but no pixel rows; otherwise
  ///                    it holds the pixel data to upload.
  ///
  /// @param flags       A bitwise-OR of GPUTextureFlags describing how the
  ///                    texture will be used (render target, antialiased,
  ///                    immutable).
  ///
  /// @warning A deep copy of the bitmap data should be made if you are uploading it to the GPU
  ///          asynchronously, it will not persist beyond this call.
  ///
  virtual void CreateTexture(uint32_t texture_id, RefPtr<Bitmap> bitmap, uint32_t flags) = 0;

  ///
  /// Update an existing non-RTT texture with new bitmap data.
  ///
  /// @param texture_id  The texture to update.
  ///
  /// @param bitmap      The new bitmap data. Always the complete texture surface, valid inside
  ///                    and outside `dirty_rect`.
  ///
  /// @param dirty_rect  The region of the bitmap, in pixels, that changed since the last upload.
  ///                    This is an optimization hint: a driver may upload only this region, or
  ///                    ignore it and upload the whole bitmap. Never larger than the bitmap
  ///                    bounds and never empty.
  ///
  /// @warning A deep copy of the bitmap data should be made if you are uploading it to the GPU
  ///          asynchronously, it will not persist beyond this call.
  ///
  virtual void UpdateTexture(uint32_t texture_id, RefPtr<Bitmap> bitmap,
                             const IntRect& dirty_rect)
      = 0;

  ///
  /// Destroy a texture.
  ///
  /// @param texture_id  The texture to destroy.
  ///
  virtual void DestroyTexture(uint32_t texture_id) = 0;

  ///
  /// Get the next available render buffer ID.
  ///
  /// This is used to generate a unique render buffer ID for each render buffer created by the
  /// library. The GPU driver implementation is responsible for mapping these IDs to a native ID.
  ///
  /// @note Numbering should start at 1, 0 is reserved for "no render buffer".
  ///
  /// @return Returns the next available render buffer ID.
  ///
  virtual uint32_t NextRenderBufferId() = 0;

  ///
  /// Create a render buffer with certain ID and buffer description.
  ///
  /// @param render_buffer_id  The render buffer ID to use for the new render buffer.
  ///
  /// @param buffer           The render buffer description.
  ///
  virtual void CreateRenderBuffer(uint32_t render_buffer_id, const RenderBuffer& buffer) = 0;

  ///
  /// Destroy a render buffer.
  ///
  /// @param render_buffer_id  The render buffer to destroy.
  ///
  virtual void DestroyRenderBuffer(uint32_t render_buffer_id) = 0;

  ///
  /// Get the next available geometry ID.
  ///
  /// This is used to generate a unique geometry ID for each geometry created by the library. The
  /// GPU driver implementation is responsible for mapping these IDs to a native ID.
  ///
  /// @note Numbering should start at 1, 0 is reserved for "no geometry".
  ///
  /// @return Returns the next available geometry ID.
  ///
  virtual uint32_t NextGeometryId() = 0;

  ///
  /// Create geometry with certain ID and vertex/index data.
  ///
  /// @param geometry_id  The geometry ID to use for the new geometry.
  ///
  /// @param vertices     The vertex buffer data.
  ///
  /// @param indices      The index buffer data.
  ///
  /// @warning A deep copy of the vertex/index data should be made if you are uploading it to the
  ///          GPU asynchronously, it will not persist beyond this call.
  ///
  virtual void CreateGeometry(uint32_t geometry_id, const VertexBuffer& vertices,
                              const IndexBuffer& indices)
      = 0;

  ///
  /// Update existing geometry with new vertex/index data.
  ///
  /// @param geometry_id  The geometry to update.
  ///
  /// @param vertices     The new vertex buffer data.
  ///
  /// @param indices      The new index buffer data.
  ///
  /// @warning A deep copy of the vertex/index data should be made if you are uploading it to the
  ///          GPU asynchronously, it will not persist beyond this call.
  ///
  virtual void UpdateGeometry(uint32_t geometry_id, const VertexBuffer& vertices,
                              const IndexBuffer& indices)
      = 0;

  ///
  /// Destroy geometry.
  ///
  /// @param geometry_id  The geometry to destroy.
  ///
  virtual void DestroyGeometry(uint32_t geometry_id) = 0;

  ///
  /// Update the pending command list with commands to execute on the GPU.
  ///
  /// Commands are dispatched to the GPU driver asynchronously via this method. The GPU driver
  /// implementation should consume these commands and execute them at an appropriate time.
  ///
  /// @param list  The list of commands to execute.
  ///
  /// @warning Implementations should make a deep copy of the command list, it will not persist
  ///          beyond this call.
  ///
  virtual void UpdateCommandList(const CommandList& list) = 0;

  ///
  /// Report this driver's optional capabilities.
  ///
  /// The engine calls this to discover optional features (for example, which
  /// block-compressed texture formats can be uploaded and sampled natively).
  /// The result may be cached.
  ///
  /// @param  caps  A zero-initialized structure to fill in. Set only the
  ///               capabilities this driver supports and leave the rest at zero.
  ///
  /// @note  The default implementation reports no optional capabilities, so a
  ///        driver that does not override this method keeps working unchanged
  ///        (the engine falls back to software where a capability is required).
  ///
  virtual void GetDeviceCaps(GPUDeviceCaps& caps) { }
};

} // namespace ultralight

// clang-format on