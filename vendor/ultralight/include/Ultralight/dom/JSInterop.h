///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// @file JSInterop.h
///
/// Conversion functions and type traits connecting the DOM and JavaScript APIs.
///
/// `#include <Ultralight/dom/JSInterop.h>`
///
/// @note  This API is a preview and may still change after 2.0.
///
/// This header connects the DOM and JavaScript APIs so you can pass DOM elements between C++ and
/// page script.
///
/// Convert elements between C++ and page script using the document's context:
///
/// ```
/// js::Context ctx = dom::GetJSContext(document);
/// dom::Element hud = document.querySelector("#hud");
///
/// // Give page script an element:
/// ctx.GlobalObject()["hud"] = dom::ToJS(ctx, hud);
///
/// // Get the element a script returns:
/// dom::Element focused = dom::FromJS(
///     ctx.Evaluate("document.activeElement").value_or(js::Value()));
/// ```
///
/// ## Elements in Bound Functions
///
/// Including this header specializes js::TypeTraits for dom::Element, letting bound functions
/// accept and return elements directly. Passing `null` or `undefined` from JavaScript to a
/// dom::Element parameter fails with a TypeError, so declare `std::optional<dom::Element>` to
/// accept them.
///
/// Bind functions that take an element and an optional element:
///
/// ```
/// js::API api("app");
/// api["highlight"] = [](dom::Element el) { el.classList.add("highlight"); };
/// api["clear"] = [](std::optional<dom::Element> el) {
///   if (el)
///     el->classList.remove("highlight");
/// };
/// ```
///
/// ## Frame Contexts
///
/// An element converts to JavaScript only with the context of its own frame. Converting an element
/// with a context from another frame or View returns an empty js::Value.
///
/// How you retrieve a context depends on the document's frame:
///
/// - **Main frame documents work with either `js::Context(view)` or dom::GetJSContext().**
/// - **Subframe documents require dom::GetJSContext().** Passing a View to `js::Context(view)`
///   wraps only the main frame.
///
/// \parblock
/// @note  When JavaScript is disabled or a frame is sandboxed, dom::GetJSContext() returns an empty
///        js::Context.
/// \endparblock
///
/// \parblock
/// @note  This header is an opt-in include that isn't part of `<Ultralight/DOM.h>` or
///        `<Ultralight/JS.h>`. When native code converts an element without including it, the
///        compile error directs you to this file.
/// \endparblock
///
/// @see  dom::ToJS(), dom::FromJS(), dom::GetJSContext(), js::TypeTraits, js::Context
///
#pragma once
#include <Ultralight/CAPI/CAPI_DOMDocument.h>
#include <Ultralight/CAPI/CAPI_DOMElement.h>
#include <Ultralight/dom/Document.h>
#include <Ultralight/dom/Element.h>
#include <Ultralight/js/Context.h>
#include <Ultralight/js/Value.h>

namespace ultralight {
namespace dom {

///
/// Get an element's JavaScript object (the same object the page's scripts see for it).
///
/// Converting one element twice gives the same object (they compare `===`).
///
/// @param  context  The JavaScript context of the element's own frame (see GetJSContext()).
///
/// @param  element  The element to convert.
///
/// @return  Returns the element's JavaScript object, or an empty Value if `context` belongs to
///          another frame or View.
///
/// @see  FromJS()
///
inline js::Value ToJS(const js::Context& context, const Element& element) {
  return js::Value::Adopt(ulDOMElementGetJSValue(element.raw(), context.raw()));
}

///
/// Get the element a JavaScript value refers to (the inverse of ToJS()).
///
/// The element can be in any frame of the value's View, subframes included. The returned
/// Element belongs to the element's own page (not the value's), so it stops working when
/// that page goes away.
///
/// @param  value  A JavaScript value that refers to a DOM element.
///
/// @return  Returns the element (empty if `value` isn't a DOM element, the element is in
///          another View, or its document is no longer loaded in a frame).
///
/// @note  When DOM diagnostics are on, the library logs why it refused an element (see
///        dom::Element).
///
/// @see  ToJS()
///
inline Element FromJS(const js::Value& value) {
  return Element::Adopt(ulDOMElementFromJSValue(value.raw()));
}

///
/// Get the JavaScript context of a document (the context its page's scripts run in).
///
/// This works for the document of any frame. View::GetJSContext() only gives the main frame's
/// context. Like any js::Context, the result stops working when the document's page goes away.
///
/// @param  document  The document.
///
/// @return  Returns the context, or an empty Context if the document's frame can't run scripts
///          (JavaScript is disabled or the document is sandboxed).
///
/// @see  ToJS()
///
inline js::Context GetJSContext(const Document& document) {
  return js::Context::Adopt(ulDOMDocumentGetJSContext(document.raw()));
}

}  // namespace dom

namespace js {

///
/// dom::Element as its JavaScript object.
///
/// With this specialization, an element can be a bound function's parameter or return value,
/// an API::Emit() payload, a Resolver result, or an item in a container:
///
/// ```
/// api["highlight"] = [](dom::Element el) { el.classList.add("highlight"); };
/// api.Emit("picked", picked_element);
/// ```
///
/// Conversions follow ToJS() and FromJS():
///
///  - **From JavaScript**, anything FromJS() refuses fails the call with a TypeError. That
///    includes `null` and `undefined`, so use `std::optional<dom::Element>` to accept them.
///  - **To JavaScript**, an element becomes its object only in its own frame. It becomes
///    `undefined` in any other frame or View and once its page is gone. An empty Element
///    becomes `null`.
///
/// @note  API::Emit() converts its payload separately for each page that receives the event,
///        so an element arrives as its object in its own frame and as `undefined` everywhere
///        else.
///
template <>
struct TypeTraits<dom::Element> {
  static bool FromJS(ULJSContext, ULJSValue borrowed, dom::Element* out, ULJSValue*) {
    ULDOMElement element = ulDOMElementFromJSValue(borrowed);
    if (!element)
      return false;
    *out = dom::Element::Adopt(element);
    return true;
  }
  static ULJSValue ToJS(ULJSContext ctx, const dom::Element& value) {
    if (value.IsEmpty())
      return ulCreateJSValueNull(ctx);
    return ulDOMElementGetJSValue(value.raw(), ctx);
  }
  static constexpr const char* SchemaType() { return "Element"; }
};

}  // namespace js
}  // namespace ultralight
