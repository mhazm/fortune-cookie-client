///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/Defines.h>
#include <Ultralight/RefPtr.h>
#include <Ultralight/Geometry.h>
#include <Ultralight/Buffer.h>

namespace ultralight {

///
/// The various Bitmap formats.
///
enum class BitmapFormat : uint8_t {
  ///
  /// Alpha channel only, 8-bits per pixel.
  ///
  /// Encoding: 8-bits per channel, unsigned normalized.
  ///
  /// Color-space: Linear (no gamma), alpha-coverage only.
  ///
  A8_UNORM,

  ///
  /// Blue Green Red Alpha channels, 32-bits per pixel.
  ///
  /// Encoding: 8-bits per channel, unsigned normalized.
  ///
  /// Color-space: sRGB gamma with premultiplied linear alpha channel.
  ///
  BGRA8_UNORM_SRGB,

  ///
  /// Red Green channels, 16-bits per pixel.
  ///
  /// Encoding: 8-bits per channel, unsigned normalized.
  ///
  /// Color-space: Linear (no gamma).
  ///
  /// Not PNG-encodable.
  ///
  RG8_UNORM,

  ///
  /// BC1 block-compressed color (4x4 blocks, 8 bytes/block). Optional 1-bit alpha.
  ///
  /// Color-space: sRGB gamma with optional alpha.
  ///
  BC1_UNORM,

  ///
  /// BC2 block-compressed color (4x4 blocks, 16 bytes/block). Explicit 4-bit alpha.
  ///
  /// Color-space: sRGB gamma with premultiplied alpha.
  ///
  BC2_UNORM,

  ///
  /// BC3 block-compressed color (4x4 blocks, 16 bytes/block). Interpolated alpha.
  ///
  /// Color-space: sRGB gamma with premultiplied alpha.
  ///
  BC3_UNORM,

  ///
  /// BC7 block-compressed color (4x4 blocks, 16 bytes/block). High-quality RGB(A).
  ///
  /// Color-space: sRGB gamma with premultiplied alpha.
  ///
  BC7_UNORM,

  ///
  /// Four half-precision float channels (16 bits per channel, 64 bits per pixel).
  ///
  /// This is a raw data format: GPUDriver should create these as sampleable
  /// textures without filtering or mipmaps and upload rows exactly as stored.
  ///
  RGBA16F,

  ///
  /// Four unsigned integer channels (16 bits per channel, 64 bits per pixel).
  ///
  /// This is a raw data format: GPUDriver should create these as sampleable
  /// textures without filtering or mipmaps and upload rows exactly as stored.
  ///
  RGBA16UI,

  ///
  /// Four float channels (32 bits per channel, 128 bits per pixel).
  ///
  /// This is a raw data format: GPUDriver should create these as sampleable
  /// textures without filtering or mipmaps and upload rows exactly as stored.
  ///
  RGBA32F,
};

///
/// Macro to get the bytes per pixel from a BitmapFormat.
///
/// @note  This is only meaningful for non-block-compressed formats. Block-compressed (BCn) formats
///        have no per-pixel size-- use GetBitmapFormatInfo() and row_bytes() / size() for storage
///        math instead.
///
#define GetBytesPerPixel(x) ((x) == BitmapFormat::A8_UNORM  ? 1  \
                           : (x) == BitmapFormat::RG8_UNORM ? 2  \
                           : (x) == BitmapFormat::RGBA16F   ? 8  \
                           : (x) == BitmapFormat::RGBA16UI  ? 8  \
                           : (x) == BitmapFormat::RGBA32F   ? 16 \
                           :                                  4)

///
/// Block geometry for a BitmapFormat.
///
/// Uncompressed formats have a 1x1 block whose block_bytes equals the bytes-per-pixel.
/// Block-compressed (BCn) formats have a 4x4 block. A bitmap's row pitch and surface size are
/// derived from the block geometry:
///
/// ```
/// row_pitch    = ceil(width / block_width) * block_bytes
/// surface_size = row_pitch * ceil(height / block_height)
/// ```
///
/// For a 1x1 block this reduces to `width * block_bytes` and `row_pitch * height`.
///
struct BitmapFormatInfo {
  ///
  /// Width of one block, in pixels (1 for uncompressed, 4 for BCn).
  ///
  uint32_t block_width;

  ///
  /// Height of one block, in pixels (1 for uncompressed, 4 for BCn).
  ///
  uint32_t block_height;

  ///
  /// Size of one block, in bytes (equals bytes-per-pixel when the block is 1x1).
  ///
  uint32_t block_bytes;
};

///
/// Get the block geometry for a given BitmapFormat.
///
/// @param  format  The pixel format to query.
///
/// @return  Returns the block width, block height, and block size in bytes for the format.
///
UExport BitmapFormatInfo GetBitmapFormatInfo(BitmapFormat format);

///
/// Whether or not a given BitmapFormat is block-compressed (a BCn format).
///
/// @param  format  The pixel format to query.
///
UExport bool IsCompressedFormat(BitmapFormat format);

///
/// Forward declaration for the LockedPixels class.
/// 
template<typename T>
class LockedPixels;

///
/// Function signature for a user-defined destruction callback to be optionally called when the
/// Bitmap is destroyed.
///
/// @param  user_data  Pointer to user-defined user-data (this will be the same value as what was
///                    passed to Bitmap::Create, if any)
///
/// @param  data       Pointer to raw Bitmap pixel data.
///
typedef void (*DestroyBitmapCallback)(void* user_data, void* data);

///
/// A container for pixel data.
///
/// The bitmap class is used to store pixel data in a variety of formats. It intelligently manages
/// the lifetime of the pixel buffer and serializes access to it while the Bitmap owns the pixels.
///
/// ## Thread Safety
///
/// LockPixels() and UnlockPixels() take the Bitmap's internal lock only when the Bitmap owns its
/// pixel buffer, ie. one created by a Create() overload that allocates, or one that copied the
/// pixels you passed in. A Bitmap that wraps memory you still own-- created with
/// `should_copy = false`, or with a destruction callback-- does no locking at all, so you must
/// serialize access to that buffer yourself.
///
/// ## Accessing Pixel Data
///
/// You can access the underlying pixel data by using the LockPixelsSafe() method. An example
/// follows:
///
/// ```
/// auto bitmap = Bitmap::Create(100, 100, BitmapFormat::BGRA8_UNORM_SRGB);
/// auto pixels = bitmap->LockPixelsSafe();
/// if (pixels && pixels.data()) {
///   // Zero out the pixel buffer by setting every byte to 0.
///   memset(pixels.data(), 0, pixels.size());
/// }
/// 
/// // 'pixels' is automatically unlocked when it goes out of scope.
/// ```
class UExport Bitmap : public WeakRefCounted {
 public:
  ///
  /// Maximum supported width or height, in pixels.
  ///
  /// Create() returns a null ref-pointer if either dimension exceeds this limit.
  ///
  /// The largest possible allocation, MaxDimension by MaxDimension at 4 bytes per pixel, is
  /// about 1 GB.
  ///
  static constexpr uint32_t MaxDimension = 16384;

  ///
  /// Create an empty Bitmap. No pixels will be allocated.
  ///
  /// @return  Returns a ref-pointer to a new Bitmap instance.
  ///
  static RefPtr<Bitmap> Create();

  ///
  /// Create a Bitmap with a certain configuration. Pixels will be allocated but not initialized.
  ///
  /// @param  width   The width in pixels.
  ///
  /// @param  height  The height in pixels.
  ///
  /// @param  format  The pixel format to use.
  ///
  /// @return  Returns a ref-pointer to a new Bitmap instance.
  ///
  static RefPtr<Bitmap> Create(uint32_t width, uint32_t height, BitmapFormat format);

  ///
  /// Create an aligned Bitmap with a certain configuration. Pixels will be allocated but not
  /// initialized. Row bytes will be padded to reach the specified alignment.
  ///
  /// @param  width   The width in pixels.
  ///
  /// @param  height  The height in pixels.
  ///
  /// @param  format  The pixel format to use.
  ///
  /// @param  alignment  The alignment (in bytes) to use. Row bytes will be padded to reach a
  ///                    multiple of this value and the underlying storage will be allocated with
  ///                    this alignment.
  ///
  /// @return  Returns a ref-pointer to a new Bitmap instance.
  ///
  static RefPtr<Bitmap> Create(uint32_t width, uint32_t height, BitmapFormat format,
                               uint32_t alignment);

  ///
  /// Create a Bitmap with existing pixel data, a copy will be made unless should_copy is false.
  ///
  /// @param  width        The width in pixels.
  ///
  /// @param  height       The height in pixels.
  ///
  /// @param  format       The pixel format to use.
  ///
  /// @param  row_bytes    The number of bytes between each row of pixels. This should be at least
  ///                      `width * bytes_per_pixel`, or for a block-compressed format, the width
  ///                      in 4x4 blocks times the block size (see GetBitmapFormatInfo()).
  ///
  /// @param  pixels       Pointer to raw pixel buffer.
  ///
  /// @param  size         Size of the raw pixel buffer.
  ///
  /// @param  should_copy  Whether or not a copy should be made of the pixels. If this is false
  ///                      the returned Bitmap will use the raw pixels passed in as its own, but
  ///                      you are still responsible for destroying your buffer afterwards.
  ///
  /// @return  Returns a ref-pointer to a new Bitmap instance.
  ///
  static RefPtr<Bitmap> Create(uint32_t width, uint32_t height, BitmapFormat format,
                               uint32_t row_bytes, const void* pixels, size_t size,
                               bool should_copy = true);

  ///
  /// Create a Bitmap that wraps existing pixel data, a user-defined destruction callback will be
  /// called when the Bitmap wants to destroy the data.
  ///
  /// @param  width        The width in pixels.
  ///
  /// @param  height       The height in pixels.
  ///
  /// @param  format       The pixel format to use.
  ///
  /// @param  row_bytes    The number of bytes between each row of pixels. This should be at least
  ///                      `width * bytes_per_pixel`, or for a block-compressed format, the width
  ///                      in 4x4 blocks times the block size (see GetBitmapFormatInfo()).
  ///
  /// @param  pixels       Pointer to raw pixel buffer.
  ///
  /// @param  size         Size of the raw pixel buffer.
  /// 
  /// @param  user_data    Optional user data that will be passed to destruction_callback when the
  ///                      Bitmap wants to destroy the pixel data. (Pass nullptr to ignore)
  ///
  /// @param  destruction_callback  Callback that will be called upon destruction.
  ///
  /// @return  Returns a ref-pointer to a new Bitmap instance.
  ///
  static RefPtr<Bitmap> Create(uint32_t width, uint32_t height, BitmapFormat format,
                               uint32_t row_bytes, const void* pixels, size_t size, 
                               void* user_data, DestroyBitmapCallback destruction_callback);

  ///
  /// Create a bitmap from a deep copy of another Bitmap.
  ///
  /// @param  bitmap  The bitmap to copy from.
  ///
  /// @return  Returns a ref-pointer to a new Bitmap instance.
  ///
  static RefPtr<Bitmap> Create(const Bitmap& bitmap);

  ///
  /// Get the width in pixels.
  ///
  virtual uint32_t width() const = 0;

  ///
  /// Get the height in pixels.
  ///
  virtual uint32_t height() const = 0;

  ///
  /// Get the bounds as an IntRect.
  ///
  virtual IntRect bounds() const = 0;

  ///
  /// Get the pixel format.
  ///
  virtual BitmapFormat format() const = 0;

  ///
  /// Get the number of bytes per pixel.
  ///
  virtual uint32_t bpp() const = 0;

  ///
  /// Get the number of bytes between each row of pixels.
  ///
  /// @note  This value is usually calculated as width * bytes_per_pixel (bpp) but it may be larger
  ///        due to alignment rules in the allocator.
  ///
  virtual uint32_t row_bytes() const = 0;

  ///
  /// Get the size in bytes of the pixel buffer.
  ///
  /// @note  Size is calculated as row_bytes() * height().
  ///
  virtual size_t size() const = 0;

  ///
  /// Whether or not this Bitmap owns the pixel buffer and will destroy it at the end of its
  /// lifetime.
  ///
  virtual bool owns_pixels() const = 0;

  ///
  /// Lock the pixel buffer for reading/writing (safe version, automatically unlocks).
  ///
  /// @return  Returns a managed container that can be used to access the pixels
  ///          (LockedPixels::data()). This container will automatically unlock the pixels when it
  ///          goes out of scope.
  ///
  virtual LockedPixels<RefPtr<Bitmap>> LockPixelsSafe() const = 0;

  ///
  /// Lock the pixel buffer for reading/writing.
  ///
  /// @return  Returns a pointer to the pixel buffer.
  ///
  virtual void* LockPixels() = 0;

  ///
  /// Unlock the pixel buffer.
  ///
  virtual void UnlockPixels() = 0;

  ///
  /// Lock the pixel buffer for reading/writing. (const)
  ///
  /// @return  Returns a const pointer to the pixel buffer.
  ///
  virtual const void* LockPixels() const = 0;

  ///
  /// Unlock the pixel buffer. (const)
  ///
  virtual void UnlockPixels() const = 0;

  ///
  /// Get the raw pixel buffer.
  ///
  /// @note  You should only call this if pixels are already locked.
  ///
  virtual void* raw_pixels() = 0;

  ///
  /// Whether or not this Bitmap is empty (no pixels allocated).
  ///
  virtual bool IsEmpty() const = 0;

  ///
  /// Erase the Bitmap (set all pixels to 0).
  ///
  virtual void Erase() = 0;

  ///
  /// Assign another bitmap to this one.
  ///
  /// @param  bitmap  The bitmap to copy from.
  ///
  /// @note  The source pixels are deep-copied in every case but one: a source that wraps memory
  ///        you still own (created with `should_copy = false` and no destruction callback) is
  ///        aliased instead, so that buffer must outlive this Bitmap.
  ///
  virtual void Set(RefPtr<Bitmap> bitmap) = 0;

  ///
  /// Draw another bitmap to this bitmap.
  ///
  /// @note  Formats do not need to match. Bitmap formats will be converted to one another
  ///        automatically.
  ///
  /// @param  src_rect    The source rectangle, relative to src bitmap.
  ///
  /// @param  dest_rect   The destination rectangle, relative to this bitmap.
  ///
  /// @param  src         The source bitmap.
  ///
  /// @param  pad_repeat  Whether or not we should pad the drawn bitmap by one pixel of repeated
  ///                     edge pixels from the source bitmap.
  ///
  /// @return  Returns whether or not the operation succeeded (this can fail if the src_rect and/or
  ///          dest_rect are invalid).
  ///
  virtual bool DrawBitmap(IntRect src_rect, IntRect dest_rect, RefPtr<Bitmap> src, bool pad_repeat)
      = 0;

  ///
  /// Encode this Bitmap as a PNG image and store the encoded bytes in a Buffer.
  ///
  /// @param  convert_to_rgba  The PNG format expects RGBA format but our bitmap is stored as BGRA,
  ///                          set this to true to perform the conversion automatically.
  ///
  /// @param  convert_to_straight_alpha  The PNG format expects semi-transparent values to be
  ///                                    stored as straight alpha instead of premultiplied alpha,
  ///                                    set this to true to perform the conversion automatically.
  ///
  /// @return  Returns a buffer containing the encoded bytes, or a null ref-pointer on failure.
  ///
  virtual RefPtr<Buffer> EncodePNG(bool convert_to_rgba = true,
                                   bool convert_to_straight_alpha = true) const = 0;

  ///
  /// Write this Bitmap out to a PNG image.
  ///
  /// @param  path  The filepath to write to (opened with fopen())
  ///
  /// @param  convert_to_rgba  The PNG format expects RGBA format but our bitmap is stored as BGRA,
  ///                          set this to true to perform the conversion automatically.
  ///
  /// @param  convert_to_straight_alpha  The PNG format expects semi-transparent values to be
  ///                                    stored as straight alpha instead of premultiplied alpha,
  ///                                    set this to true to perform the conversion automatically.
  ///
  /// @return  Returns whether or not the operation succeeded.
  ///
  virtual bool WritePNG(const char* path, bool convert_to_rgba = true,
                        bool convert_to_straight_alpha = true) const = 0;

  ///
  /// Make a resized copy of this bitmap by writing to a pre-allocated destination bitmap.
  ///
  /// @param  destination  The bitmap to store the result in, the width and height of the
  ///                      destination will be used.
  ///
  /// @param  high_quality  Whether or not a high quality resampling will be used during the
  ///                       resize. (Otherwise, just uses fast nearest-neighbor sampling)
  ///
  /// @return  Returns whether or not the operation succeeded. This operation is only valid if both
  ///          formats are BitmapFormat::BGRA8_UNORM_SRGB and the source and destination are
  ///          non-empty.
  ///
  virtual bool Resample(RefPtr<Bitmap> destination, bool high_quality) = 0;

  ///
  /// Convert a BGRA bitmap to RGBA bitmap and vice-versa by swapping the red and blue channels.
  ///
  /// @note  Only valid if the format is BitmapFormat::BGRA8_UNORM_SRGB.
  ///
  virtual void SwapRedBlueChannels() = 0;

  ///
  /// Convert a BGRA bitmap from premultiplied alpha (the default) to straight alpha.
  ///
  /// @note  Only valid if the format is BitmapFormat::BGRA8_UNORM_SRGB.
  ///
  virtual void ConvertToStraightAlpha() = 0;

  ///
  /// Convert a BGRA bitmap from straight alpha to premultiplied alpha.
  ///
  /// @note  Only valid if the format is BitmapFormat::BGRA8_UNORM_SRGB.
  ///
  virtual void ConvertToPremultipliedAlpha() = 0;

  ///
  /// Whether or not this Bitmap uses a block-compressed (BCn) format.
  ///
  /// @note  Compressed bitmaps store 4x4 pixel blocks and don't support per-pixel operations.
  ///        Erase(), DrawBitmap(), Resample(), and the channel/alpha conversions are no-ops or
  ///        fail on a compressed Bitmap.
  ///
  virtual bool is_compressed() const = 0;

 protected:
  Bitmap();
  virtual ~Bitmap();
  Bitmap(const Bitmap&);
  void operator=(const Bitmap&);
};

///
/// Managed container that keeps a pixel buffer locked for the duration of its lifetime.
///
/// You obtain an instance via Bitmap::LockPixelsSafe() or Surface::LockPixelsSafe(). The pixel
/// buffer is automatically unlocked when this object goes out of scope. This class can be moved
/// but not copied.
///
/// The template parameter `T` is the lockable type, `RefPtr<Bitmap>` or `Surface*`.
///
template <typename T>
class LockedPixels {
 public:
  LockedPixels(const LockedPixels&) = delete;
  LockedPixels& operator=(const LockedPixels&) = delete; 
  LockedPixels(int) = delete;
  explicit LockedPixels(T& lockable) : lockable_(lockable), data_(nullptr), size_(0) { lock(); }

  ~LockedPixels() {
    if (lockable_)
      lockable_->UnlockPixels();
  }

  ///
  /// Access the locked pixel data.
  /// 
  void* data() { return data_; }

  ///
  /// Access the size of the locked pixel data.
  /// 
  size_t size() { return size_; }

  explicit operator bool() const { return !!lockable_; }

  LockedPixels(LockedPixels&& other) : lockable_(other.lockable_), data_(other.data_), 
      size_(other.size_) {
    other.lockable_ = nullptr;
    other.data_ = nullptr;  
    other.size_ = 0;
  }

  LockedPixels& operator=(LockedPixels&& other) {
    if (lockable_)
      lockable_->UnlockPixels();
    lockable_ = other.lockable_;
    data_ = other.data_;
    size_ = other.size_;
    other.lockable_ = nullptr;
    other.data_ = nullptr;
    other.size_ = 0;
    return *this;
  }

 private:
  void lock() {
    if (lockable_) {
      data_ = lockable_->LockPixels();
      size_ = lockable_->size();
    }
  }

  T lockable_;
  void* data_;
  size_t size_;
};


} // namespace ultralight
