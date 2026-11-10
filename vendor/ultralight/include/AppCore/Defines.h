///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once

// Needed for limit defines, like INTMAX_MAX, which is used by the std C++ library
#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif

#include <cstdint>
#include <cstddef>
#include <limits.h>

#ifdef SWIG
#define AExport
#else

// Require C++20 Support
#if defined(_MSC_VER)
#   if _MSC_VER < 1930
#       error This project needs at least Visual Studio 2022 to build
#   elif defined(_MSVC_LANG) && _MSVC_LANG < 202002L
#       error This project must be compiled with /std:c++20 or later
#   endif
#elif __cplusplus < 202002L
#   error This project can only be compiled with a compiler that supports C++20
#endif

#if defined(ULTRALIGHT_STATIC_BUILD)
#  define AExport 
#else
#  if defined(__WIN32__) || defined(_WIN32)
#    if defined(APPCORE_IMPLEMENTATION)
#      define AExport __declspec(dllexport)
#    else
#      define AExport __declspec(dllimport)
#    endif
#  else
#    define AExport __attribute__((visibility("default")))
#  endif
#endif

#if defined(__WIN32__) || defined(_WIN32)
#  define _thread_local __declspec(thread)
#  ifndef _NATIVE_WCHAR_T_DEFINED
#    define DISABLE_NATIVE_WCHAR_T
#  endif
#else
#  define _thread_local __thread
#endif

#include <type_traits>

namespace ultralight {

///
/// Whether or not a scoped enum has opted into the bitmask operators below by declaring
/// `consteval void ul_enable_bitmask_operators(E);` beside its definition (found by
/// argument-dependent lookup). WindowFlags and WindowButtons opt in, so their values
/// combine naturally:
///
/// ```
/// auto window = Window::Create(monitor, 1024, 768, false,
///                              WindowFlags::Titled | WindowFlags::Resizable);
/// ```
///
template <typename T>
concept BitmaskEnum = std::is_enum_v<T> && requires(T e) { ul_enable_bitmask_operators(e); };

template <BitmaskEnum T>
constexpr T operator|(T a, T b) {
  using U = std::underlying_type_t<T>;
  return static_cast<T>(static_cast<U>(a) | static_cast<U>(b));
}

template <BitmaskEnum T>
constexpr T operator&(T a, T b) {
  using U = std::underlying_type_t<T>;
  return static_cast<T>(static_cast<U>(a) & static_cast<U>(b));
}

template <BitmaskEnum T>
constexpr T operator^(T a, T b) {
  using U = std::underlying_type_t<T>;
  return static_cast<T>(static_cast<U>(a) ^ static_cast<U>(b));
}

template <BitmaskEnum T>
constexpr T operator~(T a) {
  using U = std::underlying_type_t<T>;
  return static_cast<T>(~static_cast<U>(a));
}

template <BitmaskEnum T>
constexpr T& operator|=(T& a, T b) { return a = a | b; }

template <BitmaskEnum T>
constexpr T& operator&=(T& a, T b) { return a = a & b; }

template <BitmaskEnum T>
constexpr T& operator^=(T& a, T b) { return a = a ^ b; }

///
/// Whether or not every bit in `bits` is set in `flags`.
///
/// ```
/// if (HasFlag(window->window_buttons(), WindowButtons::Maximize)) { ... }
/// ```
///
template <BitmaskEnum T>
constexpr bool HasFlag(T flags, T bits) {
  using U = std::underlying_type_t<T>;
  return (static_cast<U>(flags) & static_cast<U>(bits)) == static_cast<U>(bits)
         && static_cast<U>(bits) != 0;
}

}  // namespace ultralight

#endif
