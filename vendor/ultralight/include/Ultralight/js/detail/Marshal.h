///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// Implementation detail of `<Ultralight/js/TypeTraits.h>`; include that header instead.
/// Everything here lives in ultralight::js::detail and may change without notice.
///
/// \cond INTERNAL
#pragma once
#include <Ultralight/CAPI/CAPI_JSValue.h>
#include <Ultralight/detail/Marshal.h>
#include <Ultralight/js/Error.h>
#include <Ultralight/js/Value.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>

namespace ultralight {
namespace js {
namespace detail {

// ---------------------------------------------------------------------------------------
// Marshal-failure recording.
//
// While a bound function unmarshals its arguments, the innermost conversion failure must
// surface as ONE path-qualified TypeError ("myApp.setConfig: argument 1 (volume): expected
// number, got 'loud'"). The FromJS contract has no path parameter, so composite traits
// (aggregates, containers) record their failing child here as the failure unwinds, each
// level prepending its path segment. The binding trampolines and ConvertBorrowed (Value::To
// and friends) install a frame around conversion and read it afterwards; a FromJS call with
// no frame records nothing. The ambient pointer is thread-local because a conversion can
// re-enter script (getters), which can call another binding.
// ---------------------------------------------------------------------------------------

struct MarshalFailure {
  const char* expected = nullptr;
  ULJSValue got = nullptr;
  bool got_owned = false;
  bool active = false;
  char path[192];
  size_t path_begin = sizeof(path) - 1;

  MarshalFailure() { path[sizeof(path) - 1] = '\0'; }

  // Records the innermost failure once; deeper levels win. Returns true when this call
  // recorded (and took ownership of) `child`.
  bool FailChildOwned(const char* child_expected, ULJSValue child) {
    if (active)
      return false;
    active = true;
    expected = child_expected;
    got = child;
    got_owned = true;
    return true;
  }

  void PrependName(const char* name, size_t len) {
    if (!active)
      return;
    bool has_path = path_begin < sizeof(path) - 1;
    bool needs_dot = has_path && path[path_begin] != '[';
    size_t needed = len + (needs_dot ? 1 : 0);
    if (needed > path_begin)
      return;  // Buffer full: keep the innermost (most specific) segments.
    if (needs_dot)
      path[--path_begin] = '.';
    path_begin -= len;
    memcpy(path + path_begin, name, len);
  }

  void PrependIndex(size_t index) {
    if (!active)
      return;
    char buf[24];
    int n = snprintf(buf, sizeof(buf), "[%zu]", index);
    if (n <= 0)
      return;
    bool has_path = path_begin < sizeof(path) - 1;
    bool needs_dot = has_path && path[path_begin] != '[';
    size_t needed = static_cast<size_t>(n) + (needs_dot ? 1 : 0);
    if (needed > path_begin)
      return;  // Buffer full: keep the innermost (most specific) segments.
    if (needs_dot)
      path[--path_begin] = '.';
    path_begin -= static_cast<size_t>(n);
    memcpy(path + path_begin, buf, static_cast<size_t>(n));
  }

  const char* path_or_null() const {
    return path_begin < sizeof(path) - 1 ? path + path_begin : nullptr;
  }
};

inline thread_local MarshalFailure* g_marshal_failure = nullptr;

// Installed by the binding trampolines around argument conversion. Restores the previous
// frame (bindings can re-enter through script) and destroys an owned recorded handle.
struct MarshalFailureScope {
  MarshalFailure frame;
  MarshalFailure* prev;
  MarshalFailureScope() : prev(g_marshal_failure) { g_marshal_failure = &frame; }
  ~MarshalFailureScope() {
    g_marshal_failure = prev;
    if (frame.got_owned)
      ulDestroyJSValue(frame.got);
  }
  MarshalFailureScope(const MarshalFailureScope&) = delete;
  MarshalFailureScope& operator=(const MarshalFailureScope&) = delete;
};

// Reports a failed child conversion to the ambient frame (if any) and says whether the
// child handle's ownership was consumed by the frame.
inline bool ReportChildFailure(const char* child_expected, ULJSValue child) {
  if (MarshalFailure* f = g_marshal_failure)
    return f->FailChildOwned(child_expected, child);
  return false;
}

inline void ReportPathName(const char* name, size_t len) {
  if (MarshalFailure* f = g_marshal_failure)
    f->PrependName(name, len);
}

inline void ReportPathIndex(size_t index) {
  if (MarshalFailure* f = g_marshal_failure)
    f->PrependIndex(index);
}

// ---------------------------------------------------------------------------------------
// Compile-time string building (SchemaType strings for composite types).
// ---------------------------------------------------------------------------------------

template <size_t N>
struct FixedString {
  char data[N + 1] = {};
  size_t len = 0;
  constexpr void Append(std::string_view s) {
    for (char c : s)
      data[len++] = c;
  }
};

// ---------------------------------------------------------------------------------------
// Numeric conversion: the truncate-and-wrap narrowing (the ToInt32 family) and the lossy
// predicates live in the family-neutral <Ultralight/detail/Marshal.h>; these re-exports
// keep the established js::detail spellings working.
// ---------------------------------------------------------------------------------------

using ultralight::detail::DoubleToArithmetic;
using ultralight::detail::FormatDoubleCompact;
using ultralight::detail::IsLossyFromDouble;
using ultralight::detail::PowerOfTwo;

// ---------------------------------------------------------------------------------------
// Lossy-conversion recording (diagnostics). A frame is installed per marshaled argument
// (or return value) by the binding layer ONLY when the call's diagnostics level enables
// warnings, so the Off cost is one thread-local null check per numeric conversion. The
// numeric TypeTraits record the first lossy event into the frame; the binding layer turns
// it into a warning (Warn) or a TypeError (Strict, inbound arguments only). Scope-stacked
// like MarshalFailure because a conversion can re-enter script and hit another binding.
// ---------------------------------------------------------------------------------------

struct LossyConversion {
  bool active = false;
  char detail[96];  // "3.7 became 3"
  char from[32];    // "3.7" (conversions from a JavaScript number only; empty otherwise)
};

inline thread_local LossyConversion* g_lossy_conversion = nullptr;

struct LossyConversionScope {
  LossyConversion frame;
  LossyConversion* prev;
  bool installed;
  explicit LossyConversionScope(bool enable) : prev(g_lossy_conversion), installed(enable) {
    if (enable)
      g_lossy_conversion = &frame;
  }
  ~LossyConversionScope() {
    if (installed)
      g_lossy_conversion = prev;
  }
  LossyConversionScope(const LossyConversionScope&) = delete;
  LossyConversionScope& operator=(const LossyConversionScope&) = delete;
};

// User-callable bodies run with lossy recording muted: a conversion the callable performs
// itself (js::Value::To on an unrelated value, another binding's internals reached through
// script) must not attribute its loss to the argument or return value a caller further up
// the stack is currently marshaling.
struct LossyMuteScope {
  LossyConversion* prev;
  LossyMuteScope() : prev(g_lossy_conversion) { g_lossy_conversion = nullptr; }
  ~LossyMuteScope() { g_lossy_conversion = prev; }
  LossyMuteScope(const LossyMuteScope&) = delete;
  LossyMuteScope& operator=(const LossyMuteScope&) = delete;
};

template <typename T>
void ReportLossyFromDouble(double d, T converted) {
  LossyConversion* frame = g_lossy_conversion;
  if (!frame || frame->active || !IsLossyFromDouble<T>(d))
    return;
  frame->active = true;
  FormatDoubleCompact(frame->from, sizeof(frame->from), d);
  if constexpr (std::is_signed_v<T>) {
    snprintf(frame->detail, sizeof(frame->detail), "%s became %lld", frame->from,
             static_cast<long long>(converted));
  } else {
    snprintf(frame->detail, sizeof(frame->detail), "%s became %llu", frame->from,
             static_cast<unsigned long long>(converted));
  }
}

// 64-bit integral sources: a JavaScript number is a double with 53 bits of integer
// precision, so report values that cannot round-trip. (Narrower integrals are always
// exact.) The range guard keeps the verification cast defined: a source at the very top
// of the 64-bit range rounds up to a double one past it.
template <typename T>
void ReportLossyToDouble(T value) {
  LossyConversion* frame = g_lossy_conversion;
  if (!frame || frame->active)
    return;
  double d = static_cast<double>(value);
  bool lossy;
  if constexpr (std::is_signed_v<T>)
    lossy = d >= PowerOfTwo(63) || static_cast<T>(d) != value;
  else
    lossy = d >= PowerOfTwo(64) || static_cast<T>(d) != value;
  if (!lossy)
    return;
  frame->active = true;
  frame->from[0] = '\0';
  if constexpr (std::is_signed_v<T>) {
    snprintf(frame->detail, sizeof(frame->detail), "%lld became %.0f",
             static_cast<long long>(value), d);
  } else {
    snprintf(frame->detail, sizeof(frame->detail), "%llu became %.0f",
             static_cast<unsigned long long>(value), d);
  }
}

// ---------------------------------------------------------------------------------------
// Type taxonomy used by std::variant dispatch (and the optional-argument handling in the
// binding layer): which JavaScript types a C++ target type accepts. Deliberately based on
// the value's TYPE only, never a conversion attempt, so probing alternatives cannot run
// script (a ToNumber probe could invoke valueOf).
// ---------------------------------------------------------------------------------------

template <typename T>
struct IsOptional : std::false_type {};
template <typename T>
struct IsOptional<std::optional<T>> : std::true_type {};

template <typename T>
struct IsVector : std::false_type {};
template <typename T>
struct IsVector<std::vector<T>> : std::true_type {};

template <typename T>
struct IsStringMap : std::false_type {};
template <typename T>
struct IsStringMap<std::map<std::string, T>> : std::true_type {};

template <typename T>
struct IsStdFunction : std::false_type {};
template <typename R, typename... A>
struct IsStdFunction<std::function<R(A...)>> : std::true_type {};

template <typename T>
struct IsStdArray : std::false_type {};
template <typename T, size_t N>
struct IsStdArray<std::array<T, N>> : std::true_type {};

template <typename T>
struct IsResultType : std::false_type {};
template <typename T>
struct IsResultType<Expected<T, Error>> : std::true_type {
  using value_type = T;
};

// Whether TypeTraits<U> can convert FROM JavaScript (ToJS-only traits such as
// std::string_view and const char* cannot own the converted bytes).
template <typename U>
concept ConvertibleFromJS
    = requires(ULJSContext c, ULJSValue v, U* out, ULJSValue* e) {
        TypeTraits<U>::FromJS(c, v, out, e);
      };

// A type whose FromJS yields a non-owning view of call-scoped memory (std::span over
// typed-array bytes). Legal ONLY as a direct bound-callable parameter, where the argument's
// GC protection spans the call; every position that stores the converted value past the
// call (aggregate fields, container elements, Value::To, deferred boundaries) must reject
// it or the view dangles silently.
template <typename T>
struct IsCallScopedView : std::false_type {};
template <typename T>
struct IsCallScopedView<std::span<T>> : std::true_type {};
// An optional view is still a view (it stays legal as a direct parameter, where its
// storage is call-scoped, and is rejected wherever a bare span is).
template <typename T>
struct IsCallScopedView<std::optional<T>> : IsCallScopedView<T> {};

// The stored type when an argument is captured for deferred marshaling (js::API::Emit,
// js::Resolver::Resolve): a char pointer's or string_view's bytes must be copied at the call
// site (the text may not outlive the call), so it is captured as std::string.
template <typename T>
using CapturedArg = std::conditional_t<std::is_same_v<std::decay_t<T>, const char*>
                                           || std::is_same_v<std::decay_t<T>, char*>
                                           || std::is_same_v<std::decay_t<T>, std::string_view>,
                                       std::string, std::decay_t<T>>;

// Forwards an argument into its CapturedArg slot. A null char pointer captures as the empty
// string, matching TypeTraits<const char*> (std::string cannot be built from a null pointer).
template <typename T>
decltype(auto) CaptureArg(T&& arg) {
  if constexpr (std::is_same_v<std::decay_t<T>, const char*>
                || std::is_same_v<std::decay_t<T>, char*>) {
    const char* text = arg;
    return text ? std::string(text) : std::string();
  } else {
    return std::forward<T>(arg);
  }
}

// A raw pointer to a class type: a candidate bound-class instance borrow. Such pointers
// must not cross deferred boundaries (Emit, Resolve), where delivery happens later on
// another timeline; only owning holders may.
template <typename T>
inline constexpr bool kIsClassInstancePointer
    = std::is_pointer_v<std::decay_t<T>>
    && std::is_class_v<std::remove_cv_t<std::remove_pointer_t<std::decay_t<T>>>>;

// The opt-in JavaScript-type hook, the dispatch analog of the schema-expression hook in
// detail/Schema.h: a specialization whose accepted JavaScript type is not "plain object"
// declares `static bool AcceptsJSType(js::Type, bool is_array, bool is_function)`, and
// std::variant dispatch consults it in place of the built-in taxonomy below.
template <typename U>
concept HasAcceptsJSTypeHook = requires(Type type, bool is_array, bool is_function) {
  TypeTraits<U>::AcceptsJSType(type, is_array, is_function);
};

// A hook written against the C enum doesn't match the one above; without the check in
// AcceptsJSValue it would be skipped silently and the type would fall back to "plain object".
template <typename U>
concept HasULJSTypeAcceptsHook = requires(ULJSType type, bool is_array, bool is_function) {
  TypeTraits<U>::AcceptsJSType(type, is_array, is_function);
};

template <typename T>
bool AcceptsJSValue(Type type, bool is_array, bool is_function) {
  using U = std::remove_cvref_t<T>;
  static_assert(HasAcceptsJSTypeHook<U> || !HasULJSTypeAcceptsHook<U>,
                "js::TypeTraits<T>::AcceptsJSType() takes a js::Type (compare against "
                "js::Type::String and so on), not a ULJSType");
  if constexpr (HasAcceptsJSTypeHook<U>) {
    return TypeTraits<U>::AcceptsJSType(type, is_array, is_function);
  } else if constexpr (std::is_same_v<U, Value>) {
    return true;
  } else if constexpr (std::is_same_v<U, NullType>) {
    return type == Type::Null;
  } else if constexpr (std::is_same_v<U, UndefinedType>) {
    return type == Type::Undefined;
  } else if constexpr (std::is_same_v<U, bool>) {
    return type == Type::Boolean;
  } else if constexpr (std::is_arithmetic_v<U>) {
    return type == Type::Number;
  } else if constexpr (std::is_same_v<U, std::string> || std::is_same_v<U, std::string_view>
                       || std::is_enum_v<U>) {
    return type == Type::String;
  } else if constexpr (IsOptional<U>::value) {
    return type == Type::Undefined || type == Type::Null
        || AcceptsJSValue<typename U::value_type>(type, is_array, is_function);
  } else if constexpr (IsVector<U>::value) {
    return is_array;
  } else if constexpr (IsStdFunction<U>::value) {
    return is_function;
  } else {
    // Maps, aggregates, and user-specialized types: a plain object.
    return type == Type::Object && !is_array && !is_function;
  }
}

// ---------------------------------------------------------------------------------------
// Composite SchemaType strings.
// ---------------------------------------------------------------------------------------

template <typename T>
inline constexpr auto kArraySchema = [] {
  constexpr std::string_view inner = TypeTraits<T>::SchemaType();
  FixedString<inner.size() + 2> s {};
  s.Append(inner);
  s.Append("[]");
  return s;
}();

template <typename T>
inline constexpr auto kMapSchema = [] {
  constexpr std::string_view inner = TypeTraits<T>::SchemaType();
  FixedString<inner.size() + 17> s {};
  s.Append("Record<string, ");
  s.Append(inner);
  s.Append(">");
  return s;
}();

// Named helpers, not a nested lambda: MSVC cannot evaluate a lambda that captures an
// enclosing constexpr local inside a constant expression (C2326).
template <typename... Ts>
constexpr std::array<std::string_view, sizeof...(Ts)> VariantSchemaParts() {
  return { std::string_view(TypeTraits<Ts>::SchemaType())... };
}

template <typename... Ts>
constexpr size_t VariantSchemaLength() {
  size_t n = 0;
  for (std::string_view p : VariantSchemaParts<Ts...>())
    n += p.size() + 3;
  return n;
}

template <typename... Ts>
inline constexpr auto kVariantSchema = [] {
  constexpr auto parts = VariantSchemaParts<Ts...>();
  FixedString<VariantSchemaLength<Ts...>()> s {};
  bool first = true;
  for (std::string_view p : parts) {
    if (!first)
      s.Append(" | ");
    first = false;
    s.Append(p);
  }
  return s;
}();

// Copyable owning context reference for callable wrappers (std::function requires copyable
// captures).
struct ContextRef {
  ULJSContext ctx = nullptr;
  ContextRef() = default;
  explicit ContextRef(ULJSContext c) : ctx(c ? ulCreateJSContextRef(c) : nullptr) {}
  ContextRef(const ContextRef& other)
      : ctx(other.ctx ? ulCreateJSContextRef(other.ctx) : nullptr) {}
  ContextRef(ContextRef&& other) noexcept : ctx(other.ctx) { other.ctx = nullptr; }
  ContextRef& operator=(ContextRef other) noexcept {
    std::swap(ctx, other.ctx);
    return *this;
  }
  ~ContextRef() { ulDestroyJSContext(ctx); }
};

template <typename T>
inline constexpr bool kAlwaysFalse = false;

// Defined in ValueImpl.h: the strict conversion behind Value::To, Arg::To, and js::Await, and
// the compile error naming the header an interop type's conversions need.
template <typename U>
Result<U> ConvertBorrowed(ULJSValue handle);
template <typename U>
constexpr void AssertInteropHeader();

}  // namespace detail
}  // namespace js
}  // namespace ultralight
/// \endcond
