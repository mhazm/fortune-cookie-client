///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// Implementation detail of `<Ultralight/js/API.h>`; include that header instead.
/// Everything here lives in ultralight::js::detail and may change without notice.
///
/// Schema-expression derivation: binding registration emits ULJS schema fragments (the
/// versioned JSON that ulJSAPIGetSchema() merges) derived from the bound C++ signature, so
/// the schema has typed signatures with no per-binding annotation. The
/// TypeTraits<T>::SchemaType() strings are TypeError-message text, never valid schema
/// expressions; this writer derives real expressions from the type taxonomy and quotes
/// SchemaType() only as the opaque fallback for custom specializations. A specialization
/// whose SchemaType() text is not the schema name it wants opts in by declaring
/// `static void WriteSchemaExpr(std::string&)`, which takes precedence over every built-in
/// branch (the buffer types in `<Ultralight/js/Buffer.h>` do this).
///
/// \cond INTERNAL
#pragma once
#include <Ultralight/CAPI/CAPI_JSAPI.h>
#include <Ultralight/js/TypeTraits.h>
#include <Ultralight/js/detail/Json.h>

#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

namespace ultralight {
namespace js {

template <typename T>
class Task;

namespace detail {

// The schema writer is the only variant-generic dispatch (marshaling iterates alternatives
// directly inside the trait).
template <typename T>
struct IsVariant : std::false_type {};
template <typename... Ts>
struct IsVariant<std::variant<Ts...>> : std::true_type {};

// The opt-in expression hook described in the header comment.
template <typename U>
concept HasSchemaExprHook = requires(std::string& out) { TypeTraits<U>::WriteSchemaExpr(out); };

// Named shapes (aggregates, enums) referenced as "#Name" by the expressions written so far,
// collected per registration call and emitted as one root {"types": ...} fragment
// (fragments accumulate; the schema merges them). std::map keeps emission deterministic. A
// name is recorded before its body is built so self-referential types terminate.
struct SchemaShapes {
  std::map<std::string, std::string> shapes;
};

template <typename T>
void WriteSchemaExpr(std::string& out, SchemaShapes& shapes);

#if ULTRALIGHT_REFLECTION

template <typename T>
void WriteShapeRef(std::string& out) {
  std::string ref("#");
  ref += UnqualifiedTypeName<T>();
  WriteJsonEscaped(out, ref);
}

template <typename T>
void RecordAggregateShape(SchemaShapes& shapes) {
  std::string name(UnqualifiedTypeName<T>());
  auto [it, inserted] = shapes.shapes.emplace(std::move(name), std::string());
  if (!inserted)
    return;
  std::string body = "{\"kind\":\"object\",\"fields\":{";
  [&]<size_t... Is>(std::index_sequence<Is...>) {
    auto write_field = [&](std::string_view field_name, auto field_tag, bool first) {
      if (!first)
        body += ',';
      WriteJsonEscaped(body, field_name);
      body += ':';
      WriteSchemaExpr<typename decltype(field_tag)::type>(body, shapes);
    };
    (write_field(std::string_view(kFieldNameZ<T, Is>.data, kFieldNameZ<T, Is>.len),
                 std::type_identity<FieldType<T, Is>> {}, Is == 0),
     ...);
  }(std::make_index_sequence<CountFields<T>()> {});
  body += "}}";
  it->second = std::move(body);
}

template <typename E>
void RecordEnumShape(SchemaShapes& shapes) {
  std::string name(UnqualifiedTypeName<E>());
  auto [it, inserted] = shapes.shapes.emplace(std::move(name), std::string());
  if (!inserted)
    return;
  std::string body = "{\"kind\":\"enum\",\"values\":[";
  bool first = true;
  for (const auto& entry : EnumInfo<E>::kEntries) {
    if (!first)
      body += ',';
    first = false;
    WriteJsonEscaped(body, entry.name);
  }
  body += "]}";
  it->second = std::move(body);
}

#endif  // ULTRALIGHT_REFLECTION

template <typename V>
struct VariantSchemaExpr;
template <typename... Ts>
struct VariantSchemaExpr<std::variant<Ts...>> {
  static void Write(std::string& out, SchemaShapes& shapes) {
    out += "{\"union\":[";
    bool first = true;
    auto write_alternative = [&](auto tag) {
      if (!first)
        out += ',';
      first = false;
      WriteSchemaExpr<typename decltype(tag)::type>(out, shapes);
    };
    (write_alternative(std::type_identity<Ts> {}), ...);
    out += "]}";
  }
};

template <typename F>
struct FnSchemaExpr;
template <typename R, typename... A>
struct FnSchemaExpr<std::function<R(A...)>> {
  static void Write(std::string& out, SchemaShapes& shapes) {
    out += "{\"fn\":{\"params\":[";
    bool first = true;
    auto write_param = [&](auto tag) {
      if (!first)
        out += ',';
      first = false;
      out += "{\"type\":";
      WriteSchemaExpr<typename decltype(tag)::type>(out, shapes);
      out += '}';
    };
    (write_param(std::type_identity<A> {}), ...);
    out += "],\"returns\":";
    if constexpr (IsResultType<R>::value)
      WriteSchemaExpr<typename IsResultType<R>::value_type>(out, shapes);
    else
      WriteSchemaExpr<R>(out, shapes);
    out += "}}";
  }
};

template <typename T>
void WriteSchemaExpr(std::string& out, SchemaShapes& shapes) {
  using U = std::remove_cvref_t<T>;
  if constexpr (std::is_void_v<U>) {
    out += "\"void\"";
  } else if constexpr (HasSchemaExprHook<U>) {
    TypeTraits<U>::WriteSchemaExpr(out);
  } else if constexpr (std::is_same_v<U, bool>) {
    out += "\"boolean\"";
  } else if constexpr (std::is_integral_v<U>) {
    // A refinement the SchemaType() error text deliberately does not make: messages say
    // "number" (golden-pinned), the schema says "integer" (a codegen hint; still `number`
    // in TypeScript).
    out += "\"integer\"";
  } else if constexpr (std::is_floating_point_v<U>) {
    out += "\"number\"";
  } else if constexpr (std::is_same_v<U, std::string> || std::is_same_v<U, std::string_view>
                       || std::is_same_v<U, const char*> || std::is_same_v<U, char*>) {
    out += "\"string\"";
  } else if constexpr (std::is_same_v<U, Value>) {
    out += "\"any\"";
  } else if constexpr (IsOptional<U>::value) {
    out += "{\"optional\":";
    WriteSchemaExpr<typename U::value_type>(out, shapes);
    out += '}';
  } else if constexpr (IsVector<U>::value) {
    out += "{\"array\":";
    WriteSchemaExpr<typename U::value_type>(out, shapes);
    out += '}';
  } else if constexpr (IsStringMap<U>::value) {
    out += "{\"map\":";
    WriteSchemaExpr<typename U::mapped_type>(out, shapes);
    out += '}';
  } else if constexpr (IsVariant<U>::value) {
    VariantSchemaExpr<U>::Write(out, shapes);
  } else if constexpr (IsStdFunction<U>::value) {
    FnSchemaExpr<U>::Write(out, shapes);
#if ULTRALIGHT_REFLECTION
  } else if constexpr (std::is_enum_v<U>) {
    RecordEnumShape<U>(shapes);
    WriteShapeRef<U>(out);
  } else if constexpr (kIsReflectableAggregate<U>) {
    RecordAggregateShape<U>(shapes);
    WriteShapeRef<U>(out);
#endif
  } else {
    // Custom specializations (and the reflection-off arm): the SchemaType() text becomes an
    // opaque named type; generators pass unknown strings through unchanged.
    WriteJsonEscaped(out, TypeTraits<U>::SchemaType());
  }
}

// Unwraps a bound callable's return type to its schema-visible value type: Result<T> and
// Task<T> to T, Result<Task<T>> to T (the entry's async flag already implies the Promise).
template <typename R>
struct SchemaReturn {
  using Type = R;
};
template <typename T>
struct SchemaReturn<Task<T>> {
  using Type = T;
};
template <typename T>
struct SchemaReturn<Expected<T, Error>> {
  using Type = typename SchemaReturn<T>::Type;
};

template <typename R>
using SchemaReturnT = typename SchemaReturn<std::remove_cvref_t<R>>::Type;

// Names and documentation collected from a registration site's js::Arg / js::Doc
// annotation pack. Consumed while the fragment is built, so the pointed-at text need not
// outlive the registration call.
struct AnnotationSet {
  static constexpr size_t kMaxNames = 16;  // Matches the event/argument ceiling.
  const char* names[kMaxNames] = {};
  size_t name_count = 0;
  const char* doc = nullptr;
};

// Writes the "params" array shared by callable and event fragments: one entry per
// converted parameter, named when the annotation set declares names.
template <typename Converted>
void WriteParamsArray(std::string& out, SchemaShapes& shapes, const AnnotationSet& anns) {
  out += "\"params\":[";
  [&]<size_t... Is>(std::index_sequence<Is...>) {
    auto write_param = [&](auto tag, size_t index, bool first) {
      if (!first)
        out += ',';
      out += '{';
      if (index < anns.name_count) {
        out += "\"name\":";
        WriteJsonEscaped(out, anns.names[index]);
        out += ',';
      }
      out += "\"type\":";
      WriteSchemaExpr<typename decltype(tag)::type>(out, shapes);
      out += '}';
    };
    (write_param(std::type_identity<std::tuple_element_t<Is, Converted>> {}, Is, Is == 0),
     ...);
  }(std::make_index_sequence<std::tuple_size_v<Converted>> {});
  out += ']';
}

// Builds the signature fragment for a bound callable: {"params":[{"type":E},...]} plus
// "returns" unless omitted (the trailing-Resolver form, whose result type is unknowable),
// "async" when requested (class methods and statics, whose generated entries carry no
// async flag; top-level async bindings get theirs from the waist), and the annotation
// set's names and doc when declared (C++ cannot reflect parameter names; generators
// synthesize arg0..argN for unnamed entries).
template <typename Converted, typename R, bool OmitReturns, bool Async = false>
std::string BuildCallableFragment(SchemaShapes& shapes, const AnnotationSet& anns = {}) {
  std::string out = Async ? "{\"async\":true," : "{";
  WriteParamsArray<Converted>(out, shapes, anns);
  if constexpr (!OmitReturns) {
    out += ",\"returns\":";
    WriteSchemaExpr<SchemaReturnT<R>>(out, shapes);
  }
  if (anns.doc) {
    out += ",\"doc\":";
    WriteJsonEscaped(out, anns.doc);
  }
  out += '}';
  return out;
}

// Builds the root events fragment for one declared event:
// {"events":{"<path>":{"params":[...](,"doc":...)}}}. The key is the ROOT-RELATIVE dotted
// path: what the subscription and emission checks compare, and what
// gen-typescript.py splits.
template <typename... Args>
std::string BuildEventFragment(SchemaShapes& shapes, const AnnotationSet& anns,
                               const char* event_path) {
  std::string out = "{\"events\":{";
  WriteJsonEscaped(out, event_path);
  out += ":{";
  WriteParamsArray<std::tuple<std::remove_cvref_t<Args>...>>(out, shapes, anns);
  if (anns.doc) {
    out += ",\"doc\":";
    WriteJsonEscaped(out, anns.doc);
  }
  out += "}}}";
  return out;
}

// Renders the human signature that mis-call TypeErrors carry, eg "(volume: number,
// muted: boolean)": annotation names when declared, types alone otherwise. SchemaType()
// is the message-noun vocabulary, matching the rest of the error text.
template <typename Converted>
std::string RenderSignature(const AnnotationSet& anns) {
  std::string out = "(";
  [&]<size_t... Is>(std::index_sequence<Is...>) {
    auto write_param = [&](auto tag, size_t index, bool first) {
      if (!first)
        out += ", ";
      if (index < anns.name_count) {
        out += anns.names[index];
        out += ": ";
      }
      out += TypeTraits<std::remove_cvref_t<typename decltype(tag)::type>>::SchemaType();
    };
    (write_param(std::type_identity<std::tuple_element_t<Is, Converted>> {}, Is, Is == 0),
     ...);
  }(std::make_index_sequence<std::tuple_size_v<Converted>> {});
  out += ')';
  return out;
}

// Builds the {"type": E} fragment for properties, fields, and constant overlays.
template <typename T>
std::string BuildTypeFragment(SchemaShapes& shapes) {
  std::string out = "{\"type\":";
  WriteSchemaExpr<SchemaReturnT<T>>(out, shapes);
  out += '}';
  return out;
}

// Emits collected shapes as one root fragment; root fragments merge over the whole
// document, so these enrich or add `types` entries beside the generated skeletons.
inline void EmitSchemaShapes(ULJSAPI api, const SchemaShapes& shapes) {
  if (shapes.shapes.empty())
    return;
  std::string fragment = "{\"types\":{";
  bool first = true;
  for (const auto& [name, body] : shapes.shapes) {
    if (!first)
      fragment += ',';
    first = false;
    WriteJsonEscaped(fragment, name);
    fragment += ':';
    fragment += body;
  }
  fragment += "}}";
  ulJSAPISetMetadata(api, "", fragment.c_str());
}

}  // namespace detail
}  // namespace js
}  // namespace ultralight
/// \endcond
