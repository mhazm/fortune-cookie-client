///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///

// clang-format off

///
/// @file CAPI_GPUDriver.h
///
/// User-defined GPU driver interface.
///
/// `#include <Ultralight/CAPI/CAPI_GPUDriver.h>`
///
/// The library uses this to optionally render Views on the GPU (see ulViewIsAccelerated()).
///
/// You can provide the library with your own GPU driver implementation so that all rendering is
/// performed using an existing GPU context (useful for game engines).
///
/// When a View is rendered on the GPU, you can retrieve the backing texture ID via
/// ulViewGetRenderTarget().
///
/// ## State Synchronization
///
/// During each call to ulRender(), the library updates the state of your driver (textures, render
/// buffers, geometry, and command lists) to match its own. It calls begin_synchronize before any
/// state is updated and end_synchronize after, and every create, update, and destroy callback
/// runs between the two.
///
/// ## Drawing
///
/// All drawing is done through command lists (update_command_list) so that your driver can
/// execute them later, eg, on your own render thread. Copy any data you use after a callback
/// returns: it doesn't persist beyond the call.
///
/// @see ulPlatformSetGPUDriver()
///
#ifndef ULTRALIGHT_CAPI_GPUDRIVER_H
#define ULTRALIGHT_CAPI_GPUDRIVER_H

#include <Ultralight/CAPI/CAPI_Defines.h>

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
 * GPUDriver
 *****************************************************************************/

///
/// Render buffer description.
///
/// This structure describes a render buffer that can be used as a target for drawing commands.
///
typedef struct {
  unsigned int texture_id; ///< The backing texture for this RenderBuffer
  unsigned int width;      ///< The width of the RenderBuffer texture
  unsigned int height;     ///< The height of the RenderBuffer texture
  bool has_stencil_buffer; ///< Currently unused, always false.
  bool has_depth_buffer;   ///< Currently unused, always false.
} ULRenderBuffer;

/// \cond ignore
///   This pragma pack(push, 1) command is important! 
///   GPU structs should not be padded with any bytes.
/// \endcond
#pragma pack(push, 1)

///
/// Vertex layout for path vertices.
///
/// This struct is the in-memory layout for each path vertex (useful for synthesizing or modifying
/// your own vertex data).
///
typedef struct {
  float pos[2];
  unsigned char color[4];
  float obj[2];
} ULVertex_2f_4ub_2f;

///
/// Vertex layout for quad vertices.
///
/// This struct is the in-memory layout for each quad vertex (useful for synthesizing or modifying
/// your own vertex data).
///
typedef struct {
  float pos[2];
  unsigned char color[4];
  float tex[2];
  float obj[2];
  float data0[4];
  float data1[4];
  float data2[4];
  float data3[4];
  float data4[4];
  float data5[4];
  float data6[4];
} ULVertex_2f_4ub_2f_2f_28f;

///
/// Vertex layout for instance vertices.
///
/// This struct is the in-memory layout for each instance vertex (used for instanced cell
/// rendering, see kShaderType_FillPhotonGrid).
///
typedef struct {
  float pos[2];              ///< Vertex position (x, y) in pixels.
  unsigned int header_addr;  ///< Cell header texel address in the index data page.
  unsigned int path_slot;    ///< Per-path constant-table slot (low bits) plus flags.
} ULVertex_2f_2ui;

///
/// End single-byte alignment.
///
#pragma pack(pop)

///
/// Vertex buffer formats.
///
/// This enumeration describes the format of a vertex buffer.
///
typedef enum {
  kVertexBufferFormat_2f_4ub_2f,        ///< Vertex_2f_4ub_2f (used for path rendering)
  kVertexBufferFormat_2f_4ub_2f_2f_28f, ///< Vertex_2f_4ub_2f_2f_28f (used for quad rendering)
  kVertexBufferFormat_2f_2ui,           ///< Vertex_2f_2ui (used for instanced cell rendering)
} ULVertexBufferFormat;

///
/// Vertex buffer description.
///
/// @see ULGPUDriver::create_geometry
///
typedef struct {
  ULVertexBufferFormat format; ///< The format of the vertex buffer.
  unsigned int size;           ///< The size of the vertex buffer in bytes.
  unsigned char* data;         ///< The raw vertex buffer data.
} ULVertexBuffer;

///
/// Vertex index type.
///
typedef unsigned int ULIndexType;

///
/// Index buffer description.
///
/// This structure describes an index buffer that can be used to index into a vertex buffer.
///
/// @note The index buffer is a simple array of IndexType values.
///
typedef struct {
  unsigned int size;   ///< The size of the index buffer in bytes.
  unsigned char* data; ///< The raw index buffer data.
} ULIndexBuffer;

///
/// Shader program types, used with ULGPUState::shader_type
///
/// Each of these correspond to a vertex/pixel shader pair. Stock shaders for every program ship in
/// the SDK's `platform/shaders` folder, in a compiled or generated form for each backend (DXBC,
/// SPIR-V, MSL, and GLSL).
///
typedef enum {
  kShaderType_Fill,             ///< Shader program for filling quad geometry.
  kShaderType_FillPath,         ///< Shader program for filling tessellated path geometry.
  kShaderType_FilterBasic,      ///< Shader program for basic CSS/SVG filters.
  kShaderType_FilterBlur,       ///< Shader program for blur CSS/SVG filters.
  kShaderType_FilterDropShadow, ///< Shader program for drop-shadow CSS/SVG filters.

  ///
  /// Shader program for analytic vector rendering (Photon). Uses the quad
  /// vertex layout.
  ///
  /// Photon draws carry data pages in the ULGPUState texture slots and read
  /// them with texel loads only. Bind each slot unfiltered, without mipmaps,
  /// at the register the shipped shader source declares:
  ///
  /// | ULGPUState slot | Carries                      | Format   | Shader binding      |
  /// |-----------------|------------------------------|----------|---------------------|
  /// | `texture_1_id`  | Curve page                   | RGBA16F  | `Texture0` (t0)     |
  /// | `texture_2_id`  | (never set on Photon draws)  |          |                     |
  /// | `texture_3_id`  | Constants / glyph LUT / ramp | RGBA32F  | `Texture2` (t2)     |
  /// | `texture_4_id`  | Index/header page            | RGBA16UI | `IndexTexture` (t3) |
  ///
  /// The index page has its own slot and register because it needs an
  /// integer (`uint4`) texture view; it must never share a binding point
  /// with the float views other programs use.
  ///
  /// Every driver must support this program alongside the others; there is
  /// no capability query for it.
  ///
  kShaderType_FillPhoton,

  ///
  /// Shader program for grid-based analytic vector rendering (Photon). Uses
  /// the ULVertex_2f_2ui instance layout.
  ///
  /// Binds the same data pages, slots, and registers as kShaderType_FillPhoton,
  /// with one addition: the vertex shader also reads the texture slots, so
  /// bind them to both shader stages for this program.
  ///
  /// Every driver must support this program alongside the others.
  ///
  kShaderType_FillPhotonGrid,
} ULShaderType;

///
/// Blend factors that define how source and destination colors are weighted during blending.
///
/// These factors control how much the source color (from the pixel shader) and destination color
/// (from the render buffer) contribute to the final blended result.
///
/// @see ULGPUState::blend_src_factor, ULGPUState::blend_dst_factor
///
typedef enum {
  kBlendFactor_Zero,             ///< Factor of (0, 0, 0, 0)
  kBlendFactor_One,              ///< Factor of (1, 1, 1, 1)
  kBlendFactor_SrcColor,         ///< Factor of (Rs, Gs, Bs, As)
  kBlendFactor_InvSrcColor,      ///< Factor of (1-Rs, 1-Gs, 1-Bs, 1-As)
  kBlendFactor_SrcAlpha,         ///< Factor of (As, As, As, As)
  kBlendFactor_InvSrcAlpha,      ///< Factor of (1-As, 1-As, 1-As, 1-As)
  kBlendFactor_DestColor,        ///< Factor of (Rd, Gd, Bd, Ad)
  kBlendFactor_InvDestColor,     ///< Factor of (1-Rd, 1-Gd, 1-Bd, 1-Ad)
  kBlendFactor_DestAlpha,        ///< Factor of (Ad, Ad, Ad, Ad)
  kBlendFactor_InvDestAlpha,     ///< Factor of (1-Ad, 1-Ad, 1-Ad, 1-Ad)
  kBlendFactor_SrcAlphaSaturate, ///< Factor of (f, f, f, 1) where f = min(As, 1-Ad)
} ULBlendFactor;

///
/// Mathematical operations for combining source and destination colors during blending.
///
/// @see ULGPUState::blend_equation
///
typedef enum {
  kBlendEquation_Add,         ///< Result = (Src * SrcFactor) + (Dest * DestFactor)
  kBlendEquation_Subtract,    ///< Result = (Src * SrcFactor) - (Dest * DestFactor)
  kBlendEquation_RevSubtract, ///< Result = (Dest * DestFactor) - (Src * SrcFactor)
  kBlendEquation_Min,         ///< Result = min(Src, Dest), ignores blend factors
  kBlendEquation_Max,         ///< Result = max(Src, Dest), ignores blend factors
} ULBlendEquation;

///
/// Raw 4x4 matrix as an array of floats in column-major order.
///
typedef struct {
  float data[16];
} ULMatrix4x4;

///
/// 4-component float vector
///
typedef struct {
  float value[4];
} ULvec4;

///
/// The state of the GPU for a given draw command.
///
/// This structure describes the current state of the GPU for a given draw command.
///
typedef struct {
  /// Viewport width in pixels
  unsigned int viewport_width;

  /// Viewport height in pixels
  unsigned int viewport_height;

  /// Transform matrix-- you should multiply this with the screen-space orthographic projection
  /// matrix then pass to the vertex shader.
  ULMatrix4x4 transform;

  /// Whether or not we should enable texturing for the current draw command.
  bool enable_texturing;

  /// Whether or not we should enable blending for the current draw command. If blending is
  /// disabled, any drawn pixels should overwrite existing. Mainly used so we can modify alpha
  /// values of the RenderBuffer during scissored clears.
  bool enable_blend;

  /// The blend factor for the source color (pixel shader output).
  /// The library sets it on every draw command (kBlendFactor_One for normal premultiplied-alpha
  /// drawing). Ignore it when enable_blend is false.
  unsigned char blend_src_factor;

  /// The blend factor for the destination color (render buffer).
  /// The library sets it on every draw command (kBlendFactor_InvSrcAlpha for normal
  /// premultiplied-alpha drawing). Ignore it when enable_blend is false.
  unsigned char blend_dst_factor;

  /// The mathematical equation used to combine the source and destination colors.
  /// The library sets it on every draw command (kBlendEquation_Add for normal drawing). Ignore it
  /// when enable_blend is false.
  unsigned char blend_equation;

  /// The vertex/pixel shader program pair to use for the current draw command.
  ///
  /// @see ULShaderType
  unsigned char shader_type;

  /// The render buffer to use for the current draw command.
  unsigned int render_buffer_id;

  /// The texture id to bind to slot #1. (Will be 0 if none)
  unsigned int texture_1_id;

  /// The texture id to bind to slot #2. (Will be 0 if none)
  unsigned int texture_2_id;

  /// The texture id to bind to slot #3. (Will be 0 if none)
  unsigned int texture_3_id;

  /// The texture id to bind to slot #4. (Will be 0 if none)
  ///
  /// Only the Photon shader programs use this slot: it carries the RGBA16UI
  /// index/header data page, kept on its own slot so its integer texture view
  /// never shares a binding point with the float views on slots #1-#3.
  unsigned int texture_4_id;

  /// The uniform integers (passed to the pixel shader via uniforms).
  int uniform_integer[8];

  /// The uniform scalars (passed to the pixel shader via uniforms).
  float uniform_scalar[8];

  /// The uniform vectors (passed to the pixel shader via uniforms).
  ULvec4 uniform_vector[8];

  /// The clip size (passed to the pixel shader via uniforms).
  unsigned char clip_size;

  /// The clip stack (passed to the pixel shader via uniforms).
  ULMatrix4x4 clip[8];

  /// Whether or not scissor testing should be used for the current draw command.
  bool enable_scissor;

  /// The scissor rect to use for scissor testing (units in pixels)
  ULIntRect scissor_rect;
} ULGPUState;

///
/// The types of commands.
///
/// This enumeration describes the type of command to execute on the GPU. Used with
/// ULCommand::command_type
///
typedef enum {
  kCommandType_ClearRenderBuffer, ///< Clear the specified render buffer.
  kCommandType_DrawGeometry,      ///< Draw the specified geometry to the specified render buffer.

  ///
  /// Submit pending GPU work before executing the next command.
  ///
  /// Emitted before the library draws into a render buffer whose texture was
  /// sampled by earlier draws in the same command list. What to do:
  ///
  /// - Direct3D 12 / Metal / Vulkan: usually a no-op; your per-resource
  ///   barriers or transitions already order these accesses. Verify that
  ///   ordering covers clears as well as draws.
  /// - Direct3D 11 / OpenGL: submit pending work (Flush() / glFlush()).
  ///   Some drivers otherwise reorder the pending sampling draws against the
  ///   upcoming clear and draws, producing intermittent dropouts in the
  ///   sampled output.
  ///
  /// @note  No fields of ULGPUState are meaningful on a flush command.
  ///
  kCommandType_Flush,
} ULCommandType;

///
/// A command to execute on the GPU.
///
/// This structure describes a command to be executed on the GPU.
///
/// Commands are dispatched to the GPU driver asynchronously via ULGPUDriver::update_command_list,
/// the GPU driver should consume these commands and execute them at an appropriate time.
///
/// @see ULCommandList
///
typedef struct {
  unsigned char command_type;  ///< The type of command to dispatch. @see ULCommandType
  ULGPUState gpu_state;        ///< The current GPU state.
  unsigned int geometry_id;    ///< The geometry ID to bind. (used with kCommandType_DrawGeometry)
  unsigned int indices_count;  ///< The number of indices.   (used with kCommandType_DrawGeometry)
  unsigned int indices_offset; ///< The index to start from. (used with kCommandType_DrawGeometry)
} ULCommand;

///
/// List of commands to execute on the GPU.
///
/// @see ULGPUDriver::update_command_list
///
typedef struct {
  unsigned int size;   ///< The number of commands in the list.
  ULCommand* commands; ///< The raw command list data.
} ULCommandList;

///
/// Callback for users to implement ULGPUDriver::begin_synchronize.
///
/// Called before any state (eg, create_texture(), update_texture(), destroy_texture(), etc.) is
/// updated during a call to ulRender().
///
/// This is a good time to prepare the GPU for any state updates.
///
typedef void (*ULGPUDriverBeginSynchronizeCallback)(void);

///
/// Callback for users to implement ULGPUDriver::end_synchronize.
///
/// Called after all state has been updated during a call to ulRender().
///
typedef void (*ULGPUDriverEndSynchronizeCallback)(void);

///
/// Callback for users to implement ULGPUDriver::next_texture_id.
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
typedef unsigned int (*ULGPUDriverNextTextureIdCallback)(void);

///
/// Flags passed to ULGPUDriverCreateTextureCallback describing how a texture
/// will be used. Combine with bitwise OR.
///
typedef enum {
  /// The texture is a render target (the backing texture for a render buffer). Create it so you
  /// can both render into and sample from it. Its bitmap is empty, so there's nothing to upload
  /// at texture creation.
  ///
  /// Most render targets are BGRA (kBitmapFormat_BGRA8_UNORM_SRGB). A render target may also be
  /// single-channel (kBitmapFormat_A8_UNORM): back it with a renderable one-channel format (R8 on
  /// modern graphics APIs) so that sampling it returns the stored value in the RED channel. Do not
  /// apply the alpha swizzle that sampled (non-render-target) A8 textures traditionally carry.
  /// Single-channel render targets are only ever drawn with blending disabled and are never
  /// multisampled.
  kULGPUTextureFlag_RenderTarget = 1 << 0,

  /// The render target should be multisampled (antialiased), resolved to a single-sample texture
  /// when sampled. Only set alongside kULGPUTextureFlag_RenderTarget.
  kULGPUTextureFlag_Antialiased = 1 << 1,

  /// The texture is uploaded once and never changes; the update callback is never called on it, so
  /// you may use immutable storage.
  kULGPUTextureFlag_Immutable = 1 << 2,
} ULGPUTextureFlags;

///
/// Callback for users to implement ULGPUDriver::create_texture.
///
/// Create a texture with a certain ID and bitmap.
///
/// @param texture_id  The texture ID to use for the new texture.
///
/// @param bitmap      The bitmap to initialize the texture with. Empty for a
///                    render target (see `flags`); otherwise holds the pixel
///                    data to upload.
///
/// @param flags       A bitwise-OR of ULGPUTextureFlags describing how the
///                    texture will be used.
///
/// @warning A deep copy of the bitmap data should be made if you are uploading it to the GPU
///          asynchronously, it will not persist beyond this call.
///
typedef void (*ULGPUDriverCreateTextureCallback)(unsigned int texture_id, ULBitmap bitmap,
                                                 unsigned int flags);

///
/// Callback for users to implement ULGPUDriver::update_texture.
///
/// Update an existing non-RTT texture with new bitmap data.
///
/// @param texture_id  The texture to update.
///
/// @param bitmap      The new bitmap data. Always the complete texture surface, valid inside
///                    and outside `dirty_rect`.
///
/// @param dirty_rect  The region of the bitmap, in pixels, that changed since the last upload.
///                    This is an optimization hint: you may upload only this region, or ignore
///                    it and upload the whole bitmap.
///
/// @warning A deep copy of the bitmap data should be made if you are uploading it to the GPU
///          asynchronously, it will not persist beyond this call.
///
typedef void (*ULGPUDriverUpdateTextureCallback)(unsigned int texture_id, ULBitmap bitmap,
                                                 ULIntRect dirty_rect);

///
/// Callback for users to implement ULGPUDriver::destroy_texture.
///
/// Destroy a texture.
///
/// @param texture_id  The texture to destroy.
///
typedef void (*ULGPUDriverDestroyTextureCallback)(unsigned int texture_id);

///
/// Callback for users to implement ULGPUDriver::next_render_buffer_id.
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
typedef unsigned int (*ULGPUDriverNextRenderBufferIdCallback)(void);

///
/// Callback for users to implement ULGPUDriver::create_render_buffer.
///
/// Create a render buffer with certain ID and buffer description.
///
/// @param render_buffer_id  The render buffer ID to use for the new render buffer.
///
/// @param buffer           The render buffer description.
///
typedef void (*ULGPUDriverCreateRenderBufferCallback)(unsigned int render_buffer_id,
                                                      ULRenderBuffer buffer);

///
/// Callback for users to implement ULGPUDriver::destroy_render_buffer.
///
/// Destroy a render buffer.
///
/// @param render_buffer_id  The render buffer to destroy.
///
typedef void (*ULGPUDriverDestroyRenderBufferCallback)(unsigned int render_buffer_id);

///
/// Callback for users to implement ULGPUDriver::next_geometry_id.
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
typedef unsigned int (*ULGPUDriverNextGeometryIdCallback)(void);

///
/// Callback for users to implement ULGPUDriver::create_geometry.
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
typedef void (*ULGPUDriverCreateGeometryCallback)(unsigned int geometry_id, ULVertexBuffer vertices,
                                                  ULIndexBuffer indices);

///
/// Callback for users to implement ULGPUDriver::update_geometry.
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
typedef void (*ULGPUDriverUpdateGeometryCallback)(unsigned int geometry_id, ULVertexBuffer vertices,
                                                  ULIndexBuffer indices);

///
/// Callback for users to implement ULGPUDriver::destroy_geometry.
///
/// Destroy geometry.
///
/// @param geometry_id  The geometry to destroy.
///
typedef void (*ULGPUDriverDestroyGeometryCallback)(unsigned int geometry_id);

///
/// Callback for users to implement ULGPUDriver::update_command_list.
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
typedef void (*ULGPUDriverUpdateCommandListCallback)(ULCommandList list);

///
/// Optional compressed-texture format families.
///
/// Each value is a single bit within ULGPUDeviceCaps::compressed_formats. Set a family's bit to
/// report that your create_texture callback can accept a bitmap in any of that family's formats
/// and sample it directly (the GPU decompresses the blocks when it reads them). The library
/// decompresses the formats of a family you don't report before uploading them.
///
typedef enum {
  /// BC1, BC2, and BC3 (also known as DXT1 / DXT3 / DXT5).
  kULGPUCompressedFormat_BC1_BC3 = 1 << 0,

  /// BC7.
  kULGPUCompressedFormat_BC7 = 1 << 1,
} ULGPUCompressedFormat;

///
/// Optional capabilities reported by a ULGPUDriver.
///
/// The library zero-initializes this structure and passes it to your get_device_caps callback,
/// which fills in the capabilities your driver supports. Any field left untouched reads back as
/// zero, meaning "not supported" for a capability field and "use the library default" for a
/// limit field. See `<Ultralight/platform/GPUDriver.h>` for the full field semantics.
///
typedef struct {
  unsigned int compressed_formats;      ///< Bitwise OR of supported ULGPUCompressedFormat values.
  unsigned int supports_msaa;           ///< Nonzero if multisampled render targets are supported.
  unsigned int max_texture_dimension;   ///< Largest texture width/height, in pixels (0 = default).
  unsigned int max_msaa_samples;        ///< Sample count for multisampled targets (0 = default).
  unsigned int max_geometry_size;       ///< Largest vertex/index buffer, in bytes (0 = default).
  unsigned int supports_partial_redraw; ///< Nonzero if partial redraws of large targets are cheap.
} ULGPUDeviceCaps;

///
/// Callback for users to implement ULGPUDriver::get_device_caps.
///
/// Report your driver's optional capabilities by filling in `caps` (it arrives
/// zero-initialized). The result may be cached.
///
typedef void (*ULGPUDriverGetDeviceCapsCallback)(ULGPUDeviceCaps* caps);

///
/// User-defined GPU driver interface.
///
/// You should implement each of these callbacks, then pass an instance of this struct containing
/// your callbacks to ulPlatformSetGPUDriver().
///
typedef struct {
  ULGPUDriverBeginSynchronizeCallback begin_synchronize;
  ULGPUDriverEndSynchronizeCallback end_synchronize;
  ULGPUDriverNextTextureIdCallback next_texture_id;
  ULGPUDriverCreateTextureCallback create_texture;
  ULGPUDriverUpdateTextureCallback update_texture;
  ULGPUDriverDestroyTextureCallback destroy_texture;
  ULGPUDriverNextRenderBufferIdCallback next_render_buffer_id;
  ULGPUDriverCreateRenderBufferCallback create_render_buffer;
  ULGPUDriverDestroyRenderBufferCallback destroy_render_buffer;
  ULGPUDriverNextGeometryIdCallback next_geometry_id;
  ULGPUDriverCreateGeometryCallback create_geometry;
  ULGPUDriverUpdateGeometryCallback update_geometry;
  ULGPUDriverDestroyGeometryCallback destroy_geometry;
  ULGPUDriverUpdateCommandListCallback update_command_list;
  ULGPUDriverGetDeviceCapsCallback get_device_caps; ///< May be NULL (reports no capabilities).
} ULGPUDriver;

///
/// Sets up an orthographic projection matrix with a certain viewport width and height, multiplies
/// it by 'transform', and returns the result.
///
/// This should be used to calculate the model-view projection matrix for the vertex shaders using
/// the current ULGPUState.
///
/// The 'flip_y' can be optionally used to flip the Y coordinate-space. (Usually flip_y == true for
/// OpenGL)
///
ULExport ULMatrix4x4 ulApplyProjection(ULMatrix4x4 transform, float viewport_width,
                                       float viewport_height, bool flip_y);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ULTRALIGHT_CAPI_GPUDRIVER_H

// clang-format on