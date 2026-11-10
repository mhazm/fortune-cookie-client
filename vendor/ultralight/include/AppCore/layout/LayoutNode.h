///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <AppCore/Defines.h>
#include <AppCore/layout/Length.h>
#include <AppCore/layout/Callback.h>
#include <Ultralight/Geometry.h>
#include <Ultralight/RefPtr.h>
#include <Ultralight/String.h>

#include <concepts>
#include <utility>

namespace ultralight {

class Container;
class Panel;

///
/// Base class for Panel%s and Container%s in a window layout tree.
///
/// An AppCore window arranges web views and native drawing regions into a tree of rows and columns,
/// where each node shares space along its parent container's axis. LayoutNode represents an
/// individual element in that hierarchy, whether it's a Container grouping child nodes or a Panel
/// hosting a single View.
///
/// You use node handles to manage panes and containers dynamically at runtime, adapting the
/// interface without rebuilding the layout tree.
///
/// This helper toggles a sidebar panel on and off:
///
/// ```
/// void ToggleSidebar(RefPtr<Panel> sidebar) {
///   if (sidebar->is_hidden())
///     sidebar->Show();  // back at its previous size
///   else
///     sidebar->Hide();  // its siblings take the space
/// }
/// ```
///
/// ## Showing and Hiding
///
/// Calling Hide() takes a node off screen and redistributes its space to sibling nodes, and Show()
/// restores the node at its remembered size.
///
/// Hiding affects different node types:
///
/// - **Hiding a Container hides its entire subtree.**
/// - **Hiding a Panel suspends its hosted View.** Painting and animations pause, and the page
///   receives a `visibilitychange` event.
///
/// ## Declared Sizes
///
/// Call SetSize(), SetMinSize(), or SetMaxSize() to update a node's declared size constraints at
/// runtime. In a resizable container, double-clicking an adjacent divider restores both neighboring
/// panes to the declared sizes set here (discarding any divider drags made since).
///
/// ## Layout Geometry
///
/// Accessors report layout rectangles across different coordinate spaces:
///
/// - **bounds() returns container-local logical pixels.**
/// - **device_bounds() returns window back-buffer device pixels.** Native rendering passes use this
///   coordinate space to draw beneath or above web content.
///
/// Both rectangles remain zero until the window completes its first layout pass. Calling SetSize()
/// queues a layout pass for an upcoming frame.
///
/// This registration reads the updated dimensions once the layout pass completes:
///
/// ```
/// sidebar->SetSize("320px");
///
/// // bounds() still holds the old rect here. Read it once the layout runs:
/// on_resize_ = sidebar->OnLayoutChange([](LayoutNode& node) {
///   Log("sidebar is now " + std::to_string(node.bounds().width()) + "px wide");
/// });
/// ```
///
/// Callback registrations report geometry changes:
///
/// - **OnLayoutChange() fires when layout changes the node's bounds.** It runs at most once per
///   frame after the layout pass completes.
/// - **OnUserResize() fires when a user resizes the node with a divider.** It fires on drag release
///   and after a double-click restores declared sizes, only when the node's size changed.
///
/// Both methods return a LayoutCallback that manages the registration (destroying it unregisters
/// the callback).
///
/// ## Node Lifetimes and Handles
///
/// A handle never keeps its node or its window alive (releasing one doesn't remove the node from
/// the layout tree). To remove a node, call Container::Remove() or Foreground::Remove().
///
/// After a node is removed or its window closes, handles remain safe to call. Mutations do nothing,
/// and accessors return safe fallback values.
///
/// Adding a child to a removed container or a closed window logs a warning and returns a handle
/// that isn't attached to any window.
///
/// @note  A View kept after its panel is removed loses its listeners-- register them again after
///        adopting it into another panel.
///
/// @see  Panel, Container, Container::Remove(), Foreground::Remove(), LayoutCallback
///
class AExport LayoutNode : public RefCounted {
 public:
  ///
  /// Get the node's key (empty if unkeyed).
  ///
  virtual String key() const = 0;

  ///
  /// Get the node's parent container (null at the root or when detached).
  ///
  virtual RefPtr<Container> parent() const = 0;

  ///
  /// Get the node's index within its parent (-1 when detached).
  ///
  virtual int index() const = 0;

  ///
  /// Get this node as a Panel (null if it is not one).
  ///
  virtual RefPtr<Panel> AsPanel() = 0;

  ///
  /// Get this node as a Container (null if it is not one).
  ///
  virtual RefPtr<Container> AsContainer() = 0;

  ///
  /// Hide the node and redistribute its space to its siblings.
  ///
  /// The node remembers its declared size, and Show() restores it.
  ///
  /// Hiding a Container hides its entire subtree.
  ///
  /// A hidden Panel's View stops rendering entirely-- painting and animations suspend. The page
  /// receives a `visibilitychange` event.
  ///
  /// @note  The `visibilitychange` event fires per panel rather than per window. Page code written
  ///        for tab-background semantics receives this event on every Hide() and Show().
  ///
  virtual void Hide() = 0;

  ///
  /// Show the node again, restoring its remembered size exactly.
  ///
  virtual void Show() = 0;

  ///
  /// Whether or not the node is hidden.
  ///
  /// This reports the node's own flag, so a node inside a hidden container still reports its
  /// own state even though it is not on screen.
  ///
  virtual bool is_hidden() const = 0;

  ///
  /// Get the node's rect from the most recent layout in container-local logical pixels (the same
  /// space SetBounds() takes).
  ///
  /// @note  This value is zero until the node's first layout.
  ///
  virtual Rect bounds() const = 0;

  ///
  /// Get the node's rect from the most recent layout in window back-buffer device pixels (the
  /// space your own drawing uses).
  ///
  /// @note  This value is zero until the node's first layout.
  ///
  virtual IntRect device_bounds() const = 0;

  ///
  /// Place this node manually.
  ///
  /// @param  bounds  The node's new rect, in container-local logical pixels.
  ///
  /// @note  This is only legal inside its container's layout delegate (see
  ///        Container::SetLayoutOverride()). Anywhere else it is a no-op with a warning.
  ///
  virtual void SetBounds(const Rect& bounds) = 0;

  ///
  /// Set the node's declared size along its container's axis.
  ///
  /// This acts like the size in the creation options-- double-clicking an adjacent divider
  /// restores the node to the size declared here, discarding any user divider drags made since.
  ///
  /// @param  size  The new declared size. An unset Size means one flex share (`1fr`).
  ///
  /// @note  A floating panel's size is fixed at creation. Calling SetSize(), SetMinSize(), or
  ///        SetMaxSize() on a floating panel does nothing and logs a warning.
  ///
  virtual void SetSize(Size size) = 0;

  ///
  /// Set the node's minimum size constraint.
  ///
  /// @param  min_size  The new minimum. An unset Length means no minimum.
  ///
  virtual void SetMinSize(Length min_size) = 0;

  ///
  /// Set the node's maximum size constraint.
  ///
  /// @param  max_size  The new maximum. An unset Length means no maximum.
  ///
  virtual void SetMaxSize(Length max_size) = 0;

  ///
  /// Register a callback fired when the user resizes this node with a divider.
  ///
  /// It fires on drag release, and again after a double-click resets the node to its declared
  /// size. Either way it fires only when the node's size changed.
  ///
  /// @param  callback           The function to call. It receives `user_data` and this node.
  ///
  /// @param  user_data          Passed back to the callback on every fire (can be nullptr).
  ///
  /// @param  destroy_user_data  Called once when the registration drops `user_data` (can be
  ///                            nullptr).
  ///
  /// @return  Returns a LayoutCallback controlling how long the callback stays registered.
  ///
  /// @note  Callbacks must not let exceptions propagate.
  ///
  [[nodiscard]] virtual LayoutCallback OnUserResize(
      LayoutNodeCallback callback, void* user_data,
      LayoutDestroyUserDataCallback destroy_user_data) = 0;

  ///
  /// Register a callback that fires after layout changes this node's bounds (at most once per
  /// frame).
  ///
  /// Use this to follow a node's geometry (eg, a reserved region that your own drawing fills).
  ///
  /// @param  callback           The function to call. It receives `user_data` and this node.
  ///
  /// @param  user_data          Passed back to the callback on every fire (can be nullptr).
  ///
  /// @param  destroy_user_data  Called once when the registration drops `user_data` (can be
  ///                            nullptr).
  ///
  /// @return  Returns a LayoutCallback controlling how long the callback stays registered.
  ///
  /// @note  Callbacks must not let exceptions propagate.
  ///
  [[nodiscard]] virtual LayoutCallback OnLayoutChange(
      LayoutNodeCallback callback, void* user_data,
      LayoutDestroyUserDataCallback destroy_user_data) = 0;

  ///
  /// Register a user-resize callback (invocable form).
  ///
  /// @param  callback  Any invocable. It may take the node as `LayoutNode&`, or nothing at
  ///                   all.
  ///
  /// @return  Returns a LayoutCallback controlling how long the callback stays registered.
  ///
  template <typename Callback>
    requires std::invocable<Callback&, LayoutNode&> || std::invocable<Callback&>
  [[nodiscard]] LayoutCallback OnUserResize(Callback callback) {
    return RegisterInvocable<Callback>(std::move(callback), true);
  }

  ///
  /// Register a layout-change callback (invocable form).
  ///
  /// @param  callback  Any invocable. It may take the node as `LayoutNode&`, or nothing at
  ///                   all.
  ///
  /// @return  Returns a LayoutCallback controlling how long the callback stays registered.
  ///
  template <typename Callback>
    requires std::invocable<Callback&, LayoutNode&> || std::invocable<Callback&>
  [[nodiscard]] LayoutCallback OnLayoutChange(Callback callback) {
    return RegisterInvocable<Callback>(std::move(callback), false);
  }

  ///
  /// Get the sibling after this node (null at the end, at the root, or when detached).
  ///
  RefPtr<LayoutNode> next_sibling() const;

  ///
  /// Get the sibling before this node (null at the start, at the root, or when detached).
  ///
  RefPtr<LayoutNode> previous_sibling() const;

 protected:
  virtual ~LayoutNode();

 private:
  template <typename Callback>
  LayoutCallback RegisterInvocable(Callback callback, bool user_resize) {
    auto* holder = new Callback(std::move(callback));
    LayoutNodeCallback trampoline = [](void* user_data, LayoutNode* node) {
      auto& cb = *static_cast<Callback*>(user_data);
      if constexpr (std::invocable<Callback&, LayoutNode&>)
        cb(*node);
      else
        cb();
    };
    LayoutDestroyUserDataCallback destroy
        = [](void* user_data) { delete static_cast<Callback*>(user_data); };
    return user_resize ? OnUserResize(trampoline, holder, destroy)
                       : OnLayoutChange(trampoline, holder, destroy);
  }
};

}  // namespace ultralight
