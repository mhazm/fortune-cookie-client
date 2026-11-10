///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <AppCore/layout/Options.h>
#include <AppCore/layout/Panel.h>

#include <concepts>
#include <utility>

namespace ultralight {

struct DividerStyle;

///
/// A builder element (see `<AppCore/layout/Builder.h>`): anything that can append itself to
/// a container.
///
template <typename T>
concept LayoutBuildable = requires(const T& spec, Container& parent) { spec.ApplyTo(parent); };

///
/// A layout node that organizes child nodes into a row or column.
///
/// Containers form the rows and columns of an AppCore window's layout tree. The window's root
/// layout is a container that fills the entire window, and adding child rows or columns divides
/// that space into a hierarchy of panes.
///
/// Each child can be a Panel that hosts a View, or another Container that splits space further. The
/// library recalculates child sizes automatically whenever the window resizes or moves between
/// displays with different scale factors.
///
/// The following example creates a resizable row with a sidebar and a content pane:
///
/// ```
/// RefPtr<Container> body =
///     window->layout()->AddRow({ .key = "body", .resizable = true });
/// RefPtr<Panel> sidebar =
///     body->AddPanel({ .key = "sidebar", .size = "240px", .min_size = "160px" });
/// RefPtr<Panel> content = body->AddPanel({ .key = "content" });
///
/// sidebar->view()->LoadURL("file:///sidebar.html");
/// content->view()->LoadURL("file:///content.html");
/// ```
///
/// ## Sizing Children
///
/// A container divides space along its main axis by first subtracting its padding and any gaps
/// between children. It satisfies pixel sizes first, gives percentage sizes their shares of the
/// free space left after pixel-sized children, and divides what remains among flexible children
/// according to their flex factors. A child created without an explicit size defaults to one flex
/// share (`1fr`). For what each size unit means, see Size and Length.
///
/// Containers also accept `gap` to separate adjacent children and `padding` to inset them from all
/// four edges (both in logical pixels).
///
/// ## Resizable Containers
///
/// Setting `resizable = true` on a container places draggable dividers between its direct children.
/// Dragging a divider resizes the panes on either side.
///
/// You can constrain how panes resize during a drag:
///
/// - **Set `min_size` to prevent a pane from collapsing.**
/// - **Set `fixed = true` to keep a child's size constant.**
///
/// Double-clicking a divider resets both neighboring children to their declared sizes.
///
/// You can customize divider appearance by passing a DividerStyle to SetDividerStyle().
///
/// ## Finding Children
///
/// Assigning a `key` in a child's creation options lets you look up that node across the
/// container's subtree with Find(), FindPanel(), or FindContainer(). If multiple descendants share
/// the same key, the lookup returns the newest match.
///
/// ## Reserving Space for Drawing
///
/// An empty container reserves space in the layout without hosting a View or rendering web content.
///
/// To track the container's bounds as the window resizes, register a layout change listener:
///
/// ```
/// RefPtr<Container> viewport = body->AddColumn({ .key = "viewport" });
///
/// // The callback runs while viewport_tracker_ is kept.
/// viewport_tracker_ = viewport->OnLayoutChange([](LayoutNode& node) {
///   ResizeViewport(node.device_bounds());
/// });
/// ```
///
/// ## Overriding Child Layout
///
/// Call SetLayoutOverride() to install a layout delegate that positions direct children manually by
/// calling LayoutNode::SetBounds(). The delegate runs whenever the window updates layout.
///
/// @see  LayoutNode, PanelOptions, ContainerOptions, Size, DividerStyle, Container::Build(),
///       Window::BuildLayout()
///
class AExport Container : public LayoutNode {
 public:
  ///
  /// Create a panel with a new View (window-default view configuration).
  ///
  /// @param  options   The panel's declared options.
  ///
  /// @param  position  Where to insert the panel (default: append at the end).
  ///
  /// @return  Returns the new panel (see LayoutNode if this container was removed).
  ///
  virtual RefPtr<Panel> AddPanel(const PanelOptions& options = {},
                                 InsertPosition position = {}) = 0;

  ///
  /// Create a panel with a caller-supplied ViewConfig.
  ///
  /// The fields describing the hosting window are filled in from the window for you. Every
  /// other field is yours:
  ///
  ///  - `initial_device_scale` (the window's scale)
  ///  - `display_id` (the window's display)
  ///  - `is_accelerated` (whether the window composites on the GPU)
  ///
  /// To host a deliberately CPU-rendered View on a GPU window, create the View with
  /// Renderer::CreateView() and use AdoptPanel().
  ///
  /// @param  options      The panel's declared options.
  ///
  /// @param  view_config  The configuration for the new View. The window-describing fields
  ///                      listed above are overwritten.
  ///
  /// @param  position     Where to insert the panel (default: append at the end).
  ///
  /// @return  Returns the new panel (see LayoutNode if this container was removed).
  ///
  virtual RefPtr<Panel> AddPanel(const PanelOptions& options, const ViewConfig& view_config,
                                 InsertPosition position = {}) = 0;

  ///
  /// Create a panel that adopts an existing View (including a View created through a
  /// Session).
  ///
  /// The panel takes a reference to the View and resizes it to the panel's bounds.
  ///
  /// @param  view      The View to host.
  ///
  /// @param  options   The panel's declared options.
  ///
  /// @param  position  Where to insert the panel (default: append at the end).
  ///
  /// @return  Returns the new panel (see LayoutNode if this container was removed).
  ///
  /// @note  The panel replaces the View's editor listener with its own (see Panel).
  ///
  virtual RefPtr<Panel> AdoptPanel(RefPtr<View> view, const PanelOptions& options = {},
                                   InsertPosition position = {}) = 0;

  ///
  /// Adopt an existing View (shorthand for AdoptPanel()).
  ///
  /// @param  view      The View to host.
  ///
  /// @param  options   The panel's declared options.
  ///
  /// @param  position  Where to insert the panel (default: append at the end).
  ///
  /// @return  Returns the new panel (see LayoutNode if this container was removed).
  ///
  template <typename ViewRef>
    requires std::convertible_to<ViewRef&&, RefPtr<View>>
  RefPtr<Panel> AddPanel(ViewRef&& view, const PanelOptions& options = {},
                         InsertPosition position = {}) {
    return AdoptPanel(RefPtr<View>(std::forward<ViewRef>(view)), options, position);
  }

  ///
  /// Create a child row container.
  ///
  /// @param  options   The container's declared options.
  ///
  /// @param  position  Where to insert the container (default: append at the end).
  ///
  /// @return  Returns the new container (see LayoutNode if this container was removed).
  ///
  virtual RefPtr<Container> AddRow(const ContainerOptions& options = {},
                                   InsertPosition position = {}) = 0;

  ///
  /// Create a child column container.
  ///
  /// @param  options   The container's declared options.
  ///
  /// @param  position  Where to insert the container (default: append at the end).
  ///
  /// @return  Returns the new container (see LayoutNode if this container was removed).
  ///
  virtual RefPtr<Container> AddColumn(const ContainerOptions& options = {},
                                      InsertPosition position = {}) = 0;

  ///
  /// Remove a direct child (and, for a container child, its whole subtree) from the tree.
  ///
  /// Removal detaches the child and drops each removed panel's reference to its View. A View
  /// the application holds a RefPtr to survives removal and can be adopted into another
  /// panel or window. For what removed handles do afterwards, see LayoutNode.
  ///
  /// @param  node  The direct child to remove.
  ///
  /// @return  Returns whether or not the node was a direct child and was removed.
  ///
  virtual bool Remove(RefPtr<LayoutNode> node) = 0;

  ///
  /// Remove every child. Equivalent to Remove() on each child in turn.
  ///
  virtual void RemoveAll() = 0;

  ///
  /// Reorder a direct child within this container.
  ///
  /// A node from a different container (or window) is a no-op with a warning. A position
  /// anchor that is not a direct child falls back to the end, also with a warning.
  ///
  /// @param  node      The direct child to move.
  ///
  /// @param  position  The new position (default: move to the end).
  ///
  /// @return  Returns whether or not the node was reordered.
  ///
  virtual bool Move(RefPtr<LayoutNode> node, InsertPosition position = {}) = 0;

  ///
  /// Get the number of children.
  ///
  virtual int child_count() const = 0;

  ///
  /// Get the child at an index.
  ///
  /// @param  index  The zero-based index within this container's child list.
  ///
  /// @return  Returns the child, or null when the index is out of range.
  ///
  virtual RefPtr<LayoutNode> child_at(int index) const = 0;

  ///
  /// Find a descendant by key.
  ///
  /// @param  key  The key to look for.
  ///
  /// @return  Returns the matching node, or null when nothing matches. With duplicate keys
  ///          the most recently created match wins.
  ///
  virtual RefPtr<LayoutNode> Find(const String& key) = 0;

  ///
  /// Find a descendant panel by key.
  ///
  /// @param  key  The key to look for.
  ///
  /// @return  Returns the matching panel, or null when nothing matches or the match is not a
  ///          panel.
  ///
  virtual RefPtr<Panel> FindPanel(const String& key) = 0;

  ///
  /// Find a descendant container by key.
  ///
  /// @param  key  The key to look for.
  ///
  /// @return  Returns the matching container, or null when nothing matches or the match is
  ///          not a container.
  ///
  virtual RefPtr<Container> FindContainer(const String& key) = 0;

  ///
  /// Override this container's layout with a layout delegate.
  ///
  /// The delegate runs each time the window performs layout (after a layout change, a resize,
  /// or a display scale change), but never while the container has no children.
  ///
  /// The delegate positions the container's direct children by calling LayoutNode::SetBounds():
  ///
  /// ```
  /// grid->SetLayoutOverride([](Container& c, const Rect& content_box) {
  ///   float x = content_box.x(), y = content_box.y(), w = content_box.width();
  ///   c.child_at(0)->SetBounds(Rect::FromXYWH(x, y, w, 40));
  ///   c.child_at(1)->SetBounds(
  ///       Rect::FromXYWH(x, y + 40, w, content_box.height() - 40));
  /// });
  /// ```
  ///
  /// `content_box` is the container's content box (inside its padding) in container-local
  /// logical pixels.
  ///
  /// Placed rectangles are snapped and clipped to this box.
  ///
  /// A placed rectangle persists until the delegate places that child again-- the delegate may
  /// place only what changed.
  ///
  /// Children that the delegate has never placed occupy no region.
  ///
  /// Changing the tree from inside the delegate is allowed.
  ///
  /// Setting a new delegate replaces the existing one, and every child resets to never-placed.
  ///
  /// Replacing the delegate destroys the previous delegate's user data. If replaced from inside
  /// the delegate, destruction is deferred until the delegate returns.
  ///
  /// @param  callback           The delegate to install. Pass nullptr to restore ordinary
  ///                            layout.
  ///
  /// @param  user_data          Passed back to the delegate on every run (can be nullptr).
  ///
  /// @param  destroy_user_data  Called once when the delegate drops `user_data` (can be
  ///                            nullptr).
  ///
  /// \parblock
  /// @note  `resizable` is ignored (with a warning) while a container has a layout delegate.
  ///        Dividers only exist between children that the container itself lays out.
  /// \endparblock
  ///
  /// \parblock
  /// @note  Callbacks must not let exceptions propagate.
  /// \endparblock
  ///
  virtual void SetLayoutOverride(LayoutOverrideCallback callback, void* user_data = nullptr,
                                 LayoutDestroyUserDataCallback destroy_user_data = nullptr) = 0;

  ///
  /// Whether or not this container has a layout delegate installed.
  ///
  virtual bool HasLayoutOverride() const = 0;

  ///
  /// Set this container's divider style (see `<AppCore/layout/DividerStyle.h>`).
  ///
  /// Fields left unset fall back to the window's style (Window::SetDividerStyle()), then to
  /// the built-in defaults.
  ///
  /// @param  style  The style to apply. A default-constructed style clears the override.
  ///
  virtual void SetDividerStyle(const DividerStyle& style) = 0;

  ///
  /// Install a layout delegate (invocable form).
  ///
  /// @param  callback  Any invocable. It may take `(Container&, const Rect&)` (the container
  ///                   and its content box), or just `(const Rect&)`.
  ///
  template <typename Callback>
    requires std::invocable<Callback&, Container&, const Rect&>
        || std::invocable<Callback&, const Rect&>
  void SetLayoutOverride(Callback callback) {
    auto* holder = new Callback(std::move(callback));
    LayoutOverrideCallback trampoline = [](void* user_data, Container* container,
                                           Rect content_box) {
      auto& cb = *static_cast<Callback*>(user_data);
      if constexpr (std::invocable<Callback&, Container&, const Rect&>)
        cb(*container, content_box);
      else
        cb(content_box);
    };
    SetLayoutOverride(trampoline, holder,
                      [](void* user_data) { delete static_cast<Callback*>(user_data); });
  }

  ///
  /// Iterator over a container's children (a forward range of RefPtr<LayoutNode>).
  ///
  class ChildIterator {
   public:
    using value_type = RefPtr<LayoutNode>;
    using difference_type = int;

    ChildIterator() = default;
    ChildIterator(const Container* container, int index) : container_(container), index_(index) {}

    RefPtr<LayoutNode> operator*() const { return container_->child_at(index_); }
    ChildIterator& operator++() {
      ++index_;
      return *this;
    }
    ChildIterator operator++(int) {
      ChildIterator prev = *this;
      ++index_;
      return prev;
    }
    friend bool operator==(const ChildIterator&, const ChildIterator&) = default;

   private:
    const Container* container_ = nullptr;
    int index_ = 0;
  };

  ///
  /// A range over the container's children:
  ///
  /// ```
  /// for (RefPtr<LayoutNode> child : container->children()) { ... }
  /// ```
  ///
  /// The range reads through child_at(), so mutating the child list while iterating skews
  /// the walk. Collect handles first when a loop mutates.
  ///
  class ChildRange {
   public:
    ChildRange(const Container* container, int count) : container_(container), count_(count) {}
    ChildIterator begin() const { return ChildIterator(container_, 0); }
    ChildIterator end() const { return ChildIterator(container_, count_); }

   private:
    const Container* container_;
    int count_;
  };

  ///
  /// Get a forward range over the children.
  ///
  ChildRange children() const { return ChildRange(this, child_count()); }

  ///
  /// Append builder elements (see `<AppCore/layout/Builder.h>`) to this container.
  ///
  /// Build **appends**, so calling twice appends twice. For a fresh start, call RemoveAll()
  /// first.
  ///
  /// @param  children  The builder elements to append, in order.
  ///
  /// @return  Returns this container, so the builder line composes with follow-up calls.
  ///
  template <typename... Children>
    requires(LayoutBuildable<Children> && ...)
  RefPtr<Container> Build(Children... children) {
    (children.ApplyTo(*this), ...);
    return RefPtr<Container>(this);
  }

 protected:
  virtual ~Container();
};

inline RefPtr<LayoutNode> LayoutNode::next_sibling() const {
  RefPtr<Container> p = parent();
  if (!p)
    return nullptr;
  int i = index();
  return i < 0 ? nullptr : p->child_at(i + 1);
}

inline RefPtr<LayoutNode> LayoutNode::previous_sibling() const {
  RefPtr<Container> p = parent();
  if (!p)
    return nullptr;
  int i = index();
  return i <= 0 ? nullptr : p->child_at(i - 1);
}

}  // namespace ultralight
