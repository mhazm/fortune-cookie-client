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
#include <Ultralight/CAPI/CAPI_String.h>

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <string>
#include <string_view>
#include <type_traits>

namespace ultralight {
namespace detail {

// ---------------------------------------------------------------------------------------
// String adapters over the C waist.
// ---------------------------------------------------------------------------------------

// Consumes an owning ULString into a std::string (null-safe).
inline std::string TakeString(ULString s) {
  if (!s)
    return std::string();
  std::string result(ulStringGetData(s), ulStringGetLength(s));
  ulDestroyString(s);
  return result;
}

// The C waist takes null-terminated UTF-8 for names and selectors; this adapts a
// std::string_view without allocating for typical lengths.
class CString {
 public:
  explicit CString(std::string_view text) {
    if (text.size() < sizeof(buffer_)) {
      char* out = buffer_;
      for (char c : text)
        *out++ = c;
      *out = '\0';
      data_ = buffer_;
    } else {
      spill_.assign(text);
      data_ = spill_.c_str();
    }
  }

  CString(const CString&) = delete;
  CString& operator=(const CString&) = delete;

  const char* c_str() const { return data_; }

 private:
  const char* data_;
  std::string spill_;
  char buffer_[192];
};

// ---------------------------------------------------------------------------------------
// Numeric conversion: the families' number currency is the double (JavaScript's number
// type); integral targets truncate and wrap modulo 2^bits (the ToInt32 family), so NaN,
// infinities, and out-of-range values convert deterministically instead of invoking
// undefined behavior.
// ---------------------------------------------------------------------------------------

constexpr double PowerOfTwo(unsigned bits) {
  double result = 1.0;
  for (unsigned i = 0; i < bits; i++)
    result *= 2.0;
  return result;
}

template <typename T>
T DoubleToArithmetic(double d) {
  if constexpr (std::is_floating_point_v<T>) {
    return static_cast<T>(d);
  } else {
    if (std::isnan(d) || std::isinf(d))
      return T(0);
    constexpr double kRange = PowerOfTwo(sizeof(T) * 8);
    double m = std::fmod(std::trunc(d), kRange);
    if (m < 0)
      m += kRange;
    using U = std::make_unsigned_t<T>;
    return static_cast<T>(static_cast<U>(m));
  }
}

// Compact human formatting: exact integers print in full (the interesting digits ARE the
// value), everything else uses the short scientific form.
inline void FormatDoubleCompact(char* buf, size_t size, double d) {
  if (std::isfinite(d) && d == std::trunc(d) && std::fabs(d) < 1e15)
    snprintf(buf, size, "%.0f", d);
  else
    snprintf(buf, size, "%g", d);
}

// Whether converting `d` to integral T loses information: NaN/Inf and fractional values
// truncate, out-of-range values wrap. Bounds are exact powers of two, so the double
// comparisons are exact.
template <typename T>
bool IsLossyFromDouble(double d) {
  if (std::isnan(d) || std::isinf(d) || std::trunc(d) != d)
    return true;
  constexpr double kHi
      = std::is_signed_v<T> ? PowerOfTwo(sizeof(T) * 8 - 1) : PowerOfTwo(sizeof(T) * 8);
  constexpr double kLo = std::is_signed_v<T> ? -PowerOfTwo(sizeof(T) * 8 - 1) : 0.0;
  return d < kLo || d >= kHi;
}

}  // namespace detail
}  // namespace ultralight
/// \endcond
