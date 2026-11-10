///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// Implementation detail shared by the typed C++ header families (`<Ultralight/js/*.h>`,
/// `<Ultralight/dom/*.h>`); include those instead. Everything here lives in
/// ultralight::detail and may change without notice.
///
/// \cond INTERNAL
#pragma once

#include <cstddef>
#include <string_view>

namespace ultralight {
namespace detail {

// A compile-time string usable as a template parameter (dom::css<"...">).
template <size_t N>
struct FixedString {
  char data[N] = {};

  consteval FixedString(const char (&text)[N]) {
    for (size_t i = 0; i < N; i++)
      data[i] = text[i];
  }

  constexpr std::string_view view() const { return std::string_view(data, N - 1); }
};

template <size_t N>
FixedString(const char (&)[N]) -> FixedString<N>;

}  // namespace detail
}  // namespace ultralight
/// \endcond
