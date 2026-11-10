///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/Defines.h>
#include <Ultralight/RefPtr.h>
#include <Ultralight/Bitmap.h>
#include <Ultralight/Geometry.h>

namespace ultralight {

///
/// User-defined pixel buffer surface. 
///
/// The library uses this to store pixel data when rendering Views on the CPU (see
/// ViewConfig::is_accelerated).
///
/// You can provide the library with your own Surface implementation to reduce the latency of
/// displaying pixels in your application (Views will be drawn directly to a block of memory
/// controlled by you).
///
/// When a View is rendered on the CPU, you can retrieve the backing Surface via View::surface().
///
/// @pre  This is automatically managed for you when using App::Create(), if you want to override
///       Surface or SurfaceFactory, you'll need to use Renderer::Create() instead.
///
/// ## Default Implementation
///
/// A default Surface implementation, BitmapSurface, is automatically provided by the library when
/// you call Renderer::Create() without defining a custom SurfaceFactory.
///
/// You should cast the Surface to a BitmapSurface to access the underlying Bitmap.
///
/// ## Setting the Surface Implementation
///
/// To define your own implementation, you should inherit from this class, handle the virtual
/// member functions, and then define a custom SurfaceFactory that creates/destroys an
/// instance of your class.
///
/// After that, you should pass an instance of your custom SurfaceFactory class to
/// Platform::set_surface_factory() before calling Renderer::Create().
///
class UExport Surface {
 public:
  virtual ~Surface();

  ///
  /// Width (in pixels).
  ///
  virtual uint32_t width() const = 0;

  ///
  /// Height (in pixels).
  ///
  virtual uint32_t height() const = 0;

  ///
  /// Number of bytes between rows (usually width * 4)
  ///
  virtual uint32_t row_bytes() const = 0;

  ///
  /// Size in bytes.
  ///
  virtual size_t size() const = 0;

  ///
  /// Lock the pixel buffer and get a pointer to the beginning of the data for reading/writing.
  ///
  /// @note  Native pixel format is premultiplied BGRA 32-bit (8 bits per channel).
  ///
  /// @return  Returns a pointer to the beginning of the locked pixel buffer. The buffer remains
  ///          valid until UnlockPixels() is called.
  ///
  virtual void* LockPixels() = 0;

  ///
  /// Unlock the pixel buffer.
  ///
  virtual void UnlockPixels() = 0;

  ///
  /// Lock the pixel buffer for reading/writing (safe version, automatically unlocks).
  ///
  /// @return  Returns a managed container that can be used to access the pixel buffer
  ///          (LockedPixels::data()). This container automatically unlocks the pixels when it
  ///          goes out of scope.
  ///
  LockedPixels<Surface*> LockPixelsSafe() {
    Surface* self = this;
    return LockedPixels<Surface*>(self);
  }

  ///
  /// Resize the pixel buffer to a certain width and height (both in pixels).
  ///
  /// This should never be called while pixels are locked.
  ///
  virtual void Resize(uint32_t width, uint32_t height) = 0;

  ///
  /// Shift a rectangle of pixels by an offset.
  ///
  /// The library calls this during Renderer::Render() when a page scrolls, moving already painted
  /// pixels instead of repainting them. Afterward, the library repaints only the exposed strip.
  ///
  /// The Surface pixel buffer is the source the library paints into. The destination is wherever
  /// you copy those pixels to display them (eg, a GPU texture or a window). Scroll() always
  /// shifts the source.
  ///
  /// Most implementations do this with one call to ShiftPixels().
  ///
  /// @param  rect  The rectangle to shift (in pixels). This always lies inside the surface,
  ///               and the shift is smaller than the rectangle on each axis.
  ///
  /// @param  dx    The horizontal shift, in pixels (positive moves pixels to the right).
  ///
  /// @param  dy    The vertical shift, in pixels (positive moves pixels down).
  ///
  /// @return  Return false in most cases. This means the destination was not shifted. The
  ///          library then adds `rect` to dirty_bounds() so you copy the moved pixels from the
  ///          source to the destination again. Return true only in the rare case where you
  ///          shifted the destination by the same offset yourself (eg, you scrolled your
  ///          window or copied within your GPU texture)-- the library then reports only the
  ///          newly exposed strip as dirty.
  ///
  /// \parblock
  /// @note  This is called while pixels are locked (between LockPixels() and UnlockPixels()).
  ///        Shift through the pointer you returned from LockPixels()-- do not lock the buffer
  ///        again.
  /// \endparblock
  ///
  /// \parblock
  /// @note  Rows may be padded. Step rows by row_bytes(), never by width * 4.
  /// \endparblock
  ///
  virtual bool Scroll(const IntRect& rect, int dx, int dy) = 0;

  ///
  /// Shift a rectangle of 32-bit pixels in place.
  ///
  /// Use this from Scroll() (and anywhere else you hold a locked pixel buffer) to move the
  /// pixels of `rect` by `dx` and `dy`; overlapping rows and columns are handled for you. The
  /// strip the shift exposes keeps its old contents.
  ///
  /// @param  pixels     Pointer to the first row of the buffer (as returned by LockPixels()).
  ///
  /// @param  row_bytes  Number of bytes between rows.
  ///
  /// @param  rect       The rectangle to shift (in pixels, inside the buffer).
  ///
  /// @param  dx         The horizontal shift, in pixels (positive moves pixels to the right).
  ///
  /// @param  dy         The vertical shift, in pixels (positive moves pixels down).
  ///
  static void ShiftPixels(void* pixels, uint32_t row_bytes, const IntRect& rect, int dx, int dy);

  ///
  /// Add a rectangle to the dirty area.
  ///
  /// The library calls this after it paints an area of the pixel buffer. The rectangle is clipped
  /// to the surface and kept as one of up to kMaxDirtyRects dirty rectangles (nearby ones are
  /// merged), and dirty_bounds() grows to their union, until you call ClearDirtyBounds(). An
  /// empty or out-of-bounds rectangle changes nothing.
  ///
  virtual void set_dirty_bounds(const IntRect& bounds);

  ///
  /// Get the dirty bounds.
  ///
  /// This value can be used to determine which portion of the pixel buffer has been updated since
  /// the last call to ClearDirtyBounds(). It always lies inside the surface, and it is empty when
  /// nothing has been painted since the last clear. It is the union of the rectangles
  /// dirty_rect() lists.
  ///
  /// @note  The library clears the bounds itself when it resizes the surface, then paints the
  ///        whole buffer, so a resize never leaves you a stale rectangle from the old size.
  ///
  /// The general algorithm to determine if a Surface needs display is:
  /// ```
  ///   if (!surface.dirty_bounds().IsEmpty()) {
  ///       // Surface pixels are dirty and needs display.
  ///       // Cast Surface to native Surface and use it here (pseudo code)
  ///       DisplaySurface(surface);
  ///
  ///       // Once you're done, clear the dirty bounds:
  ///       surface.ClearDirtyBounds();
  ///  }
  /// ```
  ///
  virtual IntRect dirty_bounds() const;

  ///
  /// Clear the dirty bounds.
  ///
  /// You should call this after you're done displaying the Surface.
  ///
  virtual void ClearDirtyBounds();

  ///
  /// The most dirty rectangles the library keeps apart before merging them.
  ///
  static constexpr uint32_t kMaxDirtyRects = 8;

  ///
  /// Get the number of dirty rectangles (0 when nothing has been painted since the last clear).
  ///
  uint32_t dirty_rect_count() const;

  ///
  /// Get a dirty rectangle by index.
  ///
  /// Each rectangle lies inside dirty_bounds() and the surface. dirty_bounds() is the union of
  /// these; copy each one instead of the union to move fewer pixels when the page changed in
  /// several places (eg, a scrolled page with a fixed header). If you only ever copy
  /// dirty_bounds(), you can ignore this list.
  ///
  /// @param  index  A value below dirty_rect_count().
  ///
  /// @return  Returns the rectangle, or an empty rectangle for an index out of range.
  ///
  IntRect dirty_rect(uint32_t index) const;

 protected:
  Surface();

  IntRect dirty_bounds_;
  IntRect dirty_rects_[kMaxDirtyRects];
  uint32_t dirty_rect_count_ = 0;
};

///
/// User-defined factory to provide your own surface implementation.
///
/// The library uses this to create/destroy Surface instances when rendering Views on the CPU.
///
/// @pre  This is automatically managed for you when using App::Create(), if you want to override
///       Surface or SurfaceFactory, you'll need to use Renderer::Create() instead.
///
/// ## Setting the Surface Factory
///
/// The default factory creates/destroys a BitmapSurface but you can override this by providing your
/// own factory to Platform::set_surface_factory().
///
class UExport SurfaceFactory {
 public:
  virtual ~SurfaceFactory();

  ///
  /// Create a native Surface with a certain width and height (in pixels).
  ///
  virtual Surface* CreateSurface(uint32_t width, uint32_t height) = 0;

  ///
  /// Destroy a native Surface previously created by CreateSurface().
  ///
  virtual void DestroySurface(Surface* surface) = 0;
};

///
/// The default surface implementation, backed by a bitmap.
///
/// This is automatically provided by the library when you call Renderer::Create() without defining
/// a custom SurfaceFactory.
///
/// This implementation uses a Bitmap to store pixel data (retrieve it via BitmapSurface::bitmap()).
///
class UExport BitmapSurface : public Surface {
 public:
  virtual uint32_t width() const override;

  virtual uint32_t height() const override;

  virtual uint32_t row_bytes() const override;

  virtual size_t size() const override;

  virtual void* LockPixels() override;

  virtual void UnlockPixels() override;

  virtual void Resize(uint32_t width, uint32_t height) override;

  virtual bool Scroll(const IntRect& rect, int dx, int dy) override;

  ///
  /// Get the underlying Bitmap.
  ///
  RefPtr<Bitmap> bitmap();

 protected:
  BitmapSurface(uint32_t width, uint32_t height);
  virtual ~BitmapSurface();
  BitmapSurface(const BitmapSurface&) = delete;
  void operator=(const BitmapSurface&) = delete;
  friend class BitmapSurfaceFactory;

  void* impl_;
};

///
/// Get the default Bitmap Surface Factory singleton. (Do not destroy this, this singleton is owned
/// by the library).
///
UExport SurfaceFactory* GetBitmapSurfaceFactory();

} // namespace ultralight
