///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// Implementation detail of `<Ultralight/dom/data/Schema.h>`; include that header instead.
/// Everything here lives in ultralight::dom::data::detail and may change without notice.
///
/// Schema construction machinery: token/annotation classification, the flat indexed
/// storage schemas live in, the construction plan (Reflect() expansion, annotation
/// positions), the consteval name checks, and the constructed Schema type with its tuple
/// protocol. Included by Schema.h between the token declarations (whose types the
/// machinery names) and the public factories (which call it).
///
/// \cond INTERNAL
#pragma once

#include <array>
#include <cstddef>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace ultralight {
namespace dom {
namespace data {
namespace detail {

template <typename T>
struct IsValidateWith : std::false_type {};
template <typename F>
struct IsValidateWith<ValidateWith<F>> : std::true_type {};

template <typename T>
struct IsValidateEntry : std::false_type {};
template <typename F>
struct IsValidateEntry<ValidateEntry<F>> : std::true_type {};

// Flag folding for Field tags. Unknown tag types are rejected here so the diagnostic
// lands at the token factory call.
template <typename... Tags>
consteval uint8_t FieldFlags(const Tags&...) {
  uint8_t flags = 0;
  auto apply = [&]<typename Tag>() {
    if constexpr (std::is_same_v<Tag, EditableTag>) {
      flags |= kEntryFlags_Editable;
    } else if constexpr (std::is_same_v<Tag, InternalTag>) {
      flags |= kEntryFlags_Internal;
    } else if constexpr (IsValidateWith<Tag>::value) {
      // Stored in the token's validator slot, not in flags.
    } else {
      static_assert(kAlwaysFalse<Tag>, "unknown tag on Field(): expected dd::Editable, "
                                       "dd::Internal, or dd::Validate(fn)");
    }
  };
  (apply.template operator()<Tags>(), ...);
  return flags;
}

template <typename... Tags>
consteval uint8_t ListFlags(const Tags&...) {
  uint8_t flags = 0;
  auto apply = [&]<typename Tag>() {
    if constexpr (std::is_same_v<Tag, InternalTag>) {
      flags |= kEntryFlags_Internal;
    } else {
      static_assert(kAlwaysFalse<Tag>, "unknown tag on List(): only dd::Internal applies "
                                       "(Editable and Validate apply to Field entries)");
    }
  };
  (apply.template operator()<Tags>(), ...);
  return flags;
}

template <typename... Tags>
consteval uint8_t ActionFlags(const Tags&...) {
  uint8_t flags = 0;
  auto apply = [&]<typename Tag>() {
    if constexpr (std::is_same_v<Tag, OnlyLatestTag>) {
      flags |= kEntryFlags_OnlyLatest;
    } else {
      static_assert(kAlwaysFalse<Tag>, "unknown tag on Action(): only dd::OnlyLatest "
                                       "applies");
    }
  };
  (apply.template operator()<Tags>(), ...);
  return flags;
}

consteval NoValidate PickValidator() { return {}; }
template <typename First, typename... Rest>
consteval auto PickValidator(const First& first, const Rest&... rest) {
  if constexpr (IsValidateWith<First>::value)
    return first;
  else
    return PickValidator(rest...);
}

template <typename E>
struct IsFieldToken : std::false_type {};
template <typename O, typename V, typename A, typename Va>
struct IsFieldToken<FieldToken<O, V, A, Va>> : std::true_type {};

template <typename E>
struct IsListToken : std::false_type {};
template <typename O, typename C, typename A, typename K>
struct IsListToken<ListToken<O, C, A, K>> : std::true_type {};

template <typename E>
struct IsActionToken : std::false_type {};
template <typename P>
struct IsActionToken<ActionToken<P>> : std::true_type {};

template <typename E>
struct IsVarToken : std::false_type {};
template <typename O, typename V, typename A>
struct IsVarToken<VarToken<O, V, A>> : std::true_type {};

template <typename E>
struct IsReflectEntry : std::false_type {};
#if ULTRALIGHT_REFLECTION
template <typename T>
struct IsReflectEntry<ReflectEntry<T>> : std::true_type {
  using type = T;
};
#endif

template <typename E>
inline constexpr bool kIsToken = IsFieldToken<E>::value || IsListToken<E>::value
    || IsActionToken<E>::value || IsVarToken<E>::value;

template <typename E>
inline constexpr bool kIsPairEntry
    = std::is_same_v<E, EditableEntry> || std::is_same_v<E, InternalEntry>;

template <typename E>
inline constexpr bool kIsSchemaEntry = kIsToken<E> || kIsPairEntry<E>
    || IsValidateEntry<E>::value || IsReflectEntry<E>::value;

// ---- Flat indexed storage ----------------------------------------------------------------

// Schemas hold their tokens in a PackStorage, not a std::tuple. Element I is a PackLeaf
// base found by one template-argument deduction, while std::tuple_element and std::get
// recurse through the element list on some standard libraries: instantiations quadratic
// in the entry count, each naming the whole list, which exhausts the compiler's memory
// at a few dozen entries.
template <size_t I, typename T>
struct PackLeaf {
  T value;
};

template <typename Seq, typename... Ts>
struct PackStorage;

template <size_t... Is, typename... Ts>
struct PackStorage<std::index_sequence<Is...>, Ts...> : PackLeaf<Is, Ts>... {
  static constexpr size_t kSize = sizeof...(Ts);

  constexpr PackStorage(const Ts&... values) : PackLeaf<Is, Ts> { values }... {}
};

template <typename... Ts>
using Pack = PackStorage<std::index_sequence_for<Ts...>, Ts...>;

template <size_t I, typename T>
constexpr const T& PackGet(const PackLeaf<I, T>& leaf) {
  return leaf.value;
}

template <size_t I, typename T>
constexpr T& PackGet(PackLeaf<I, T>& leaf) {
  return leaf.value;
}

template <size_t I, typename T>
std::type_identity<T> PackLeafType(const PackLeaf<I, T>&);

template <size_t I, typename P>
using PackElement = typename decltype(PackLeafType<I>(std::declval<const P&>()))::type;

// ---- Schema() construction ---------------------------------------------------------------

// Every step is linear in template instantiations. A type-level plan says which entries
// yield tokens (a Reflect() entry yields one per member) and where the annotations sit;
// one pack expansion materializes the tokens; and every name rule runs as plain loops
// over arrays of per-entry facts inside one consteval call. Names are values, not
// template arguments, so matching them per pair in templates would instantiate once per
// pair.

#if ULTRALIGHT_REFLECTION

// The generated accessor for reflected member I of T. A concrete callable class (not a
// lambda) so tokens and thunks can default-construct it from its type alone. Reads by
// const reference: walks must not copy members (move-only members cannot be copied at
// all).
template <typename T, size_t I>
struct ReflectedAccessor {
  constexpr const ultralight::detail::FieldType<T, I>& operator()(const T& instance) const {
    return *ultralight::detail::MemberPointer<I>(instance);
  }
};

template <size_t Member, typename T>
constexpr auto TokenFromEntry(const ReflectEntry<T>&) {
  return FieldToken<T, ultralight::detail::FieldType<T, Member>, ReflectedAccessor<T, Member>> {
    ultralight::detail::kFieldNameZ<T, Member>.data, ReflectedAccessor<T, Member> {},
    NoValidate {}, 0
  };
}

#endif  // ULTRALIGHT_REFLECTION

template <size_t Member, typename E>
constexpr E TokenFromEntry(const E& token) {
  return token;
}

template <typename E>
consteval size_t TokenCount() {
  static_assert(kIsSchemaEntry<E>,
                "unknown entry passed to Schema(): expected Field/List/Action/Var tokens, "
                "Reflect<T>(), or name-addressed annotations");
#if ULTRALIGHT_REFLECTION
  if constexpr (IsReflectEntry<E>::value)
    return ultralight::detail::CountFields<typename IsReflectEntry<E>::type>();
  else
#endif
    return kIsToken<E> ? 1 : 0;
}

struct TokenSource {
  size_t entry;   // The Schema() argument the token comes from.
  size_t member;  // The member index within a Reflect() entry (0 otherwise).
};

template <size_t Count, size_t N>
consteval std::array<TokenSource, Count> TokenSources(const std::array<size_t, N>& counts) {
  std::array<TokenSource, Count> sources {};
  size_t k = 0;
  for (size_t e = 0; e < N; e++) {
    for (size_t m = 0; m < counts[e]; m++)
      sources[k++] = { e, m };
  }
  return sources;
}

template <size_t Count, size_t N>
consteval std::array<size_t, Count> IndicesWhere(const std::array<bool, N>& keep) {
  std::array<size_t, Count> indices {};
  size_t k = 0;
  for (size_t i = 0; i < N; i++) {
    if (keep[i])
      indices[k++] = i;
  }
  return indices;
}

// Sums over a braced list instead of a fold expression: a fold nests one level per Schema()
// argument and hits the compiler's nesting limit (256 on clang) long before the token limit.
template <typename T, size_t N>
consteval size_t Sum(const std::array<T, N>& values) {
  size_t sum = 0;
  for (size_t i = 0; i < N; i++)
    sum += size_t(values[i]);
  return sum;
}

template <typename... Entries>
struct SchemaPlan {
  static constexpr std::array<size_t, sizeof...(Entries)> kTokenCounts { TokenCount<Entries>()... };
  static constexpr std::array<bool, sizeof...(Entries)> kIsPair { kIsPairEntry<Entries>... };
  static constexpr std::array<bool, sizeof...(Entries)> kIsValidator {
    IsValidateEntry<Entries>::value...
  };

  static constexpr size_t kTokenCount = Sum(kTokenCounts);
  static constexpr size_t kPairCount = Sum(kIsPair);
  static constexpr size_t kValidatorCount = Sum(kIsValidator);

  static constexpr auto kTokenSources = TokenSources<kTokenCount>(kTokenCounts);
  static constexpr auto kPairIndices = IndicesWhere<kPairCount>(kIsPair);
  static constexpr auto kValidatorIndices = IndicesWhere<kValidatorCount>(kIsValidator);
};

template <typename Plan, typename All, size_t K>
using PlannedToken = decltype(TokenFromEntry<Plan::kTokenSources[K].member>(
    PackGet<Plan::kTokenSources[K].entry>(std::declval<const All&>())));

template <typename Plan, typename All, size_t... Ks>
consteval auto MaterializeTokens(const All& all, std::index_sequence<Ks...>) {
  return Pack<PlannedToken<Plan, All, Ks>...>(TokenFromEntry<Plan::kTokenSources[Ks].member>(
      PackGet<Plan::kTokenSources[Ks].entry>(all))...);
}

template <typename Plan, typename All, size_t... Vs>
consteval auto SelectValidators(const All& all, std::index_sequence<Vs...>) {
  return Pack<PackElement<Plan::kValidatorIndices[Vs], All>...>(
      PackGet<Plan::kValidatorIndices[Vs]>(all)...);
}

template <typename M>
struct MemberFnArity {};
template <typename R, typename L, typename A0, typename... As>
struct MemberFnArity<R (L::*)(A0, As...) const> {
  static constexpr size_t value = 1 + sizeof...(As);
  using first_arg = A0;
};
template <typename R, typename L, typename A0, typename... As>
struct MemberFnArity<R (L::*)(A0, As...) const noexcept> {
  static constexpr size_t value = 1 + sizeof...(As);
  using first_arg = A0;
};

// Whether a validator callable is plausibly compatible with a field of value type V. The
// pure form is checked exactly, with one carve-out: string fields may spell their
// validators on the slot String rather than the field's own string type, and the slot
// representation is not visible from this header, so a callable whose value parameter is
// the String class is admitted (the surface that invokes validators owns the exact
// check). For the two-argument cross-field form only the value parameter can be checked
// here: the snapshot parameter's type is defined downstream of this header. Generic
// callables cannot be introspected and are admitted on the same basis.
template <typename Fn, typename V>
consteval bool ValidatorPlausible() {
  if constexpr (std::is_invocable_v<const Fn&, V>) {
    return true;
  } else if constexpr (requires { MemberFnArity<decltype(&Fn::operator())>::value; }) {
    using Arity = MemberFnArity<decltype(&Fn::operator())>;
    using First = std::remove_cvref_t<typename Arity::first_arg>;
    if constexpr (std::is_same_v<First, ultralight::String>)
      return true;
    else if constexpr (Arity::value == 2)
      return std::is_convertible_v<V, First>;
    else
      return false;
  } else {
    return true;
  }
}

// A non-field token has no value type to check a validator against; pairing one there
// fails as SchemaErrorAnnotationTargetNotAField instead.
struct NotAField {};

template <typename Tok>
struct FieldValueOf {
  using type = NotAField;
};
template <typename O, typename V, typename A, typename Va>
struct FieldValueOf<FieldToken<O, V, A, Va>> {
  using type = V;
};

template <typename Fn, typename V>
consteval bool ValidatorFits() {
  if constexpr (std::is_same_v<V, NotAField>)
    return true;
  else
    return ValidatorPlausible<Fn, V>();
}

// ---- Name checks -------------------------------------------------------------------------

enum class TokenKind : uint8_t { kField, kList, kAction, kVar };

struct TokenFacts {
  const char* name = nullptr;
  TokenKind kind = TokenKind::kVar;
  uint8_t flags = 0;
  bool has_validator = false;  // A validator attached with the Field() tag form.
};

struct AnnotationFacts {
  const char* name = nullptr;
  uint8_t flag = 0;  // kEntryFlags_Editable or kEntryFlags_Internal.
};

template <size_t N, size_t P, size_t V>
struct SchemaFacts {
  std::array<TokenFacts, N> tokens {};
  std::array<AnnotationFacts, P> annotations {};
  std::array<const char*, V> validator_names {};
  std::array<std::array<bool, N>, V> validator_fits {};  // [validator][token]
  std::array<size_t, N> validator_index {};               // Output: kNpos, or a validator.
};

template <typename Tok>
constexpr TokenFacts FactsOf(const Tok& token) {
  TokenFacts facts;
  facts.name = token.name;
  if constexpr (IsFieldToken<Tok>::value) {
    facts.kind = TokenKind::kField;
    facts.has_validator = Tok::kHasValidator;
  } else if constexpr (IsListToken<Tok>::value) {
    facts.kind = TokenKind::kList;
  } else if constexpr (IsActionToken<Tok>::value) {
    facts.kind = TokenKind::kAction;
  }
  if constexpr (requires { token.flags; })
    facts.flags = token.flags;
  return facts;
}

template <typename Tok>
constexpr void StoreFlags(Tok& token, uint8_t flags) {
  if constexpr (requires { token.flags; })
    token.flags = flags;
}

// The schema rules that compare names, in a fixed order so the first broken rule is the
// one reported: name-addressed Editable/Internal pairing, name-addressed validators, tag
// validators on non-Editable fields, then duplicate names. Pairing writes the token flags;
// validator pairing fills validator_index.
template <size_t N, size_t P, size_t V>
consteval void ResolveSchema(SchemaFacts<N, P, V>& facts) {
  for (const AnnotationFacts& annotation : facts.annotations) {
    size_t matches = 0;
    for (TokenFacts& token : facts.tokens) {
      if (!NamesEqual(token.name, annotation.name))
        continue;
      matches++;
      const bool applies = token.kind == TokenKind::kField
          || (token.kind == TokenKind::kList && annotation.flag == kEntryFlags_Internal);
      if (!applies)
        SchemaErrorAnnotationTargetNotAField(annotation.name);
      if (token.flags & annotation.flag)
        SchemaErrorAnnotationDuplicated(annotation.name);
      token.flags = static_cast<uint8_t>(token.flags | annotation.flag);
    }
    if (matches == 0)
      SchemaErrorAnnotationTargetNotFound(annotation.name);
  }

  for (size_t i = 0; i < N; i++)
    facts.validator_index[i] = kNpos;
  for (size_t v = 0; v < V; v++) {
    const char* name = facts.validator_names[v];
    for (size_t w = v + 1; w < V; w++) {
      if (NamesEqual(facts.validator_names[w], name))
        SchemaErrorValidatorDuplicated(name);
    }
    size_t matches = 0;
    for (size_t i = 0; i < N; i++) {
      const TokenFacts& token = facts.tokens[i];
      if (!NamesEqual(token.name, name))
        continue;
      matches++;
      if (token.kind != TokenKind::kField)
        SchemaErrorAnnotationTargetNotAField(name);
      if (token.has_validator)
        SchemaErrorValidatorDuplicated(name);
      if (!facts.validator_fits[v][i])
        SchemaErrorValidatorSignatureMismatch(name);
      if (!(token.flags & kEntryFlags_Editable))
        SchemaErrorValidatorWithoutEditable(name);
      facts.validator_index[i] = v;
    }
    if (matches == 0)
      SchemaErrorAnnotationTargetNotFound(name);
  }

  // Validators run only for page edits, so one on a field the page can't edit would never
  // run. Runs after pairing, so a name-addressed Editable counts.
  for (const TokenFacts& token : facts.tokens) {
    if (token.has_validator && !(token.flags & kEntryFlags_Editable))
      SchemaErrorValidatorWithoutEditable(token.name);
  }

  for (size_t i = 0; i < N; i++) {
    for (size_t j = i + 1; j < N; j++) {
      if (NamesEqual(facts.tokens[i].name, facts.tokens[j].name))
        SchemaErrorDuplicateEntryName(facts.tokens[i].name);
    }
  }
}

///
/// The constructed schema: the described type's token list plus its surviving
/// name-addressed validator entries. Tuple-like over the tokens (tuple protocol +
/// structured bindings). Created only through `Schema(...)`; the type itself is an
/// implementation detail.
///
template <typename TokenPack, typename ValidatorPack>
struct Schema {
  static constexpr size_t kCount = TokenPack::kSize;

  TokenPack tokens;
  ValidatorPack validators;
  std::array<const char*, kCount> names;
  std::array<size_t, kCount> validator_index;  ///< The paired entry in `validators`, or kNpos.

  template <size_t I>
  constexpr const auto& get() const {
    return PackGet<I>(tokens);
  }

  constexpr size_t IndexOf(std::string_view name) const {
    for (size_t i = 0; i < kCount; i++) {
      if (std::string_view(names[i]) == name)
        return i;
    }
    return kNpos;
  }

  constexpr bool Contains(std::string_view name) const { return IndexOf(name) != kNpos; }
};

template <typename T>
struct IsSchemaInstance : std::false_type {};
template <typename TT, typename VT>
struct IsSchemaInstance<Schema<TT, VT>> : std::true_type {};

template <typename Plan, typename All, typename Facts, size_t... Ps>
consteval void CollectAnnotations(const All& all, Facts& facts, std::index_sequence<Ps...>) {
  ((facts.annotations[Ps]
    = { PackGet<Plan::kPairIndices[Ps]>(all).name,
        std::is_same_v<PackElement<Plan::kPairIndices[Ps], All>, EditableEntry>
            ? uint8_t(kEntryFlags_Editable)
            : uint8_t(kEntryFlags_Internal) }),
   ...);
}

template <typename TokenPack, typename Fn, size_t... Ks>
consteval std::array<bool, sizeof...(Ks)> ValidatorFitsRow(std::index_sequence<Ks...>) {
  return { ValidatorFits<Fn, typename FieldValueOf<PackElement<Ks, TokenPack>>::type>()... };
}

template <typename TokenPack, typename ValidatorPack, typename Facts, size_t... Vs>
consteval void CollectValidators(const ValidatorPack& validators, Facts& facts,
                                 std::index_sequence<Vs...>) {
  ((facts.validator_names[Vs] = PackGet<Vs>(validators).name,
    facts.validator_fits[Vs]
    = ValidatorFitsRow<TokenPack, decltype(PackGet<Vs>(validators).fn)>(
        std::make_index_sequence<TokenPack::kSize> {})),
   ...);
}

template <typename TokenPack, typename ValidatorPack, typename Facts, size_t... Ks>
consteval auto FinishSchema(TokenPack tokens, const ValidatorPack& validators, Facts& facts,
                            std::index_sequence<Ks...>) {
  ((facts.tokens[Ks] = FactsOf(PackGet<Ks>(tokens))), ...);
  ResolveSchema(facts);
  (StoreFlags(PackGet<Ks>(tokens), facts.tokens[Ks].flags), ...);
  return Schema<TokenPack, ValidatorPack> { tokens, validators,
                                            { facts.tokens[Ks].name... },
                                            facts.validator_index };
}

template <typename... Entries>
consteval auto BuildSchema(const Entries&... entries) {
  using Plan = SchemaPlan<Entries...>;
  const Pack<Entries...> all(entries...);
  auto tokens = MaterializeTokens<Plan>(all, std::make_index_sequence<Plan::kTokenCount> {});
  auto validators
      = SelectValidators<Plan>(all, std::make_index_sequence<Plan::kValidatorCount> {});
  using TokenPack = decltype(tokens);
  SchemaFacts<Plan::kTokenCount, Plan::kPairCount, Plan::kValidatorCount> facts;
  CollectAnnotations<Plan>(all, facts, std::make_index_sequence<Plan::kPairCount> {});
  CollectValidators<TokenPack>(validators, facts,
                               std::make_index_sequence<Plan::kValidatorCount> {});
  return FinishSchema(tokens, validators, facts,
                      std::make_index_sequence<Plan::kTokenCount> {});
}

}  // namespace detail
}  // namespace data
}  // namespace dom
}  // namespace ultralight

template <size_t... Is, typename... Ts>
struct std::tuple_size<
    ultralight::dom::data::detail::PackStorage<std::index_sequence<Is...>, Ts...>>
    : std::integral_constant<size_t, sizeof...(Ts)> {};

template <size_t I, size_t... Is, typename... Ts>
struct std::tuple_element<
    I, ultralight::dom::data::detail::PackStorage<std::index_sequence<Is...>, Ts...>> {
  using type = ultralight::dom::data::detail::PackElement<
      I, ultralight::dom::data::detail::PackStorage<std::index_sequence<Is...>, Ts...>>;
};

template <typename TT, typename VT>
struct std::tuple_size<ultralight::dom::data::detail::Schema<TT, VT>>
    : std::integral_constant<size_t, TT::kSize> {};

template <size_t I, typename TT, typename VT>
struct std::tuple_element<I, ultralight::dom::data::detail::Schema<TT, VT>> {
  using type = ultralight::dom::data::detail::PackElement<I, TT>;
};
/// \endcond
