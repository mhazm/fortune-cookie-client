///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// Implementation detail of `<Ultralight/js/TypeTraits.h>`; include that header instead.
/// Everything here lives in ultralight::js::detail and may change without notice.
///
/// The js-specific policy over the shared name-reflection engine in
/// `<Ultralight/detail/Reflection.h>` (its ULTRALIGHT_REFLECTION gate is documented in
/// `<Ultralight/js/TypeTraits.h>`): which aggregates the automatic conversion admits, and the
/// js::EnumRange-driven enumerator table with its schema string.
///
#pragma once
#include <Ultralight/detail/Reflection.h>
#include <Ultralight/js/Holder.h>
#include <Ultralight/js/detail/Marshal.h>

#include <array>
#include <cstddef>
#include <string_view>
#include <type_traits>
#include <utility>

/// \cond INTERNAL

#if ULTRALIGHT_REFLECTION

namespace ultralight {
namespace js {

// Declared here, defined (with its documentation) in <Ultralight/js/TypeTraits.h>.
template <typename E>
struct EnumRange;

namespace detail {

// The name-reflection engine lives in the family-neutral ultralight::detail; these
// re-exports keep the established js::detail spellings working.
using ultralight::detail::CountFields;
using ultralight::detail::EnumeratorName;
using ultralight::detail::FieldNameOf;
using ultralight::detail::FieldType;
using ultralight::detail::kFieldNames;
using ultralight::detail::kFieldNameZ;
using ultralight::detail::kTypeNameZ;
using ultralight::detail::MemberPointer;
using ultralight::detail::UnqualifiedTypeName;

template <typename E>
struct EnumInfo {
  static constexpr long long kMin = EnumRange<E>::min;
  static constexpr long long kMax = EnumRange<E>::max;
  static constexpr size_t kSpan = static_cast<size_t>(kMax - kMin + 1);
  static_assert(kSpan >= 1 && kSpan <= 1024, "js::EnumRange span must be 1..1024");

  static constexpr size_t CountValid() {
    return []<size_t... Is>(std::index_sequence<Is...>) {
      return ((EnumeratorName<E, kMin + static_cast<long long>(Is)>().empty() ? 0u : 1u)
              + ...);
    }(std::make_index_sequence<kSpan> {});
  }

  static constexpr size_t kCount = CountValid();
  static_assert(kCount > 0, "no enumerators found in js::EnumRange; widen the range or "
                            "specialize js::TypeTraits for this enum");

  struct Entry {
    long long value;
    std::string_view name;
  };

  static constexpr std::array<Entry, kCount> kEntries = [] {
    std::array<Entry, kCount> entries {};
    size_t filled = 0;
    [&]<size_t... Is>(std::index_sequence<Is...>) {
      auto consider = [&](long long value, std::string_view name) {
        if (!name.empty())
          entries[filled++] = Entry { value, name };
      };
      (consider(kMin + static_cast<long long>(Is),
                EnumeratorName<E, kMin + static_cast<long long>(Is)>()),
       ...);
    }(std::make_index_sequence<kSpan> {});
    return entries;
  }();

  static constexpr auto kSchema = [] {
    constexpr size_t total = [] {
      size_t n = 0;
      for (const Entry& e : kEntries)
        n += e.name.size() + 5;
      return n;
    }();
    FixedString<total> s {};
    bool first = true;
    for (const Entry& e : kEntries) {
      if (!first)
        s.Append(" | ");
      first = false;
      s.Append("'");
      s.Append(e.name);
      s.Append("'");
    }
    return s;
  }();
};

// The aggregate specialization applies to plain user structs only: std::array is an
// aggregate but reflects through its internal member, the js::null / js::undefined tag
// structs are (empty) aggregates with dedicated traits and JSON forms of their own, and
// types with their own dedicated specialization must not also match here. Holders are
// excluded in BOTH contract shapes: an aggregate-shaped weak holder must stay a holder,
// never silently become a reflected {field...} object.
template <typename T>
inline constexpr bool kIsReflectableAggregate = std::is_aggregate_v<T> && std::is_class_v<T>
    && !IsStdArray<T>::value && !std::is_polymorphic_v<T> && !std::is_same_v<T, NullType>
    && !std::is_same_v<T, UndefinedType> && !Holder<T> && !LockableHolder<T>;

}  // namespace detail
}  // namespace js
}  // namespace ultralight

#endif  // ULTRALIGHT_REFLECTION
/// \endcond
