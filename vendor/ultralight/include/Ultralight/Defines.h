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
#define UExport
#else

#include <Ultralight/Config.h>
#include <Ultralight/Exports.h>

// Require C++20 Support
#if defined(_MSC_VER)
#  if _MSC_VER < 1930
#    error This project needs at least Visual Studio 2022 to build
#  elif defined(_MSVC_LANG) && _MSVC_LANG < 202002L
#    error This project must be compiled with /std:c++20 or later
#  endif
#elif __cplusplus < 202002L
#  error This project can only be compiled with a compiler that supports C++20
#endif

#if INTPTR_MAX == INT32_MAX
#  define UL_ARCH_32_BIT
#elif INTPTR_MAX == INT64_MAX
#  define UL_ARCH_64_BIT
#else
#  error "Unknown CPU architecture: environment not 32 or 64-bit."
#endif

#if defined(__aarch64__)
#  define UL_ARCH_ARM64
#  if defined(__APPLE__)
#    define UL_ARCH_ARM64_APPLE_SILICON
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

#ifndef UL_COMPILER_GCC_LIKE
#  if defined(__clang__) || defined(__GNUC__) || defined(__GNUG__)
#    define UL_COMPILER_GCC_LIKE
#  endif
#endif

#ifndef UL_ALWAYS_INLINE
#if defined(UL_COMPILER_GCC_LIKE) && defined(NDEBUG)
#    define UL_ALWAYS_INLINE inline __attribute__((__always_inline__))
#  elif defined(_MSC_VER) && defined(NDEBUG)
#    define UL_ALWAYS_INLINE __forceinline
#  else
#    define UL_ALWAYS_INLINE inline
#  endif
#endif

#ifndef UL_UNLIKELY
#  if defined(UL_COMPILER_GCC_LIKE)
#    define UL_UNLIKELY(x) __builtin_expect(!!(x), 0)
#  else
#    define UL_UNLIKELY(x) (x)
#  endif
#endif

#ifndef UL_LIKELY
#  if defined(UL_COMPILER_GCC_LIKE)
#    define UL_LIKELY(x) __builtin_expect(!!(x), 1)
#  else
#    define UL_LIKELY(x) (x)
#  endif
#endif

#ifndef UL_ALIGN
    #if defined(UL_COMPILER_GCC_LIKE) 
        #define UL_ALIGN(x) __attribute__((aligned(x)))
    #elif defined(__cplusplus) && __cplusplus >= 201103L
        #define UL_ALIGN(x) alignas(x)
    #elif defined(_MSC_VER)
        #define UL_ALIGN(x) __declspec(align(x))
    #else
        #define UL_ALIGN(x)
    #endif
#endif

#endif // #ifdef SWIG

#define ULTRALIGHT_VERSION "2.0.0"
#define ULTRALIGHT_VERSION_MAJOR 2
#define ULTRALIGHT_VERSION_MINOR 0
#define ULTRALIGHT_VERSION_PATCH 0

#define WEBKIT_VERSION "615.1.18.100.1"
#define WEBKIT_VERSION_MAJOR 615
#define WEBKIT_VERSION_MINOR 1
#define WEBKIT_VERSION_TINY 18
#define WEBKIT_VERSION_MICRO 100
#define WEBKIT_VERSION_NANO 1

///
/// The platform token of ULTRALIGHT_USER_AGENT (the parenthesized part), chosen for the
/// build's target OS. Apple builds report Intel on every architecture, as Safari does.
///
#if defined(_WIN32)
#  define ULTRALIGHT_USER_AGENT_PLATFORM "Windows NT 10.0; Win64; x64"
#elif defined(__APPLE__)
#  define ULTRALIGHT_USER_AGENT_PLATFORM "Macintosh; Intel Mac OS X 10_15_7"
#elif defined(__linux__) && defined(__aarch64__)
#  define ULTRALIGHT_USER_AGENT_PLATFORM "X11; Linux aarch64"
#elif defined(__linux__)
#  define ULTRALIGHT_USER_AGENT_PLATFORM "X11; Linux x86_64"
#else
#  define ULTRALIGHT_USER_AGENT_PLATFORM "Windows NT 10.0; Win64; x64"
#endif

#define ULTRALIGHT_USER_AGENT "Mozilla/5.0 (" ULTRALIGHT_USER_AGENT_PLATFORM ") " \
                              "AppleWebKit/" WEBKIT_VERSION " (KHTML, like Gecko) " \
                              "Ultralight/" ULTRALIGHT_VERSION " Version/16.4.1 " \
                              "Safari/" WEBKIT_VERSION

#ifdef __cplusplus
extern "C" {
#endif

///
/// Full library version string (corresponds to ULTRALIGHT_VERSION)
/// 
UExport const char* UltralightVersionString();

UExport uint32_t UltralightVersionMajor();
UExport uint32_t UltralightVersionMinor();
UExport uint32_t UltralightVersionPatch();

///
/// Full WebKit version string (corresponds to WEBKIT_VERSION)
/// 
UExport const char* WebKitVersionString();

UExport uint32_t WebKitVersionMajor();
UExport uint32_t WebKitVersionMinor();
UExport uint32_t WebKitVersionTiny();
UExport uint32_t WebKitVersionMicro();
UExport uint32_t WebKitVersionNano();

#ifdef __cplusplus
}
#endif