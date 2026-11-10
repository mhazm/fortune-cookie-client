///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///

///
/// @namespace ultralight::js
///
/// Type-checked JavaScript bridge between C++ and web pages.
///
/// `#include <Ultralight/JS.h>`
///
/// @note  This API is a preview and may still change after 2.0.
///
/// The JavaScript API provides two-way communication between your C++ application and the web pages
/// loaded into a View. You can bind functions and properties under a global object that page
/// scripts call like any Web API, or reach into a page to run scripts and call JavaScript functions
/// directly.
///
/// Conversions between C++ types and JavaScript values happen automatically. The library checks
/// types at compile time with static assertions and at runtime during every JavaScript call.
///
/// Register a bound function on an API attached to a View, then call into the page once the
/// document loads:
///
/// ```
/// js::API game_api("app");  // keep the API alive as long as pages use it
///
/// void SetupUI(View* view) {
///   game_api["add"] = [](double a, double b) { return a + b; };  // app.add(2, 3)
///   if (game_api.AttachTo(view))
///     view->LoadURL("file:///app.html");
/// }
///
/// void MyApp::OnDOMReady(View* caller, uint64_t frame_id, bool is_main_frame,
///                        const String& url) {
///   if (!is_main_frame)
///     return;
///
///   js::Context ctx(caller);
///   ctx["ShowMessage"]("Howdy!");  // calls the page's ShowMessage()
/// }
/// ```
///
/// ## Where to Start
///
/// Common tasks begin with these types:
///
/// | Task                                    | Type                   |
/// |-----------------------------------------|------------------------|
/// | Bind functions and properties           | js::API                |
/// | Return Promises from async functions    | js::Task, js::Resolver |
/// | Call page functions or evaluate scripts | js::Context            |
/// | Inspect and convert script values       | js::Value              |
/// | Work directly with raw JavaScriptCore   | View::LockJSContext()  |
///
/// ## Core Rules
///
/// - **Call the API on the Renderer's thread.** API::Emit() works from any thread, and copying,
///   moving, or destroying a handle is safe from any thread too.
/// - **A handle never keeps its page alive.** When a page navigates away or its View is destroyed,
///   the handle's page is gone and operations fail safely (see js::Value for the Valid, Empty, and
///   Gone states).
/// - **The API never throws C++ exceptions.** A call that can fail returns a js::Result, which
///   holds either the value or a js::Error.
///
/// \parblock
/// @note  The bridge requires C++20, and every file that uses the API must use the same C++
///        exception setting.
/// \endparblock
///
/// \parblock
/// @note  DOM elements cross the bridge only with `<Ultralight/dom/JSInterop.h>`, which this header
///        doesn't include.
/// \endparblock
///
/// @see  js::API, js::Context, js::Value, View::LockJSContext()
///
#pragma once
#include <Ultralight/Color.h>
#include <Ultralight/String.h>
#include <Ultralight/URL.h>
#include <Ultralight/js/API.h>
#include <Ultralight/js/Buffer.h>
#include <Ultralight/js/Class.h>
#include <Ultralight/js/Context.h>
#include <Ultralight/js/Task.h>
#include <Ultralight/js/WeakValue.h>

#include <string>
#include <utility>

namespace ultralight {
namespace js {

///
/// ultralight::String as a JavaScript string in UTF-8, the same as std::string.
///
/// @note  A null character at the very end of a JavaScript string is dropped when it converts
///        to a String (null characters elsewhere are kept). Use std::string if you need it.
///
template <>
struct TypeTraits<String> {
  static bool FromJS(ULJSContext ctx, ULJSValue borrowed, String* out, ULJSValue* exception) {
    std::string text;
    if (!TypeTraits<std::string>::FromJS(ctx, borrowed, &text, exception))
      return false;
    *out = String(text.data(), text.size());
    return true;
  }
  static ULJSValue ToJS(ULJSContext ctx, const String& value) {
    const String8& utf8 = value.utf8();
    return ulCreateJSValueStringUTF8(ctx, utf8.data(), utf8.length());
  }
  static constexpr const char* SchemaType() { return "string"; }
  static bool AcceptsJSType(Type type, bool, bool) { return type == Type::String; }
};

///
/// URL as its canonical string (see URL::str()).
///
/// From JavaScript, the string must parse as an absolute URL. Anything else (a relative URL or
/// an empty string) fails with `expected url`. Declare std::optional<URL> to accept `undefined`
/// or `null` as no URL.
///
/// An invalid URL converts to JavaScript as `null`.
///
template <>
struct TypeTraits<URL> {
  static bool FromJS(ULJSContext ctx, ULJSValue borrowed, URL* out, ULJSValue* exception) {
    String text;
    if (!TypeTraits<String>::FromJS(ctx, borrowed, &text, exception))
      return false;
    URL parsed(text);
    if (!parsed.is_valid())
      return false;
    *out = std::move(parsed);
    return true;
  }
  static ULJSValue ToJS(ULJSContext ctx, const URL& value) {
    if (!value.is_valid())
      return ulCreateJSValueNull(ctx);
    return TypeTraits<String>::ToJS(ctx, value.str());
  }
  static constexpr const char* SchemaType() { return "url"; }

  // SchemaType() is the TypeError noun; the marshaled value is a plain string, or null for an
  // invalid URL, so the schema expression (and the generated .d.ts) says so.
  static void WriteSchemaExpr(std::string& out) { out += "{\"union\":[\"string\",\"null\"]}"; }
  static bool AcceptsJSType(Type type, bool, bool) { return type == Type::String; }
};

///
/// Color as CSS color text.
///
/// From JavaScript, the text goes through Color::Parse(): named colors, hex, `transparent`, and
/// the CSS color functions such as rgb(), hsl(), hwb(), lab(), oklch(), and color().
/// Wide-gamut colors narrow to sRGB. Text that doesn't parse fails with `expected color`, and so
/// do `currentcolor` and the system color keywords.
///
/// To JavaScript, a Color converts as lowercase hex from Color::ToHexString() (`#rrggbb`, or
/// `#rrggbbaa` when it isn't fully opaque). An unset or invalid Color converts as `null`.
///
template <>
struct TypeTraits<Color> {
  static bool FromJS(ULJSContext ctx, ULJSValue borrowed, Color* out, ULJSValue* exception) {
    String text;
    if (!TypeTraits<String>::FromJS(ctx, borrowed, &text, exception))
      return false;
    Color parsed = Color::Parse(text);
    if (parsed.invalid())
      return false;
    *out = parsed;
    return true;
  }
  static ULJSValue ToJS(ULJSContext ctx, const Color& value) {
    if (!value)
      return ulCreateJSValueNull(ctx);
    return TypeTraits<String>::ToJS(ctx, value.ToHexString());
  }
  static constexpr const char* SchemaType() { return "color"; }

  // The marshaled value is CSS color text, or null for an unset or invalid Color.
  static void WriteSchemaExpr(std::string& out) { out += "{\"union\":[\"color\",\"null\"]}"; }
  static bool AcceptsJSType(Type type, bool, bool) { return type == Type::String; }
};

}  // namespace js
}  // namespace ultralight
