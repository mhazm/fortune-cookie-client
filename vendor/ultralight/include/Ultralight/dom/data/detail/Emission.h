///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// Implementation detail of `<Ultralight/dom/data/Binding.h>` and
/// `<Ultralight/dom/data/Context.h>`; include those instead. Everything here lives in
/// ultralight::dom::data::detail and may change without notice.
///
/// The emission side of action payloads: the marshal admission test, lowering payload
/// structs and scalars to named C entries, and the compile-time payload-parameter probe
/// behind the typed PostAction.
///
/// \cond INTERNAL
#pragma once
#include <Ultralight/CAPI/CAPI_DOMData.h>
#include <Ultralight/detail/FixedString.h>
#include <Ultralight/dom/data/detail/EntryInfo.h>

#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace ultralight {
namespace dom {
namespace data {
namespace detail {

// A payload type marshals when every schema entry is a readable leaf field of a payload
// kind. Lists, objects, vars, actions, StyleValue, and Color fields have no payload form.
template <typename P>
consteval bool PayloadMarshalable() {
  using SchemaType = std::remove_cvref_t<decltype(TypeTraits<P>::schema)>;
  return []<size_t... Is>(std::index_sequence<Is...>) {
    bool ok = true;
    auto check = [&]<size_t I>() {
      using Tok = std::tuple_element_t<I, SchemaType>;
      if constexpr (!IsFieldToken<Tok>::value) {
        ok = false;
      } else if constexpr (EntryInfo<Tok>::kClass != EntryClass::kLeaf || !Tok::kHasAccessor) {
        ok = false;
      } else {
        constexpr ValueKind kind = EntryInfo<Tok>::kKind;
        if (kind != ValueKind::Bool && kind != ValueKind::Int64 && kind != ValueKind::Double
            && kind != ValueKind::String)
          ok = false;
      }
    };
    (check.template operator()<Is>(), ...);
    return ok;
  }(std::make_index_sequence<SchemaType::kCount> {});
}

// Lower a payload struct to named C entries. `strings` owns the UTF-8 backing the string
// entries borrow; both vectors are reserved up front so the borrowed pointers stay put.
template <typename P>
void MarshalPayload(const P& payload, std::vector<std::string>& strings,
                    std::vector<ULDOMDataPayloadEntry>& entries) {
  using SchemaType = std::remove_cvref_t<decltype(TypeTraits<P>::schema)>;
  strings.reserve(SchemaType::kCount);
  entries.reserve(SchemaType::kCount);
  [&]<size_t... Is>(std::index_sequence<Is...>) {
    auto emit = [&]<size_t I>() {
      using Tok = std::tuple_element_t<I, SchemaType>;
      using V = std::remove_cvref_t<typename Tok::value_type>;
      constexpr ValueKind kind = EntryInfo<Tok>::kKind;
      const auto& accessor = TypeTraits<P>::schema.template get<I>().accessor;
      ULDOMDataPayloadEntry e {};
      e.name = TypeTraits<P>::schema.template get<I>().name;
      e.value.kind = static_cast<unsigned char>(kind);
      if constexpr (kind == ValueKind::Bool) {
        e.value.bool_value = ValueTraits<V>::ToSlot(ReadAccessor(accessor, payload));
      } else if constexpr (kind == ValueKind::Int64) {
        e.value.int64_value = ValueTraits<V>::ToSlot(ReadAccessor(accessor, payload));
      } else if constexpr (kind == ValueKind::Double) {
        e.value.double_value = ValueTraits<V>::ToSlot(ReadAccessor(accessor, payload));
      } else {
        // Copy before reading UTF-8: utf8() caches inside the String, and the slot value
        // may be a reference to the caller's const payload member.
        ultralight::String owned(ValueTraits<V>::ToSlot(ReadAccessor(accessor, payload)));
        const ultralight::String8& utf8 = owned.utf8();
        strings.emplace_back(utf8.data(), utf8.length());
        e.value.string_value = strings.back().c_str();
        e.value.string_length = strings.back().size();
      }
      entries.push_back(e);
    };
    (emit.template operator()<Is>(), ...);
  }(std::make_index_sequence<SchemaType::kCount> {});
}

// Lower a scalar payload to its one "value" entry. `backing` owns the UTF-8 a string
// entry borrows; keep it alive across the emission call.
template <typename P>
void MarshalScalarPayload(const P& payload, std::string& backing,
                          ULDOMDataPayloadEntry& entry) {
  using Plain = std::decay_t<P>;
  constexpr ValueKind kKind = ValueTraits<Plain>::kind;
  entry.name = "value";
  entry.value.kind = static_cast<unsigned char>(kKind);
  if constexpr (kKind == ValueKind::Bool) {
    entry.value.bool_value = ValueTraits<Plain>::ToSlot(payload);
  } else if constexpr (kKind == ValueKind::Int64) {
    entry.value.int64_value = ValueTraits<Plain>::ToSlot(payload);
  } else if constexpr (kKind == ValueKind::Double) {
    entry.value.double_value = ValueTraits<Plain>::ToSlot(payload);
  } else {
    ultralight::String owned(ValueTraits<Plain>::ToSlot(payload));
    const ultralight::String8& utf8 = owned.utf8();
    backing.assign(utf8.data(), utf8.length());
    entry.value.string_value = backing.c_str();
    entry.value.string_length = backing.size();
  }
}

// A binding-addressed emission (a null `payload` posts none). ulDOMDataBindingGetContext()
// returns a reference, released here.
inline bool PostBindingAction(ULDOMDataBinding binding, unsigned int slot,
                              const ULDOMDataPayloadEntry* payload = nullptr,
                              size_t payload_count = 0) {
  ULDOMDataContext context = ulDOMDataBindingGetContext(binding);
  const bool posted
      = ulDOMDataContextPostActionWithPayload(context, binding, slot, payload, payload_count);
  ulDestroyDOMDataContext(context);
  return posted;
}

// The declared-payload parameter type for PostAction<Name>: the action's declared payload
// (decayed) when Name resolves to a payload-carrying root action, else a take-anything
// placeholder that keeps the call selectable so the body's named static_asserts produce
// the diagnostic instead of an opaque overload-resolution failure.
struct AnyPayloadArg {
  template <typename U>
  constexpr AnyPayloadArg(U&&) noexcept {}
};

template <typename T, ultralight::detail::FixedString Name>
consteval auto ActionPayloadProbe() {
  constexpr std::string_view path = Name.view();
  constexpr size_t I = SchemaIndexOf<T>(path);
  if constexpr (I == kNpos) {
    return std::type_identity<AnyPayloadArg> {};
  } else {
    using SchemaType = std::remove_cvref_t<decltype(TypeTraits<T>::schema)>;
    using Tok = std::tuple_element_t<I, SchemaType>;
    if constexpr (!IsActionToken<Tok>::value)
      return std::type_identity<AnyPayloadArg> {};
    else if constexpr (std::is_void_v<typename Tok::payload_type>)
      return std::type_identity<AnyPayloadArg> {};
    else
      return std::type_identity<std::decay_t<typename Tok::payload_type>> {};
  }
}

template <typename T, ultralight::detail::FixedString Name>
using ActionPayloadArg = typename decltype(ActionPayloadProbe<T, Name>())::type;

}  // namespace detail
}  // namespace data
}  // namespace dom
}  // namespace ultralight
/// \endcond
