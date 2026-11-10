///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/dom/Holders.h>
#include <Ultralight/dom/data/Schema.h>
#include <Ultralight/dom/data/ValueTraits.h>

#include <array>
#include <type_traits>

namespace ultralight {
namespace dom {
namespace data {

/// \cond INTERNAL
namespace detail {

template <typename T>
struct IsStdArray : std::false_type {};
template <typename E, size_t N>
struct IsStdArray<std::array<E, N>> : std::true_type {};

// The intrusive detector admits only a static member that actually is a constructed
// schema, so a coincidental member named `schema` cannot select the intrusive tier.
template <typename T>
concept HasIntrusiveSchema = requires { T::schema; }
    && IsSchemaInstance<std::remove_cvref_t<decltype(T::schema)>>::value;

#if ULTRALIGHT_REFLECTION
// The automatic tier's admission envelope: a plain aggregate class (no unions, nothing
// polymorphic, not std::array) that is not already a bindable leaf.
template <typename T>
inline constexpr bool kIsAutoReflectable = std::is_aggregate_v<T> && std::is_class_v<T>
    && !std::is_polymorphic_v<T> && !IsStdArray<T>::value && !Bindable<T>;
#endif

// The library's built-in schemas. TypeTraits inherits this tier, so a user specialization of
// TypeTraits always takes priority, and a release can add built-ins here without colliding
// with specializations users already wrote.
template <typename T, typename = void>
struct BuiltinTypeTraits {};

// The intrusive tier: forwards the type's own schema (and Sync, when declared).
template <typename T>
struct BuiltinTypeTraits<T, std::enable_if_t<HasIntrusiveSchema<T>>> {
  static constexpr auto schema = T::schema;

  template <typename M>
  static constexpr void Sync(const T& instance, M model)
    requires requires { T::Sync(instance, model); }
  {
    T::Sync(instance, model);
  }
};

#if ULTRALIGHT_REFLECTION
// The automatic tier: plain aggregates reflect their members.
template <typename T>
struct BuiltinTypeTraits<T, std::enable_if_t<!HasIntrusiveSchema<T> && kIsAutoReflectable<T>>> {
  static constexpr auto schema = data::Schema(data::Reflect<T>());
};
#endif

}  // namespace detail
/// \endcond

///
/// Traits template for describing a type to data bindings.
///
/// The library uses TypeTraits to find the schema that exposes a C++ type to page markup.
/// Specializing this template describes a type from outside its declaration, letting you bind
/// existing application classes to HTML and CSS without modifying their headers.
///
/// This specialization describes an existing class with member functions and nested data:
///
/// ```
/// class Player {
///  public:
///   const std::string& nickname() const;
///   int level() const;
///   const Stats& stats() const;
/// };
///
/// template <> struct dd::TypeTraits<Player> {
///   static constexpr auto schema = dd::Schema(
///       dd::Field("nickname", &Player::nickname),  // player.nickname
///       dd::Field("level", &Player::level),        // player.level
///       dd::Field("stats", &Player::stats));       // player.stats.armor, ...
/// };
/// ```
///
/// ## Choosing a Schema Approach
///
/// How you expose a native type depends on whether you can modify its declaration:
///
/// | Type                                         | Approach                  |
/// |----------------------------------------------|---------------------------|
/// | Existing class                               | TypeTraits specialization |
/// | Type written for UI                          | `schema` member           |
/// | Plain struct                                 | Automatic reflection      |
/// | Plain struct with actions or editable fields | Reflect() in TypeTraits   |
///
/// When a type qualifies for more than one approach, the library resolves them in priority order:
///
/// - **A TypeTraits specialization takes priority over every other schema.**
/// - **A `schema` member takes priority over automatic reflection.** A struct with a `schema`
///   member exposes only the entries that member declares.
///
/// ## Reflecting and Annotating
///
/// You can combine automatic reflection with explicit annotations by calling Reflect() inside a
/// schema. This exposes every member of a plain struct while letting you add actions, mark fields
/// editable, or attach validators by name. Because Reflect() needs a complete type to inspect its
/// members, the schema must go in a TypeTraits specialization rather than inside the struct itself.
///
/// This specialization reflects a toolbar struct and attaches annotations by name:
///
/// ```
/// struct Toolbar {
///   std::string address;
///   double progress = 0;
/// };
///
/// template <> struct dd::TypeTraits<Toolbar> {
///   static constexpr auto schema = dd::Schema(
///       dd::Reflect<Toolbar>(),  // address and progress
///       dd::Editable("address"),
///       dd::Action("reload"));
/// };
/// ```
///
/// ## Custom Sync Functions
///
/// A static `Sync()` function optimizes updates by skipping unchanged data, such as gating a
/// collection behind a revision counter. When a type has a TypeTraits specialization, the library
/// ignores a static `Sync()` declared on the type itself, so the function has to go in the
/// specialization. It receives the native instance and a dom::data::Model to selectively update
/// properties. See dom::data::Model for how to synchronize individual fields.
///
/// \parblock
/// @note  Automatic reflection requires Clang, GCC, or MSVC 19.40+. The struct must be a plain
///        aggregate with at most 24 members, without custom constructors, virtual functions, or C
///        array members.
/// \endparblock
///
/// \parblock
/// @note  dom::data::TypeTraits is unrelated to js::TypeTraits from the JavaScript API.
/// \endparblock
///
/// @see  dom::data::Schema(), dom::data::Reflect(), dom::data::Model, dom::data::ValueTraits,
///       dom::data::Described
///
template <typename T, typename = void>
struct TypeTraits : detail::BuiltinTypeTraits<T> {};

///
/// Whether or not T has a schema (ignoring const, volatile, and references), ie. it's a
/// described type (see TypeTraits).
///
template <typename T>
concept Described = requires { TypeTraits<std::remove_cvref_t<T>>::schema; }
    && detail::IsSchemaInstance<std::remove_cvref_t<
        decltype(TypeTraits<std::remove_cvref_t<T>>::schema)>>::value;

}  // namespace data
}  // namespace dom
}  // namespace ultralight
