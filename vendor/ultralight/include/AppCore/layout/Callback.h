///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <AppCore/Defines.h>
#include <Ultralight/Geometry.h>
#include <Ultralight/RefPtr.h>

namespace ultralight {

class Container;
class LayoutNode;
class LayoutRegistration;
class Panel;
class Window;
struct EditableState;

///
/// Destroy hook for a callback registration's user data.
///
/// @param  user_data  The user data you passed to the registration method.
///
/// @note  This is invoked exactly once, when the registration finally drops the user data. It is
///        never invoked while the callback itself is executing.
///
typedef void (*LayoutDestroyUserDataCallback)(void* user_data);

///
/// Callback fired for a layout node event.
///
/// @param  user_data  The user data you passed to the registration method.
///
/// @param  node       The node the event is for.
///
/// @see  LayoutNode::OnLayoutChange()
///
typedef void (*LayoutNodeCallback)(void* user_data, LayoutNode* node);

///
/// Callback fired for a panel event.
///
/// @param  user_data  The user data you passed to the registration method.
///
/// @param  panel      The panel the event is for.
///
/// @see  Panel::OnDismiss()
///
typedef void (*LayoutPanelCallback)(void* user_data, Panel* panel);

///
/// Callback fired when a window's focused panel changes.
///
/// @param  user_data  The user data you passed to the registration method.
///
/// @param  window     The window whose focus changed.
///
/// @param  focused    The newly focused panel (can be null when nothing holds panel focus).
///
/// @see  Window::OnFocusChange()
///
typedef void (*LayoutFocusCallback)(void* user_data, Window* window, Panel* focused);

///
/// Callback fired when the focused panel's editable state changes.
///
/// @param  user_data  The user data you passed to the registration method.
///
/// @param  window     The window the state belongs to.
///
/// @param  panel      The panel the state describes (can be null when nothing holds panel
///                    focus).
///
/// @param  state      The new editable state.
///
/// @see  Window::OnEditableStateChange()
///
typedef void (*LayoutEditableStateCallback)(void* user_data, Window* window, Panel* panel,
                                            const EditableState& state);

///
/// Layout delegate, called to place a container's children yourself.
///
/// @param  user_data    The user data you passed to the registration method.
///
/// @param  container    The container being laid out.
///
/// @param  content_box  The container's content box, in container-local logical pixels.
///
/// @see  Container::SetLayoutOverride()
///
typedef void (*LayoutOverrideCallback)(void* user_data, Container* container, Rect content_box);

///
/// Scoped handle that controls how long a layout callback stays registered.
///
/// Methods that register layout and panel callbacks return a LayoutCallback. The callback stays
/// active for as long as this handle exists.
///
/// To keep receiving events, store the handle in an object whose lifetime matches the listener.
///
/// This status bar keeps a focus-change callback as a member variable:
///
/// ```
/// class StatusBar {
///  public:
///   explicit StatusBar(Window* window)
///       : on_focus_(window->OnFocusChange([](Panel* focused) {
///           ShowFocusedKey(focused ? focused->key() : String());
///         })) {}
///
///  private:
///   LayoutCallback on_focus_;  // unregisters when the StatusBar goes away
/// };
/// ```
///
/// ## Callback Lifetime
///
/// A registered callback stops firing when:
///
/// - **The handle is destroyed.** Discarding the return value of a registration call unregisters
///   the callback immediately.
/// - **You call Remove().** The callback unregisters early, even from inside the callback itself.
/// - **The target node is removed or its window closes.**
///
/// Call IsActive() to check whether the callback can still fire.
///
/// ## Detaching Callbacks
///
/// To keep a callback running without storing its handle, call Detach(). The callback remains
/// registered for the rest of the node's lifetime. Once detached, the callback can't be removed.
///
/// Chain Detach() onto the registration call:
///
/// ```
/// sidebar->OnLayoutChange([] { Relayout(); }).Detach();
/// ```
///
/// @note  Callbacks must not let exceptions propagate.
///
/// @see  LayoutNode::OnLayoutChange(), LayoutNode::OnUserResize(), Panel::OnDismiss(),
///       Window::OnFocusChange(), Window::OnEditableStateChange()
///
class AExport LayoutCallback {
 public:
  ///
  /// Create an empty LayoutCallback (it owns no registration and IsActive() is false).
  ///
  LayoutCallback();

  LayoutCallback(const LayoutCallback&) = delete;
  LayoutCallback& operator=(const LayoutCallback&) = delete;

  ///
  /// Move constructor (transfers the registration, `other` becomes empty).
  ///
  LayoutCallback(LayoutCallback&& other) noexcept;

  ///
  /// Move assignment. Removes any registration this object held, then takes over `other`'s.
  ///
  LayoutCallback& operator=(LayoutCallback&& other) noexcept;

  ///
  /// Unregister the callback (if it is still registered) and release the registration.
  ///
  ~LayoutCallback();

  ///
  /// Whether or not the callback can still fire. This is false for an empty object, after
  /// Remove(), and once the node is removed or its window closes.
  ///
  explicit operator bool() const { return IsActive(); }

  ///
  /// The named form of operator bool.
  ///
  bool IsActive() const;

  ///
  /// Stop receiving callbacks now.
  ///
  /// You can call this more than once, after the node is removed or its window closes, and
  /// from inside the callback itself.
  ///
  void Remove();

  ///
  /// Leave the callback registered for the rest of its node's lifetime and give up this object.
  ///
  /// The callback keeps firing until the node is removed or its window closes. There is no way
  /// to remove it afterwards.
  ///
  void Detach();

  /// \cond ignore
  static LayoutCallback Adopt(RefPtr<LayoutRegistration> registration);

  LayoutRegistration* registration() const;
  /// \endcond

 private:
  explicit LayoutCallback(RefPtr<LayoutRegistration> registration);
  void Reset();

  RefPtr<LayoutRegistration> registration_;
};

}  // namespace ultralight
