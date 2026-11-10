///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/CAPI/CAPI_DOMData.h>
#include <Ultralight/dom/data/ActionInfo.h>
#include <Ultralight/dom/data/Holders.h>
#include <Ultralight/dom/data/Value.h>
#include <Ultralight/dom/data/detail/Delivery.h>
#include <Ultralight/dom/data/detail/Emission.h>
#include <Ultralight/dom/data/detail/Receivers.h>

#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace ultralight {
namespace dom {
namespace data {

/// \cond INTERNAL
namespace detail {

// The parameter list of a non-generic callable (a lambda or function object with one
// non-template operator()). Generic lambdas and overloaded function objects have no single
// signature, so their forms stay checked by invocability alone.
template <typename Fn, typename = void>
struct HandlerSignature {
  static constexpr bool kKnown = false;
};
template <typename Fn>
struct HandlerSignature<Fn, std::void_t<decltype(&Fn::operator())>> {
  static constexpr bool kKnown = true;
  using args = typename MemberCallableSig<decltype(&Fn::operator())>::args;
};

// Whether a one-parameter non-generic handler's parameter decays to one of `Forms`. The
// delivery code tests invocability, which any implicit conversion passes (a `bool` handler
// on a double field); this pins the parameter to the documented forms. Handlers of any other
// arity, and generic ones, are left to the delivery code's own checks.
template <typename Fn, typename... Forms>
consteval bool HandlerParamIsOneOf() {
  using Plain = std::remove_cvref_t<Fn>;
  if constexpr (!HandlerSignature<Plain>::kKnown) {
    return true;
  } else {
    using Args = typename HandlerSignature<Plain>::args;
    if constexpr (std::tuple_size_v<Args> != 1)
      return true;
    else
      return (std::is_same_v<std::decay_t<std::tuple_element_t<0, Args>>, Forms> || ...);
  }
}

template <typename V, ValueKind kKind, typename Fn>
consteval void CheckChangeHandlerForm() {
  if constexpr (kKind == ValueKind::Color) {
    static_assert(HandlerParamIsOneOf<Fn, Color>(),
                  "dom::data: an OnChange handler for a color field takes an "
                  "ultralight::Color");
  } else if constexpr (kKind == ValueKind::String && std::is_enum_v<V>) {
    static_assert(HandlerParamIsOneOf<Fn, V, Value>(),
                  "dom::data: an OnChange handler for an enum field takes the enum value "
                  "(or a dd::Value)");
  } else if constexpr (kKind == ValueKind::String) {
    static_assert(HandlerParamIsOneOf<Fn, V, ultralight::String, std::string_view, Value>(),
                  "dom::data: an OnChange handler for a string field takes the field's "
                  "value type, a const String&, or a dd::Value");
  } else if constexpr (kKind == ValueKind::Bool) {
    static_assert(HandlerParamIsOneOf<Fn, V, bool, Value>(),
                  "dom::data: an OnChange handler takes the field's value type (or its "
                  "slot type, or a dd::Value)");
  } else {
    static_assert(HandlerParamIsOneOf<Fn, V, int64_t, double, Value>(),
                  "dom::data: an OnChange handler takes the field's value type (or its "
                  "slot type, or a dd::Value)");
  }
}

template <typename P, typename Fn>
consteval void CheckRootActionHandlerForm() {
  constexpr bool kOk = [] {
    if constexpr (std::is_void_v<P>)
      return HandlerParamIsOneOf<Fn, Value>();
    else
      return HandlerParamIsOneOf<Fn, std::decay_t<P>, Value>();
  }();
  static_assert(kOk,
                "dom::data: an OnAction handler takes nothing, the action's declared "
                "payload (its struct by const reference, or the scalar by value), a "
                "dd::Value, or (dd::Value, dd::ActionInfo)");
}

template <typename R, ValueKind kKeyKind, typename Fn>
consteval void CheckRowActionHandlerForm() {
  using Key = std::conditional_t<kKeyKind == ValueKind::String, std::string_view, int64_t>;
  static_assert(HandlerParamIsOneOf<Fn, R, Key, Value>(),
                "dom::data: a row OnAction handler takes nothing, the row type by const "
                "reference, the row key (int64_t, or std::string_view for string-keyed "
                "lists), a dd::Value, or (dd::Value, dd::ActionInfo)");
}

}  // namespace detail
/// \endcond

///
/// A handle that keeps an instance bound to pages and registers its input handlers.
///
/// A Binding keeps a native instance bound under the name page markup uses for as long as you keep
/// the handle. It's where you register the handlers for that instance's edits and actions from the
/// page.
///
/// Store the handle alongside your data to keep the binding active for the lifetime of your
/// component.
///
/// This HUD component stores its binding as a member variable and routes an action to a member
/// function:
///
/// ```
/// class Hud {
///  public:
///   explicit Hud(dd::Context& ctx)
///       : binding_(ctx.Bind("player", player_)
///                      .OnAction<"usePotion">(&Player::UsePotion)) {}
///
///  private:
///   Player player_;
///   dd::Binding<Player> binding_;  // unbinds when the Hud is destroyed
/// };
/// ```
///
/// ## Handling Input
///
/// Call OnChange() to handle edits to an Editable field from a `ul-value` form control on the page.
/// The handler receives the new value, but the library never writes directly to your data. Store
/// the value in your instance to accept the edit, or leave the field unchanged to decline it and
/// let the control snap back at the end of the sync.
///
/// This handler accepts volume edits by writing the new value back to the settings struct:
///
/// ```
/// dd::Binding settings_binding = ctx.Bind("settings", settings);
/// settings_binding.OnChange<"volume">([&](double volume) {
///   settings.volume = volume;  // storing the value accepts the edit
/// });
/// ```
///
/// Call OnAction() to receive actions triggered by `ul-on` markup on the page or posted through
/// PostAction(). A dotted name like `"items.drop"` routes actions from list rows. Handlers run
/// during Context::Sync() on the context's home thread, delivering all value edits before running
/// any actions. Registering a handler for a field or action that already has one replaces the
/// previous handler.
///
/// ## Handler Forms
///
/// Both OnChange() and OnAction() route events to any of these targets:
///
/// - **A callable receives the event directly.** Pass a lambda or function object that accepts the
///   field value, the action payload, or no arguments.
/// - **A borrowed receiver calls a member function on an existing object.** The library doesn't
///   copy the object, so you must keep it alive while the binding exists.
/// - **A smart pointer manages receiver lifetime.** An owning pointer like `std::shared_ptr` keeps
///   the target alive, while a `std::weak_ptr` skips deliveries once its object is gone.
/// - **A member function of the bound type runs on the bound instance.** It does nothing if you
///   bound a `const` instance.
///
/// ## Posting Actions from Native Code
///
/// Call PostAction() to send an action from native code as if page markup triggered it. The action
/// enters the context's action queue and reaches its handler during the next Context::Sync().
/// PostAction() is safe to call from any thread, which lets background threads hand work to the
/// thread that owns the data. It returns `false` if the queue is full or if the binding stopped
/// working.
///
/// This queues an action for delivery at the next sync:
///
/// ```
/// // From any thread (eg, the input thread's potion hotkey):
/// binding.PostAction<"usePotion">();
/// ```
///
/// ## Binding Lifetime
///
/// Destroying a Binding unbinds the instance and releases its handlers.
///
/// The unbind timing depends on which thread destroys the handle:
///
/// - **On the home thread, unbinding happens immediately.** The context stops reading the instance
///   right away.
/// - **From any other thread, unbinding takes effect at the start of the next Context::Sync().**
///   The binding receives no further input during that synchronization cycle.
///
/// Binding the same name again replaces the earlier binding, and destroying the Context ends all
/// its bindings. A replaced binding or one whose Context was destroyed remains safe to hold, but
/// its handlers are dropped and PostAction() returns `false`.
///
/// Inspect the state of a handle using these methods:
///
/// - **IsAlive() checks whether the binding is active.** It returns `false` when the binding was
///   replaced or its Context was destroyed, even though the `bool` test stays `true`.
/// - **IsEmpty() checks whether the handle holds nothing.** It returns `true` after a move or when
///   Context::Bind() fails.
///
/// This checks whether a binding remains active after another component bound the same name:
///
/// ```
/// dd::Binding hud = ctx.Bind("player", player);
/// dd::Binding menu = ctx.Bind("player", player);  // replaces hud's binding
///
/// if (!hud.IsAlive())
///   Log("hud's binding was replaced");
/// ```
///
/// @note  Actions are requests that can arrive after the state that prompted them changed, so your
///        handler should verify that prerequisites still hold before applying the request.
///
/// @warning  Calling ulDOMDataBindingSetChangeCallback() or ulDOMDataBindingSetActionCallback() on
///           raw() replaces this Binding's typed handlers, and registering typed handlers replaces
///           any C callbacks.
///
/// @see  dom::data::Context::Bind(), dom::data::Context::PostAction(), dom::data::ActionInfo,
///       dom::data::Value, dom::data::Editable
///
template <typename T>
class [[nodiscard]] Binding {
 public:
  ///
  /// The bound type.
  ///
  using described_type = T;

  ///
  /// Create an empty Binding.
  ///
  Binding() = default;

  Binding(const Binding&) = delete;
  Binding& operator=(const Binding&) = delete;

  ///
  /// Move constructor (`other` becomes empty).
  ///
  /// @param  other  The Binding to move from.
  ///
  Binding(Binding&& other) noexcept
      : handle_(other.handle_), handlers_(other.handlers_),
        access_(std::move(other.access_)) {
    other.handle_ = nullptr;
    other.handlers_ = nullptr;
    other.access_ = nullptr;
  }

  ///
  /// Move assignment (releases the binding this one held, then takes over `other`'s).
  ///
  /// @param  other  The Binding to move from.
  ///
  /// @return  Returns this Binding.
  ///
  Binding& operator=(Binding&& other) noexcept {
    if (this != &other) {
      ulDestroyDOMDataBinding(handle_);
      handle_ = other.handle_;
      handlers_ = other.handlers_;
      access_ = std::move(other.access_);
      other.handle_ = nullptr;
      other.handlers_ = nullptr;
      other.access_ = nullptr;
    }
    return *this;
  }

  ///
  /// Destroy this Binding (see Binding Lifetime in the class description).
  ///
  ~Binding() { ulDestroyDOMDataBinding(handle_); }

  ///
  /// Whether or not this Binding holds a binding (false for an empty Binding, eg, after a move
  /// or a failed Context::Bind()).
  ///
  /// @note  It stays true after the binding name is bound again (see Binding Lifetime in the
  ///        class description). Use IsAlive() to check whether the binding still works.
  ///
  explicit operator bool() const { return handle_ != nullptr; }

  ///
  /// Whether or not this Binding holds nothing (eg, after a move or a failed Context::Bind()).
  ///
  /// @return  Returns true for an empty Binding.
  ///
  /// @note  Safe to call from any thread.
  ///
  bool IsEmpty() const { return handle_ == nullptr; }

  ///
  /// Whether or not this Binding still works.
  ///
  /// @return  Returns false for an empty Binding, or one that stopped working (its binding name was
  ///          bound again, or its Context was destroyed).
  ///
  /// @note  Safe to call from any thread.
  ///
  bool IsAlive() const { return ulDOMDataBindingIsAlive(handle_); }

  ///
  /// Register the handler for edits to an Editable field (replacing any earlier one).
  ///
  /// The handler runs during Context::Sync() after the user edits a `ul-value` control bound to
  /// the field. It runs at most once per field per Sync (with the latest value) and before any
  /// action. Store the value in your instance to accept it:
  ///
  /// ```
  /// b.OnChange<"volume">([&](float v) { settings.volume = v; audio.SetVolume(v); });
  /// ```
  ///
  /// The handler takes the value as one of these:
  ///
  /// - **The field's type** (or `int64_t` or `double` for a number field).
  /// - **`const String&`** for a string field, or `std::string_view` for a std::string field
  ///   (valid only during the call).
  /// - **ultralight::Color** for a color field (its only form).
  /// - **dd::Value** for any field except a color.
  ///
  /// A parameter of another type doesn't compile, even one the value converts to (eg, `bool`
  /// for a number field).
  ///
  /// If the field has a validator (see Validate()), the handler gets the value it accepted.
  ///
  /// @param  fn  The handler.
  ///
  /// @return  Returns this Binding (for chaining).
  ///
  /// @note  The field name and the handler are checked at compile time. The field must be an
  ///        Editable bool, number, string, enum, or color field of the bound type itself (not a
  ///        row or a nested object).
  ///
  /// @warning  For a field whose type has your own ValueTraits specialization, a handler that takes
  ///           that type still compiles when the type can be constructed from the kind's C++ type
  ///           (eg, `double`). The handler then receives the raw value converted by that
  ///           constructor rather than your ValueTraits specialization. Take the kind's C++ type or
  ///           a dd::Value instead (see ValueTraits).
  ///
  template <ultralight::detail::FixedString Field, typename Fn>
    requires(!std::is_member_pointer_v<std::decay_t<Fn>>)
  Binding& OnChange(Fn&& fn) & {
    constexpr std::string_view path = Field.view();
    static_assert(path.find('.') == std::string_view::npos,
                  "dom::data: nested and row change delivery is not yet available; "
                  "OnChange registers root-level Editable fields");
    if constexpr (path.find('.') == std::string_view::npos) {
      constexpr size_t I = detail::SchemaIndexOf<T>(path);
      static_assert(I != detail::kNpos,
                    "dom::data: the bound type's schema has no entry with this name");
      if constexpr (I != detail::kNpos) {
        using SchemaType = std::remove_cvref_t<decltype(TypeTraits<T>::schema)>;
        using Tok = std::tuple_element_t<I, SchemaType>;
        static_assert(detail::IsFieldToken<Tok>::value
                          && detail::EntryInfo<Tok>::kClass == detail::EntryClass::kLeaf,
                      "dom::data: OnChange registers leaf Field entries");
        if constexpr (detail::IsFieldToken<Tok>::value
                      && detail::EntryInfo<Tok>::kClass == detail::EntryClass::kLeaf) {
          static_assert(
              (TypeTraits<T>::schema.template get<I>().flags & kEntryFlags_Editable) != 0,
              "dom::data: OnChange registers Editable fields (changes are never staged "
              "for a field the UI cannot edit)");
          detail::CheckChangeHandlerForm<std::remove_cvref_t<typename Tok::value_type>,
                                         detail::EntryInfo<Tok>::kKind, Fn>();
          detail::WithBindingOwnerTurn(handle_, [&] {
            if (auto* table = EnsureHandlers()) {
              table->changes[static_cast<uint32_t>(I)]
                  = detail::MakeChangeHandler<T, I>(std::forward<Fn>(fn));
            }
          });
        }
      }
    }
    return *this;
  }

  ///
  /// Register a member function of `receiver` as the handler for edits to an Editable field.
  ///
  /// Works like the callable form of OnChange().
  ///
  /// @param  receiver  The object to call `method` on. It isn't copied, so keep it alive while
  ///                   the handler can run.
  ///
  /// @param  method    The member function to call.
  ///
  /// @return  Returns this Binding (for chaining).
  ///
  template <ultralight::detail::FixedString Field, typename C, typename M>
    requires(std::is_member_function_pointer_v<M> && !LockableHolder<C>)
  Binding& OnChange(C& receiver, M method) & {
    return OnChange<Field>(detail::WrapBorrowedMember(receiver, method));
  }

  ///
  /// Register a member function as the handler for edits to an Editable field, called through
  /// a smart pointer.
  ///
  /// Works like the callable form of OnChange().
  ///
  /// @param  holder  The smart pointer to the object (copied). A std::shared_ptr keeps its object
  ///                 alive while the handler is registered. A std::weak_ptr skips deliveries once
  ///                 its object is gone.
  ///
  /// @param  method  The member function to call.
  ///
  /// @return  Returns this Binding (for chaining).
  ///
  template <ultralight::detail::FixedString Field, typename H, typename M>
    requires(LockableHolder<H> && std::is_member_function_pointer_v<M>)
  Binding& OnChange(H holder, M method) & {
    return OnChange<Field>(detail::WrapHolderMember(std::move(holder), method));
  }

  ///
  /// Register a member function of the bound type as the handler for edits to an Editable
  /// field, called on the bound instance.
  ///
  /// Works like the callable form of OnChange(). The handler runs only when Context::Bind() got
  /// an instance it can change (see Handler Forms in the class description).
  ///
  /// @param  method  The member function to call.
  ///
  /// @return  Returns this Binding (for chaining).
  ///
  template <ultralight::detail::FixedString Field, typename M>
    requires std::is_member_function_pointer_v<M>
  Binding& OnChange(M method) & {
    return OnChange<Field>(detail::WrapImplicitMember<T>(access_, method));
  }

  ///
  /// Register the handler for edits to an Editable field on a Binding you just created.
  ///
  /// This lets a chain start at Context::Bind() and end in the variable you keep. It takes the
  /// same arguments as the other forms of OnChange().
  ///
  /// @param  args  The handler (a callable, an object or smart pointer and a member function, or
  ///               a member function of the bound type).
  ///
  /// @return  Returns this Binding by value.
  ///
  template <ultralight::detail::FixedString Field, typename... A>
  [[nodiscard]] Binding OnChange(A&&... args) && {
    this->template OnChange<Field>(std::forward<A>(args)...);
    return std::move(*this);
  }

  ///
  /// Register the handler for an action (replacing any earlier one).
  ///
  /// The handler runs during Context::Sync() after all changes, in the order the actions fired.
  /// Its parameter list decides what it receives:
  ///
  /// ```
  /// b.OnAction<"voteSkip">([&] { match.VoteSkip(); });                   // nothing
  /// b.OnAction<"moveItem">([&](const MoveItem& move) { ... });           // the payload
  /// b.OnAction<"roster.focusPlayer">([&](const ScoreRow& row) { ... });  // the row
  /// b.OnAction<"roster.focusPlayer">([&](int64_t key) { ... });          // the row key
  /// b.OnAction<"save">([&](dd::Value payload, dd::ActionInfo info) { ... });
  /// ```
  ///
  /// - **No parameters** works for every action (any payload is ignored).
  /// - **The declared payload** (a struct by const reference, or a scalar like `double` by value).
  ///   An action whose payload doesn't match is dropped. Actions from `ul-on:*` markup have no
  ///   payload, so this form never receives them.
  /// - **dd::Value** holds the payload. You can add a dd::ActionInfo parameter after it (the
  ///   action name and row key).
  /// - **The row type** (by const reference, for a row action) holds the row's values as of the
  ///   last Context::Sync(). The action is dropped if no row has that key anymore (or, in a list
  ///   without keys, that position).
  /// - **The row key** (for a row action) is an `int64_t` (in a list without keys, the row's
  ///   position when the page fired the action) or a `std::string_view` for string keys (valid
  ///   only during the call).
  ///
  /// A row action on a list without keys addresses the row by its position when the page fired
  /// it, so rows added or removed before the next Sync() make it reach another row (the page
  /// warns when it compiles one). Declare a key for a list whose rows can move.
  ///
  /// The row type form rebuilds the row, so the row type must be default-constructible and every
  /// field in its schema must read a data member (a member pointer or a Reflect() field) of a
  /// bool, number, enum (only when ULTRALIGHT_REFLECTION is 1), StyleValue, or Color type, or of a
  /// string type constructible from a `const char*` and a length (eg, `std::string`). A row with
  /// a list, a nested object, a Var(), a getter or lambda accessor, or a `std::string_view` member
  /// doesn't compile in this form, so take the row key instead.
  ///
  /// A dotted name (`list.action`) addresses an action of a list's row type. Only one level is
  /// supported, so actions of a nested object or of a list inside a row can't be registered. The
  /// name and the handler are checked at compile time, and a parameter must have one of these
  /// types exactly (eg, a `bool` parameter for a `double` payload doesn't compile).
  ///
  /// @param  fn  The handler.
  ///
  /// @return  Returns this Binding (for chaining).
  ///
  template <ultralight::detail::FixedString Name, typename Fn>
    requires(!std::is_member_pointer_v<std::decay_t<Fn>>)
  Binding& OnAction(Fn&& fn) & {
    constexpr std::string_view path = Name.view();
    constexpr size_t dot = path.find('.');
    if constexpr (dot == std::string_view::npos) {
      constexpr size_t I = detail::SchemaIndexOf<T>(path);
      static_assert(I != detail::kNpos,
                    "dom::data: the bound type's schema has no entry with this name");
      if constexpr (I != detail::kNpos) {
        using SchemaType = std::remove_cvref_t<decltype(TypeTraits<T>::schema)>;
        using Tok = std::tuple_element_t<I, SchemaType>;
        static_assert(detail::IsActionToken<Tok>::value,
                      "dom::data: OnAction registers Action entries (the named entry is "
                      "not an Action)");
        if constexpr (detail::IsActionToken<Tok>::value) {
          detail::CheckRootActionHandlerForm<typename Tok::payload_type, Fn>();
          detail::WithBindingOwnerTurn(handle_, [&] {
            if (auto* table = EnsureHandlers()) {
              table->actions[std::make_pair(detail::kRootActionSlot,
                                            static_cast<uint32_t>(I))]
                  = detail::MakeRootActionHandler<T, I>(std::forward<Fn>(fn));
            }
          });
        }
      }
    } else {
      static_assert(path.find('.', dot + 1) == std::string_view::npos,
                    "dom::data: one dotted level is supported (list.action); deeper "
                    "paths are not yet deliverable");
      constexpr size_t L = detail::SchemaIndexOf<T>(path.substr(0, dot));
      static_assert(L != detail::kNpos,
                    "dom::data: the bound type's schema has no entry named by the first "
                    "path segment");
      if constexpr (L != detail::kNpos && path.find('.', dot + 1) == std::string_view::npos) {
        using SchemaType = std::remove_cvref_t<decltype(TypeTraits<T>::schema)>;
        using LTok = std::tuple_element_t<L, SchemaType>;
        static_assert(detail::EntryInfo<LTok>::kClass != detail::EntryClass::kObject
                          && detail::EntryInfo<LTok>::kClass
                              != detail::EntryClass::kNullableObject,
                      "dom::data: actions behind object fields are not yet deliverable");
        static_assert(detail::EntryInfo<LTok>::kClass == detail::EntryClass::kList,
                      "dom::data: OnAction's dotted form addresses an action of a list's "
                      "row type");
        if constexpr (detail::EntryInfo<LTok>::kClass == detail::EntryClass::kList) {
          using R = typename detail::EntryInfo<LTok>::child_type;
          constexpr size_t A = detail::SchemaIndexOf<R>(path.substr(dot + 1));
          static_assert(A != detail::kNpos,
                        "dom::data: the row type's schema has no entry named by the "
                        "second path segment");
          if constexpr (A != detail::kNpos) {
            using RTok = std::tuple_element_t<
                A, std::remove_cvref_t<decltype(TypeTraits<R>::schema)>>;
            static_assert(detail::IsActionToken<RTok>::value,
                          "dom::data: OnAction registers Action entries (the named row "
                          "entry is not an Action)");
            if constexpr (detail::IsActionToken<RTok>::value) {
              constexpr ValueKind kKeyKind = [] {
                if constexpr (detail::IsListToken<LTok>::value)
                  return detail::EntryInfo<LTok>::KeyKind();
                else
                  return ValueKind::Int64;  // A Field-declared list is matched by position.
              }();
              detail::CheckRowActionHandlerForm<R, kKeyKind, Fn>();
              detail::WithBindingOwnerTurn(handle_, [&] {
                if (auto* table = EnsureHandlers()) {
                  table->actions[std::make_pair(static_cast<uint32_t>(L),
                                                static_cast<uint32_t>(A))]
                      = detail::MakeRowActionHandler<T, L, A>(std::forward<Fn>(fn));
                }
              });
            }
          }
        }
      }
    }
    return *this;
  }

  ///
  /// Register a member function of `receiver` as the handler for an action.
  ///
  /// Works like the callable form of OnAction().
  ///
  /// @param  receiver  The object to call `method` on. It isn't copied, so keep it alive while
  ///                   the handler can run.
  ///
  /// @param  method    The member function to call.
  ///
  /// @return  Returns this Binding (for chaining).
  ///
  template <ultralight::detail::FixedString Name, typename C, typename M>
    requires(std::is_member_function_pointer_v<M> && !LockableHolder<C>)
  Binding& OnAction(C& receiver, M method) & {
    return OnAction<Name>(detail::WrapBorrowedMember(receiver, method));
  }

  ///
  /// Register a member function as the handler for an action, called through a smart pointer.
  ///
  /// Works like the callable form of OnAction().
  ///
  /// @param  holder  The smart pointer to the object (copied). A std::shared_ptr keeps its object
  ///                 alive while the handler is registered. A std::weak_ptr skips deliveries once
  ///                 its object is gone.
  ///
  /// @param  method  The member function to call.
  ///
  /// @return  Returns this Binding (for chaining).
  ///
  template <ultralight::detail::FixedString Name, typename H, typename M>
    requires(LockableHolder<H> && std::is_member_function_pointer_v<M>)
  Binding& OnAction(H holder, M method) & {
    return OnAction<Name>(detail::WrapHolderMember(std::move(holder), method));
  }

  ///
  /// Register a member function of the bound type as the handler for an action, called on the
  /// bound instance.
  ///
  /// Works like the callable form of OnAction(). The handler runs only when Context::Bind() got
  /// an instance it can change (see Handler Forms in the class description).
  ///
  /// @param  method  The member function to call.
  ///
  /// @return  Returns this Binding (for chaining).
  ///
  template <ultralight::detail::FixedString Name, typename M>
    requires std::is_member_function_pointer_v<M>
  Binding& OnAction(M method) & {
    return OnAction<Name>(detail::WrapImplicitMember<T>(access_, method));
  }

  ///
  /// Register the handler for an action on a Binding you just created.
  ///
  /// This lets a chain start at Context::Bind() and end in the variable you keep. It takes the
  /// same arguments as the other forms of OnAction().
  ///
  /// @param  args  The handler (a callable, an object or smart pointer and a member function, or
  ///               a member function of the bound type).
  ///
  /// @return  Returns this Binding by value.
  ///
  template <ultralight::detail::FixedString Name, typename... A>
  [[nodiscard]] Binding OnAction(A&&... args) && {
    this->template OnAction<Name>(std::forward<A>(args)...);
    return std::move(*this);
  }

  ///
  /// Remove the handler for edits to an Editable field (if there is one).
  ///
  /// The field name is checked at compile time like OnChange().
  ///
  /// @return  Returns this Binding (for chaining).
  ///
  template <ultralight::detail::FixedString Field>
  Binding& RemoveChangeHandler() & {
    constexpr std::string_view path = Field.view();
    static_assert(path.find('.') == std::string_view::npos,
                  "dom::data: nested and row change delivery is not yet available; "
                  "change handlers register root-level Editable fields");
    if constexpr (path.find('.') == std::string_view::npos) {
      constexpr size_t I = detail::SchemaIndexOf<T>(path);
      static_assert(I != detail::kNpos,
                    "dom::data: the bound type's schema has no entry with this name");
      if constexpr (I != detail::kNpos) {
        using SchemaType = std::remove_cvref_t<decltype(TypeTraits<T>::schema)>;
        using Tok = std::tuple_element_t<I, SchemaType>;
        static_assert(detail::IsFieldToken<Tok>::value
                          && detail::EntryInfo<Tok>::kClass == detail::EntryClass::kLeaf,
                      "dom::data: change handlers address leaf Field entries");
        if constexpr (detail::IsFieldToken<Tok>::value
                      && detail::EntryInfo<Tok>::kClass == detail::EntryClass::kLeaf) {
          static_assert(
              (TypeTraits<T>::schema.template get<I>().flags & kEntryFlags_Editable) != 0,
              "dom::data: change handlers address Editable fields (changes are never "
              "staged for a field the UI cannot edit)");
          detail::WithBindingOwnerTurn(handle_, [&] {
            if (auto* table = FindHandlers())
              table->changes.erase(static_cast<uint32_t>(I));
          });
        }
      }
    }
    return *this;
  }

  ///
  /// Remove the handler for edits to an Editable field on a Binding you just created.
  ///
  /// @return  Returns this Binding by value.
  ///
  template <ultralight::detail::FixedString Field>
  [[nodiscard]] Binding RemoveChangeHandler() && {
    this->template RemoveChangeHandler<Field>();
    return std::move(*this);
  }

  ///
  /// Remove the handler for an action (if there is one).
  ///
  /// The action name is checked at compile time like OnAction() (a dotted `list.action` name
  /// works too).
  ///
  /// @return  Returns this Binding (for chaining).
  ///
  template <ultralight::detail::FixedString Name>
  Binding& RemoveActionHandler() & {
    constexpr std::string_view path = Name.view();
    constexpr size_t dot = path.find('.');
    if constexpr (dot == std::string_view::npos) {
      constexpr size_t I = detail::SchemaIndexOf<T>(path);
      static_assert(I != detail::kNpos,
                    "dom::data: the bound type's schema has no entry with this name");
      if constexpr (I != detail::kNpos) {
        using SchemaType = std::remove_cvref_t<decltype(TypeTraits<T>::schema)>;
        using Tok = std::tuple_element_t<I, SchemaType>;
        static_assert(detail::IsActionToken<Tok>::value,
                      "dom::data: action handlers address Action entries (the named "
                      "entry is not an Action)");
        if constexpr (detail::IsActionToken<Tok>::value) {
          detail::WithBindingOwnerTurn(handle_, [&] {
            if (auto* table = FindHandlers()) {
              table->actions.erase(
                  std::make_pair(detail::kRootActionSlot, static_cast<uint32_t>(I)));
            }
          });
        }
      }
    } else {
      static_assert(path.find('.', dot + 1) == std::string_view::npos,
                    "dom::data: one dotted level is supported (list.action); deeper "
                    "paths are not yet deliverable");
      constexpr size_t L = detail::SchemaIndexOf<T>(path.substr(0, dot));
      static_assert(L != detail::kNpos,
                    "dom::data: the bound type's schema has no entry named by the first "
                    "path segment");
      if constexpr (L != detail::kNpos && path.find('.', dot + 1) == std::string_view::npos) {
        using SchemaType = std::remove_cvref_t<decltype(TypeTraits<T>::schema)>;
        using LTok = std::tuple_element_t<L, SchemaType>;
        static_assert(detail::EntryInfo<LTok>::kClass == detail::EntryClass::kList,
                      "dom::data: the dotted form addresses an action of a list's row "
                      "type");
        if constexpr (detail::EntryInfo<LTok>::kClass == detail::EntryClass::kList) {
          using R = typename detail::EntryInfo<LTok>::child_type;
          constexpr size_t A = detail::SchemaIndexOf<R>(path.substr(dot + 1));
          static_assert(A != detail::kNpos,
                        "dom::data: the row type's schema has no entry named by the "
                        "second path segment");
          if constexpr (A != detail::kNpos) {
            using RTok = std::tuple_element_t<
                A, std::remove_cvref_t<decltype(TypeTraits<R>::schema)>>;
            static_assert(detail::IsActionToken<RTok>::value,
                          "dom::data: action handlers address Action entries (the named "
                          "row entry is not an Action)");
            if constexpr (detail::IsActionToken<RTok>::value) {
              detail::WithBindingOwnerTurn(handle_, [&] {
                if (auto* table = FindHandlers()) {
                  table->actions.erase(
                      std::make_pair(static_cast<uint32_t>(L), static_cast<uint32_t>(A)));
                }
              });
            }
          }
        }
      }
    }
    return *this;
  }

  ///
  /// Remove the handler for an action on a Binding you just created.
  ///
  /// @return  Returns this Binding by value.
  ///
  template <ultralight::detail::FixedString Name>
  [[nodiscard]] Binding RemoveActionHandler() && {
    this->template RemoveActionHandler<Name>();
    return std::move(*this);
  }

  ///
  /// Send one of this binding's actions that has no payload, as if the page fired it.
  ///
  /// ```
  /// b.PostAction<"voteSkip">();
  /// ```
  ///
  /// The action reaches its handler at the next Context::Sync(), like an action from the page.
  /// The name is checked at compile time and must be an action of the bound type itself (not a
  /// `list.action` name).
  ///
  /// @return  Returns true if the action was queued. Returns false for an empty Binding, one that
  ///          stopped working (see IsAlive()), or a full action queue.
  ///
  /// @note  Safe to call from any thread.
  ///
  /// @see  Context::PostAction()
  ///
  template <ultralight::detail::FixedString Name>
  bool PostAction() {
    constexpr std::string_view path = Name.view();
    static_assert(path.find('.') == std::string_view::npos,
                  "dom::data: PostAction addresses root actions (row-scoped actions fire "
                  "from bound markup with their row's identity)");
    if constexpr (path.find('.') == std::string_view::npos) {
      constexpr size_t I = detail::SchemaIndexOf<T>(path);
      static_assert(I != detail::kNpos,
                    "dom::data: the bound type's schema has no entry with this name");
      if constexpr (I != detail::kNpos) {
        using SchemaType = std::remove_cvref_t<decltype(TypeTraits<T>::schema)>;
        using Tok = std::tuple_element_t<I, SchemaType>;
        static_assert(detail::IsActionToken<Tok>::value,
                      "dom::data: PostAction emits Action entries (the named entry is "
                      "not an Action)");
        if constexpr (detail::IsActionToken<Tok>::value) {
          static_assert(std::is_void_v<typename Tok::payload_type>,
                        "dom::data: this action declares a payload; pass it as "
                        "PostAction's argument");
          if constexpr (std::is_void_v<typename Tok::payload_type>) {
            return detail::PostBindingAction(handle_, static_cast<unsigned int>(I));
          }
        }
      }
    }
    return false;
  }

  ///
  /// Send one of this binding's actions with its payload, as if the page fired it.
  ///
  /// ```
  /// b.PostAction<"moveItem">({ .from = a, .to = b });  // dd::Action<MoveItem>
  /// b.PostAction<"seek">(0.5f);                        // dd::Action<double>
  /// ```
  ///
  /// Works like the form without a payload.
  ///
  /// @param  payload  The payload (of the action's declared type).
  ///
  /// @return  Returns true if the action was queued. Returns false for an empty Binding, one that
  ///          stopped working (see IsAlive()), or a full action queue.
  ///
  /// @note  Safe to call from any thread.
  ///
  template <ultralight::detail::FixedString Name>
  bool PostAction(const detail::ActionPayloadArg<T, Name>& payload) {
    constexpr std::string_view path = Name.view();
    static_assert(path.find('.') == std::string_view::npos,
                  "dom::data: PostAction addresses root actions (row-scoped actions fire "
                  "from bound markup with their row's identity)");
    if constexpr (path.find('.') == std::string_view::npos) {
      constexpr size_t I = detail::SchemaIndexOf<T>(path);
      static_assert(I != detail::kNpos,
                    "dom::data: the bound type's schema has no entry with this name");
      if constexpr (I != detail::kNpos) {
        using SchemaType = std::remove_cvref_t<decltype(TypeTraits<T>::schema)>;
        using Tok = std::tuple_element_t<I, SchemaType>;
        static_assert(detail::IsActionToken<Tok>::value,
                      "dom::data: PostAction emits Action entries (the named entry is "
                      "not an Action)");
        if constexpr (detail::IsActionToken<Tok>::value) {
          static_assert(!std::is_void_v<typename Tok::payload_type>,
                        "dom::data: this action declares no payload; call PostAction "
                        "with no argument");
          if constexpr (!std::is_void_v<typename Tok::payload_type>) {
            if constexpr (Described<typename Tok::payload_type>) {
              static_assert(
                  detail::PayloadMarshalable<typename Tok::payload_type>(),
                  "dom::data: a payload type carries plain data fields only (readable "
                  "leaf members of boolean, integral, floating-point, string, or "
                  "reflected enum type)");
              std::vector<std::string> strings;
              std::vector<ULDOMDataPayloadEntry> entries;
              detail::MarshalPayload(payload, strings, entries);
              return detail::PostBindingAction(handle_, static_cast<unsigned int>(I),
                                               entries.empty() ? nullptr : entries.data(),
                                               entries.size());
            } else {
              std::string backing;
              ULDOMDataPayloadEntry entry {};
              detail::MarshalScalarPayload(payload, backing, entry);
              return detail::PostBindingAction(handle_, static_cast<unsigned int>(I), &entry,
                                               1);
            }
          }
        }
      }
    }
    return false;
  }

  ///
  /// Get the binding name.
  ///
  /// @return  Returns the binding name, or an empty string for an empty Binding or one whose
  ///          name was bound again.
  ///
  /// @note  Call this on the home thread (unlike PostAction(), which is safe from any thread).
  ///
  std::string name() const {
    if (!handle_)
      return std::string();
    ULString text = ulDOMDataBindingGetName(handle_);
    if (!text)
      return std::string();
    std::string result(ulStringGetData(text), ulStringGetLength(text));
    ulDestroyString(text);
    return result;
  }

  // --- Interop with the C API (most embedders never touch raw handles) -------------------

  ///
  /// Wrap a C handle you own, taking ownership of it.
  ///
  /// @param  handle  A handle from the C API that you would otherwise destroy with
  ///                 ulDestroyDOMDataBinding() (NULL gives an empty Binding).
  ///
  /// @return  Returns a Binding that releases `handle` when it's done.
  ///
  static Binding Adopt(ULDOMDataBinding handle) { return Binding(handle); }

  ///
  /// Wrap a C handle someone else owns (eg, another Binding's raw()), adding a reference.
  ///
  /// @param  handle  The borrowed handle (NULL gives an empty Binding).
  ///
  /// @return  Returns a Binding with its own reference.
  ///
  static Binding FromBorrowed(ULDOMDataBinding handle) {
    return Binding(handle ? ulCreateDOMDataBindingRef(handle) : nullptr);
  }

  ///
  /// Get the C handle, for passing to the `<Ultralight/CAPI/CAPI_DOMData.h>` functions.
  ///
  /// @return  Returns the handle (NULL for an empty Binding). This Binding still owns it, so
  ///          don't destroy it.
  ///
  ULDOMDataBinding raw() const { return handle_; }

  ///
  /// Give up ownership of the C handle and return it. This Binding becomes empty.
  ///
  /// @return  Returns the handle. You must call ulDestroyDOMDataBinding() when finished.
  ///
  ULDOMDataBinding LeakRef() {
    ULDOMDataBinding handle = handle_;
    handle_ = nullptr;
    handlers_ = nullptr;
    access_ = nullptr;
    return handle;
  }

 private:
  friend class Context;
  explicit Binding(ULDOMDataBinding handle) : handle_(handle) {}

  // Find or install the type-erased handler table behind both C callback slots. Returns
  // null for an empty or stale guard (its name was rebound, or the binding is gone), so
  // registration degrades to a benign no-op and a freed table is never touched.
  //
  // The table's identity lives on the BINDING (its installed change slot), never in this
  // guard alone: every guard to one underlying binding probes the slot and adopts the
  // table it already carries, so a second guard extends the shared registration set
  // instead of replacing (and freeing) the first guard's table.
  detail::HandlerTable* EnsureHandlers() {
    if (!handle_ || !ulDOMDataBindingGetStagingTable(handle_))
      return nullptr;
    ULDOMDataChangeCallback current = nullptr;
    void* current_data = nullptr;
    ulDOMDataBindingGetChangeCallback(handle_, &current, &current_data);
    if (current == &detail::HandlerTableChangeThunk) {
      handlers_ = static_cast<detail::HandlerTable*>(current_data);
      return handlers_;
    }
    auto* table = new detail::HandlerTable();
    table->refs = 3;  // Both callback slots, plus this frame's registration probe.
    table->context = detail::BorrowBindingContext(handle_);
    ulDOMDataBindingSetChangeCallback(handle_, &detail::HandlerTableChangeThunk, table,
                                      &detail::ReleaseHandlerTable);
    ulDOMDataBindingSetActionCallback(handle_, &detail::HandlerTableActionThunk, table,
                                      &detail::ReleaseHandlerTable);
    const bool registered = table->refs == 3;
    detail::ReleaseHandlerTable(table);
    if (!registered)
      return nullptr;
    handlers_ = table;
    return handlers_;
  }

  // Find the installed handler table WITHOUT installing one (the removal path): null
  // when the guard is empty or its page is gone, or when no typed registration exists yet.
  detail::HandlerTable* FindHandlers() {
    if (!handle_)
      return nullptr;
    ULDOMDataChangeCallback current = nullptr;
    void* current_data = nullptr;
    ulDOMDataBindingGetChangeCallback(handle_, &current, &current_data);
    if (current == &detail::HandlerTableChangeThunk) {
      handlers_ = static_cast<detail::HandlerTable*>(current_data);
      return handlers_;
    }
    return nullptr;
  }

  ULDOMDataBinding handle_ = nullptr;
  detail::HandlerTable* handlers_ = nullptr;
  // The implicit-receiver forms' route to the bound instance; empty when the bind cannot
  // grant mutable access (const binds, const-element holders, raw-handle guards).
  detail::InstanceAccess<T> access_;
};

}  // namespace data
}  // namespace dom
}  // namespace ultralight
