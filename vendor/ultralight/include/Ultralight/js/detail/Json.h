///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// Implementation detail of `<Ultralight/js/API.h>`; include that header instead.
/// JSON writing for structured constants: no live context exists at registration time, so
/// constants serialize to JSON and the registry re-applies them per page.
///
/// \cond INTERNAL
#pragma once
#include <Ultralight/js/Value.h>
#include <Ultralight/js/detail/Marshal.h>
#include <Ultralight/js/detail/Reflection.h>

#include <charconv>
#include <cmath>
#include <cstdio>
#if defined(__APPLE__)
#include <xlocale.h>
#endif
#include <string>
#include <string_view>
#include <type_traits>

namespace ultralight {
namespace js {
namespace detail {

inline void WriteJsonEscaped(std::string& out, std::string_view text) {
  out += '"';
  for (char c : text) {
    unsigned char u = static_cast<unsigned char>(c);
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (u < 0x20) {
          char buf[8];
          snprintf(buf, sizeof(buf), "\\u%04x", u);
          out += buf;
        } else {
          out += c;
        }
    }
  }
  out += '"';
}

template <typename T>
void WriteJson(std::string& out, const T& value) {
  using U = std::remove_cvref_t<T>;
  if constexpr (std::is_same_v<U, NullType>) {
    out += "null";
  } else if constexpr (std::is_same_v<U, bool>) {
    out += value ? "true" : "false";
  } else if constexpr (std::is_arithmetic_v<U>) {
    double d = static_cast<double>(value);
    if (std::isnan(d) || std::isinf(d)) {
      out += "null";  // JSON has no NaN/Infinity (JSON.stringify emits null too).
    } else {
#if defined(__APPLE__)
      // The floating-point std::to_chars overloads need OS libc++ support
      // newer than the library's minimum macOS version. snprintf_l with a
      // null locale (the C locale) matches to_chars' locale independence;
      // round-trip precision is enough here, shortest form is not required.
      char buf[32];
      int n = ::snprintf_l(buf, sizeof(buf), nullptr, "%.15g", d);
      if (::strtod_l(buf, nullptr, nullptr) != d)
        n = ::snprintf_l(buf, sizeof(buf), nullptr, "%.17g", d);
      out.append(buf, static_cast<size_t>(n));
#else
      char buf[32];
      auto [end, ec] = std::to_chars(buf, buf + sizeof(buf), d);
      out.append(buf, end);
#endif
    }
  } else if constexpr (std::is_same_v<U, std::string> || std::is_same_v<U, std::string_view>) {
    WriteJsonEscaped(out, value);
  } else if constexpr (std::is_same_v<U, const char*> || std::is_same_v<U, char*>) {
    WriteJsonEscaped(out, value ? std::string_view(value) : std::string_view());
  } else if constexpr (IsOptional<U>::value) {
    if (value)
      WriteJson(out, *value);
    else
      out += "null";
  } else if constexpr (IsVector<U>::value) {
    out += '[';
    bool first = true;
    for (const auto& element : value) {
      if (!first)
        out += ',';
      first = false;
      WriteJson(out, element);
    }
    out += ']';
  } else if constexpr (IsStringMap<U>::value) {
    out += '{';
    bool first = true;
    for (const auto& [key, entry] : value) {
      if (!first)
        out += ',';
      first = false;
      WriteJsonEscaped(out, key);
      out += ':';
      WriteJson(out, entry);
    }
    out += '}';
#if ULTRALIGHT_REFLECTION
  } else if constexpr (std::is_enum_v<U>) {
    for (const auto& entry : EnumInfo<U>::kEntries) {
      if (entry.value == static_cast<long long>(value)) {
        WriteJsonEscaped(out, entry.name);
        return;
      }
    }
    WriteJson(out, static_cast<double>(static_cast<long long>(value)));
  } else if constexpr (kIsReflectableAggregate<U>) {
    constexpr size_t kCount = CountFields<U>();
    out += '{';
    U& mutable_value = const_cast<U&>(value);  // MemberPointer reads only.
    [&]<size_t... Is>(std::index_sequence<Is...>) {
      auto write_field = [&](std::string_view name, const auto& field, bool first) {
        if (!first)
          out += ',';
        WriteJsonEscaped(out, name);
        out += ':';
        WriteJson(out, field);
      };
      (write_field(std::string_view(kFieldNameZ<U, Is>.data, kFieldNameZ<U, Is>.len),
                   *MemberPointer<Is>(mutable_value), Is == 0),
       ...);
    }(std::make_index_sequence<kCount> {});
    out += '}';
#endif
  } else {
    static_assert(!sizeof(U*),
                  "this type cannot be a constant (constants serialize to JSON with no live "
                  "context, and JSON cannot represent it); bind a function or property "
                  "instead. js::undefined has no JSON form either; use js::null");
  }
}

// The exact type set constants support (what WriteJson can serialize), applied recursively
// so rejection happens at the binding assignment rather than deep inside the serializer.
template <typename T>
constexpr bool IsJsonConstant() {
  using U = std::remove_cvref_t<T>;
  if constexpr (std::is_same_v<U, NullType> || std::is_same_v<U, bool>
                || std::is_arithmetic_v<U>) {
    return true;
  } else if constexpr (std::is_same_v<U, std::string> || std::is_same_v<U, std::string_view>
                       || std::is_same_v<U, const char*> || std::is_same_v<U, char*>) {
    return true;
  } else if constexpr (IsOptional<U>::value || IsVector<U>::value) {
    return IsJsonConstant<typename U::value_type>();
  } else if constexpr (IsStringMap<U>::value) {
    return IsJsonConstant<typename U::mapped_type>();
#if ULTRALIGHT_REFLECTION
  } else if constexpr (std::is_enum_v<U>) {
    return true;
  } else if constexpr (kIsReflectableAggregate<U>) {
    return []<size_t... Is>(std::index_sequence<Is...>) {
      return (IsJsonConstant<FieldType<U, Is>>() && ...);
    }(std::make_index_sequence<CountFields<U>()> {});
#endif
  } else {
    return false;
  }
}

template <typename T>
inline constexpr bool kIsJsonConstant = IsJsonConstant<T>();

}  // namespace detail
}  // namespace js
}  // namespace ultralight
/// \endcond
