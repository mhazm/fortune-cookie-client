///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// Implementation detail of `<Ultralight/dom/data/ValueTraits.h>`; include that header
/// instead. Everything here lives in ultralight::dom::data::detail and may change without
/// notice.
///
/// The dd enumerator-name table over the family-neutral reflection engine in
/// `<Ultralight/detail/Reflection.h>`, driven by the dd::EnumRange customization point
/// (per-family, exactly like the js table over js::EnumRange).
///
#pragma once
#include <Ultralight/detail/Reflection.h>

#include <array>
#include <cstddef>
#include <string_view>
#include <utility>

/// \cond INTERNAL

#if ULTRALIGHT_REFLECTION

namespace ultralight {
namespace dom {
namespace data {

// Declared here, defined (with its documentation) in <Ultralight/dom/data/ValueTraits.h>.
template <typename E>
struct EnumRange;

namespace detail {

template <typename E>
struct EnumInfo {
  static constexpr long long kMin = EnumRange<E>::min;
  static constexpr long long kMax = EnumRange<E>::max;
  static constexpr size_t kSpan = static_cast<size_t>(kMax - kMin + 1);
  static_assert(kSpan >= 1 && kSpan <= 1024, "dom::data::EnumRange span must be 1..1024");

  static constexpr size_t CountValid() {
    return []<size_t... Is>(std::index_sequence<Is...>) {
      return ((ultralight::detail::EnumeratorName<E, kMin + static_cast<long long>(Is)>()
                       .empty()
                   ? 0u
                   : 1u)
              + ...);
    }(std::make_index_sequence<kSpan> {});
  }

  static constexpr size_t kCount = CountValid();
  static_assert(kCount > 0, "no enumerators found in dom::data::EnumRange; widen the range "
                            "or specialize dom::data::ValueTraits for this enum");

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
                ultralight::detail::EnumeratorName<E, kMin + static_cast<long long>(Is)>()),
       ...);
    }(std::make_index_sequence<kSpan> {});
    return entries;
  }();

  static constexpr std::string_view NameOf(long long value) {
    for (const Entry& entry : kEntries) {
      if (entry.value == value)
        return entry.name;
    }
    return {};
  }
};

}  // namespace detail
}  // namespace data
}  // namespace dom
}  // namespace ultralight

#endif  // ULTRALIGHT_REFLECTION
/// \endcond
