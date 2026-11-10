///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/Buffer.h>
#include <Ultralight/Defines.h>
#include <Ultralight/RefPtr.h>
#include <Ultralight/String.h>
#include <cstddef>

namespace ultralight {

///
/// A single clipboard value: empty, UTF-8 text, or raw bytes.
///
/// This is the variant value type of a ClipboardData entry. It converts between strings and
/// buffers so you can pass either directly to ClipboardData::Set().
///
class UExport ClipboardValue {
 public:
  ///
  /// Create an empty value.
  ///
  ClipboardValue();

  ///
  /// Create an empty value (lets you pass `nullptr` where a value is expected).
  ///
  ClipboardValue(std::nullptr_t);

  ///
  /// Create a text value.
  ///
  ClipboardValue(const String& text);

  ///
  /// Create a text value from a null-terminated UTF-8 string.
  ///
  ClipboardValue(const char* text);

  ///
  /// Create a bytes value (a null buffer creates an empty value).
  ///
  ClipboardValue(RefPtr<Buffer> bytes);

  ///
  /// Copy constructor.
  ///
  ClipboardValue(const ClipboardValue& other);

  ///
  /// Copy assignment.
  ///
  ClipboardValue& operator=(const ClipboardValue& other);

  ///
  /// Move constructor (`other` becomes empty).
  ///
  ClipboardValue(ClipboardValue&& other) noexcept;

  ///
  /// Move assignment (`other` becomes empty).
  ///
  ClipboardValue& operator=(ClipboardValue&& other) noexcept;

  ///
  /// Destructor.
  ///
  ~ClipboardValue();

  ///
  /// Whether or not this value is empty (neither text nor bytes).
  ///
  [[nodiscard]] bool is_empty() const;

  ///
  /// Whether or not this value holds text.
  ///
  [[nodiscard]] bool is_text() const;

  ///
  /// Whether or not this value holds raw bytes.
  ///
  [[nodiscard]] bool is_bytes() const;

  ///
  /// Get the text (an empty string when this value doesn't hold text).
  ///
  [[nodiscard]] const String& text() const;

  ///
  /// Get the bytes (a null pointer when this value doesn't hold bytes).
  ///
  [[nodiscard]] RefPtr<Buffer> bytes() const;

 private:
  enum class Kind : uint8_t { Empty, Text, Bytes };
  Kind kind_;
  String text_;
  RefPtr<Buffer> bytes_;
};

///
/// Clipboard contents, stored as multiple type representations of the same data.
///
/// Clipboard data is stored in multiple formats, keyed by MIME type (eg, `text/plain`, `text/html`)
/// or an application-defined type string.
///
/// ## Building a Payload
///
/// Use ClipboardData::Create() to create a payload, then call Set() to add entries.
/// Set() is chainable, so you can build a multi-format payload in one expression:
///
/// ```
///  // Create a payload with a single text/plain entry.
///  auto data = ClipboardData::Create("hello world");
///
///  // Add a text/html entry to the same payload.
///  data->Set("text/html", "<b>hello world</b>");
/// ```
///
/// Entry order is preserved: when you build a payload yourself you should add the richest
/// format first (the order the OS clipboard will advertise them in).
///
/// @note  Use bare MIME types (`text/plain`, not `text/plain;charset=utf-8`)-- the library
///        normalizes charset-suffixes at its own boundaries.
///
class UExport ClipboardData : public RefCounted {
 public:
  ///
  /// Create an empty payload.
  ///
  /// @return  Returns a ref-pointer to a new ClipboardData instance.
  ///
  static RefPtr<ClipboardData> Create();

  ///
  /// Create a payload holding a single `text/plain` entry.
  ///
  /// @return  Returns a ref-pointer to a new ClipboardData instance.
  ///
  static RefPtr<ClipboardData> Create(const String& text);

  ///
  /// Set the value for a type (replaces any existing entry with the same type).
  ///
  /// @param  type   The entry's type (a bare MIME type or an application-defined string).
  ///
  /// @param  value  The value to store (text or bytes; converts implicitly from String,
  ///                C-string, and RefPtr<Buffer>).
  ///
  /// @return  Returns this ClipboardData so you can chain further calls.
  ///
  virtual ClipboardData& Set(const String& type, const ClipboardValue& value) = 0;

  ///
  /// Remove the entry for a type (does nothing if no such entry exists).
  ///
  /// @return  Returns this ClipboardData so you can chain further calls.
  ///
  virtual ClipboardData& Remove(const String& type) = 0;

  ///
  /// Whether or not an entry exists for a type.
  ///
  [[nodiscard]] virtual bool Has(const String& type) const = 0;

  ///
  /// Get the value for a type (an empty value when no such entry exists).
  ///
  [[nodiscard]] virtual ClipboardValue Get(const String& type) const = 0;

  ///
  /// Get the `text/plain` entry's text (an empty string when there is none, or when that entry
  /// holds bytes).
  ///
  [[nodiscard]] virtual String AsText() const = 0;

  ///
  /// Get the `text/html` entry's text (an empty string when there is none, or when that entry
  /// holds bytes).
  ///
  [[nodiscard]] virtual String AsHTML() const = 0;

  ///
  /// Get the number of entries.
  ///
  [[nodiscard]] virtual size_t size() const = 0;

  ///
  /// Get the type of the entry at a certain index, in insertion order (an empty string when
  /// `index` is out of range).
  ///
  [[nodiscard]] virtual String type_at(size_t index) const = 0;

  ///
  /// Get the URL of the document this payload came from (can be empty).
  ///
  [[nodiscard]] virtual String source_url() const = 0;

  ///
  /// Set the URL of the document this payload came from.
  ///
  /// The library sets this when copying page content; on Windows it becomes the HTML clipboard
  /// format's SourceURL.
  ///
  virtual void set_source_url(const String& url) = 0;

 protected:
  ClipboardData();
  virtual ~ClipboardData();
  ClipboardData(const ClipboardData&);
  void operator=(const ClipboardData&);
};

} // namespace ultralight
