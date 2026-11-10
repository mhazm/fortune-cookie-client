///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// Implementation detail of the `<Ultralight/dom/data/*.h>` headers; include those
/// instead. Everything here lives in ultralight::dom::data::detail and may change without
/// notice.
///
/// Entry classification and schema queries: how a schema entry's TYPES determine what the
/// entry is (a bindable leaf, a nested or nullable object, a list of rows, an action, a
/// var) and which slot kind it stores. Classification runs here, where the whole trait
/// surface is visible, not at token construction.
///
/// \cond INTERNAL
#pragma once
#include <Ultralight/dom/data/TypeTraits.h>
#include <Ultralight/dom/data/ValueTraits.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <type_traits>

namespace ultralight {
namespace dom {
namespace data {
namespace detail {

// A field's kind comes from its value type's traits, never its declaration: a bindable
// leaf, a described sub-object, a nullable described sub-object (pointer/optional/smart
// pointer; null means absent), or a container of described rows (an index-keyed list).

template <typename V>
struct NullableInfo {
  static constexpr bool value = false;
};
template <typename U>
struct NullableInfo<U*> {
  static constexpr bool value = true;
  using pointee = std::remove_cv_t<U>;
};
template <typename U>
struct NullableInfo<std::optional<U>> {
  static constexpr bool value = true;
  using pointee = U;
};
template <typename U, typename D>
struct NullableInfo<std::unique_ptr<U, D>> {
  static constexpr bool value = true;
  using pointee = std::remove_cv_t<U>;
};
template <typename U>
struct NullableInfo<std::shared_ptr<U>> {
  static constexpr bool value = true;
  using pointee = std::remove_cv_t<U>;
};

enum class EntryClass : uint8_t {
  kLeaf,
  kObject,
  kNullableObject,
  kList,
  kAction,
  kVar,
};

template <typename Token>
struct EntryInfo;

template <typename O, typename V, typename A, typename Va>
struct EntryInfo<FieldToken<O, V, A, Va>> {
  static consteval EntryClass Classify() {
    if constexpr (Bindable<V>) {
      return EntryClass::kLeaf;
    } else if constexpr (NullableInfo<V>::value) {
      static_assert(Described<typename NullableInfo<V>::pointee>,
                    "a nullable field's pointee type is not described (no schema found "
                    "through the TypeTraits ladder)");
      return EntryClass::kNullableObject;
    } else if constexpr (Described<V>) {
      return EntryClass::kObject;
    } else if constexpr (IsIterable<V>) {
      static_assert(Described<ListRow<V>>,
                    "list rows must be described types (no schema found for the row type "
                    "through the TypeTraits ladder)");
      return EntryClass::kList;
    } else {
      static_assert(kAlwaysFalse<V>,
                    "field's value type is neither a bindable leaf (no ValueTraits) nor a "
                    "described type (no schema found through the TypeTraits ladder)");
      return EntryClass::kLeaf;
    }
  }

  static constexpr EntryClass kClass = Classify();
  static constexpr bool kHasAccessor = FieldToken<O, V, A, Va>::kHasAccessor;

  // The child described type for object/list classes; void otherwise. Computed with
  // `if constexpr` so only the taken branch's type is ever formed.
  static consteval auto ChildType() {
    if constexpr (NullableInfo<V>::value)
      return std::type_identity<typename NullableInfo<V>::pointee> {};
    else if constexpr (Bindable<V>)
      return std::type_identity<void> {};
    else if constexpr (Described<V>)
      return std::type_identity<V> {};
    else if constexpr (IsIterable<V>)
      return std::type_identity<ListRow<V>> {};
    else
      return std::type_identity<void> {};
  }
  using child_type = typename decltype(ChildType())::type;

  static consteval ValueKind Kind() {
    if constexpr (Bindable<V>)
      return ValueTraits<std::remove_cvref_t<V>>::kind;
    else if constexpr (IsIterable<V> && !NullableInfo<V>::value && !Described<V>)
      return ValueKind::List;
    else
      return ValueKind::Object;
  }
  static constexpr ValueKind kKind = Kind();
};

template <typename O, typename C, typename A, typename K>
struct EntryInfo<ListToken<O, C, A, K>> {
  static constexpr EntryClass kClass = EntryClass::kList;
  static constexpr bool kHasAccessor = true;
  using child_type = typename ListToken<O, C, A, K>::row_type;
  static_assert(Described<child_type>,
                "list rows must be described types (no schema found for the row type "
                "through the TypeTraits ladder)");
  static constexpr ValueKind kKind = ValueKind::List;

  static consteval ValueKind KeyKind() {
    if constexpr (ListToken<O, C, A, K>::kKeyed) {
      using KeyValue = AccessorValue<K>;
      static_assert(Bindable<KeyValue>, "list key member must be a bindable leaf type");
      constexpr ValueKind kind = ValueTraits<std::remove_cvref_t<KeyValue>>::kind;
      static_assert(kind == ValueKind::Int64 || kind == ValueKind::String,
                    "list keys must marshal as integers or strings");
      return kind;
    } else {
      return ValueKind::Int64;  // Index-keyed rows are identified by position.
    }
  }
};

template <typename P>
struct EntryInfo<ActionToken<P>> {
  static constexpr EntryClass kClass = EntryClass::kAction;
  static constexpr bool kHasAccessor = false;
  static_assert(std::is_void_v<P> || Described<P> || ScalarPayload<P>,
                "action payloads are described structs or marshalable leaves (boolean, "
                "integral, floating-point, string, or reflected enum; StyleValue and "
                "Color have no payload form)");
  using child_type = P;
  static constexpr ValueKind kKind = ValueKind::Action;
};

template <typename O, typename V, typename A>
struct EntryInfo<VarToken<O, V, A>> {
  static constexpr EntryClass kClass = EntryClass::kVar;
  static constexpr bool kHasAccessor = true;
  using child_type = void;

  static consteval ValueKind Kind() {
    static_assert(Bindable<V>, "Var accessor's value type is not a bindable leaf");
    constexpr ValueKind kind = ValueTraits<std::remove_cvref_t<V>>::kind;
    static_assert(kind == ValueKind::Int64 || kind == ValueKind::Double
                      || kind == ValueKind::StyleValue || kind == ValueKind::Color,
                  "Var accessors publish numbers, StyleValues, or colors");
    return kind;
  }
  static constexpr ValueKind kKind = Kind();
};

// Descriptor/record flags for an entry: declared token flags plus the structural bits
// computed from its classification.
template <typename Token>
consteval uint8_t EntryFlagsOf(const Token& token) {
  uint8_t flags = 0;
  if constexpr (requires { token.flags; })
    flags = token.flags;
  if constexpr (IsListToken<Token>::value) {
    if constexpr (Token::kKeyed)
      flags = static_cast<uint8_t>(flags | kEntryFlags_Keyed);
  }
  if constexpr (IsVarToken<Token>::value)
    flags = static_cast<uint8_t>(flags | kEntryFlags_Var);
  if constexpr (IsFieldToken<Token>::value) {
    if constexpr (EntryInfo<Token>::kClass == EntryClass::kNullableObject)
      flags = static_cast<uint8_t>(flags | kEntryFlags_Nullable);
  }
  return flags;
}

/// The schema index of a type's entry by name (kNpos when absent).
template <typename T>
consteval size_t SchemaIndexOf(std::string_view name) {
  return TypeTraits<T>::schema.IndexOf(name);
}

}  // namespace detail
}  // namespace data
}  // namespace dom
}  // namespace ultralight
/// \endcond
