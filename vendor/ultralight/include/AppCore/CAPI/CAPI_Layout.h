///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// @file CAPI_Layout.h
///
/// Panels and containers for arranging Views across an AppCore window in C.
///
/// `#include <AppCore/CAPI/CAPI_Layout.h>`
///
/// The layout C API arranges Views across an AppCore window using a tree of panels and containers.
/// Each panel hosts a single View, while row and column containers organize them into a tiled
/// hierarchy.
///
/// You declare sizes once when creating nodes, so you don't need to recalculate bounds manually
/// when the window resizes or its display scale changes.
///
/// This example splits a window into a resizable sidebar and a content pane:
///
/// ```c
/// static ULWindow main_window = NULL;  // From ulCreateWindow().
/// static ULPanel sidebar = NULL;
/// static ULPanel content = NULL;
///
/// void BuildLayout(void) {
///   ULContainerDesc body_desc = {0};
///   body_desc.struct_size = sizeof(ULContainerDesc);
///   body_desc.key = "body";
///   body_desc.flags = kULLayoutFlags_Resizable;
///
///   ULPanelDesc sidebar_desc = {0};
///   sidebar_desc.struct_size = sizeof(ULPanelDesc);
///   sidebar_desc.key = "sidebar";
///   sidebar_desc.size = ulLayoutSizePx(240);
///   sidebar_desc.min_size = ulLayoutSizePx(160);
///
///   ULContainer root = ulWindowGetLayout(main_window);
///   ULContainer body = ulContainerAddRow(root, &body_desc, NULL);
///   sidebar = ulContainerAddPanel(body, &sidebar_desc, NULL, NULL);
///   content = ulContainerAddPanel(body, NULL, NULL, NULL);
///
///   // The tree keeps both containers.
///   ulDestroyContainer(body);
///   ulDestroyContainer(root);
/// }
/// ```
///
/// ## Building the Layout Tree
///
/// The C++ layout headers (starting with `<AppCore/Layout.h>`) explain the underlying concepts for
/// organizing panels into nested rows and columns under a window's root container. The C API
/// doesn't provide a fluent builder, so you build the tree one call at a time.
///
/// Call ulWindowGetLayout() to obtain the window's root container, which is a column that spans the
/// window's content area. To configure the root container's options (such as padding, gap, or
/// resizable dividers), call ulWindowConfigureLayout().
///
/// ## Descriptors and Sizes
///
/// Zero-initialize every descriptor struct. Passing `NULL` applies default options for every field.
///
/// Construct ULLayoutSize values using ulLayoutSizePx() for logical pixels, ulLayoutSizePct() for
/// percentages, or ulLayoutSizeFr() for flex factors. A zero-initialized ULLayoutSize leaves the
/// dimension unset so the field's default applies, which differs from an explicit `0px`.
///
/// @warning  Leaving `struct_size` set to 0 causes the library to ignore the descriptor, log a
///           warning, and fall back to default values.
///
/// ## Handle Ownership
///
/// Layout handles manage references independently of the tree:
///
/// - **Layout getters and factory functions return owned handles.** Release them with matching
///   destroy functions such as ulDestroyContainer(), ulDestroyPanel(), or ulDestroyLayoutNode().
/// - **Destroying a handle doesn't remove the node from the tree.** To detach a node and its
///   children from the layout, call ulContainerRemove() (or ulForegroundRemove() for a floating
///   panel).
/// - **ulPanelGetView() returns an owned View handle.** Destroying it releases only your reference,
///   while the panel keeps the hosted View alive until removed.
///
/// @note  A View that outlives its panel loses its `ulViewSet*Callback()` callbacks-- register them
///        again after adopting the View into another panel.
///
/// ## Handle Identity and Lifetime
///
/// Layout handles track identity rather than lifetime:
///
/// - **Compare node identity with ulLayoutNodeIsSame(), never `==`.** Separate lookups of the same
///   node return distinct pointer addresses. To compare two panel or container handles, convert
///   both to ULLayoutNode handles using ulPanelAsLayoutNode() or ulContainerAsLayoutNode(), and
///   destroy those temporary handles when you're done.
/// - **Handles whose node is gone or whose window has closed remain safe to call.** Passing `NULL`
///   behaves the same way. An `IsAlive` function like ulLayoutNodeIsAlive() returns `false` for
///   both `NULL` handles and handles whose node is gone, so check for `NULL` first to tell the two
///   apart.
/// - **Call layout operations on the main thread.** Reference, destroy, and `IsAlive` functions
///   (such as ulCreateLayoutNodeRef(), ulDestroyLayoutNode(), and ulLayoutNodeIsAlive()) are safe
///   from any thread.
///
/// ## Event Callbacks
///
/// Layout and window events use individual callback setters:
///
/// - **Each event accepts only one callback per node, panel, or window.** Setting a new callback
///   replaces the existing registration, and passing `NULL` removes it.
/// - **Releasing a handle leaves its callbacks active.** A callback continues firing until you
///   replace or clear it, remove the node, or close the window.
/// - **Pass application state through `user_data`.** The optional `destroy_user_data` hook (see
///   ULUserDataDestroyCallback) cleans up that state when the library drops the registration.
/// - **Callback parameters are borrowed handles.** They stay valid only during the call, and you
///   must never destroy them. To keep a handle beyond the callback, duplicate it using
///   ulCreateLayoutNodeRef() or ulCreatePanelRef().
/// - **Compare window handles in callbacks using ulWindowIsSame(), never `==`.** Focus-change and
///   editable-state callbacks pass a borrowed ULWindow handle that doesn't equal your original
///   window pointer.
///
/// This example tracks a container's bounds to resize an external drawing viewport:
///
/// ```c
/// static void OnViewportLayout(void* user_data, ULLayoutNode node) {
///   (void)user_data;
///   ResizeViewport(ulLayoutNodeGetDeviceBounds(node));  // node is borrowed
/// }
///
/// // Reserve a region of the row for your own drawing.
/// void AddViewport(ULContainer body) {
///   ULContainer viewport = ulContainerAddColumn(body, NULL, NULL);
///   ULLayoutNode node = ulContainerAsLayoutNode(viewport);
///   ulLayoutNodeSetLayoutChangeCallback(node, OnViewportLayout, NULL, NULL);
///   ulDestroyLayoutNode(node);
///   ulDestroyContainer(viewport);
/// }
/// ```
///
/// @see  ulWindowGetLayout(), ulWindowGetForeground(), ulContainerAddPanel(), ulContainerRemove(),
///       ulLayoutNodeIsSame(), ulWindowIsSame(), ulWindowConfigureLayout(),
///       ULUserDataDestroyCallback
///
#ifndef APPCORE_CAPI_LAYOUT_H
#define APPCORE_CAPI_LAYOUT_H

#include <AppCore/CAPI/CAPI_Defines.h>
#include <Ultralight/CAPI/CAPI_Editor.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Opaque handle to a layout node, the shared base of panels and containers.
/// @see  ulPanelAsLayoutNode(), ulContainerAsLayoutNode(), ulLayoutNodeIsSame(),
/// ulDestroyLayoutNode()
typedef struct C_LayoutNode* ULLayoutNode;

/// Opaque handle to a panel (a layout node hosting a View). @see  ulContainerAddPanel(),
/// ulDestroyPanel()
typedef struct C_Panel* ULPanel;

/// Opaque handle to a container (a row or column of child nodes). @see  ulWindowGetLayout(),
/// ulContainerAddRow(), ulDestroyContainer()
typedef struct C_Container* ULContainer;

/// Opaque handle to a window's foreground layer (floating panels composited above the
/// tiled layout). @see  ulWindowGetForeground(), ulDestroyForeground()
typedef struct C_Foreground* ULForeground;

///
/// The unit of a ULLayoutSize.
///
typedef enum {
  kULLayoutSizeUnit_Default = 0,  ///< Unset: the receiving field's default applies.
  kULLayoutSizeUnit_Px      = 1,  ///< Logical pixels.
  kULLayoutSizeUnit_Pct     = 2,  ///< For a size, percent of the container's free space.
                                  ///< For a constraint, percent of the container's content box.
  kULLayoutSizeUnit_Fr      = 3,  ///< A flex factor (sizes only). A constraint field
                                  ///< treats it as unset with a warning.
} ULLayoutSizeUnit;

///
/// A size or constraint length, as a value plus a unit.
///
/// Passed and returned by value. This API is purely numeric-- CSS strings never cross the C
/// boundary, so each language binding owns its own literal form.
///
/// A zero-initialized ULLayoutSize is unset (the receiving field's default applies), which
/// is distinct from an explicit `0px`.
///
typedef struct {
  double value;         ///< The numeric value (px, percent points, or flex factor).
  unsigned char unit;   ///< A ULLayoutSizeUnit value.
} ULLayoutSize;

///
/// Create a ULLayoutSize in logical pixels.
///
static inline ULLayoutSize ulLayoutSizePx(double value) {
  ULLayoutSize size;
  size.value = value;
  size.unit = kULLayoutSizeUnit_Px;
  return size;
}

///
/// Create a percent ULLayoutSize.
///
/// For a size, the value is a percent of the container's free space. For a constraint, it is
/// a percent of the container's content box.
///
static inline ULLayoutSize ulLayoutSizePct(double value) {
  ULLayoutSize size;
  size.value = value;
  size.unit = kULLayoutSizeUnit_Pct;
  return size;
}

///
/// Create a flex-factor ULLayoutSize (sizes only).
///
static inline ULLayoutSize ulLayoutSizeFr(double value) {
  ULLayoutSize size;
  size.value = value;
  size.unit = kULLayoutSizeUnit_Fr;
  return size;
}

///
/// A rectangle in device pixels (window-backbuffer space).
///
typedef struct {
  int x;
  int y;
  int width;
  int height;
} ULLayoutDeviceRect;

///
/// The kind of a layout node.
///
/// @see  ulLayoutNodeGetKind()
///
typedef enum {
  kULLayoutNodeKind_None      = 0,  ///< NULL or invalid handle.
  kULLayoutNodeKind_Panel     = 1,
  kULLayoutNodeKind_Container = 2,
} ULLayoutNodeKind;

///
/// Flags for the desc structs' `flags` field.
///
typedef enum {
  kULLayoutFlags_Fixed     = 1 << 0,  ///< The user cannot resize this node with a divider
                                      ///< drag (panels and containers).
  kULLayoutFlags_Hidden    = 1 << 1,  ///< Created hidden, shows later without a layout
                                      ///< flash (all descs).
  kULLayoutFlags_Resizable = 1 << 2,  ///< The user can resize this container's children by
                                      ///< dragging the dividers between them (containers
                                      ///< only).
} ULLayoutFlags;

///
/// Options for creating a panel.
///
/// @see  ulContainerAddPanel()
///
typedef struct {
  uint32_t struct_size;   ///< Must be sizeof(ULPanelDesc).

  ///
  /// Optional identity for lookup (ulContainerFind()) and diagnostics, as null-terminated
  /// UTF-8 (copied). NULL means unkeyed, and unkeyed panels are fully supported. Duplicate
  /// keys log a warning, and lookup returns the most recently created match.
  ///
  const char* key;

  ///
  /// The panel's size along its container's axis. Unset means one flex share (1fr). A percent
  /// value is relative to the container's free space (after gaps and pixel sizes).
  ///
  ULLayoutSize size;

  ///
  /// Minimum size constraint. A percent value is relative to the container's content box. Unset
  /// means no minimum.
  ///
  ULLayoutSize min_size;

  ///
  /// Maximum size constraint. A percent value is relative to the container's content box. Unset
  /// means no maximum.
  ///
  ULLayoutSize max_size;

  uint32_t flags;         ///< A ULLayoutFlags combination: Fixed, Hidden.
} ULPanelDesc;

///
/// Options for creating a container (a row or column of child nodes).
///
/// @see  ulContainerAddRow(), ulContainerAddColumn()
///
typedef struct {
  uint32_t struct_size;   ///< Must be sizeof(ULContainerDesc).

  /// Optional identity for lookup and diagnostics (see ULPanelDesc.key).
  const char* key;

  /// The container's size along its parent's axis. Unset means one flex share (1fr).
  ULLayoutSize size;

  /// Minimum size constraint (see ULPanelDesc.min_size).
  ULLayoutSize min_size;

  /// Maximum size constraint (see ULPanelDesc.max_size).
  ULLayoutSize max_size;

  uint32_t flags;         ///< A ULLayoutFlags combination: Fixed, Resizable, Hidden.

  ///
  /// The gap between adjacent children (logical px only, other units are ignored with a
  /// warning). Unset means no gap.
  ///
  ULLayoutSize gap;

  ///
  /// Padding between the container's edges and its children, applied on all four sides
  /// (logical px only). Unset means no padding.
  ///
  ULLayoutSize padding;
} ULContainerDesc;

///
/// Styling for the dividers between a resizable container's children.
///
/// Fields left unset fall back to the window's style, then to the built-in defaults.
///
/// @see  ulWindowSetDividerStyle(), ulContainerSetDividerStyle()
///
typedef struct {
  uint32_t struct_size;     ///< Must be sizeof(ULDividerStyleDesc).

  ///
  /// The width of the divider's pointer target, in logical px. Zero means the default (6).
  ///
  double hit_width;

  ///
  /// The thickness of the painted divider line, in logical px. Zero means the default (1).
  ///
  double visual_thickness;

  ///
  /// The color of the divider line. Unset means no line is painted (the window background
  /// shows through).
  ///
  ULColor color;

  ///
  /// The hover highlight color, shown over the divider while the pointer rests on it or
  /// drags it. Unset means the default (a half-opacity gray).
  ///
  ULColor hover_color;
} ULDividerStyleDesc;

///
/// Keyboard-focus policy for a foreground (floating) panel.
///
typedef enum {
  kULFocusPolicy_Auto = 0,  ///< Takes focus when clicked, like any panel. Showing the panel
                            ///< never takes focus.
  kULFocusPolicy_Grab = 1,  ///< Takes keyboard focus when created (unless hidden) and each time
                            ///< it is shown, even from inside a click handler (dropdowns,
                            ///< palettes, in-window dialogs).
  kULFocusPolicy_None = 2,  ///< Never takes keyboard focus. Calling ulPanelFocus() on it is
                            ///< ignored with a warning (toasts, HUDs).
} ULFocusPolicy;

///
/// The placement form of a ULAnchorDesc.
///
typedef enum {
  kULAnchorKind_Default      = 0,  ///< The window origin (with the default 100% sizes, a
                                   ///< full-window panel).
  kULAnchorKind_At           = 1,  ///< Absolute window-space logical px.
  kULAnchorKind_WindowCorner = 2,  ///< A window corner, following the window.
  kULAnchorKind_WindowCenter = 3,  ///< Centered in the window, following the window.
  kULAnchorKind_Below        = 4,  ///< Under an anchor rect in a target panel.
} ULAnchorKind;

///
/// A window corner (kULAnchorKind_WindowCorner).
///
typedef enum {
  kULAnchorCorner_TopLeft     = 0,
  kULAnchorCorner_TopRight    = 1,
  kULAnchorCorner_BottomLeft  = 2,
  kULAnchorCorner_BottomRight = 3,
} ULAnchorCorner;

///
/// Cross-axis alignment of the floating panel against its anchor rect (kULAnchorKind_Below).
///
typedef enum {
  kULAnchorAlign_Start  = 0,  ///< Left edges align.
  kULAnchorAlign_Center = 1,
  kULAnchorAlign_End    = 2,  ///< Right edges align.
} ULAnchorAlign;

///
/// Fit behavior when an anchored floating panel does not fit on its preferred side
/// (kULAnchorKind_Below).
///
typedef enum {
  kULAnchorFit_None = 0,
  kULAnchorFit_Flip = 1,  ///< Place above the rect when it does not fit beneath.
} ULAnchorFit;

///
/// Placement for a foreground (floating) panel, the flattened form of the C++ Anchor.
///
/// Zero-initialize the desc. Only the fields the declared `kind` reads are meaningful, and
/// the rest stay zero. A zero-initialized desc is the window origin.
///
typedef struct {
  unsigned char kind;    ///< A ULAnchorKind value.
  unsigned char corner;  ///< A ULAnchorCorner value (WindowCorner).
  unsigned char align;   ///< A ULAnchorAlign value (Below).
  unsigned char fit;     ///< A ULAnchorFit value (Below).
  double x;              ///< At: absolute window-space logical px.
  double y;
  double offset_x;       ///< A logical-px nudge applied after any placement form resolves.
  double offset_y;

  ///
  /// Below: the panel the anchor rect is local to (borrowed at the call, the floating
  /// panel keeps its own reference). Must be a live panel of the same window.
  ///
  ULPanel target;

  ///
  /// Below: the anchor rect in the target panel's local logical px, which equals the
  /// hosted page's CSS px (an element rect passes straight through).
  ///
  ULLayoutRect rect;
} ULAnchorDesc;

///
/// Options for creating a foreground (floating) panel.
///
/// A zero-initialized desc creates an unkeyed, full-window floating panel.
///
/// @see  ulForegroundAddPanel()
///
typedef struct {
  uint32_t struct_size;   ///< Must be sizeof(ULForegroundPanelDesc).

  /// Optional identity for lookup and diagnostics (see ULPanelDesc.key).
  const char* key;

  ///
  /// The panel's width. Percent means percent of the window's width. A flex factor is
  /// treated as unset with a warning. Unset means 100%.
  ///
  ULLayoutSize width;

  /// The panel's height (see `width`, percent of the window's height).
  ULLayoutSize height;

  /// Where the panel sits in the window. Zero-initialized means the window origin.
  ULAnchorDesc placement;

  ///
  /// The auto-dismiss policy, as a ULDismiss value (see `<AppCore/CAPI.h>`).
  ///
  /// kULDismiss_Auto hides the panel the way native menus close (a press outside it, Esc, a
  /// window move/resize/deactivation), firing its dismiss callbacks. The closing press is
  /// consumed. (Default = kULDismiss_Manual)
  ///
  unsigned char dismiss;

  /// The keyboard-focus policy, as a ULFocusPolicy value. (Default = kULFocusPolicy_Auto)
  unsigned char focus;

  uint32_t flags;         ///< A ULLayoutFlags combination: Hidden.
} ULForegroundPanelDesc;

///
/// The callback invoked for a layout node event.
///
/// @param  node  The node the event is about. Borrowed. Call ulCreateLayoutNodeRef() to
///               keep it beyond the call.
///
/// @see  ulLayoutNodeSetLayoutChangeCallback(), ulLayoutNodeSetUserResizeCallback()
///
typedef void (*ULLayoutNodeCallback)(void* user_data, ULLayoutNode node);

///
/// The callback invoked for a panel event.
///
/// @param  panel  The panel the event is about. Borrowed. Call ulCreatePanelRef() to keep
///                it beyond the call.
///
/// @see  ulPanelSetDismissCallback()
///
typedef void (*ULPanelCallback)(void* user_data, ULPanel panel);

///
/// The callback invoked when a window's focused panel changes.
///
/// @param  window   The window whose focused panel changed. Borrowed, valid only for the
///                  duration of the call. Keep your own window handle if you need one
///                  afterwards, and never destroy this one. Compare it to your own handle
///                  with ulWindowIsSame(), never `==`.
///
/// @param  focused  The panel that now holds focus, or NULL when no panel does. Borrowed.
///                  Call ulCreatePanelRef() to keep it beyond the call.
///
/// @see  ulWindowSetFocusChangeCallback()
///
typedef void (*ULFocusChangeCallback)(void* user_data, ULWindow window, ULPanel focused);

///
/// The callback invoked when the focused panel's editable state changes.
///
/// @param  window  The window reporting the change. Borrowed, with the same rules as
///                 ULFocusChangeCallback's `window`.
///
/// @param  panel   The panel the state describes, or NULL when no panel holds focus.
///                 Borrowed. Call ulCreatePanelRef() to keep it beyond the call.
///
/// @param  state   The new editable state (see ULEditableState in
///                 `<Ultralight/CAPI/CAPI_Editor.h>`).
///
/// @see  ulWindowSetEditableStateCallback()
///
typedef void (*ULEditableStateCallback)(void* user_data, ULWindow window, ULPanel panel,
                                        ULEditableState state);

///
/// The callback invoked to lay out a container's children manually.
///
/// @param  container    The container being laid out. Borrowed. Call
///                      ulCreateContainerRef() to keep it beyond the call.
///
/// @param  content_box  The container's content box, in container-local logical pixels.
///
/// @see  ulContainerSetLayoutOverride()
///
typedef void (*ULLayoutOverrideCallback)(void* user_data, ULContainer container,
                                         ULLayoutRect content_box);

/******************************************************************************
 * Handle lifetime
 *****************************************************************************/

///
/// Duplicate a layout-node handle.
///
/// @return  Returns a new ULLayoutNode referring to the same node. You must call
///          ulDestroyLayoutNode() when finished, on this handle and the original both.
///
/// @note  Safe to call from any thread.
///
ACExport ULLayoutNode ulCreateLayoutNodeRef(ULLayoutNode node);

///
/// Destroy a layout-node handle.
///
/// Destroying a handle never removes the node from its tree (see ulContainerRemove()).
///
/// @note  Safe to call from any thread.
///
ACExport void ulDestroyLayoutNode(ULLayoutNode node);

///
/// Whether or not the node is still part of a live window's tree.
///
/// @note  Safe to call from any thread. The answer is advisory, since it can change as
///        soon as it is returned.
///
ACExport bool ulLayoutNodeIsAlive(ULLayoutNode node);

///
/// Duplicate a panel handle.
///
/// @return  Returns a new ULPanel referring to the same panel. You must call
///          ulDestroyPanel() when finished, on this handle and the original both.
///
/// @note  Safe to call from any thread.
///
ACExport ULPanel ulCreatePanelRef(ULPanel panel);

///
/// Destroy a panel handle.
///
/// Destroying a handle never removes the panel from its tree (see ulContainerRemove()).
///
/// @note  Safe to call from any thread.
///
ACExport void ulDestroyPanel(ULPanel panel);

///
/// Whether or not the panel is still part of a live window's tree.
///
/// @note  Safe to call from any thread. The answer is advisory.
///
ACExport bool ulPanelIsAlive(ULPanel panel);

///
/// Duplicate a container handle.
///
/// @return  Returns a new ULContainer referring to the same container. You must call
///          ulDestroyContainer() when finished, on this handle and the original both.
///
/// @note  Safe to call from any thread.
///
ACExport ULContainer ulCreateContainerRef(ULContainer container);

///
/// Destroy a container handle.
///
/// Destroying a handle never removes the container from its tree (see ulContainerRemove()).
///
/// @note  Safe to call from any thread.
///
ACExport void ulDestroyContainer(ULContainer container);

///
/// Whether or not the container is still part of a live window's tree.
///
/// @note  Safe to call from any thread. The answer is advisory.
///
ACExport bool ulContainerIsAlive(ULContainer container);

///
/// Duplicate a foreground handle.
///
/// @return  Returns a new ULForeground referring to the same foreground layer. You must
///          call ulDestroyForeground() when finished, on this handle and the original both.
///
/// @note  Safe to call from any thread.
///
ACExport ULForeground ulCreateForegroundRef(ULForeground foreground);

///
/// Destroy a foreground handle.
///
/// Destroying a handle never affects the foreground layer or its floating panels.
///
/// @note  Safe to call from any thread.
///
ACExport void ulDestroyForeground(ULForeground foreground);

///
/// Whether or not the foreground still belongs to a live window.
///
/// @note  Safe to call from any thread. The answer is advisory.
///
ACExport bool ulForegroundIsAlive(ULForeground foreground);

///
/// Whether or not two handles refer to the same node.
///
/// Handles are references, so two lookups of one node return distinct handles that compare
/// equal here.
///
/// @return  Returns false when either handle is NULL, or when the nodes differ.
///
ACExport bool ulLayoutNodeIsSame(ULLayoutNode a, ULLayoutNode b);

///
/// Whether or not two handles refer to the same window.
///
/// The focus-change and editable-state callbacks pass a borrowed window handle that never
/// equals the one ulCreateWindow() returned, so compare the two with this.
///
/// @param  a  The first window handle (can be NULL).
///
/// @param  b  The second window handle (can be NULL).
///
/// @return  Returns false when either handle is NULL, or when the windows differ.
///
ACExport bool ulWindowIsSame(ULWindow a, ULWindow b);

///
/// Get the kind of a layout node.
///
/// @return  Returns the node's kind, or kULLayoutNodeKind_None when `node` is NULL.
///
ACExport ULLayoutNodeKind ulLayoutNodeGetKind(ULLayoutNode node);

///
/// Get a node as a panel.
///
/// @return  Returns a new ULPanel instance, or NULL when the node is not a panel. You must
///          call ulDestroyPanel() when finished.
///
ACExport ULPanel ulLayoutNodeAsPanel(ULLayoutNode node);

///
/// Get a node as a container.
///
/// @return  Returns a new ULContainer instance, or NULL when the node is not a container.
///          You must call ulDestroyContainer() when finished.
///
ACExport ULContainer ulLayoutNodeAsContainer(ULLayoutNode node);

///
/// Get a panel's base layout-node handle.
///
/// @return  Returns a new ULLayoutNode instance (NULL when `panel` is NULL). You must call
///          ulDestroyLayoutNode() when finished.
///
ACExport ULLayoutNode ulPanelAsLayoutNode(ULPanel panel);

///
/// Get a container's base layout-node handle.
///
/// @return  Returns a new ULLayoutNode instance (NULL when `container` is NULL). You must
///          call ulDestroyLayoutNode() when finished.
///
ACExport ULLayoutNode ulContainerAsLayoutNode(ULContainer container);

/******************************************************************************
 * LayoutNode
 *****************************************************************************/

///
/// Get the node's key (empty if unkeyed).
///
/// @return  Returns a new ULString instance. You must call ulDestroyString() when finished.
///
ACExport ULString ulLayoutNodeGetKey(ULLayoutNode node);

///
/// Get the node's parent container.
///
/// @return  Returns a new ULContainer instance, or NULL at the root or when detached. You
///          must call ulDestroyContainer() when finished.
///
ACExport ULContainer ulLayoutNodeGetParent(ULLayoutNode node);

///
/// Get the node's index within its parent.
///
/// @return  Returns the index, or -1 when the node is detached.
///
ACExport int ulLayoutNodeGetIndex(ULLayoutNode node);

///
/// Hide the node, redistributing its space to its siblings.
///
/// The declared size is remembered and restored by ulLayoutNodeShow(). Hiding a container
/// hides its whole subtree.
///
/// @note  A hidden panel's View stops rendering entirely. Painting and animations suspend,
///        and the page receives a `visibilitychange` event (a per-panel signal).
///
ACExport void ulLayoutNodeHide(ULLayoutNode node);

///
/// Show the node again, restoring its remembered size exactly.
///
ACExport void ulLayoutNodeShow(ULLayoutNode node);

///
/// Whether or not the node is hidden.
///
/// This reports the node's own hidden flag, so a node inside a hidden container still
/// reports its own state.
///
ACExport bool ulLayoutNodeIsHidden(ULLayoutNode node);

///
/// Get the node's rect from the most recent layout, in container-local logical pixels.
///
/// This is the same space ulLayoutNodeSetBounds() takes.
///
/// @return  Returns the node's rect, or a zero rect before the node's first layout and after
///          the node is removed or its window closes.
///
ACExport ULLayoutRect ulLayoutNodeGetBounds(ULLayoutNode node);

///
/// Get the node's rect from the most recent layout, in window back-buffer device pixels.
///
/// This is the coordinate space your own drawing uses.
///
/// @return  Returns the node's rect, or a zero rect before the node's first layout and after
///          the node is removed or its window closes.
///
ACExport ULLayoutDeviceRect ulLayoutNodeGetDeviceBounds(ULLayoutNode node);

///
/// Place the node manually, in container-local logical pixels.
///
/// @pre  Only legal inside its container's layout delegate (see
///       ulContainerSetLayoutOverride()). Anywhere else this is a no-op with a warning.
///
ACExport void ulLayoutNodeSetBounds(ULLayoutNode node, ULLayoutRect bounds);

///
/// Set the node's declared size along its container's axis.
///
ACExport void ulLayoutNodeSetSize(ULLayoutNode node, ULLayoutSize size);

///
/// Set the node's minimum size constraint (px or percent, an `fr` unit is ignored with a
/// warning).
///
ACExport void ulLayoutNodeSetMinSize(ULLayoutNode node, ULLayoutSize min_size);

///
/// Set the node's maximum size constraint (px or percent, an `fr` unit is ignored with a
/// warning).
///
ACExport void ulLayoutNodeSetMaxSize(ULLayoutNode node, ULLayoutSize max_size);

/******************************************************************************
 * Container
 *****************************************************************************/

///
/// Create a panel in a container.
///
/// @param  container      The receiving container.
///
/// @param  desc           The panel's options (may be NULL for all defaults).
///
/// @param  view_config    Configuration for the panel's new View (may be NULL for window
///                        defaults). The fields describing the hosting window (device
///                        scale, display id, acceleration) are filled in from the window
///                        for you, and every other field is yours.
///
/// @param  insert_before  The direct child to insert before (may be NULL to append at
///                        the end). An anchor that is not a direct child logs a warning
///                        and the panel appends at the end.
///
/// @return  Returns a new ULPanel instance. If the container was removed or its window
///          closed, it logs a warning and returns a panel that isn't in any window (changes
///          to it do nothing). You must call ulDestroyPanel() when finished.
///
/// @note  To host a deliberately CPU-rendered View on a GPU window, create the View with
///        ulCreateView() and adopt it with ulContainerAdoptPanel().
///
/// @see  Container::AddPanel()
///
ACExport ULPanel ulContainerAddPanel(ULContainer container, const ULPanelDesc* desc,
                                     ULViewConfig view_config, ULLayoutNode insert_before);

///
/// Create a panel that adopts an existing View (including a View created through a
/// Session).
///
/// The panel takes a reference to the View and resizes it to the panel's bounds.
///
/// @param  container      The receiving container.
///
/// @param  desc           The panel's options (may be NULL for all defaults).
///
/// @param  view           The View to adopt. Must not be NULL.
///
/// @param  insert_before  The direct child to insert before (may be NULL to append at
///                        the end).
///
/// @return  Returns a new ULPanel instance. If the container was removed or its window
///          closed, or `view` is NULL, it logs a warning and returns a panel that isn't in any
///          window (changes to it do nothing). You must call ulDestroyPanel() when finished.
///
/// @see  Container::AdoptPanel()
///
ACExport ULPanel ulContainerAdoptPanel(ULContainer container, const ULPanelDesc* desc,
                                       ULView view, ULLayoutNode insert_before);

///
/// Create a child row container (children arranged horizontally).
///
/// @param  container      The receiving container.
///
/// @param  desc           The new container's options (may be NULL for all defaults).
///
/// @param  insert_before  The direct child to insert before (may be NULL to append at
///                        the end).
///
/// @return  Returns a new ULContainer instance. If the container was removed or its window
///          closed, it logs a warning and returns a container that isn't in any window
///          (changes to it do nothing). You must call ulDestroyContainer() when finished.
///
/// @see  Container::AddRow()
///
ACExport ULContainer ulContainerAddRow(ULContainer container, const ULContainerDesc* desc,
                                       ULLayoutNode insert_before);

///
/// Create a child column container (children arranged vertically).
///
/// @param  container      The receiving container.
///
/// @param  desc           The new container's options (may be NULL for all defaults).
///
/// @param  insert_before  The direct child to insert before (may be NULL to append at
///                        the end).
///
/// @return  Returns a new ULContainer instance. If the container was removed or its window
///          closed, it logs a warning and returns a container that isn't in any window
///          (changes to it do nothing). You must call ulDestroyContainer() when finished.
///
/// @see  Container::AddColumn()
///
ACExport ULContainer ulContainerAddColumn(ULContainer container, const ULContainerDesc* desc,
                                          ULLayoutNode insert_before);

///
/// Remove a direct child (and, for a container child, its whole subtree) from the tree.
///
/// Removal detaches the child and drops each removed panel's reference to its View. A View
/// you hold a reference to survives removal and can be adopted into another panel or
/// window. Removed handles stay safe to use, and changes to them do nothing.
///
/// @return  Returns whether or not the node was a direct child and was removed.
///
ACExport bool ulContainerRemove(ULContainer container, ULLayoutNode node);

///
/// Remove every child. Equivalent to ulContainerRemove() on each child in turn.
///
ACExport void ulContainerRemoveAll(ULContainer container);

///
/// Reorder a direct child within its container.
///
/// A node from a different container (or window) is a no-op with a warning. An anchor that
/// is not a direct child falls back to the end, with a warning.
///
/// @param  container    The container holding the child.
///
/// @param  node         The direct child to move.
///
/// @param  move_before  The direct child to move `node` before (may be NULL to move to
///                      the end).
///
/// @return  Returns whether or not the node was reordered.
///
ACExport bool ulContainerMove(ULContainer container, ULLayoutNode node,
                              ULLayoutNode move_before);

///
/// Get the number of children.
///
ACExport int ulContainerGetChildCount(ULContainer container);

///
/// Get the child at an index.
///
/// @return  Returns a new ULLayoutNode instance, or NULL when the index is out of range.
///          You must call ulDestroyLayoutNode() when finished.
///
ACExport ULLayoutNode ulContainerGetChildAt(ULContainer container, int index);

///
/// Find a descendant by key.
///
/// @return  Returns a new ULLayoutNode instance, or NULL when not found. With duplicate
///          keys the most recently created match wins. You must call ulDestroyLayoutNode()
///          when finished.
///
ACExport ULLayoutNode ulContainerFind(ULContainer container, const char* key);

///
/// Find a descendant panel by key.
///
/// @return  Returns a new ULPanel instance, or NULL when not found or the match is not a
///          panel. You must call ulDestroyPanel() when finished.
///
ACExport ULPanel ulContainerFindPanel(ULContainer container, const char* key);

///
/// Find a descendant container by key.
///
/// @return  Returns a new ULContainer instance, or NULL when not found or the match is not
///          a container. You must call ulDestroyContainer() when finished.
///
ACExport ULContainer ulContainerFindContainer(ULContainer container, const char* key);

///
/// Override a container's layout with a layout delegate.
///
/// The delegate runs each time the window performs layout (after any layout change, a resize, or a
/// display scale change). It never runs while the container has no children.
///
/// The delegate places the container's **direct** children by calling ulLayoutNodeSetBounds().
///
/// `content_box` is the container's content box (inside its padding) in container-local logical
/// pixels.
///
/// Placed rects are snapped and clipped to the content box.
///
/// A placed rect persists until the delegate places that child again.
///
/// Children the delegate has never placed occupy no region.
///
/// Pass a NULL `callback` to restore default layout.
///
/// @note  The `Resizable` flag is ignored (with a warning) while a container has a layout
///        delegate. Dividers only exist between children the container itself lays out.
///
ACExport void ulContainerSetLayoutOverride(ULContainer container,
                                           ULLayoutOverrideCallback callback,
                                           void* user_data,
                                           ULUserDataDestroyCallback destroy_user_data);

///
/// Whether or not a container has a layout delegate installed.
///
ACExport bool ulContainerHasLayoutOverride(ULContainer container);

///
/// Set this container's divider style, overriding the window style field by field for the
/// dividers between its own children.
///
/// @param  container  The container whose dividers to style.
///
/// @param  style      The style desc, or NULL to clear the override back to the window
///                    style. A zeroed desc clears it too.
///
/// @note  Negative or non-finite widths and invalid colors are ignored with a warning.
///
/// @see  Container::SetDividerStyle()
///
ACExport void ulContainerSetDividerStyle(ULContainer container,
                                         const ULDividerStyleDesc* style);

/******************************************************************************
 * Panel
 *****************************************************************************/

///
/// Get the panel's hosted View.
///
/// @return  Returns a new ULView instance referring to the panel's View, or NULL after the
///          panel is removed or its window closes. You must call ulDestroyView() when
///          finished, which releases only your reference, never the panel's.
///
ACExport ULView ulPanelGetView(ULPanel panel);

///
/// Grant the panel exclusive keyboard focus.
///
/// You should always focus a panel through this function (or a user click) rather than
/// focusing its View directly, otherwise the panel's window won't know where to route
/// keyboard input.
///
/// @note  Focusing a hidden panel (or one inside a hidden container) is ignored with a
///        warning. Focusing a foreground panel created with kULFocusPolicy_None is ignored too,
///        since such a panel never takes keyboard focus.
///
ACExport void ulPanelFocus(ULPanel panel);

///
/// Bring a floating panel to the top of the foreground layer.
///
/// The panel paints on top of other floating panels and receives mouse input first.
///
/// @note  A tiled panel composites in tree order. Calling this function on a tiled panel is a
///        no-op with a warning.
///
ACExport void ulPanelBringToFront(ULPanel panel);

/******************************************************************************
 * Foreground
 *****************************************************************************/

///
/// Create a floating panel, composited above every tiled panel.
///
/// The panel is sized on both axes against the window and placed per its ULAnchorDesc.
/// Floating panels stack in creation order, refined by ulPanelBringToFront(), and the
/// top-most panel is hit-tested first.
///
/// @param  foreground   The window's foreground layer.
///
/// @param  desc         The panel's options, or NULL for an unkeyed full-window floating
///                      panel.
///
/// @param  view_config  The View configuration, or NULL for the window default. The
///                      fields describing the hosting window (device scale, display id,
///                      acceleration) are filled in from the window for you, and every
///                      other field is yours.
///
/// @return  Returns a new ULPanel instance. If the window has closed, it logs a warning and
///          returns a panel that isn't in any window (changes to it do nothing). You must call
///          ulDestroyPanel() when finished.
///
/// @see  Foreground::AddPanel()
///
ACExport ULPanel ulForegroundAddPanel(ULForeground foreground, const ULForegroundPanelDesc* desc,
                                      ULViewConfig view_config);

///
/// Create a floating panel that adopts an existing View (including one created through a
/// Session, or one that survived a removal).
///
/// The panel takes a reference to the View and resizes it to the panel's bounds.
///
/// @param  foreground  The window's foreground layer.
///
/// @param  desc        The panel's options, or NULL for an unkeyed full-window floating
///                     panel.
///
/// @param  view        The View to adopt. Must not be NULL.
///
/// @return  Returns a new ULPanel instance. If `view` is NULL or the window has closed, it
///          logs a warning and returns a panel that isn't in any window (changes to it do
///          nothing). You must call ulDestroyPanel() when finished.
///
/// @see  Foreground::AdoptPanel()
///
ACExport ULPanel ulForegroundAdoptPanel(ULForeground foreground, const ULForegroundPanelDesc* desc,
                                        ULView view);

///
/// Remove a floating panel from the foreground layer.
///
/// Removal drops the panel's reference to its View, and a reference you hold keeps the View
/// adoptable. The removed handle stays safe to use, and changes to it do nothing.
///
/// @return  Returns whether or not the panel was one of this window's floating panels and
///          was removed. A panel that is not one of them is a no-op with a warning.
///
ACExport bool ulForegroundRemove(ULForeground foreground, ULPanel panel);

///
/// Get the number of floating panels (hidden panels included).
///
ACExport int ulForegroundGetPanelCount(ULForeground foreground);

///
/// Get the floating panel at an index in current z-order, bottom-most first.
///
/// @return  Returns a new ULPanel instance, or NULL when the index is out of range. You
///          must call ulDestroyPanel() when finished.
///
ACExport ULPanel ulForegroundGetPanelAt(ULForeground foreground, int index);

///
/// Find a floating panel by key.
///
/// @return  Returns a new ULPanel instance, or NULL when not found. With duplicate keys the
///          most recently created match wins. You must call ulDestroyPanel() when finished.
///
ACExport ULPanel ulForegroundFindPanel(ULForeground foreground, const char* key);

/******************************************************************************
 * Window entries
 *****************************************************************************/

///
/// Get a window's layout, the root container of the tiled panel tree (a column).
///
/// Build the window's content by adding panels and nested containers to it.
///
/// @return  Returns a new ULContainer instance (after the window closes, changes to it do
///          nothing). You must call ulDestroyContainer() when finished.
///
ACExport ULContainer ulWindowGetLayout(ULWindow window);

///
/// Get a window's foreground layer, which holds floating panels displayed above the
/// window's layout (eg, a toast, an in-window dialog, or a command palette).
///
/// Floating panels clip to the window. For menus and dropdowns that need to extend
/// outside it, use ulCreatePopupWindow() instead.
///
/// @return  Returns a new ULForeground instance (after the window closes, changes to it do
///          nothing). You must call ulDestroyForeground() when finished.
///
/// @see  Window::foreground()
///
ACExport ULForeground ulWindowGetForeground(ULWindow window);

///
/// Configure the window's root container.
///
/// This applies `key`, the Resizable and Hidden flags, `gap`, and `padding` from the desc
/// to the root of the tiled tree. The root always fills the window, so the sizing fields
/// and the Fixed flag are ignored.
///
/// The desc re-applies wholesale on every call, so an unset field restores its default.
///
/// @param  window  The window whose root container to configure.
///
/// @param  desc    The root's options, or NULL to reset the root to defaults.
///
/// @see  Window::BuildLayout()
///
ACExport void ulWindowConfigureLayout(ULWindow window, const ULContainerDesc* desc);

///
/// Add a panel to the window's root container.
///
/// This is shorthand for ulContainerAddPanel() on ulWindowGetLayout() with an append
/// position. A bare panel with a NULL desc fills the whole window.
///
/// @param  window       The window to add the panel to.
///
/// @param  desc         The panel's options (may be NULL for all defaults).
///
/// @param  view_config  Configuration for the panel's new View (may be NULL for window
///                      defaults, see ulContainerAddPanel()).
///
/// @return  Returns a new ULPanel instance, or NULL when `window` is NULL. After the window
///          closes, it logs a warning and returns a panel that isn't in any window (changes to
///          it do nothing). You must call ulDestroyPanel() when finished.
///
ACExport ULPanel ulWindowAddPanel(ULWindow window, const ULPanelDesc* desc,
                                  ULViewConfig view_config);

///
/// Find a panel by key anywhere in the window: the tiled layout first, then the foreground
/// layer.
///
/// @return  Returns a new ULPanel instance, or NULL when not found. You must call
///          ulDestroyPanel() when finished.
///
ACExport ULPanel ulWindowFindPanel(ULWindow window, const char* key);

///
/// Get the panel holding keyboard focus.
///
/// @return  Returns a new ULPanel instance, or NULL when no panel holds focus. You must
///          call ulDestroyPanel() when finished.
///
ACExport ULPanel ulWindowGetFocusedPanel(ULWindow window);

///
/// Take keyboard focus away from every panel.
///
/// As with ulViewUnfocus(), the page's focused element gets a blur event but stays focused
/// in the document, and shows focus again the next time you call ulPanelFocus().
///
/// @note  If a panel had focus, the window's focus-change callback fires with a NULL panel.
///
/// @see  ulPanelFocus(), ulWindowSetFocusChangeCallback()
///
ACExport void ulWindowClearFocus(ULWindow window);

///
/// Set the window-level divider style, the field-by-field fallback for every resizable
/// container that does not override a field itself.
///
/// @param  window  The window whose dividers to style.
///
/// @param  style   The style desc, or NULL to restore the built-in defaults. A zeroed desc
///                 restores them too.
///
/// @note  Negative or non-finite widths and invalid colors are ignored with a warning.
///
/// @see  Window::SetDividerStyle()
///
ACExport void ulWindowSetDividerStyle(ULWindow window, const ULDividerStyleDesc* style);

/******************************************************************************
 * Callbacks
 *****************************************************************************/

///
/// Set callback for when the user resizes the node with a divider.
///
/// It fires on drag release and after a double-click reset, and only when the node's size
/// changed.
///
ACExport void ulLayoutNodeSetUserResizeCallback(ULLayoutNode node,
                                                ULLayoutNodeCallback callback,
                                                void* user_data,
                                                ULUserDataDestroyCallback destroy_user_data);

///
/// Set callback for when layout changes the node's bounds.
///
/// The callback runs at most once per frame.
///
/// Use this callback to follow a node's geometry, such as a reserved region your own drawing
/// fills.
///
ACExport void ulLayoutNodeSetLayoutChangeCallback(ULLayoutNode node,
                                                  ULLayoutNodeCallback callback,
                                                  void* user_data,
                                                  ULUserDataDestroyCallback destroy_user_data);

///
/// Set callback for when the library dismisses a floating panel (an auto-dismiss trigger,
/// see ULForegroundPanelDesc).
///
/// Application-initiated hides never fire it.
///
ACExport void ulPanelSetDismissCallback(ULPanel panel, ULPanelCallback callback,
                                        void* user_data,
                                        ULUserDataDestroyCallback destroy_user_data);

///
/// Set callback for when the window's focused panel changes (the new panel may be NULL).
///
ACExport void ulWindowSetFocusChangeCallback(ULWindow window, ULFocusChangeCallback callback,
                                             void* user_data,
                                             ULUserDataDestroyCallback destroy_user_data);

///
/// Set callback for when the focused panel's editable state changes.
///
/// This is the window-level form of ulViewSetChangeEditableStateCallback(): one callback
/// reports the focused panel's state, so a multi-panel app doesn't need to set one on each
/// View. You can use it to show and hide an on-screen keyboard, for example.
///
/// @note  The window uses each panel View's editable-state hook to track this. If you call
///        ulViewSetChangeEditableStateCallback() on a hosted View, that View's changes stop
///        reaching this callback and the window's input method handling.
///
ACExport void ulWindowSetEditableStateCallback(ULWindow window,
                                               ULEditableStateCallback callback,
                                               void* user_data,
                                               ULUserDataDestroyCallback destroy_user_data);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // APPCORE_CAPI_LAYOUT_H
