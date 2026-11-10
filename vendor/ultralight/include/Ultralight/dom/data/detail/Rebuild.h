///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// Implementation detail of `<Ultralight/dom/data/Binding.h>`; include that header
/// instead. Everything here lives in ultralight::dom::data::detail and may change without
/// notice.
///
/// Rebuilding C++ values from delivered C data: writable-member detection, the
/// Reconstructible admission test, and the leaf/payload/row-snapshot readers typed
/// delivery hands rebuilt structs to.
///
/// \cond INTERNAL
#pragma once
#include <Ultralight/CAPI/CAPI_DOMData.h>
#include <Ultralight/dom/data/ValueTraits.h>
#include <Ultralight/dom/data/detail/EntryInfo.h>

#include <cstring>
#include <string_view>
#include <type_traits>
#include <utility>

namespace ultralight {
namespace dom {
namespace data {
namespace detail {

// Write access to the member behind a schema token: member data pointers and
// reflection-generated accessors are writable; getter and callable accessors are not.
template <typename A>
struct ReflectedAccessorIndex {
  static constexpr bool value = false;
  static constexpr size_t index = kNpos;
};
#if ULTRALIGHT_REFLECTION
template <typename T, size_t J>
struct ReflectedAccessorIndex<ReflectedAccessor<T, J>> {
  static constexpr bool value = true;
  static constexpr size_t index = J;
};
#endif

template <typename Tok>
inline constexpr bool kTokenHasWritableMember = [] {
  if constexpr (!IsFieldToken<Tok>::value) {
    return false;
  } else {
    using A = std::remove_cvref_t<typename Tok::accessor_type>;
    return std::is_member_object_pointer_v<A> || ReflectedAccessorIndex<A>::value;
  }
}();

template <typename Owner, size_t I, typename V>
void StoreMember(Owner& out, V&& value) {
  using SchemaType = std::remove_cvref_t<decltype(TypeTraits<Owner>::schema)>;
  using Tok = std::tuple_element_t<I, SchemaType>;
  using A = std::remove_cvref_t<typename Tok::accessor_type>;
  if constexpr (std::is_member_object_pointer_v<A>) {
    out.*(TypeTraits<Owner>::schema.template get<I>().accessor) = std::forward<V>(value);
  }
#if ULTRALIGHT_REFLECTION
  else if constexpr (ReflectedAccessorIndex<A>::value) {
    *ultralight::detail::MemberPointer<ReflectedAccessorIndex<A>::index>(out)
        = std::forward<V>(value);
  }
#endif
  // Non-writable accessors never reach here: Reconstructible() gates every caller.
}

// What a leaf member can be rebuilt from. Payload values cross as C leaf values (no
// StyleValue/Color form); row snapshots read the record's staging table (StyleValue and
// Color included).
enum class RebuildSource { kPayload, kRow };

template <typename V, RebuildSource S>
consteval bool LeafMemberRebuildable() {
  using Plain = std::remove_cvref_t<V>;
  if constexpr (std::is_same_v<Plain, bool> || std::is_integral_v<Plain>
                || std::is_floating_point_v<Plain>) {
    return true;
  } else if constexpr (std::is_same_v<Plain, StyleValue> || std::is_same_v<Plain, Color>) {
    return S == RebuildSource::kRow;
  } else if constexpr (std::is_enum_v<Plain>) {
#if ULTRALIGHT_REFLECTION
    return true;
#else
    return false;
#endif
  } else if constexpr (std::is_same_v<Plain, std::string_view>
                       || std::is_same_v<Plain, const char*>
                       || std::is_same_v<Plain, char*>) {
    return false;  // Non-owning string members would dangle.
  } else if constexpr (std::is_same_v<Plain, ultralight::String>
                       || std::is_constructible_v<Plain, const char*, size_t>) {
    return true;
  } else {
    return false;
  }
}

// Whether a described type can be rebuilt by value from delivered leaves: default
// constructible, every field a writable leaf member. Lists, nested objects, and vars make
// a type non-rebuildable (a partially-rebuilt snapshot would misrepresent the row).
template <typename P, RebuildSource S>
consteval bool Reconstructible() {
  using SchemaType = std::remove_cvref_t<decltype(TypeTraits<P>::schema)>;
  return []<size_t... Is>(std::index_sequence<Is...>) {
    bool ok = std::is_default_constructible_v<P>;
    auto check = [&]<size_t I>() {
      using Tok = std::tuple_element_t<I, SchemaType>;
      if constexpr (IsFieldToken<Tok>::value) {
        if constexpr (EntryInfo<Tok>::kClass != EntryClass::kLeaf)
          ok = false;
        else if constexpr (!kTokenHasWritableMember<Tok>)
          ok = false;
        else if constexpr (!LeafMemberRebuildable<typename Tok::value_type, S>())
          ok = false;
      } else if constexpr (IsListToken<Tok>::value || IsVarToken<Tok>::value) {
        ok = false;
      }
      // Action entries carry no stored value and are skipped.
    };
    (check.template operator()<Is>(), ...);
    return ok;
  }(std::make_index_sequence<SchemaType::kCount> {});
}

// Store one delivered C leaf value into the member behind token I (kind-checked; an
// unreflected enum name or a kind mismatch fails).
template <typename Owner, size_t I>
bool StoreLeaf(Owner& out, const ULDOMDataLeafValue& v) {
  using SchemaType = std::remove_cvref_t<decltype(TypeTraits<Owner>::schema)>;
  using Tok = std::tuple_element_t<I, SchemaType>;
  using V = std::remove_cvref_t<typename Tok::value_type>;
  const auto kind = static_cast<ValueKind>(v.kind);
  if constexpr (std::is_same_v<V, bool>) {
    if (kind != ValueKind::Bool)
      return false;
    StoreMember<Owner, I>(out, v.bool_value);
  } else if constexpr (std::is_enum_v<V>) {
#if ULTRALIGHT_REFLECTION
    if (kind != ValueKind::String || !v.string_value)
      return false;
    const std::string_view name(v.string_value, v.string_length);
    for (const auto& entry : EnumInfo<V>::kEntries) {
      if (entry.name == name) {
        StoreMember<Owner, I>(out, static_cast<V>(entry.value));
        return true;
      }
    }
    return false;
#else
    return false;
#endif
  } else if constexpr (std::is_integral_v<V>) {
    if (kind != ValueKind::Int64)
      return false;
    StoreMember<Owner, I>(out, static_cast<V>(v.int64_value));
  } else if constexpr (std::is_floating_point_v<V>) {
    if (kind == ValueKind::Double)
      StoreMember<Owner, I>(out, static_cast<V>(v.double_value));
    else if (kind == ValueKind::Int64)
      StoreMember<Owner, I>(out, static_cast<V>(v.int64_value));
    else
      return false;
  } else {
    if (kind != ValueKind::String || !v.string_value)
      return false;
    if constexpr (std::is_same_v<V, ultralight::String>)
      StoreMember<Owner, I>(out, ultralight::String(v.string_value, v.string_length));
    else
      StoreMember<Owner, I>(out, V(v.string_value, v.string_length));
  }
  return true;
}

// Rebuild a described payload struct from delivered named entries. A missing field, kind
// mismatch, or unreflected enum name fails the WHOLE reconstruction: the emission does not
// match the declared payload type, and the delivery drops rather than half-building.
template <typename P>
bool ReconstructPayload(const ULDOMDataPayloadEntry* payload, size_t count, P& out) {
  using SchemaType = std::remove_cvref_t<decltype(TypeTraits<P>::schema)>;
  bool ok = true;
  [&]<size_t... Is>(std::index_sequence<Is...>) {
    auto fill = [&]<size_t I>() {
      using Tok = std::tuple_element_t<I, SchemaType>;
      if constexpr (IsFieldToken<Tok>::value) {
        if (!ok)
          return;
        const std::string_view name(TypeTraits<P>::schema.template get<I>().name);
        const ULDOMDataLeafValue* found = nullptr;
        for (size_t e = 0; e < count; e++) {
          if (payload[e].name && std::string_view(payload[e].name) == name) {
            found = &payload[e].value;
            break;
          }
        }
        ok = found && StoreLeaf<P, I>(out, *found);
      }
    };
    (fill.template operator()<Is>(), ...);
  }(std::make_index_sequence<SchemaType::kCount> {});
  return ok;
}

// Rebuild row type R from the row table's slots (token index == slot by construction).
template <typename R>
bool ReadTableInto(ULDOMDataTable row, R& out) {
  using SchemaType = std::remove_cvref_t<decltype(TypeTraits<R>::schema)>;
  bool ok = true;
  [&]<size_t... Is>(std::index_sequence<Is...>) {
    auto read = [&]<size_t I>() {
      using Tok = std::tuple_element_t<I, SchemaType>;
      if constexpr (IsFieldToken<Tok>::value) {
        if (!ok)
          return;
        using V = std::remove_cvref_t<typename Tok::value_type>;
        constexpr auto slot = static_cast<unsigned int>(I);
        if constexpr (std::is_same_v<V, bool>) {
          StoreMember<R, I>(out, ulDOMDataTableGetBool(row, slot));
        } else if constexpr (std::is_enum_v<V> || std::is_same_v<V, ultralight::String>
                             || (!std::is_integral_v<V> && !std::is_floating_point_v<V>
                                 && !std::is_same_v<V, StyleValue>
                                 && !std::is_same_v<V, Color>)) {
          ULString text = ulDOMDataTableGetString(row, slot);
          ULDOMDataLeafValue leaf {};
          leaf.kind = static_cast<unsigned char>(ValueKind::String);
          leaf.string_value = ulStringGetData(text);
          leaf.string_length = ulStringGetLength(text);
          ok = StoreLeaf<R, I>(out, leaf);
          ulDestroyString(text);
        } else if constexpr (std::is_integral_v<V>) {
          StoreMember<R, I>(out, static_cast<V>(ulDOMDataTableGetInt64(row, slot)));
        } else if constexpr (std::is_floating_point_v<V>) {
          StoreMember<R, I>(out, static_cast<V>(ulDOMDataTableGetDouble(row, slot)));
        } else if constexpr (std::is_same_v<V, StyleValue>) {
          double value = 0;
          unsigned int unit = 0;
          ulDOMDataTableGetStyle(row, slot, &value, &unit);
          StoreMember<R, I>(out, StyleValue { value, static_cast<StyleUnit>(unit) });
        } else if constexpr (std::is_same_v<V, Color>) {
          ULColor color {};
          ulDOMDataTableGetColor(row, slot, &color);
          StoreMember<R, I>(out, Color(color));
        }
      }
    };
    (read.template operator()<Is>(), ...);
  }(std::make_index_sequence<SchemaType::kCount> {});
  return ok;
}

// Read the row identified by `source` out of the delivered staging table (the values the
// model most recently gave the record). The table arrives WITH the delivery: reading it
// back through a binding handle here would race a concurrent last-handle destroy. False =
// a stale key (the row left the model between fire and drain; the delivery drops,
// mirroring the raw tier's stale-key rule).
template <typename R, uint32_t ListSlot>
bool ReadRowSnapshot(ULDOMDataTable table, const ULDOMDataActionSource* source, R& out) {
  if (!source || !table)
    return false;
  const unsigned long long count = ulDOMDataTableGetListCount(table, ListSlot);
  for (unsigned long long i = 0; i < count; i++) {
    bool match = false;
    if (source->is_string_key) {
      ULString key = ulDOMDataTableGetListRowKeyString(table, ListSlot, i);
      match = ulStringGetLength(key) == source->string_key_length
          && std::memcmp(ulStringGetData(key), source->string_key,
                         source->string_key_length)
              == 0;
      ulDestroyString(key);
    } else {
      match = ulDOMDataTableGetListRowKeyInt64(table, ListSlot, i) == source->int_key;
    }
    if (match) {
      ULDOMDataTable row = ulDOMDataTableGetListRow(table, ListSlot, i);
      return row && ReadTableInto<R>(row, out);
    }
  }
  return false;
}

}  // namespace detail
}  // namespace data
}  // namespace dom
}  // namespace ultralight
/// \endcond
