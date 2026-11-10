///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/Defines.h>
#include <Ultralight/String.h>
#include <Ultralight/Buffer.h>

namespace ultralight {

///
/// Represents a font file, either on-disk path or in-memory file contents.
///
class UExport FontFile : public RefCounted {
 public:
  ///
  /// Create a font file from an on-disk file path.
  ///
  /// @param  filepath    The path to the font file on disk. The file should already exist.
  ///
  /// @param  face_index  The index of the face to use when the file is a font collection (eg, a
  ///                     `.ttc` holding several families). Pass -1 to let the library choose the
  ///                     face that best matches the requested weight and style.
  ///
  /// @return  Returns a ref-pointer to a new FontFile instance.
  ///
  static RefPtr<FontFile> Create(const String& filepath, int face_index = -1);

  ///
  /// Create a font file from an in-memory buffer.
  ///
  /// @param  buffer      The font file data (TTF/OTF, or a collection).
  ///
  /// @param  face_index  The index of the face to use when the data is a font collection. Pass
  ///                     -1 to let the library choose the face that best matches the requested
  ///                     weight and style.
  ///
  /// @return  Returns a ref-pointer to a new FontFile instance.
  ///
  static RefPtr<FontFile> Create(RefPtr<Buffer> buffer, int face_index = -1);

  ///
  /// Whether or not this font file was created from an in-memory buffer.
  ///
  virtual bool is_in_memory() const = 0;

  ///
  /// The file path (if any).
  ///
  virtual String filepath() const = 0;

  ///
  /// The in-memory buffer (if any).
  ///
  virtual RefPtr<Buffer> buffer() const = 0;

  ///
  /// The index of the face within a font collection, or -1 when the library chooses the face.
  ///
  virtual int face_index() const = 0;

  ///
  /// Unique hash (if this is a filepath, only the path string is hashed).
  ///
  virtual uint32_t hash() const = 0;

 protected:
  FontFile();
  virtual ~FontFile();
  FontFile(const FontFile&);
  void operator=(const FontFile&);
};

///
/// User-defined font loader interface.
/// 
/// The library uses this to load a font file (eg, `Arial.ttf`) for a given font description (eg, 
/// `font-family: Arial;`).
///
/// Every OS has its own library of installed system fonts. The FontLoader interface is used to
/// lookup these fonts and fetch the actual font data (raw TTF/OTF file data) for a given font
/// description.
///
/// You can provide the library with your own font loader implementation so that you can bundle
/// fonts with your application rather than relying on the system's installed fonts.
/// 
/// ## Default Implementation
///
/// A platform-specific implementation of FontLoader is provided for you when you call
/// App::Create().
///
/// If you are using Renderer::Create(), you **must** provide your own. You can still use the
/// default implementation however-- see the helper functions defined in <AppCore/Platform.h>.
///
/// ## Threading
///
/// The library calls these methods from the thread you created the Renderer on and from the
/// threads that run Web Workers. Those calls can overlap, so your implementation **must** be safe
/// to call from more than one thread at once.
///
/// ## Setting the Font Loader
///
/// To provide your own custom FontLoader implementation, you should inherit from this class,
/// handle the virtual member functions, and then pass an instance of your class to
/// Platform::set_font_loader() before calling Renderer::Create() or App::Create().
///
class UExport FontLoader {
 public:
  virtual ~FontLoader();

  ///
  /// Fallback font family name. Will be used if all other fonts fail to load.
  ///
  /// @note  This font should be guaranteed to exist (eg, FontLoader::Load won't fail when passed
  ///        this font family name).
  ///
  virtual String fallback_font() const = 0;

  ///
  /// Fallback font family name that can render the specified characters. Mainly used to support
  /// CJK (Chinese, Japanese, Korean) text display.
  ///
  /// @param  characters  One or more UTF-16 characters. This is almost always a single character.
  ///
  /// @param  weight      The CSS numeric font weight, from 100 (thin) to 900 (black), rounded to
  ///                     the nearest multiple of 100. Normal is 400, bold is 700.
  ///
  /// @param  italic      Whether or not italic is requested.
  ///
  /// @return  Returns a font family name that can render the text.
  ///
  virtual String fallback_font_for_characters(const String& characters, int weight,
                                              bool italic) const = 0;

  ///
  /// Get the actual font file data (TTF/OTF) for a given font description.
  ///
  /// @param  family  Font family name.
  ///
  /// @param  weight  The CSS numeric font weight, from 100 (thin) to 900 (black), rounded to the
  ///                 nearest multiple of 100. Normal is 400, bold is 700.
  ///
  /// @param  italic  Whether or not italic is requested.
  ///
  /// @return  A font file matching the given description (either an on-disk font filepath or an
  ///          in-memory file contents). You can return NULL here and the library will fallback to
  ///          another font.
  ///
  /// @note  Return the closest face you have. When the face you return isn't already bold for a
  ///        weight of 600 or heavier, or isn't already slanted for an italic request, the library
  ///        synthesizes the difference by emboldening or slanting the outlines. A page can turn
  ///        that off with the CSS `font-synthesis` property.
  ///
  virtual RefPtr<FontFile> Load(const String& family, int weight, bool italic) = 0;
};

} // namespace ultralight
