///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// @file Layout.h
///
/// Panels and containers for arranging web views across a window.
///
/// `#include <AppCore/Layout.h>`
///
/// The layout API arranges web content across an AppCore window using panels and containers. You
/// declare sizes once, and the window recalculates the layout whenever it resizes or its display
/// scale changes.
///
/// This example splits a window into a sidebar and a content pane:
///
/// ```
/// RefPtr<Container> body = window->layout()->AddRow();
/// RefPtr<Panel> sidebar = body->AddPanel({ .size = "240px" });
/// RefPtr<Panel> content = body->AddPanel();  // takes the rest of the row
///
/// sidebar->view()->LoadURL("file:///sidebar.html");
/// content->view()->LoadURL("file:///content.html");
/// ```
///
/// ## The Layout Tree
///
/// A window's tiled layout forms a tree rooted at Window::layout(). The root container is a column
/// that spans the window's content area, holding child Panel%s and nested row or column
/// Container%s. Each Panel hosts one View, while containers manage the placement and sizing of
/// their children.
///
/// Floating panels sit outside this tree in the window's Foreground layer, reached through
/// Window::foreground(). They float above tiled panels without shifting the layout underneath,
/// which suits overlays such as toasts, menus, and heads-up displays.
///
/// This diagram shows the relationship between a window, its tiled layout tree, and its floating
/// layer:
///
/// ```text
/// Window
///  +- layout()          Container (a column)
///  |   +- Panel         View
///  |   +- Container     (a row)
///  |       +- Panel     View
///  |       +- Panel     View
///  +- foreground()      Foreground
///      +- Panel         View
/// ```
///
/// ## Where to Start
///
/// Choose the type that fits your layout task:
///
/// | Task                                     | Type                  |
/// |------------------------------------------|-----------------------|
/// | Arrange views in rows or columns         | Container             |
/// | Host an individual web view              | Panel                 |
/// | Float overlays above the layout          | Foreground            |
/// | Track bounds or listen for resizes       | LayoutNode            |
/// | Declare a whole layout in one expression | Window::BuildLayout() |
///
/// ## Rules for Layout Handles
///
/// Every container and panel handle follows these conventions:
///
/// - **Call layout methods only on the main thread.** Layout handles and tree mutations aren't
///   thread-safe, so background threads must dispatch layout work through App::PostTask().
/// - **A handle doesn't keep its node or window alive.** Releasing a handle leaves its node in the
///   tree, while calls on a handle after its node is removed or its window closes do nothing (see
///   LayoutNode).
/// - **Changes apply together in one layout pass on an upcoming frame.** Geometry accessors like
///   LayoutNode::bounds() report the updated layout once that pass completes.
/// - **Mistakes log a warning rather than throwing.** Disallowed actions do nothing and write a
///   warning to the log.
///
/// @note  `<AppCore/Window.h>` already includes this header, so applications that create windows
///        don't need to include `<AppCore/Layout.h>` directly.
///
/// @see  Window::layout(), Window::foreground(), Window::BuildLayout()
///
#pragma once
#include <AppCore/layout/Length.h>
#include <AppCore/layout/DividerStyle.h>
#include <AppCore/layout/Anchor.h>
#include <AppCore/layout/Options.h>
#include <AppCore/layout/Callback.h>
#include <AppCore/layout/LayoutNode.h>
#include <AppCore/layout/Panel.h>
#include <AppCore/layout/Container.h>
#include <AppCore/layout/Foreground.h>
#include <AppCore/layout/Builder.h>
