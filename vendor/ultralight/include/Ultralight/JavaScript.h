///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <JavaScriptCore/JavaScript.h>
#include <Ultralight/Defines.h>
#include <Ultralight/RefPtr.h>
#include <Ultralight/String.h>

// The typed JavaScript layer (ultralight::js) is opt-in: include <Ultralight/JS.h> (the
// whole bridge) or individual <Ultralight/js/*.h> headers directly. The js/ headers carry
// the ULJS C API type surface, so they are deliberately not included here.

namespace ultralight {

///
/// This class wraps a JSContextRef (a JavaScript execution context for use with JavaScriptCore)
/// and locks the context on the current thread for the duration of its lifetime.
///
class UExport JSContext : public RefCounted {
public:
  /// Get the underlying JSContextRef for use with JavaScriptCore C API
  virtual JSContextRef ctx() = 0;

  /// Typecast to a JSContextRef for use with JavaScriptCore C API
  operator JSContextRef();

protected:
  virtual ~JSContext();
};

}  // namespace ultralight
