///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// Implementation detail of `<Ultralight/dom/data/detail/Descriptors.h>`; include the
/// `<Ultralight/dom/data/*.h>` headers instead. Everything here lives in
/// ultralight::dom::data::detail and may change without notice.
///
/// Descriptor lowering: the compiled read and validator thunks, enum metadata text,
/// scalar-payload child tables, the per-type descriptor storage behind Descriptors<T>(),
/// and the child-link resolver. Included by Descriptors.h between the table declarations
/// and Descriptors<T>().
///
/// \cond INTERNAL
#pragma once
#include <Ultralight/dom/data/detail/CallGuard.h>

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace ultralight {
namespace dom {
namespace data {
namespace detail {

/// Resolve an entry's child table. Compiled tables link children through the nullary
/// `child` function (the indirection that breaks self-referential type cycles);
/// runtime-registered tables link directly through `child_table`. Every child-link
/// consumer goes through here so both producers stay readable.
inline const TypeDescriptor* ChildTableOf(const EntryDescriptor& entry) {
  return entry.child ? entry.child() : entry.child_table;
}

template <typename T, size_t I>
const auto& TokenAt() {
  return TypeTraits<T>::schema.template get<I>();
}

template <typename T, size_t I>
bool ReadEntryBool(const void* instance) {
  const auto& token = TypeTraits<T>::schema.template get<I>();
  using Value = typename std::remove_cvref_t<decltype(token)>::value_type;
  return ValueTraits<Value>::ToSlot(
      ReadAccessor(token.accessor, *static_cast<const T*>(instance)));
}

template <typename T, size_t I>
int64_t ReadEntryInt64(const void* instance) {
  const auto& token = TypeTraits<T>::schema.template get<I>();
  using Value = typename std::remove_cvref_t<decltype(token)>::value_type;
  return ValueTraits<Value>::ToSlot(
      ReadAccessor(token.accessor, *static_cast<const T*>(instance)));
}

template <typename T, size_t I>
double ReadEntryDouble(const void* instance) {
  const auto& token = TypeTraits<T>::schema.template get<I>();
  using Value = typename std::remove_cvref_t<decltype(token)>::value_type;
  return ValueTraits<Value>::ToSlot(
      ReadAccessor(token.accessor, *static_cast<const T*>(instance)));
}

template <typename T, size_t I>
void ReadEntryString(const void* instance, String* out) {
  const auto& token = TypeTraits<T>::schema.template get<I>();
  using Value = typename std::remove_cvref_t<decltype(token)>::value_type;
  *out = ValueTraits<Value>::ToSlot(
      ReadAccessor(token.accessor, *static_cast<const T*>(instance)));
}

template <typename T, size_t I>
StyleValue ReadEntryStyle(const void* instance) {
  const auto& token = TypeTraits<T>::schema.template get<I>();
  using Value = typename std::remove_cvref_t<decltype(token)>::value_type;
  return ValueTraits<Value>::ToSlot(
      ReadAccessor(token.accessor, *static_cast<const T*>(instance)));
}

template <typename T, size_t I>
Color ReadEntryColor(const void* instance) {
  const auto& token = TypeTraits<T>::schema.template get<I>();
  using Value = typename std::remove_cvref_t<decltype(token)>::value_type;
  return ValueTraits<Value>::ToSlot(
      ReadAccessor(token.accessor, *static_cast<const T*>(instance)));
}

// ---- Validator lowering ----------------------------------------------------------------

template <typename, typename...>
inline constexpr bool kNoValidatorForm = false;

template <typename V>
struct IsStdOptional : std::false_type {};
template <typename U>
struct IsStdOptional<std::optional<U>> : std::true_type {
  using value_type = U;
};

/// Index of the name-addressed validator entry paired to token I (kNpos when none).
template <typename T, size_t I>
consteval size_t NamedValidatorIndex() {
  return TypeTraits<T>::schema.validator_index[I];
}

template <typename T, size_t I>
consteval bool HasValidatorFor() {
  using SchemaType = std::remove_cvref_t<decltype(TypeTraits<T>::schema)>;
  using Tok = std::tuple_element_t<I, SchemaType>;
  if constexpr (!IsFieldToken<Tok>::value)
    return false;
  else if constexpr (Tok::kHasValidator)
    return true;
  else
    return NamedValidatorIndex<T, I>() != kNpos;
}

template <typename T, size_t I>
constexpr const auto& ValidatorFn() {
  const auto& token = TypeTraits<T>::schema.template get<I>();
  using Tok = std::remove_cvref_t<decltype(token)>;
  if constexpr (Tok::kHasValidator)
    return token.validator.fn;
  else
    return PackGet<NamedValidatorIndex<T, I>()>(TypeTraits<T>::schema.validators).fn;
}

/// Invoke token I's validator with `value` in slot representation (enum-typed fields
/// receive their enum value, reflected from the slot's enumerator name; an unreflected
/// name rejects). Cross-field forms receive a Snapshot of the published generation.
/// Returns false to reject; on acceptance `value` carries the (possibly adjusted)
/// result back in slot representation.
template <typename T, size_t I, typename Slot>
bool RunValidator(Slot& value, RecordOps* published) {
  const auto& fn = ValidatorFn<T, I>();
  using Fn = std::remove_cvref_t<decltype(fn)>;
  using SchemaType = std::remove_cvref_t<decltype(TypeTraits<T>::schema)>;
  using Tok = std::tuple_element_t<I, SchemaType>;
  using Value = typename Tok::value_type;
  using Info = EntryInfo<Tok>;

  auto finish = [&](auto&& result) -> bool {
    using R = std::remove_cvref_t<decltype(result)>;
    if constexpr (IsStdOptional<R>::value) {
      if (!result)
        return false;
      return [&](auto&& inner) {
        using Inner = std::remove_cvref_t<decltype(inner)>;
        static_assert(ValueTraits<Inner>::kind == Info::kKind,
                      "dom::data: a Validate callable must return its field's value in "
                      "the same slot kind it received");
        value = Slot(ValueTraits<Inner>::ToSlot(inner));
        return true;
      }(*result);
    } else {
      static_assert(ValueTraits<R>::kind == Info::kKind,
                    "dom::data: a Validate callable must return its field's value in "
                    "the same slot kind it received");
      value = Slot(ValueTraits<R>::ToSlot(result));
      return true;
    }
  };

  auto invoke = [&](auto&& arg) -> bool {
    using Arg = decltype(arg);
    if constexpr (std::is_invocable_v<const Fn&, Arg, Snapshot<T>>) {
      return finish(fn(static_cast<Arg>(arg), Snapshot<T>(published)));
    } else if constexpr (std::is_invocable_v<const Fn&, Arg>) {
      return finish(fn(static_cast<Arg>(arg)));
    } else {
      static_assert(kNoValidatorForm<Fn, Arg>,
                    "dom::data: a Validate callable takes its field's value (in slot "
                    "representation; enum fields take the enum) plus an optional "
                    "Snapshot of the owning type, and nothing else");
      return false;
    }
  };

  if constexpr (std::is_enum_v<Value>) {
#if ULTRALIGHT_REFLECTION
    // Slot representation is the enumerator-name string: reflect it back so the
    // callable speaks the enum's own type end to end.
    const String8& utf8 = const_cast<String&>(static_cast<const String&>(value)).utf8();
    const std::string_view name(utf8.data(), utf8.length());
    for (const auto& entry : EnumInfo<Value>::kEntries) {
      if (entry.name == name)
        return invoke(static_cast<Value>(entry.value));
    }
    return false;
#else
    // Unreachable by construction: without reflection no ValueTraits exists for enums,
    // so an enum field cannot be a schema leaf and this thunk never lowers.
    return false;
#endif
  } else if constexpr (Info::kKind == ValueKind::String
                       && !std::is_invocable_v<const Fn&, const Slot&, Snapshot<T>>
                       && !std::is_invocable_v<const Fn&, const Slot&>) {
    // A field-typed string validator (the common std::string spelling): bridge the slot
    // String through the field's own value type and back.
    static_assert(std::is_constructible_v<Value, const char*, size_t>,
                  "dom::data: a Validate callable on this string field must take the "
                  "slot String (its value type cannot be built from the slot)");
    const String8& utf8 = const_cast<String&>(static_cast<const String&>(value)).utf8();
    return invoke(Value(utf8.data(), utf8.length()));
  } else {
    return invoke(static_cast<const Slot&>(value));
  }
}

// A validator that throws rejects the edit (reported under the field's name).
template <typename T, size_t I, typename Slot>
bool RunValidatorGuarded(Slot& value, RecordOps* published) {
  bool accepted = false;
  CallHandler(nullptr, TypeTraits<T>::schema.template get<I>().name,
              [&] { accepted = RunValidator<T, I>(value, published); });
  return accepted;
}

template <typename T, size_t I>
bool ValidateEntryBool(bool* value, RecordOps* published) {
  return RunValidatorGuarded<T, I>(*value, published);
}
template <typename T, size_t I>
bool ValidateEntryInt64(int64_t* value, RecordOps* published) {
  return RunValidatorGuarded<T, I>(*value, published);
}
template <typename T, size_t I>
bool ValidateEntryDouble(double* value, RecordOps* published) {
  return RunValidatorGuarded<T, I>(*value, published);
}
template <typename T, size_t I>
bool ValidateEntryString(String* value, RecordOps* published) {
  return RunValidatorGuarded<T, I>(*value, published);
}
template <typename T, size_t I>
bool ValidateEntryColor(Color* value, RecordOps* published) {
  return RunValidatorGuarded<T, I>(*value, published);
}

#if ULTRALIGHT_REFLECTION

/// Joined enumerator-name storage behind an enum-typed field's "enum" metadata pair:
/// the schema export's channel for enum name sets (consumers split on ',').
template <typename E>
struct EnumNameText {
  static consteval size_t Length() {
    size_t length = 0;
    for (const auto& entry : EnumInfo<E>::kEntries)
      length += entry.name.size() + 1;
    return length;
  }
  static constexpr auto kText = [] {
    std::array<char, Length()> text {};
    size_t pos = 0;
    for (const auto& entry : EnumInfo<E>::kEntries) {
      if (pos)
        text[pos++] = ',';
      for (char c : entry.name)
        text[pos++] = c;
    }
    return text;
  }();
};

template <typename E>
inline constexpr DescriptorKV kEnumMetadata[]
    = { { "enum", EnumNameText<E>::kText.data() } };

#endif  // ULTRALIGHT_REFLECTION

/// The synthesized child table behind a scalar action payload (`Action<double>`): one
/// entry with the reserved name "value" carrying the payload's leaf kind, so the payload
/// shape flows through introspection and the schema export exactly like a described
/// struct's child table. The table's name is the kind's display name (payload leaves have
/// no type of their own to name).
template <typename P>
consteval EntryDescriptor MakeScalarPayloadEntry() {
  EntryDescriptor d {};
  d.name = "value";
  d.kind = static_cast<uint8_t>(ValueTraits<std::remove_cvref_t<P>>::kind);
  d.flags = std::is_same_v<std::remove_cvref_t<P>, float> ? kEntryFlags_Float32 : 0;
  d.key_kind = kDescriptorNoKeyKind;
  d.slot = 0;
#if ULTRALIGHT_REFLECTION
  if constexpr (std::is_enum_v<std::remove_cvref_t<P>>) {
    d.kv = kEnumMetadata<std::remove_cvref_t<P>>;
    d.kv_count = 1;
  }
#endif
  return d;
}

template <typename P>
const TypeDescriptor* ScalarPayloadTable() {
  constexpr ValueKind kind = ValueTraits<std::remove_cvref_t<P>>::kind;
  static constexpr EntryDescriptor kEntry = MakeScalarPayloadEntry<P>();
  static constexpr TypeDescriptor kTable { kDescriptorFormatVersion,
                                           kind == ValueKind::Bool        ? "bool"
                                               : kind == ValueKind::Int64 ? "int64"
                                               : kind == ValueKind::Double
                                               ? "double"
                                               : "string",
                                           1, &kEntry };
  return &kTable;
}

template <typename T, size_t I>
consteval EntryDescriptor MakeEntryDescriptor() {
  using SchemaType = std::remove_cvref_t<decltype(TypeTraits<T>::schema)>;
  using Tok = std::tuple_element_t<I, SchemaType>;
  using Info = EntryInfo<Tok>;

  EntryDescriptor d {};
  d.name = TypeTraits<T>::schema.template get<I>().name;
  d.kind = static_cast<uint8_t>(Info::kKind);
  d.flags = EntryFlagsOf(TypeTraits<T>::schema.template get<I>());
  d.key_kind = kDescriptorNoKeyKind;
  d.slot = static_cast<uint16_t>(I);

  // Page edits come back only for bool, number, string, and color leaves; classification
  // needs the complete trait surface, so this rule runs here rather than in Schema().
  if (d.flags & kEntryFlags_Editable) {
    if constexpr (Info::kClass != EntryClass::kLeaf)
      SchemaErrorEditableOnNonLeaf(d.name);
    else if constexpr (Info::kKind == ValueKind::StyleValue)
      SchemaErrorEditableOnStyleValue(d.name);
  }

  if constexpr (IsListToken<Tok>::value) {
    if constexpr (Tok::kKeyed)
      d.key_kind = static_cast<uint8_t>(Info::KeyKind());
  }

  // Structural children: a described type compiles its own table; a scalar action
  // payload (the only non-described child the schema admits) synthesizes one.
  if constexpr (!std::is_void_v<typename Info::child_type>) {
    if constexpr (Described<typename Info::child_type>)
      d.child = &Descriptors<typename Info::child_type>;
    else
      d.child = &ScalarPayloadTable<typename Info::child_type>;
  }

  if constexpr (Info::kClass == EntryClass::kLeaf || Info::kClass == EntryClass::kVar) {
    if constexpr (std::is_same_v<std::remove_cvref_t<typename Tok::value_type>, float>)
      d.flags = static_cast<uint8_t>(d.flags | kEntryFlags_Float32);
  }

  if constexpr ((Info::kClass == EntryClass::kLeaf || Info::kClass == EntryClass::kVar)
                && Info::kHasAccessor) {
    if constexpr (Info::kKind == ValueKind::Bool)
      d.read_bool = &ReadEntryBool<T, I>;
    else if constexpr (Info::kKind == ValueKind::Int64)
      d.read_int64 = &ReadEntryInt64<T, I>;
    else if constexpr (Info::kKind == ValueKind::Double)
      d.read_double = &ReadEntryDouble<T, I>;
    else if constexpr (Info::kKind == ValueKind::String)
      d.read_string = &ReadEntryString<T, I>;
    else if constexpr (Info::kKind == ValueKind::StyleValue)
      d.read_style = &ReadEntryStyle<T, I>;
    else if constexpr (Info::kKind == ValueKind::Color)
      d.read_color = &ReadEntryColor<T, I>;
  }

#if ULTRALIGHT_REFLECTION
  if constexpr (Info::kClass == EntryClass::kLeaf || Info::kClass == EntryClass::kVar) {
    if constexpr (std::is_enum_v<typename Tok::value_type>) {
      d.kv = kEnumMetadata<typename Tok::value_type>;
      d.kv_count = 1;
    }
  }
#endif

  // StyleValue-kind fields have no capture path, so their validators lower no thunk.
  if constexpr (Info::kClass == EntryClass::kLeaf && HasValidatorFor<T, I>()) {
    if constexpr (Info::kKind == ValueKind::Bool)
      d.validate_bool = &ValidateEntryBool<T, I>;
    else if constexpr (Info::kKind == ValueKind::Int64)
      d.validate_int64 = &ValidateEntryInt64<T, I>;
    else if constexpr (Info::kKind == ValueKind::Double)
      d.validate_double = &ValidateEntryDouble<T, I>;
    else if constexpr (Info::kKind == ValueKind::String)
      d.validate_string = &ValidateEntryString<T, I>;
    else if constexpr (Info::kKind == ValueKind::Color)
      d.validate_color = &ValidateEntryColor<T, I>;
  }
  return d;
}

template <typename T, size_t... Is>
consteval auto MakeEntryDescriptors(std::index_sequence<Is...>) {
  return std::array<EntryDescriptor, sizeof...(Is)> { MakeEntryDescriptor<T, Is>()... };
}

template <typename T>
inline constexpr auto kEntryDescriptors
    = MakeEntryDescriptors<T>(std::make_index_sequence<TypeTraits<T>::schema.kCount> {});

template <typename T>
inline constexpr TypeDescriptor kTypeDescriptor = {
  kDescriptorFormatVersion,
#if ULTRALIGHT_REFLECTION
  ultralight::detail::kTypeNameZ<T>.data,
#else
  "",
#endif
  static_cast<uint32_t>(TypeTraits<T>::schema.kCount),
  kEntryDescriptors<T>.data(),
};

}  // namespace detail
}  // namespace data
}  // namespace dom
}  // namespace ultralight
/// \endcond
