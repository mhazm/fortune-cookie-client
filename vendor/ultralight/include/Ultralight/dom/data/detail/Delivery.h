///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// Implementation detail of `<Ultralight/dom/data/Binding.h>`; include that header
/// instead. Everything here lives in ultralight::dom::data::detail and may change without
/// notice.
///
/// The typed delivery machinery: the registration surface multiplexes many
/// OnChange/OnAction handlers through the waist's single per-binding change and action
/// callback slots. The first registration installs one type-erased HandlerTable as both
/// slots' user data, and the trampolines dispatch by slot (changes) or (list slot, action
/// slot) key (actions). Everything here compiles in the embedder's TU; only the C
/// callback registration crosses the waist.
///
/// Each registration lowers to a thin capture lambda over a named Dispatch* function that
/// re-derives every type from T and the slot indices in its own body. MSVC needs the
/// split: an enclosing function template's local type alias used inside a
/// nested lambda body miscompiles there (the alias reaches EnumInfo unresolved and the
/// compiler faults), while the derive-inside-a-named-template shape is the same lowering
/// discipline the descriptor thunks already prove on both compilers.
///
/// \cond INTERNAL
#pragma once
#include <Ultralight/dom/data/ActionInfo.h>
#include <Ultralight/dom/data/Value.h>
#include <Ultralight/dom/data/detail/EntryInfo.h>
#include <Ultralight/dom/data/detail/CallGuard.h>
#include <Ultralight/dom/data/detail/Rebuild.h>

#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace ultralight {
namespace dom {
namespace data {
namespace detail {

inline constexpr uint32_t kRootActionSlot = 0xFFFFFFFFu;

// A handler's type-erased form. The context receives the handler's drop reports
// (ulDOMDataContextReportDrop).
using ChangeHandler = std::function<void(ULDOMDataContext, const ULDOMDataLeafValue*)>;
using ActionHandler
    = std::function<void(ULDOMDataContext, const ULDOMDataActionSource*,
                         const ULDOMDataPayloadEntry*, size_t, ULDOMDataTable)>;

struct HandlerTable {
  std::unordered_map<uint32_t, ChangeHandler> changes;
  std::map<std::pair<uint32_t, uint32_t>, ActionHandler> actions;
  // Borrowed: the table lives inside one of this context's bindings.
  ULDOMDataContext context = nullptr;
  // Joint ownership by the two callback slots, plus a transient registration probe. A
  // release landing mid-dispatch (a handler unbinding its own binding) defers the delete
  // until the dispatch unwinds.
  int refs = 0;
  int in_dispatch = 0;
  bool doomed = false;
};

// The table's context: ulDOMDataBindingGetContext() returns a reference, released at once
// because the table lives inside one of that context's bindings.
inline ULDOMDataContext BorrowBindingContext(ULDOMDataBinding binding) {
  ULDOMDataContext context = ulDOMDataBindingGetContext(binding);
  ulDestroyDOMDataContext(context);
  return context;
}

// Runs `edit` inside the binding's context owner turn. Registration changes the table
// maps that a thread-safe context's Sync on another thread dispatches through; the turn
// serializes them (on a fixed-home context it's a plain call). Nothing runs for an empty
// handle.
template <typename F>
void WithBindingOwnerTurn(ULDOMDataBinding binding, F&& edit) {
  ULDOMDataContext context = ulDOMDataBindingGetContext(binding);
  if (!context)
    return;
  using Edit = std::remove_reference_t<F>;
  ulDOMDataContextWithOwnerTurn(
      context, [](void* user_data) { (*static_cast<Edit*>(user_data))(); }, &edit);
  ulDestroyDOMDataContext(context);
}

inline void ReleaseHandlerTable(void* user_data) {
  auto* table = static_cast<HandlerTable*>(user_data);
  if (--table->refs == 0) {
    if (table->in_dispatch > 0)
      table->doomed = true;
    else
      delete table;
  }
}

// Both thunks invoke a COPY of the registered callable: a handler that re-registers or
// removes itself must not destroy the callable currently executing.
inline void HandlerTableChangeThunk(void* user_data, unsigned int slot,
                                    const ULDOMDataLeafValue* value) {
  auto* table = static_cast<HandlerTable*>(user_data);
  auto it = table->changes.find(slot);
  if (it == table->changes.end())
    return;
  auto fn = it->second;
  table->in_dispatch++;
  fn(table->context, value);
  if (--table->in_dispatch == 0 && table->doomed)
    delete table;
}

inline void HandlerTableActionThunk(void* user_data, unsigned int slot,
                                    const ULDOMDataActionSource* source,
                                    const ULDOMDataPayloadEntry* payload,
                                    size_t payload_count, ULDOMDataTable staging) {
  auto* table = static_cast<HandlerTable*>(user_data);
  auto it = table->actions.find(
      std::make_pair(source ? source->list_slot : kRootActionSlot, slot));
  if (it == table->actions.end())
    return;
  auto fn = it->second;
  table->in_dispatch++;
  fn(table->context, source, payload, payload_count, staging);
  if (--table->in_dispatch == 0 && table->doomed)
    delete table;
}

// ---- Scalar payloads --------------------------------------------------------------------
//
// A scalar action payload (`Action<double>`) crosses the waist as ONE entry with the
// reserved name "value": representationally identical to a one-field struct payload
// named `value`, by design, so C emitters and receivers need no scalar-specific shape.

inline const ULDOMDataLeafValue* FindPayloadEntry(const ULDOMDataPayloadEntry* payload,
                                                  size_t count, std::string_view name) {
  for (size_t i = 0; i < count; i++) {
    if (payload[i].name && std::string_view(payload[i].name) == name)
      return &payload[i].value;
  }
  return nullptr;
}

// Deliver a scalar payload to a handler taking the declared scalar type. The "value"
// entry rebuilds by kind (numeric crossover and enum-name resolution mirror the
// struct-payload field rules); a missing entry or kind mismatch drops the delivery.
// String scalars deliver borrowed views (`std::string_view`, `const char*`) or owned
// copies (`String`, owning string classes) per the declared type.
template <typename P, typename Fn>
bool DeliverScalarPayload(Fn& fn, const ULDOMDataPayloadEntry* payload, size_t count) {
  using Plain = std::decay_t<P>;
  const ULDOMDataLeafValue* found = FindPayloadEntry(payload, count, "value");
  if (!found)
    return false;
  const auto kind = static_cast<ValueKind>(found->kind);
  if constexpr (std::is_same_v<Plain, bool>) {
    if (kind != ValueKind::Bool)
      return false;
    fn(found->bool_value);
  } else if constexpr (std::is_enum_v<Plain>) {
#if ULTRALIGHT_REFLECTION
    if (kind != ValueKind::String || !found->string_value)
      return false;
    const std::string_view name(found->string_value, found->string_length);
    for (const auto& entry : EnumInfo<Plain>::kEntries) {
      if (entry.name == name) {
        fn(static_cast<Plain>(entry.value));
        return true;
      }
    }
#endif
    return false;
  } else if constexpr (std::is_integral_v<Plain>) {
    if (kind != ValueKind::Int64)
      return false;
    fn(static_cast<Plain>(found->int64_value));
  } else if constexpr (std::is_floating_point_v<Plain>) {
    if (kind == ValueKind::Double)
      fn(static_cast<Plain>(found->double_value));
    else if (kind == ValueKind::Int64)
      fn(static_cast<Plain>(found->int64_value));
    else
      return false;
  } else {
    if (kind != ValueKind::String)
      return false;
    const char* data = found->string_value ? found->string_value : "";
    const size_t length = found->string_value ? found->string_length : 0;
    if constexpr (std::is_same_v<Plain, const char*> || std::is_same_v<Plain, char*>) {
      fn(data);
    } else if constexpr (std::is_same_v<Plain, std::string_view>) {
      fn(std::string_view(data, length));
    } else if constexpr (std::is_same_v<Plain, ultralight::String>) {
      fn(ultralight::String(data, length));
    } else {
      fn(Plain(data, length));
    }
  }
  return true;
}

// Why a typed payload handler got nothing (reported through ulDOMDataContextReportDrop).
inline const char* PayloadDropReason(size_t payload_count) {
  return payload_count == 0
      ? "a delivery was dropped because it arrived without a payload (actions from the page "
        "have none), so a handler taking the payload type cannot receive it (take a "
        "dd::Value or no argument)"
      : "a delivery was dropped because its payload does not match the action's declared "
        "payload type (a missing field, another value kind, or an unknown enum name)";
}

// The generic handler form's payload Value: a scalar-declared action delivers its single
// "value" entry as a LEAF Value, never a one-member object; struct payloads deliver the
// flat object form.
template <typename P>
Value PayloadValueFor(const ULDOMDataPayloadEntry* payload, size_t count) {
  constexpr bool kScalar = [] {
    if constexpr (std::is_void_v<P>)
      return false;
    else
      return !Described<P>;
  }();
  if constexpr (kScalar) {
    if (const ULDOMDataLeafValue* found = FindPayloadEntry(payload, count, "value"))
      return Value::FromLeaf(*found);
    return Value();
  } else {
    return Value::FromPayload(payload, count);
  }
}

// ---- Handler construction (form selection is compile-time, most-specific first) ---------

template <typename T, size_t I, typename Fn>
void DispatchChange(Fn& fn, const ULDOMDataLeafValue& v) {
  using SchemaType = std::remove_cvref_t<decltype(TypeTraits<T>::schema)>;
  using Tok = std::tuple_element_t<I, SchemaType>;
  using V = std::remove_cvref_t<typename Tok::value_type>;
  constexpr ValueKind kKind = EntryInfo<Tok>::kKind;
  if (static_cast<ValueKind>(v.kind) != kKind)
    return;
  if constexpr (kKind == ValueKind::Bool) {
    if constexpr (std::is_invocable_v<Fn&, bool>)
      fn(v.bool_value);
    else
      fn(Value::FromLeaf(v));
  } else if constexpr (kKind == ValueKind::Int64) {
    if constexpr (std::is_invocable_v<Fn&, V>)
      fn(static_cast<V>(v.int64_value));
    else if constexpr (std::is_invocable_v<Fn&, int64_t>)
      fn(v.int64_value);
    else
      fn(Value::FromLeaf(v));
  } else if constexpr (kKind == ValueKind::Double) {
    if constexpr (std::is_invocable_v<Fn&, V>)
      fn(static_cast<V>(v.double_value));
    else if constexpr (std::is_invocable_v<Fn&, double>)
      fn(v.double_value);
    else
      fn(Value::FromLeaf(v));
  } else if constexpr (kKind == ValueKind::Color) {
    // Slot representation end to end (the validator convention): handlers take the
    // typed color, never a string form.
    fn(Color(v.color_value));
  } else if constexpr (std::is_enum_v<V>) {
#if ULTRALIGHT_REFLECTION
    if constexpr (std::is_invocable_v<Fn&, V>) {
      const std::string_view name(v.string_value ? v.string_value : "",
                                  v.string_value ? v.string_length : 0);
      for (const auto& entry : EnumInfo<V>::kEntries) {
        if (entry.name == name) {
          fn(static_cast<V>(entry.value));
          return;
        }
      }
      // An unreflected name never reaches here (containment validates the slot), but
      // dropping is the safe answer if it ever does.
    } else {
      fn(Value::FromLeaf(v));
    }
#else
    fn(Value::FromLeaf(v));
#endif
  } else {
    const size_t length = v.string_value ? v.string_length : 0;
    if constexpr (std::is_invocable_v<Fn&, const ultralight::String&>) {
      fn(ultralight::String(v.string_value ? v.string_value : "", length));
    } else if constexpr (std::is_invocable_v<Fn&, const V&>
                         && std::is_constructible_v<V, const char*, size_t>) {
      fn(V(v.string_value ? v.string_value : "", length));
    } else {
      fn(Value::FromLeaf(v));
    }
  }
}

template <typename T, size_t I, typename Fn>
ChangeHandler MakeChangeHandler(Fn fn) {
  using SchemaType = std::remove_cvref_t<decltype(TypeTraits<T>::schema)>;
  using Tok = std::tuple_element_t<I, SchemaType>;
  using V = std::remove_cvref_t<typename Tok::value_type>;
  constexpr ValueKind kKind = EntryInfo<Tok>::kKind;
  static_assert(kKind == ValueKind::Bool || kKind == ValueKind::Int64
                    || kKind == ValueKind::Double || kKind == ValueKind::String
                    || kKind == ValueKind::Color,
                "dom::data: two-way delivery covers boolean, integer, floating-point, "
                "string, and color fields");
  if constexpr (kKind == ValueKind::Color) {
    static_assert(std::is_invocable_v<Fn&, Color>,
                  "dom::data: an OnChange handler for a color field takes an "
                  "ultralight::Color");
  } else if constexpr (kKind == ValueKind::String && std::is_enum_v<V>) {
    static_assert(std::is_invocable_v<Fn&, V> || std::is_invocable_v<Fn&, Value>,
                  "dom::data: an OnChange handler for an enum field takes the enum value "
                  "(or a dd::Value)");
  } else if constexpr (kKind == ValueKind::String) {
    static_assert(std::is_invocable_v<Fn&, const ultralight::String&>
                      || (std::is_invocable_v<Fn&, const V&>
                          && std::is_constructible_v<V, const char*, size_t>)
                      || std::is_invocable_v<Fn&, Value>,
                  "dom::data: an OnChange handler for a string field takes the field's "
                  "value type, a const String&, or a dd::Value");
  } else {
    static_assert(std::is_invocable_v<Fn&, V> || std::is_invocable_v<Fn&, int64_t>
                      || std::is_invocable_v<Fn&, double>
                      || std::is_invocable_v<Fn&, Value>,
                  "dom::data: an OnChange handler takes the field's value type (or its "
                  "slot type, or a dd::Value)");
  }
  return [fn = std::move(fn)](ULDOMDataContext context, const ULDOMDataLeafValue* v) mutable {
    CallHandler(context, TypeTraits<T>::schema.template get<I>().name,
                [&] { DispatchChange<T, I>(fn, *v); });
  };
}

template <typename T, size_t I, typename Fn>
void DispatchRootAction(Fn& fn, ULDOMDataContext context, const ULDOMDataActionSource* source,
                        const ULDOMDataPayloadEntry* payload, size_t payload_count,
                        ULDOMDataTable /*staging*/) {
  const char* name = TypeTraits<T>::schema.template get<I>().name;
  using SchemaType = std::remove_cvref_t<decltype(TypeTraits<T>::schema)>;
  using Tok = std::tuple_element_t<I, SchemaType>;
  using P = typename Tok::payload_type;
  constexpr bool kScalarPayload = [] {
    if constexpr (std::is_void_v<P>)
      return false;
    else
      return !Described<P>;
  }();
  constexpr bool kPayloadForm = [] {
    if constexpr (std::is_void_v<P>)
      return false;
    else if constexpr (Described<P>)
      return std::is_invocable_v<Fn&, const P&>;
    else
      return std::is_invocable_v<Fn&, std::decay_t<P>>;
  }();
  if constexpr (kPayloadForm && kScalarPayload) {
    if (!DeliverScalarPayload<P>(fn, payload, payload_count))
      ulDOMDataContextReportDrop(context, name, PayloadDropReason(payload_count));
  } else if constexpr (kPayloadForm) {
    static_assert(Reconstructible<P, RebuildSource::kPayload>(),
                  "dom::data: this payload type cannot be rebuilt for typed delivery (it "
                  "needs to be default constructible with every field a writable leaf "
                  "member; take a dd::Value instead)");
    P value {};
    if (!ReconstructPayload<P>(payload, payload_count, value)) {
      ulDOMDataContextReportDrop(context, name, PayloadDropReason(payload_count));
      return;
    }
    fn(static_cast<const P&>(value));
  } else if constexpr (std::is_invocable_v<Fn&, Value, ActionInfo>) {
    fn(PayloadValueFor<P>(payload, payload_count), ActionInfo(name, source));
  } else if constexpr (std::is_invocable_v<Fn&, Value>) {
    fn(PayloadValueFor<P>(payload, payload_count));
  } else {
    (void)context;
    (void)name;
    (void)source;
    (void)payload;
    (void)payload_count;
    fn();
  }
}

template <typename T, size_t I, typename Fn>
ActionHandler MakeRootActionHandler(Fn fn) {
  using SchemaType = std::remove_cvref_t<decltype(TypeTraits<T>::schema)>;
  using Tok = std::tuple_element_t<I, SchemaType>;
  using P = typename Tok::payload_type;
  constexpr bool kPayloadForm = [] {
    if constexpr (std::is_void_v<P>)
      return false;
    else if constexpr (Described<P>)
      return std::is_invocable_v<Fn&, const P&>;
    else
      return std::is_invocable_v<Fn&, std::decay_t<P>>;
  }();
  static_assert(kPayloadForm || std::is_invocable_v<Fn&, Value, ActionInfo>
                    || std::is_invocable_v<Fn&, Value> || std::is_invocable_v<Fn&>,
                "dom::data: an OnAction handler takes nothing, the action's declared "
                "payload (its struct by const reference, or the scalar by value), a "
                "dd::Value, or (dd::Value, dd::ActionInfo)");
  return [fn = std::move(fn)](ULDOMDataContext context, const ULDOMDataActionSource* source,
                              const ULDOMDataPayloadEntry* payload, size_t payload_count,
                              ULDOMDataTable staging) mutable {
    CallHandler(context, TypeTraits<T>::schema.template get<I>().name, [&] {
      DispatchRootAction<T, I>(fn, context, source, payload, payload_count, staging);
    });
  };
}

template <typename T, size_t ListIndex, size_t ActionIndex>
const char* RowActionName() {
  using SchemaType = std::remove_cvref_t<decltype(TypeTraits<T>::schema)>;
  using R = typename EntryInfo<std::tuple_element_t<ListIndex, SchemaType>>::child_type;
  return TypeTraits<R>::schema.template get<ActionIndex>().name;
}

template <typename T, size_t ListIndex, size_t ActionIndex, typename Fn>
void DispatchRowAction(Fn& fn, const ULDOMDataActionSource* source,
                       const ULDOMDataPayloadEntry* payload, size_t payload_count,
                       ULDOMDataTable staging) {
  using SchemaType = std::remove_cvref_t<decltype(TypeTraits<T>::schema)>;
  using LTok = std::tuple_element_t<ListIndex, SchemaType>;
  using R = typename EntryInfo<LTok>::child_type;
  using RTok
      = std::tuple_element_t<ActionIndex, std::remove_cvref_t<decltype(TypeTraits<R>::schema)>>;
  constexpr ValueKind kKeyKind = [] {
    if constexpr (IsListToken<LTok>::value)
      return EntryInfo<LTok>::KeyKind();
    else
      return ValueKind::Int64;  // Field-declared lists are index-keyed.
  }();
  if constexpr (std::is_invocable_v<Fn&, const R&>) {
    static_assert(Reconstructible<R, RebuildSource::kRow>(),
                  "dom::data: this row type cannot be rebuilt for typed delivery (it "
                  "needs to be default constructible with every field a writable leaf "
                  "member; take the row key instead)");
    if (!source)
      return;
    R row {};
    if (!ReadRowSnapshot<R, static_cast<uint32_t>(ListIndex)>(staging, source, row))
      return;
    // The row's identity is its declared key member, not a schema slot: write it from
    // the firing source so the snapshot carries it.
    if constexpr (IsListToken<LTok>::value && LTok::kKeyed) {
      const auto key_ptr = TypeTraits<T>::schema.template get<ListIndex>().key;
      using KV = std::remove_cvref_t<AccessorValue<decltype(key_ptr)>>;
      static_assert(LeafMemberRebuildable<KV, RebuildSource::kRow>(),
                    "dom::data: the row key member's type cannot receive the firing key "
                    "(use an owning integer or string type)");
      if constexpr (kKeyKind == ValueKind::Int64) {
        row.*key_ptr = static_cast<KV>(source->int_key);
      } else if constexpr (std::is_enum_v<KV>) {
#if ULTRALIGHT_REFLECTION
        // An enum key travels as its enumerator name; an unreflected name isn't delivered.
        const std::string_view name(source->string_key ? source->string_key : "",
                                    source->string_key ? source->string_key_length : 0);
        bool found = false;
        for (const auto& entry : EnumInfo<KV>::kEntries) {
          if (entry.name == name) {
            row.*key_ptr = static_cast<KV>(entry.value);
            found = true;
            break;
          }
        }
        if (!found)
          return;
#endif
      } else {
        row.*key_ptr = KV(source->string_key ? source->string_key : "",
                          source->string_key_length);
      }
    }
    fn(static_cast<const R&>(row));
  } else if constexpr (kKeyKind == ValueKind::Int64 && std::is_invocable_v<Fn&, int64_t>) {
    if (!source || source->is_string_key)
      return;
    fn(source->int_key);
  } else if constexpr (kKeyKind == ValueKind::String
                       && std::is_invocable_v<Fn&, std::string_view>) {
    if (!source || !source->is_string_key)
      return;
    fn(std::string_view(source->string_key ? source->string_key : "",
                        source->string_key_length));
  } else if constexpr (std::is_invocable_v<Fn&, Value, ActionInfo>) {
    fn(PayloadValueFor<typename RTok::payload_type>(payload, payload_count),
       ActionInfo(TypeTraits<R>::schema.template get<ActionIndex>().name, source));
  } else if constexpr (std::is_invocable_v<Fn&, Value>) {
    fn(PayloadValueFor<typename RTok::payload_type>(payload, payload_count));
  } else {
    (void)payload;
    (void)payload_count;
    fn();
  }
}

template <typename T, size_t ListIndex, size_t ActionIndex, typename Fn>
ActionHandler MakeRowActionHandler(Fn fn) {
  using SchemaType = std::remove_cvref_t<decltype(TypeTraits<T>::schema)>;
  using LTok = std::tuple_element_t<ListIndex, SchemaType>;
  using R = typename EntryInfo<LTok>::child_type;
  constexpr ValueKind kKeyKind = [] {
    if constexpr (IsListToken<LTok>::value)
      return EntryInfo<LTok>::KeyKind();
    else
      return ValueKind::Int64;
  }();
  static_assert(std::is_invocable_v<Fn&, const R&>
                    || (kKeyKind == ValueKind::Int64 && std::is_invocable_v<Fn&, int64_t>)
                    || (kKeyKind == ValueKind::String
                        && std::is_invocable_v<Fn&, std::string_view>)
                    || std::is_invocable_v<Fn&, Value, ActionInfo>
                    || std::is_invocable_v<Fn&, Value> || std::is_invocable_v<Fn&>,
                "dom::data: a row OnAction handler takes nothing, the row type by const "
                "reference, the row key (int64_t, or std::string_view for string-keyed "
                "lists), a dd::Value, or (dd::Value, dd::ActionInfo)");
  return [fn = std::move(fn)](ULDOMDataContext context, const ULDOMDataActionSource* source,
                              const ULDOMDataPayloadEntry* payload, size_t payload_count,
                              ULDOMDataTable staging) mutable {
    CallHandler(context, RowActionName<T, ListIndex, ActionIndex>(), [&] {
      DispatchRowAction<T, ListIndex, ActionIndex>(fn, source, payload, payload_count,
                                                   staging);
    });
  };
}

}  // namespace detail
}  // namespace data
}  // namespace dom
}  // namespace ultralight
/// \endcond
