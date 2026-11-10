///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// Implementation detail shared by the typed C++ header families (`<Ultralight/js/*.h>`,
/// `<Ultralight/dom/*.h>`); include those instead. Everything here lives in
/// ultralight::detail and may change without notice.
///
/// Defines ULTRALIGHT_REFLECTION and the name-reflection engine behind the families'
/// aggregate and enum conversions: enumerator and field names are parsed out of the
/// compiler's function-signature strings at compile time (no macros, no code generation).
/// Each family layers its own admission policy and customization points (for example
/// js::EnumRange and the js aggregate gate) over this engine.
///
#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <limits>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

// Define ULTRALIGHT_REFLECTION=0 before including any Ultralight header to disable the
// engine (and with it every family's reflected-name tier) when a compiler's signature
// format is unsupported.
#ifndef ULTRALIGHT_REFLECTION
#if defined(__clang__) || defined(__GNUC__) || (defined(_MSC_VER) && _MSC_VER >= 1940)
#define ULTRALIGHT_REFLECTION 1
#else
#define ULTRALIGHT_REFLECTION 0
#endif
#endif

/// \cond INTERNAL

#if ULTRALIGHT_REFLECTION

namespace ultralight {
namespace detail {

template <auto V>
constexpr std::string_view RawValueSig() {
#if defined(__clang__) || defined(__GNUC__)
  return __PRETTY_FUNCTION__;
#else
  return __FUNCSIG__;
#endif
}

template <typename T>
constexpr std::string_view RawTypeSig() {
#if defined(__clang__) || defined(__GNUC__)
  return __PRETTY_FUNCTION__;
#else
  return __FUNCSIG__;
#endif
}

constexpr bool IsIdentChar(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
      || c == '_';
}

// The region of the signature that renders the template argument. Clang renders
// "... [V = <value>]"; GCC renders "... [with auto V = <value>; std::string_view =
// std::basic_string_view<char>]" (the return type's alias expansion follows the
// argument, so the region is the first "= " inside the bracket, up to the next
// parameter separator); MSVC renders "...RawValueSig<<value>>(void)...".
constexpr std::string_view ValueRegion(std::string_view sig) {
#if defined(__clang__) || defined(__GNUC__)
  size_t open = sig.find('[');
  if (open == std::string_view::npos)
    return {};
  size_t eq = sig.find("= ", open);
  if (eq == std::string_view::npos)
    return {};
  size_t end = sig.rfind(']');
  if (end == std::string_view::npos || end <= eq + 2)
    return {};
  size_t next = sig.find("; ", eq);
  if (next != std::string_view::npos && next < end)
    end = next;
  return sig.substr(eq + 2, end - (eq + 2));
#else
  size_t open = sig.find('<');
  if (open == std::string_view::npos)
    return {};
  size_t end = sig.rfind(">(void)");
  if (end == std::string_view::npos || end <= open + 1)
    return {};
  return sig.substr(open + 1, end - (open + 1));
#endif
}

// An enumerator renders as a qualified name ending in the enumerator's identifier
// ("Size::Small", possibly behind "(anonymous namespace)::"); any other value of the enum
// type renders as a cast expression ending in a numeric literal ("(Size)70", "0x2"). The
// trailing identifier run distinguishes them: a name never starts with a digit.
constexpr std::string_view ParseEnumeratorName(std::string_view sig) {
  std::string_view region = ValueRegion(sig);
  if (region.empty())
    return {};
  size_t start = region.size();
  while (start > 0 && IsIdentChar(region[start - 1]))
    start--;
  if (start == region.size())
    return {};
  std::string_view name = region.substr(start);
  if (name[0] >= '0' && name[0] <= '9')
    return {};
  return name;
}

// A field access through the fake object renders as "...fake_obj<T>.name" (or "->name" on
// MSVC; GCC qualifies it as ".ns::T::name"); the field name is the last identifier of
// the qualified name after the LAST member-access token.
constexpr std::string_view ParseFieldName(std::string_view sig) {
  size_t anchor = std::string_view::npos;
  for (size_t i = 0; i + 1 < sig.size(); i++) {
    if (sig[i] == '.' && IsIdentChar(sig[i + 1]))
      anchor = i + 1;
    if (sig[i] == '-' && sig[i + 1] == '>' && i + 2 < sig.size() && IsIdentChar(sig[i + 2]))
      anchor = i + 2;
  }
  if (anchor == std::string_view::npos)
    return {};
  size_t end = anchor;
  while (end < sig.size() && IsIdentChar(sig[end]))
    end++;
  while (end + 2 < sig.size() && sig[end] == ':' && sig[end + 1] == ':' && IsIdentChar(sig[end + 2])) {
    anchor = end + 2;
    end = anchor;
    while (end < sig.size() && IsIdentChar(sig[end]))
      end++;
  }
  return sig.substr(anchor, end - anchor);
}

// The unqualified name of T ("Settings" for ultralight::Settings; "Vec3" for a Vec3<float>
// class-template instantiation, whose rendered argument list is skipped nesting-aware).
constexpr std::string_view ParseTypeName(std::string_view sig) {
  std::string_view region = ValueRegion(sig);
  if (region.empty())
    return {};
  size_t end = region.size();
  while (end > 0 && region[end - 1] == ' ')
    end--;
  if (end > 0 && region[end - 1] == '>') {
    int depth = 0;
    do {
      end--;
      if (region[end] == '>')
        depth++;
      else if (region[end] == '<')
        depth--;
    } while (end > 0 && depth > 0);
  }
  size_t start = end;
  while (start > 0 && IsIdentChar(region[start - 1]))
    start--;
  return region.substr(start, end - start);
}

// Null-terminated compile-time name storage (names handed to C APIs at runtime).
template <size_t N>
struct ZString {
  char data[N + 1] = {};
  size_t len = 0;
  constexpr void Append(std::string_view s) {
    for (char c : s)
      data[len++] = c;
  }
};

// --- Enum reflection ---------------------------------------------------------------------

template <typename U>
constexpr bool InUnderlyingRange(long long v) {
  if constexpr (std::is_signed_v<U>) {
    return v >= static_cast<long long>((std::numeric_limits<U>::min)())
        && v <= static_cast<long long>((std::numeric_limits<U>::max)());
  } else {
    return v >= 0
        && static_cast<unsigned long long>(v)
        <= static_cast<unsigned long long>((std::numeric_limits<U>::max)());
  }
}

// The scanned value is produced by bit_cast from the underlying type, never by
// static_cast: for an enum without a fixed underlying type, static_cast of a value beyond
// the enum's representable range is invalid in constant expressions (a hard error on
// current compilers, and not one a requires-clause can absorb). The bit_cast value renders
// as a cast expression for non-enumerators, which the parser already rejects, and values
// outside the underlying type itself are skipped so they cannot alias real enumerators.
template <typename E, long long I>
constexpr std::string_view EnumeratorName() {
  using U = std::underlying_type_t<E>;
  if constexpr (!InUnderlyingRange<U>(I)) {
    return {};
  } else {
    return ParseEnumeratorName(RawValueSig<std::bit_cast<E>(static_cast<U>(I))>());
  }
}

// Compile-time canaries: if a compiler changes its signature format, fail loudly here
// rather than silently reflecting wrong names.
enum class ReflectionProbe { kProbeValue = 1 };
static_assert(EnumeratorName<ReflectionProbe, 1>() == "kProbeValue",
              "enum name reflection failed on this compiler; define "
              "ULTRALIGHT_REFLECTION=0 and specialize the family's TypeTraits explicitly");
static_assert(EnumeratorName<ReflectionProbe, 2>().empty(),
              "enum name reflection failed to reject a non-enumerator value");
enum ReflectionUnscopedProbe { kUnscopedProbeA, kUnscopedProbeB };
static_assert(EnumeratorName<ReflectionUnscopedProbe, 1>() == "kUnscopedProbeB",
              "unscoped-enum name reflection failed on this compiler; define "
              "ULTRALIGHT_REFLECTION=0 and specialize the family's TypeTraits explicitly");
static_assert(EnumeratorName<ReflectionUnscopedProbe, 63>().empty(),
              "enum name reflection failed to skip a value beyond an unscoped enum's "
              "representable range");

// --- Aggregate field reflection ----------------------------------------------------------

struct AnyType {
  template <typename T>
  constexpr operator T() const;  // Never defined; used in unevaluated contexts only.
};

template <typename T, size_t... Is>
constexpr bool BraceConstructible(std::index_sequence<Is...>) {
  return requires { T { ((void)Is, AnyType {})... }; };
}

template <typename T, size_t N = 0>
constexpr size_t CountFields() {
  if constexpr (N > 24) {
    static_assert(N <= 24, "aggregate reflection supports at most 24 fields; specialize "
                           "the family's TypeTraits for larger types");
    return 0;
  } else if constexpr (BraceConstructible<T>(std::make_index_sequence<N + 1> {})) {
    return CountFields<T, N + 1>();
  } else {
    return N;
  }
}

template <size_t I, typename... Ms>
constexpr auto NthAddress(Ms&... members) noexcept {
  return &std::get<I>(std::tie(members...));
}

template <size_t I, typename T>
constexpr auto MemberPointer(T& t) noexcept {
  constexpr size_t kCount = CountFields<std::remove_cvref_t<T>>();
  static_assert(I < kCount);
  if constexpr (kCount == 1) {
    auto& [m0] = t;
    return NthAddress<I>(m0);
  } else if constexpr (kCount == 2) {
    auto& [m0, m1] = t;
    return NthAddress<I>(m0, m1);
  } else if constexpr (kCount == 3) {
    auto& [m0, m1, m2] = t;
    return NthAddress<I>(m0, m1, m2);
  } else if constexpr (kCount == 4) {
    auto& [m0, m1, m2, m3] = t;
    return NthAddress<I>(m0, m1, m2, m3);
  } else if constexpr (kCount == 5) {
    auto& [m0, m1, m2, m3, m4] = t;
    return NthAddress<I>(m0, m1, m2, m3, m4);
  } else if constexpr (kCount == 6) {
    auto& [m0, m1, m2, m3, m4, m5] = t;
    return NthAddress<I>(m0, m1, m2, m3, m4, m5);
  } else if constexpr (kCount == 7) {
    auto& [m0, m1, m2, m3, m4, m5, m6] = t;
    return NthAddress<I>(m0, m1, m2, m3, m4, m5, m6);
  } else if constexpr (kCount == 8) {
    auto& [m0, m1, m2, m3, m4, m5, m6, m7] = t;
    return NthAddress<I>(m0, m1, m2, m3, m4, m5, m6, m7);
  } else if constexpr (kCount == 9) {
    auto& [m0, m1, m2, m3, m4, m5, m6, m7, m8] = t;
    return NthAddress<I>(m0, m1, m2, m3, m4, m5, m6, m7, m8);
  } else if constexpr (kCount == 10) {
    auto& [m0, m1, m2, m3, m4, m5, m6, m7, m8, m9] = t;
    return NthAddress<I>(m0, m1, m2, m3, m4, m5, m6, m7, m8, m9);
  } else if constexpr (kCount == 11) {
    auto& [m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10] = t;
    return NthAddress<I>(m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10);
  } else if constexpr (kCount == 12) {
    auto& [m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11] = t;
    return NthAddress<I>(m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11);
  } else if constexpr (kCount == 13) {
    auto& [m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12] = t;
    return NthAddress<I>(m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12);
  } else if constexpr (kCount == 14) {
    auto& [m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13] = t;
    return NthAddress<I>(m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13);
  } else if constexpr (kCount == 15) {
    auto& [m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14] = t;
    return NthAddress<I>(m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14);
  } else if constexpr (kCount == 16) {
    auto& [m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15] = t;
    return NthAddress<I>(m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                         m15);
  } else if constexpr (kCount == 17) {
    auto& [m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16] = t;
    return NthAddress<I>(m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                         m15, m16);
  } else if constexpr (kCount == 18) {
    auto& [m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17]
        = t;
    return NthAddress<I>(m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                         m15, m16, m17);
  } else if constexpr (kCount == 19) {
    auto& [m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17,
           m18]
        = t;
    return NthAddress<I>(m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                         m15, m16, m17, m18);
  } else if constexpr (kCount == 20) {
    auto& [m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17,
           m18, m19]
        = t;
    return NthAddress<I>(m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                         m15, m16, m17, m18, m19);
  } else if constexpr (kCount == 21) {
    auto& [m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17,
           m18, m19, m20]
        = t;
    return NthAddress<I>(m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                         m15, m16, m17, m18, m19, m20);
  } else if constexpr (kCount == 22) {
    auto& [m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17,
           m18, m19, m20, m21]
        = t;
    return NthAddress<I>(m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                         m15, m16, m17, m18, m19, m20, m21);
  } else if constexpr (kCount == 23) {
    auto& [m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17,
           m18, m19, m20, m21, m22]
        = t;
    return NthAddress<I>(m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                         m15, m16, m17, m18, m19, m20, m21, m22);
  } else {
    auto& [m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17,
           m18, m19, m20, m21, m22, m23]
        = t;
    return NthAddress<I>(m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                         m15, m16, m17, m18, m19, m20, m21, m22, m23);
  }
}

// Declared, never defined: a compile-time address anchor so field access renders in the
// signature string. The fake object is never constructed (non-literal field types are fine).
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wundefined-var-template"
#endif

template <typename T>
extern T fake_obj;

// The field pointer travels boxed in a structural class, never as a bare template
// argument: MSVC renders a bare pointer-to-subobject argument with the FIRST
// specialization that produced the same pointee shape, silently collapsing distinct
// fields (and types) onto one signature string. The boxed form renders each field
// correctly there, is equally parseable on Clang/GCC, and so is the only route.
template <typename P>
struct FieldAnchor {
  P ptr;
};
template <typename P>
FieldAnchor(P) -> FieldAnchor<P>;

template <typename T, size_t I>
constexpr std::string_view FieldNameOf() {
  return ParseFieldName(RawValueSig<FieldAnchor { MemberPointer<I>(fake_obj<T>) }>());
}

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

template <typename T>
constexpr std::string_view UnqualifiedTypeName() {
  return ParseTypeName(RawTypeSig<T>());
}

// Compile-time canaries for the field and type-name parsers, mirroring the enum probes.
struct ReflectionFieldProbe {
  int probe_field;
};
static_assert(FieldNameOf<ReflectionFieldProbe, 0>() == "probe_field",
              "field name reflection failed on this compiler; define "
              "ULTRALIGHT_REFLECTION=0 and specialize the family's TypeTraits explicitly");
static_assert(UnqualifiedTypeName<ReflectionFieldProbe>() == "ReflectionFieldProbe",
              "type name reflection failed on this compiler; define "
              "ULTRALIGHT_REFLECTION=0 and specialize the family's TypeTraits explicitly");
template <typename T>
struct ReflectionTemplateProbe {
  T probe_field;
};
static_assert(UnqualifiedTypeName<ReflectionTemplateProbe<int>>() == "ReflectionTemplateProbe",
              "type name reflection failed on a class-template instantiation; define "
              "ULTRALIGHT_REFLECTION=0 and specialize the family's TypeTraits explicitly");
// Deliberately layout-identical to ReflectionFieldProbe and instantiated after it: a
// compiler that keys subobject-pointer arguments by shape rather than identity reflects
// the FIRST probe's field name here, so this canary turns that silent collapse loud.
struct ReflectionFieldCollapseProbe {
  int collapse_probe_field;
};
static_assert(FieldNameOf<ReflectionFieldCollapseProbe, 0>() == "collapse_probe_field",
              "field name reflection collapsed across types on this compiler; define "
              "ULTRALIGHT_REFLECTION=0 and specialize the family's TypeTraits explicitly");

template <typename T, size_t I>
inline constexpr auto kFieldNameZ = [] {
  constexpr std::string_view name = FieldNameOf<T, I>();
  ZString<name.size()> s {};
  s.Append(name);
  return s;
}();

template <typename T>
inline constexpr auto kTypeNameZ = [] {
  constexpr std::string_view name = UnqualifiedTypeName<T>();
  ZString<name.size()> s {};
  s.Append(name);
  return s;
}();

template <typename T, size_t I>
using FieldType
    = std::remove_pointer_t<decltype(MemberPointer<I>(std::declval<T&>()))>;

template <typename T>
inline constexpr auto kFieldNames = []<size_t... Is>(std::index_sequence<Is...>) {
  return std::array<const char*, sizeof...(Is)> { kFieldNameZ<T, Is>.data... };
}(std::make_index_sequence<CountFields<T>()> {});

}  // namespace detail
}  // namespace ultralight

#endif  // ULTRALIGHT_REFLECTION
/// \endcond
