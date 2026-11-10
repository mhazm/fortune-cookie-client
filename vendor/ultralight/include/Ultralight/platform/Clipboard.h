///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/ClipboardData.h>
#include <Ultralight/Defines.h>

namespace ultralight {

///
/// User-defined clipboard interface.
///
/// The library uses this to read and write data to the system's clipboard, as whole
/// multi-format payloads (see ClipboardData).
///
/// A platform-specific implementation of Clipboard that cuts, copies, and pastes to the OS
/// clipboard is provided for you when you call App::Create().
///
/// If you are using Renderer::Create() instead of App::Create(), you will need to provide your own
/// implementation of this. @see Platform::set_clipboard().
///
/// ## Types the Library Uses
///
/// The library currently reads and writes these well-known types: `text/plain`, `text/html`,
/// and `text/uri-list`. Your implementation should map each type it supports to the matching OS
/// clipboard format and simply skip types it doesn't recognize (a backend that only supports
/// text is fine-- write the `text/plain` entry and ignore the rest).
///
/// @note  The library only calls these methods on the thread you created the Renderer or App on,
///        so your implementation doesn't need to be safe to call from more than one thread.
///
class UExport Clipboard {
 public:
  virtual ~Clipboard();

  ///
  /// Clear the clipboard.
  ///
  /// The library doesn't call this today (a copy replaces the previous contents through
  /// Write()); it exists so applications sharing your Clipboard instance can clear the OS
  /// clipboard through the same interface.
  ///
  virtual void Clear() = 0;

  ///
  /// Read the clipboard's current contents.
  ///
  /// This is called when the library wants to paste from the OS clipboard. You should return one
  /// entry per format you can read (at minimum the `text/plain` entry, if the clipboard holds
  /// text).
  ///
  /// @return  Returns the clipboard contents. Returning a null pointer is equivalent to
  ///          returning an empty payload (the library treats both as an empty clipboard).
  ///
  virtual RefPtr<ClipboardData> Read() = 0;

  ///
  /// Write a payload to the clipboard, replacing its previous contents.
  ///
  /// This is called when the library wants to copy to the OS clipboard. All of the payload's
  /// entries should be written in one atomic clipboard transaction (on Windows: one
  /// OpenClipboard / EmptyClipboard / SetClipboardData per entry / CloseClipboard cycle), so a
  /// multi-format copy can never partially replace an earlier write.
  ///
  /// @param  data  The payload to write (never null; may hold types you don't support, which you
  ///               should skip).
  ///
  virtual void Write(RefPtr<ClipboardData> data) = 0;
};

} // namespace ultralight
