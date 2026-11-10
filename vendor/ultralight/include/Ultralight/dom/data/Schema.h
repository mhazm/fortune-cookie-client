///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// @file Schema.h
///
/// Factories and tags for defining a type's data-binding schema.
///
/// `#include <Ultralight/dom/data/Schema.h>`
///
/// @note  This API is a preview and may still change after 2.0.
///
/// A schema declares which members of a native type the page can access. Passing entries to
/// Schema() defines how HTML and CSS display your values and route user input back to native code.
///
/// This struct declares its schema as a `schema` member:
///
/// ```
/// struct Buff {
///   int64_t id = 0;
///   std::string icon;
/// };
///
/// struct Hud {
///   int health = 100;
///   double volume = 0.8;
///   std::vector<Buff> buffs;
///
///   static constexpr auto schema = dd::Schema(
///       dd::Field("health", &Hud::health),                // {{hud.health}}
///       dd::Field("volume", &Hud::volume, dd::Editable),  // ul-value="hud.volume"
///       dd::List("buffs", &Hud::buffs, &Buff::id),        // ul-for="hud.buffs"
///       dd::Action("respawn"),                            // ul-on:click="hud.respawn"
///       dd::Var("--hp", &Hud::health));                   // var(--hp) in CSS
/// };
/// ```
///
/// ## Choosing Schema Entries
///
/// Each task pairs with an entry or tag:
///
/// | What the Page Needs              | Entry or Tag |
/// |----------------------------------|--------------|
/// | Show a value                     | Field()      |
/// | Repeat rows                      | List()       |
/// | Let the page request something   | Action()     |
/// | Set a CSS variable               | Var()        |
/// | Expose all members of a struct   | Reflect()    |
/// | Accept edits from a form control | Editable     |
/// | Hide an entry from page markup   | Internal     |
/// | Deliver only the newest action   | OnlyLatest   |
/// | Check or clamp edits             | Validate()   |
///
/// ## Rules for Every Schema
///
/// - **Apply tags directly to an entry or supply them as standalone declarations.** Add tags like
///   Editable or Internal to Field() or List(), or target an already declared member by name using
///   `dd::Editable("volume")` for properties exposed by Reflect().
/// - **Mistakes fail at compile time.** Duplicate entries and malformed declarations produce
///   compilation errors that identify the violated rule (see Schema()).
/// - **Store the schema in the type itself or in a traits specialization.** Define a
///   `static constexpr auto schema` member inside types you control, or specialize
///   dom::data::TypeTraits for existing classes and lambda accessors (see dom::data::TypeTraits).
/// - **Include this header on its own in a type's header.** A header defining your types needs only
///   `<Ultralight/dom/data/Schema.h>`, while `<Ultralight/dom/data/Context.h>` includes it
///   automatically for code that binds instances.
///
/// @see  dom::data::Schema(), dom::data::TypeTraits, dom::data::Reflect()
///
#pragma once
#include <Ultralight/detail/FixedString.h>
#include <Ultralight/detail/Reflection.h>
#include <Ultralight/dom/data/detail/Accessors.h>
#include <Ultralight/dom/detail/Support.h>

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace ultralight {

class String;

namespace dom {
namespace data {

///
/// Flags on a schema entry.
///
/// You set Editable on fields, Internal on fields and lists, and OnlyLatest on actions. The
/// library sets Keyed, Var, and Nullable from the entry's declaration. Tools read the flags from
/// the schema JSON (see Context::schema()).
///
/// @note  These values stay the same across releases. New flags are added at the end.
///
enum EntryFlags : uint8_t {
  kEntryFlags_Editable = 1 << 0,    ///< The page may request changes to this field.
  kEntryFlags_Internal = 1 << 1,    ///< Pages can't reach this entry (see dd::Internal).
  kEntryFlags_OnlyLatest = 1 << 2,  ///< Only the newest pending action is delivered.

  kEntryFlags_Keyed = 1 << 3,       ///< A list whose rows have a key member.
  kEntryFlags_Var = 1 << 4,         ///< A CSS custom property (see Var()).
  kEntryFlags_Nullable = 1 << 5,    ///< A nested object that may be absent.
};

/// \cond INTERNAL
namespace detail {

inline constexpr size_t kNpos = static_cast<size_t>(-1);

// Consteval-validation failure markers. Reaching one of these while Schema(...) is
// constant-evaluated IS the diagnostic: the compiler reports a non-constant expression and
// names the function, which names the violated rule. Never defined, never reached at
// runtime (the factories are consteval).
void SchemaErrorDuplicateEntryName(const char* name);
void SchemaErrorEntryNameNotAnIdentifier(const char* name);
void SchemaErrorVarNameMustStartWithDashDash(const char* name);
void SchemaErrorVarNameMalformed(const char* name);
void SchemaErrorAnnotationTargetNotFound(const char* name);
void SchemaErrorAnnotationTargetNotAField(const char* name);
void SchemaErrorAnnotationDuplicated(const char* name);
void SchemaErrorValidatorDuplicated(const char* name);
void SchemaErrorValidatorSignatureMismatch(const char* name);
void SchemaErrorValidatorWithoutEditable(const char* name);
void SchemaErrorEditableOnNonLeaf(const char* name);
void SchemaErrorEditableOnStyleValue(const char* name);

template <typename T>
inline constexpr bool kAlwaysFalse = false;

// Stops at the first differing character: the schema checks compare every name pair, and a
// string_view compare would measure both names first.
constexpr bool NamesEqual(const char* a, const char* b) {
  while (*a && *a == *b) {
    a++;
    b++;
  }
  return *a == *b;
}

// The one name grammar of the data-binding system, shared by the consteval schema checks
// and the engine's runtime checks (Bind, DefineFormat, the C type builder). Field, list,
// action, binding, and format names are markup path segments or pipe names:
// [A-Za-z_][A-Za-z0-9_]*. A leading '$' stays free for names the library defines.
constexpr bool IsIdentifierName(std::string_view name) {
  if (name.empty())
    return false;
  for (size_t i = 0; i < name.size(); i++) {
    const char c = name[i];
    const bool alpha = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
    if (!alpha && (i == 0 || c < '0' || c > '9'))
      return false;
  }
  return true;
}

// Var names are CSS custom properties in the lowercase form `ul-var-*` markup produces:
// "--" followed by [a-z0-9-]+.
constexpr bool IsVarName(std::string_view name) {
  if (name.size() < 3 || name[0] != '-' || name[1] != '-')
    return false;
  for (char c : name.substr(2)) {
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'))
      return false;
  }
  return true;
}

consteval void CheckTokenName(const char* name, bool is_var) {
  const std::string_view n(name);
  if (!is_var) {
    if (!IsIdentifierName(n))
      SchemaErrorEntryNameNotAnIdentifier(name);
  } else if (n.size() < 3 || n[0] != '-' || n[1] != '-') {
    SchemaErrorVarNameMustStartWithDashDash(name);
  } else if (!IsVarName(n)) {
    SchemaErrorVarNameMalformed(name);
  }
}

template <typename C>
using ListRow = std::remove_cvref_t<decltype(*std::begin(std::declval<const C&>()))>;

template <typename C>
concept IsIterable = requires(const C& c) {
  std::begin(c);
  std::end(c);
};

}  // namespace detail
/// \endcond

/// \cond INTERNAL

///
/// Absence marker for a field token's validator slot (the default when no Validate tag is
/// attached).
///
struct NoValidate {};

///
/// A Validate annotation attached directly to a Field token, holding the user's callable.
///
/// Produced by Validate(fn); consumed when the owning schema entry is lowered.
///
template <typename Fn>
struct ValidateWith {
  Fn fn;  ///< The validation callable (captureless; see Validate()).
};

///
/// A name-addressed Editable annotation entry, pairing onto an already-declared field.
///
/// Produced by `Editable("name")`; consumed (and removed) by Schema() construction.
///
struct EditableEntry {
  const char* name;  ///< Name of the field the annotation applies to.
};

///
/// A name-addressed Internal annotation entry, pairing onto an already-declared field.
///
/// Produced by `Internal("name")`; consumed (and removed) by Schema() construction.
///
struct InternalEntry {
  const char* name;  ///< Name of the field the annotation applies to.
};

///
/// A name-addressed Validate annotation entry, pairing onto an already-declared field.
///
/// Produced by `Validate("name", fn)`. Unlike Editable/Internal entries it is retained by
/// the constructed schema (the callable's type cannot merge into the field token), and is
/// resolved against its field when the schema is lowered.
///
template <typename Fn>
struct ValidateEntry {
  const char* name;  ///< Name of the field the validator applies to.
  Fn fn;             ///< The validation callable (captureless; see Validate()).
};

/// \endcond

///
/// Marks a field the page may request changes to.
///
/// The page requests a change through a form control bound with `ul-value`. The library never
/// writes the change into your object. It delivers the request to the field's
/// Binding::OnChange() handler inside Context::Sync(), and your handler applies it (with no
/// handler, the change is dropped).
///
/// Use it on the field itself, or as a Schema() entry for a field that's already declared (eg,
/// by Reflect()):
///
/// ```
/// dd::Field("volume", &Settings::volume, dd::Editable)
/// dd::Editable("volume")
/// ```
///
/// @note  Page edits come back only for an Editable bool, number, string, enum, or color field of
///        the bound type itself. On a field of a row or a nested object, `ul-value` only shows the
///        value. `ul-value` on a field that isn't Editable doesn't bind the control.
///
struct EditableTag {
  ///
  /// Mark an already-declared field as Editable (a Schema() entry).
  ///
  /// @param  name  The field's name.
  ///
  /// @return  Returns the entry to pass to Schema().
  ///
  consteval EditableEntry operator()(const char* name) const { return { name }; }
};
inline constexpr EditableTag Editable {};

///
/// Marks a field or list that pages can't reach.
///
/// Markup can't bind to an Internal entry, but your Sync function can still read it with
/// Snapshot::Get(). Use it for values that only steer your sync logic (eg, a revision counter
/// that decides whether to skip work). The schema %JSON (Context::schema()) lists
/// Internal entries with an `internal` flag, and the schema tools leave them out of generated
/// typings and mocks.
///
/// Use it on the entry itself, or as a Schema() entry for one that's already declared:
///
/// ```
/// dd::Field("revision", &Inventory::revision, dd::Internal)
/// dd::Internal("revision")
/// ```
///
struct InternalTag {
  ///
  /// Mark an already-declared field or list as Internal (a Schema() entry).
  ///
  /// @param  name  The entry's name.
  ///
  /// @return  Returns the entry to pass to Schema().
  ///
  consteval InternalEntry operator()(const char* name) const { return { name }; }
};
inline constexpr InternalTag Internal {};

///
/// Marks an action whose pending emissions collapse into the newest one.
///
/// Use it for continuous input like scroll positions or pointer previews:
///
/// ```
/// dd::Action<double>("scroll", dd::OnlyLatest)
/// ```
///
/// Each Context::Sync() then delivers the action at most once with the newest payload. The
/// action keeps the queue position of its first pending emission. Without this tag, every
/// emission is delivered in order.
///
struct OnlyLatestTag {};
inline constexpr OnlyLatestTag OnlyLatest {};

///
/// Attach a validator to a field (a Field() tag).
///
/// A validator checks or corrects a value the page requests before the request is queued for
/// your handler. It takes the requested value and returns the value to accept:
///
/// ```
/// dd::Field("volume", &Settings::volume, dd::Editable,
///           dd::Validate([](float v) { return std::clamp(v, 0.0f, 1.0f); }))
/// ```
///
/// Return a `std::optional` to reject a request (an empty optional). The control then shows the
/// last accepted value again.
///
/// To check the value against other fields, take a second parameter. It gets a Snapshot of the
/// values the page shows. Declare it `auto` when you write the validator inside the
/// schema, since the type isn't complete there:
///
/// ```
/// dd::Field("bid", &Auction::bid, dd::Editable,
///           dd::Validate([](int64_t bid, auto s) -> std::optional<int64_t> {
///             if (bid > s.template Get<"gold">())
///               return std::nullopt;
///             return bid;
///           }))
/// ```
///
/// The value parameter can use the field's own type or the library's form of it (`int64_t` or
/// `double` for numbers, `const String&` for strings). An enum field gets the enum value. A
/// request for a name that isn't one of its enumerators is rejected before your validator runs.
///
/// @param  fn  The validator (a lambda or function object with no captures). A function pointer
///             doesn't compile.
///
/// @return  Returns the tag to pass to Field().
///
/// @note  Validators run on the Renderer's thread, so they must not read or write your objects
///        (use the Snapshot parameter). They run only for page edits of an Editable bool,
///        number, string, enum, or color field of the bound type itself. Values you stage from C
///        (ulDOMDataContextStageChangeBool() and its siblings) skip them.
///
/// @warning  A validator must not throw. Reject a value by returning an empty optional.
///
template <typename Fn>
  requires std::is_class_v<Fn>
consteval ValidateWith<Fn> Validate(Fn fn) {
  return { fn };
}

///
/// Attach a validator to an already-declared field (a Schema() entry).
///
/// Use this form for a field that Reflect() declares. The validator follows the rules of the
/// Field() tag form.
///
/// @param  name  The field's name.
///
/// @param  fn    The validator.
///
/// @return  Returns the entry to pass to Schema().
///
template <typename Fn>
  requires std::is_class_v<Fn>
consteval ValidateEntry<Fn> Validate(const char* name, Fn fn) {
  return { name, fn };
}

///
/// A field entry (the value Field() returns). Pass it to Schema().
///
template <typename Owner, typename Value, typename Accessor, typename Validator = NoValidate>
struct FieldToken {
  using owner_type = Owner;          ///< The type the accessor reads from (`void` without one).
  using value_type = Value;          ///< The field's value type.
  using accessor_type = Accessor;    ///< The accessor's type (`std::nullptr_t` without one).
  using validator_type = Validator;  ///< The type of the validator from Validate(), if any.

  const char* name;  ///< The field's name.
  Accessor accessor;  ///< Reads the value from a const instance.
  ULTRALIGHT_DOM_NO_UNIQUE_ADDRESS Validator validator;  ///< The validator, if any.
  uint8_t flags = 0;  ///< The EntryFlags set on this field.

  ///
  /// Whether or not the field has an accessor.
  ///
  static constexpr bool kHasAccessor = !std::is_same_v<Accessor, std::nullptr_t>;

  ///
  /// Whether or not a validator is attached with the Field() tag form of Validate().
  ///
  static constexpr bool kHasValidator = !std::is_same_v<Validator, NoValidate>;
};

///
/// A list entry (the value List() returns). Pass it to Schema().
///
template <typename Owner, typename Container, typename Accessor, typename KeyPtr>
struct ListToken {
  using owner_type = Owner;          ///< The type the accessor reads from.
  using container_type = Container;  ///< The container type.
  using row_type = detail::ListRow<Container>;  ///< The row type.
  using key_pointer_type = KeyPtr;  ///< The key member pointer's type (else `std::nullptr_t`).

  const char* name;  ///< The list's name.
  Accessor accessor;  ///< Reads the container from a const instance.
  KeyPtr key;         ///< The row member that identifies each row (null without a key).
  uint8_t flags = 0;  ///< The EntryFlags set on this list.

  ///
  /// Whether or not rows have a key (rows are matched by position otherwise).
  ///
  static constexpr bool kKeyed = !std::is_same_v<KeyPtr, std::nullptr_t>;
};

///
/// An action entry (the value Action() returns). Pass it to Schema().
///
template <typename Payload>
struct ActionToken {
  using payload_type = Payload;  ///< The payload type (`void` for none).

  const char* name;   ///< The action's name.
  uint8_t flags = 0;  ///< The EntryFlags set on this action.
};

///
/// A CSS custom property entry (the value Var() returns). Pass it to Schema().
///
template <typename Owner, typename Value, typename Accessor>
struct VarToken {
  using owner_type = Owner;        ///< The type the accessor reads from.
  using value_type = Value;        ///< The accessor's value type.
  using accessor_type = Accessor;  ///< The accessor's type.

  const char* name;   ///< The property's name, including the leading two hyphens.
  Accessor accessor;  ///< Reads the value from a const instance.
};

#if ULTRALIGHT_REFLECTION

///
/// A Reflect() entry (the value Reflect() returns). Pass it to Schema().
///
template <typename T>
struct ReflectEntry {};

///
/// Declare a field for each member of a plain struct (a Schema() entry).
///
/// Each field gets the member's name. Use it when you want every member plus a few
/// annotations:
///
/// ```
/// template <> struct ultralight::dom::data::TypeTraits<ToolbarState> {
///   static constexpr auto schema = dd::Schema(
///       dd::Reflect<ToolbarState>(), dd::Editable("url"),
///       dd::Action("back"), dd::Action("reload"));
/// };
/// ```
///
/// A plain struct with no schema gets the same fields automatically (see TypeTraits).
///
/// @return  Returns the entry to pass to Schema().
///
/// \parblock
/// @note  `T` must be a plain aggregate (no constructors, virtual functions, or C array
///        members) with at most 24 members. It must have external linkage (not declared in an
///        anonymous namespace or inside a function).
/// \endparblock
///
/// \parblock
/// @note  Reflect() needs a complete type, so use it in a TypeTraits specialization (as above)
///        rather than in the type's own `schema` member.
/// \endparblock
///
template <typename T>
consteval ReflectEntry<T> Reflect() {
  static_assert(std::is_aggregate_v<T> && std::is_class_v<T> && !std::is_polymorphic_v<T>,
                "Reflect<T>() requires a plain aggregate class type");
  return {};
}

#endif  // ULTRALIGHT_REFLECTION

}  // namespace data
}  // namespace dom
}  // namespace ultralight

// Schema construction machinery (entry classification, Reflect expansion, the consteval
// validation passes, the constructed Schema type). Definitions must follow the token
// declarations above and precede the factories below.
#include <Ultralight/dom/data/detail/SchemaImpl.h>

namespace ultralight {
namespace dom {
namespace data {

///
/// Declare a field.
///
/// The accessor reads the value from a const instance. It can be a data member pointer, a const
/// member function pointer, or a lambda with no captures that takes a const reference to the
/// instance. The field's type decides what the entry is:
///
///  - **A value type** (bool, a number, a string, an enum, a StyleValue, a Color, or any type
///    with a ValueTraits specialization) is a value pages can show.
///  - **A described type** (a type with a schema) is a nested object. Pages reach its fields
///    with a longer path (eg, `hud.player.name`).
///  - **A pointer, `std::optional`, `std::unique_ptr`, or `std::shared_ptr` to a described
///    type** is a nested object that may be absent (null).
///  - **A container of described rows** (eg, `std::vector<Row>`) is a list whose rows are
///    matched by position. Use List() to give rows a key.
///
/// Any other type fails to compile (eg, `std::vector<float>` or `std::optional<int>`).
///
/// @param  name      The field's name (pages use it in paths like `hud.health`).
///
/// @param  accessor  How to read the value from a const instance.
///
/// @param  tags      Optional tags (dd::Editable, dd::Internal, and one Validate()).
///
/// @return  Returns the entry to pass to Schema().
///
/// @note  A lambda accessor reads the type's members, so it needs the complete type. Declare a
///        schema that uses one in a TypeTraits specialization (inside the type's own `schema`
///        member, the type isn't complete yet).
///
template <detail::IsAccessor A, typename... Tags>
consteval auto Field(const char* name, A accessor, Tags... tags) {
  detail::CheckTokenName(name, false);
  static_assert((int(detail::IsValidateWith<Tags>::value) + ... + 0) <= 1,
                "only one Validate(fn) per Field");
  auto validator = detail::PickValidator(tags...);
  return FieldToken<detail::AccessorOwner<A>, detail::AccessorValue<A>, A,
                    decltype(validator)> { name, accessor, validator,
                                           detail::FieldFlags(tags...) };
}

///
/// Declare a field without an accessor.
///
/// Give the value type as the template argument, since there's no accessor to take it from
/// (eg, `dd::Field<int>("score")`). The type's Sync function writes the value with Model::Set().
///
/// @param  name  The field's name.
///
/// @param  tags  Optional tags, as in the accessor form.
///
/// @return  Returns the entry to pass to Schema().
///
/// @note  The library's default sync skips a field without an accessor, so declare a Sync
///        function for the type (see TypeTraits).
///
template <typename Value, typename... Tags>
  requires (!detail::IsAccessor<Value>)
consteval auto Field(const char* name, Tags... tags) {
  detail::CheckTokenName(name, false);
  static_assert((int(detail::IsValidateWith<Tags>::value) + ... + 0) <= 1,
                "only one Validate(fn) per Field");
  auto validator = detail::PickValidator(tags...);
  return FieldToken<void, Value, std::nullptr_t, decltype(validator)> {
    name, nullptr, validator, detail::FieldFlags(tags...)
  };
}

///
/// Declare a keyed list.
///
/// The accessor returns an iterable container of rows (by reference or by value), and each row
/// is a described type. The key is the row member that identifies a row. When the list
/// reorders, each row keeps its elements in the page:
///
/// ```
/// dd::List("roster", &Match::roster, &PlayerRow::id)
/// ```
///
/// @param  name      The list's name.
///
/// @param  accessor  How to read the container from a const instance.
///
/// @param  key       A pointer to the row member that identifies each row. The member must be
///                   an integer, an enum, or a string.
///
/// @param  tags      Optional tags (dd::Internal).
///
/// @return  Returns the entry to pass to Schema().
///
/// @note  Keys must be unique among the rows. With duplicates, the library logs a warning and
///        the last row wins.
///
template <detail::IsAccessor A, typename KeyPtr, typename... Tags>
  requires std::is_member_object_pointer_v<KeyPtr>
consteval auto List(const char* name, A accessor, KeyPtr key, Tags... tags) {
  detail::CheckTokenName(name, false);
  using Container = detail::AccessorValue<A>;
  static_assert(detail::IsIterable<Container>,
                "List accessor must return an iterable container of rows");
  static_assert(std::is_base_of_v<detail::AccessorOwner<KeyPtr>, detail::ListRow<Container>>,
                "List key must be a member pointer of the row type");
  return ListToken<detail::AccessorOwner<A>, Container, A, KeyPtr> {
    name, accessor, key, detail::ListFlags(tags...)
  };
}

///
/// Declare a list whose rows are matched by position.
///
/// This suits a list that only grows at the end. When rows reorder, each position keeps its
/// elements and shows the new row's values. Use the keyed form when rows move.
///
/// @param  name      The list's name.
///
/// @param  accessor  How to read the container from a const instance.
///
/// @param  tags      Optional tags (dd::Internal).
///
/// @return  Returns the entry to pass to Schema().
///
template <detail::IsAccessor A, typename... Tags>
  requires (!std::is_member_object_pointer_v<Tags> && ...)
consteval auto List(const char* name, A accessor, Tags... tags) {
  detail::CheckTokenName(name, false);
  using Container = detail::AccessorValue<A>;
  static_assert(detail::IsIterable<Container>,
                "List accessor must return an iterable container of rows");
  return ListToken<detail::AccessorOwner<A>, Container, A, std::nullptr_t> {
    name, accessor, nullptr, detail::ListFlags(tags...)
  };
}

///
/// Declare an action (a request the page sends to your code).
///
/// Pages fire an action from markup (eg, `ul-on:click="hud.respawn"`), and your code can send
/// one with Binding::PostAction(). Handlers registered with Binding::OnAction() receive it inside
/// Context::Sync().
///
/// To send a payload with each action, give its type as the template argument:
///
/// ```
/// dd::Action("respawn")               // no payload
/// dd::Action<MoveItem>("moveItem")    // a described struct of value fields
/// dd::Action<double>("seek")          // one bool, number, string, or enum
/// ```
///
/// In the schema JSON, a single-value payload is a struct with one field, `value`. Handlers
/// receive the value itself.
///
/// @param  name  The action's name.
///
/// @param  tags  Optional tags (dd::OnlyLatest).
///
/// @return  Returns the entry to pass to Schema().
///
/// \parblock
/// @note  Actions fired from markup carry no payload, so a handler that takes a payload never
///        receives them.
/// \endparblock
///
/// \parblock
/// @note  The action queue holds 1024 actions by default, and a full queue drops new ones
///        (see Context::set_action_queue_capacity()).
/// \endparblock
///
template <typename Payload = void, typename... Tags>
consteval ActionToken<Payload> Action(const char* name, Tags... tags) {
  detail::CheckTokenName(name, false);
  return ActionToken<Payload> { name, detail::ActionFlags(tags...) };
}

///
/// Declare a CSS custom property that follows your data.
///
/// The page needs no markup for it. On the bound type itself, the property is set on the page's
/// root element, so the whole document can use it. On a row type, it's set on each row's root
/// element:
///
/// ```
/// dd::Var("--hp", &HUD::health)   // .bar { width: calc(var(--hp) * 1%); }
/// ```
///
/// The accessor returns a number (a value without a unit), a StyleValue (a value with a unit),
/// or a Color (written as hex, eg `#ff8000`).
///
/// @param  name      The property's name, starting with two hyphens.
///
/// @param  accessor  How to read the value from a const instance.
///
/// @return  Returns the entry to pass to Schema().
///
/// \parblock
/// @note  A Var on a type that's only used as a nested object is never set.
/// \endparblock
///
/// \parblock
/// @note  A change to a Var on the bound type makes every style in the page that uses it
///        recompute. For a value that changes every frame, prefer a `ul-var-*` attribute on the
///        element that uses it. If two bindings set the same property on the root element, the
///        last one written wins and the library logs a warning.
/// \endparblock
///
template <detail::IsAccessor A>
consteval auto Var(const char* name, A accessor) {
  detail::CheckTokenName(name, true);
  return VarToken<detail::AccessorOwner<A>, detail::AccessorValue<A>, A> { name, accessor };
}

///
/// Declare a type's schema.
///
/// A schema lists what pages can show and do with a type. Store it in the type as
/// `static constexpr auto schema`, or in a TypeTraits specialization for a type you can't edit:
///
/// ```
/// struct HUD {
///   int health = 100;
///   std::vector<Buff> buffs;
///
///   static constexpr auto schema = dd::Schema(
///       dd::Field("health", &HUD::health),
///       dd::List("buffs", &HUD::buffs, &Buff::id),
///       dd::Action("respawn"),
///       dd::Var("--hp", &HUD::health));
/// };
/// ```
///
/// Entries keep the order you declare them in (Reflect() adds its fields in member order).
///
/// ## Compile-Time Checks
///
/// Schema() runs at compile time, and these mistakes fail to compile:
///
///  - **Two entries with the same name.**
///  - **A malformed name.** A name starts with a letter or an underscore, followed by letters,
///    digits, and underscores (eg, `maxHealth` or `slot_2`). A Var() name is two hyphens
///    followed by lowercase letters, digits, and hyphens (eg, `--max-hp`).
///  - **An annotation for a missing entry**, or for an entry it doesn't apply to (eg,
///    dd::Editable on a list).
///  - **A second Editable, Internal, or validator on the same field.**
///  - **A validator that doesn't fit its field's type**, or one on a field that isn't
///    dd::Editable (validators only check page edits).
///  - **dd::Editable on a field that isn't a bool, number, string, enum, or color** (eg, a
///    nested object or a StyleValue). This check runs where the type is bound.
///
/// Look for a function like `SchemaErrorDuplicateEntryName` in the compiler's error. It states
/// the rule the schema breaks.
///
/// @param  entries  The entries in order (Field(), List(), Action(), Var(), and Reflect()), plus
///                  any annotations for declared entries (`dd::Editable("name")`,
///                  `dd::Internal("name")`, and `dd::Validate("name", fn)`).
///
/// @return  Returns the schema.
///
template <typename... Entries>
consteval auto Schema(Entries... entries) {
  return detail::BuildSchema(entries...);
}

}  // namespace data
}  // namespace dom
}  // namespace ultralight
