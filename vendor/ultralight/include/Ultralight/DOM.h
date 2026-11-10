///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///

///
/// @namespace ultralight::dom
///
/// Direct C++ access to modify page elements and handle events.
///
/// `#include <Ultralight/DOM.h>`
///
/// @note  This API is a preview and may still change after 2.0.
///
/// The DOM API connects your application to the HTML pages a View loads, letting native code update
/// the user interface and respond to user input.
///
/// Names and behavior follow the web's DOM, so page script ports over nearly line for line.
///
/// This example configures a DOM-ready hook and a click listener on a View before loading a page:
///
/// ```
/// dom::Triggers page;
///
/// page.OnDOMReady([](dom::Document document) {
///   auto status = document.getElementById("status");
///   status.textContent = "Connected";
///   status.classList.add("online");
/// });
///
/// page.On("#save", "click", [](dom::Element button) {
///   button.textContent = "Saved";
/// });
///
/// if (page.AttachTo(view.get()))
///   view->LoadURL("file:///app.html");
/// ```
///
/// ## Accessing Page Content
///
/// You can interact with a View through persistent listeners or a direct document handle:
///
/// - **Attach a dom::Triggers set to handle every page a View loads.** Listeners and hooks persist
///   across navigations until you detach the set or destroy the object.
/// - **Obtain a dom::Document from the View in LoadListener::OnDOMReady().** The handle applies to
///   the current page only.
///
/// ## Where to Start
///
/// Recommended types and headers for common tasks:
///
/// | Task                           | Starting Point                   |
/// |--------------------------------|----------------------------------|
/// | Finding and changing elements  | dom::Document, dom::Element      |
/// | Listening for events           | dom::Element::addEventListener() |
/// | Viewport and window events     | dom::Window                      |
/// | Inspecting call failures       | dom::Error                       |
/// | Passing elements to JavaScript | `<Ultralight/dom/JSInterop.h>`   |
///
/// ## Rules for All DOM Types
///
/// Every DOM class follows these common conventions:
///
/// - **Call DOM operations on the Renderer's thread.** To work with the DOM from another thread,
///   post tasks using Renderer::PostTask(). Copying, moving, and destroying handles is safe on any
///   thread.
/// - **A handle never keeps its page alive.** Once a page navigates away, calls on its handles fail
///   safely (for handle states, see dom::Element).
/// - **The API never throws C++ exceptions.** A call that fails returns an empty value, or a
///   dom::Result holding a dom::Error if you pass dom::Checked.
/// - **Callbacks that throw are caught and logged.**
/// - **A const handle can still modify its node.** Constness applies to the handle itself rather
///   than the page element, so you can still assign to properties and insert child nodes.
/// - **Counts and indices use `size_t`.** Where web standards return `-1` or `null` for a missing
///   position, methods return `std::optional<size_t>` holding `std::nullopt`.
///
/// @note  The DOM API works even with JavaScript disabled
///        (`ViewConfig::enable_javascript = false`).
///
/// @see  dom::Document, dom::Element, dom::Triggers, LoadListener::OnDOMReady()
///
#pragma once
#include <Ultralight/dom/Triggers.h>
#include <Ultralight/Color.h>
#include <Ultralight/dom/ComputedStyle.h>
#include <Ultralight/dom/CSSPropertyName.h>
#include <Ultralight/dom/Document.h>
#include <Ultralight/dom/DocumentFragment.h>
#include <Ultralight/dom/DOMRect.h>
#include <Ultralight/dom/Element.h>
#include <Ultralight/dom/ElementList.h>
#include <Ultralight/dom/Error.h>
#include <Ultralight/dom/Event.h>
#include <Ultralight/dom/EventListener.h>
#include <Ultralight/dom/Node.h>
#include <Ultralight/dom/NodeList.h>
#include <Ultralight/dom/Range.h>
#include <Ultralight/dom/Selection.h>
#include <Ultralight/dom/StyleValue.h>
#include <Ultralight/dom/Window.h>
