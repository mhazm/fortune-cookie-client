///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// @file Builder.h
///
/// Declarative functions for constructing layout trees.
///
/// `#include <AppCore/layout/Builder.h>`
///
/// The panel(), row(), and column() functions return builder elements that describe panels and
/// containers. They let you declare an entire layout hierarchy in a single expression instead of
/// adding nodes step by step with Container::AddPanel(), Container::AddRow(), and
/// Container::AddColumn().
///
/// This declares a toolbar above a resizable body with a sidebar and a content pane:
///
/// ```
/// RefPtr<Panel> toolbar, sidebar, content;
///
/// window->BuildLayout({},
///   panel({ .key = "toolbar", .size = "44px", .fixed = true }, &toolbar),
///   row({ .key = "body", .resizable = true },
///     panel({ .key = "sidebar", .size = "240px" }, &sidebar),
///     panel({ .key = "content" }, &content)));
///
/// toolbar->view()->LoadURL("file:///toolbar.html");
/// ```
///
/// ## Building Layout Trees
///
/// Builder elements construct nodes through a window or an existing container:
///
/// - **Window::BuildLayout() populates the window while setting options on its root container.**
///   Pass ContainerOptions for the root container as the first argument, followed by the child
///   elements.
/// - **Container::Build() appends children to an existing container.** Pass the child elements
///   directly to add them to that container.
///
/// The builder functions accept the same PanelOptions and ContainerOptions as
/// Container::AddPanel(), Container::AddRow(), and Container::AddColumn(), and they produce the
/// same Panel and Container nodes.
///
/// ## Capturing Node Handles
///
/// You can capture handles to nodes during layout construction by passing the address of a RefPtr
/// handle to panel(), row(), or column(). When the layout is built, each function populates the
/// target pointer with the created Panel or Container. This lets you interact with nodes
/// immediately (such as calling Panel::view() to load content) so you don't need to look them up by
/// key later.
///
/// @note  Window::BuildLayout() and Container::Build() append children to the existing tree.
///        Calling either method again appends duplicate children. Call Container::RemoveAll() first
///        if you want to rebuild from a clean slate.
///
/// @see  Window::BuildLayout(), Container::Build(), panel(), row(), column()
///
#pragma once
#include <AppCore/layout/Container.h>

#include <tuple>
#include <utility>

namespace ultralight {

///
/// A panel element inside a Build()/BuildLayout() expression. Create with panel().
///
struct PanelSpec {
  PanelOptions options;
  RefPtr<Panel>* out = nullptr;

  void ApplyTo(Container& parent) const {
    RefPtr<Panel> result = parent.AddPanel(options);
    if (out)
      *out = result;
  }
};

///
/// Describe a panel in a builder expression (see Builder.h for an example).
///
/// @param  options  The panel's declared options.
///
/// @param  out      Receives the created panel when the layout is built. Pass nullptr if you
///                  don't need the handle.
///
/// @return  Returns a builder element describing the panel.
///
inline PanelSpec panel(PanelOptions options = {}, RefPtr<Panel>* out = nullptr) {
  return PanelSpec { std::move(options), out };
}

///
/// A container element inside a Build()/BuildLayout() expression. Create with row() or
/// column().
///
template <typename... Children>
struct ContainerSpec {
  ContainerOptions options;
  bool is_row = false;
  RefPtr<Container>* out = nullptr;
  std::tuple<Children...> children;

  void ApplyTo(Container& parent) const {
    RefPtr<Container> result
        = is_row ? parent.AddRow(options) : parent.AddColumn(options);
    if (out)
      *out = result;
    std::apply([&](const Children&... child) { (child.ApplyTo(*result), ...); }, children);
  }
};

///
/// Describe a row container with child elements.
///
/// @param  options   The container's declared options.
///
/// @param  children  The child elements to append, in order.
///
/// @return  Returns a builder element describing the row.
///
template <typename... Children>
  requires(LayoutBuildable<Children> && ...)
ContainerSpec<Children...> row(ContainerOptions options, Children... children) {
  return ContainerSpec<Children...> { std::move(options), true, nullptr,
                                      std::make_tuple(std::move(children)...) };
}

///
/// Describe a row container, capturing the created handle in `out`.
///
/// @param  options   The container's declared options.
///
/// @param  out       Receives the created container when the layout is built.
///
/// @param  children  The child elements to append, in order.
///
/// @return  Returns a builder element describing the row.
///
template <typename... Children>
  requires(LayoutBuildable<Children> && ...)
ContainerSpec<Children...> row(ContainerOptions options, RefPtr<Container>* out,
                               Children... children) {
  return ContainerSpec<Children...> { std::move(options), true, out,
                                      std::make_tuple(std::move(children)...) };
}

///
/// Describe a column container with child elements.
///
/// @param  options   The container's declared options.
///
/// @param  children  The child elements to append, in order.
///
/// @return  Returns a builder element describing the column.
///
template <typename... Children>
  requires(LayoutBuildable<Children> && ...)
ContainerSpec<Children...> column(ContainerOptions options, Children... children) {
  return ContainerSpec<Children...> { std::move(options), false, nullptr,
                                      std::make_tuple(std::move(children)...) };
}

///
/// Describe a column container, capturing the created handle in `out`.
///
/// @param  options   The container's declared options.
///
/// @param  out       Receives the created container when the layout is built.
///
/// @param  children  The child elements to append, in order.
///
/// @return  Returns a builder element describing the column.
///
template <typename... Children>
  requires(LayoutBuildable<Children> && ...)
ContainerSpec<Children...> column(ContainerOptions options, RefPtr<Container>* out,
                                  Children... children) {
  return ContainerSpec<Children...> { std::move(options), false, out,
                                      std::make_tuple(std::move(children)...) };
}

}  // namespace ultralight
