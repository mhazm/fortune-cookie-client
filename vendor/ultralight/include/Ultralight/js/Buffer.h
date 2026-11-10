///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/Buffer.h>
#include <Ultralight/CAPI/CAPI_JSBuffer.h>
#include <Ultralight/RefPtr.h>
#include <Ultralight/js/TypeTraits.h>
#include <Ultralight/js/Value.h>
#include <Ultralight/js/detail/BufferBridge.h>

#include <cstdint>
#include <span>
#include <string>
#include <type_traits>

namespace ultralight {
namespace js {

///
/// Handle to a JavaScript ArrayBuffer for sharing raw binary memory with the page.
///
/// ArrayBuffer wraps a JavaScript ArrayBuffer, letting native code and page script share raw binary
/// memory without copying. You can pass native memory directly to the page as an ArrayBuffer, or
/// wrap a buffer received from the page.
///
/// Share native memory with the page by wrapping a Buffer in an ArrayBuffer:
///
/// ```
/// RefPtr<Buffer> pixels = Buffer::Create(
///     pixel_data, pixel_size, nullptr,
///     [](void* user_data, void* data) { FreePixels(data); });
/// ctx["pixels"] = ctx.MakeArrayBuffer(pixels);
/// ```
///
/// Page script accesses the shared memory through a typed array:
///
/// ```js
/// const bytes = new Uint8Array(pixels);  // the same memory, no copy
/// ```
///
/// ## Reading Bytes from the Page
///
/// Native code accesses the page's bytes through a Buffer that views the memory without copying:
///
/// - **Accept a RefPtr<Buffer> in a bound function.** Page script passes an ArrayBuffer as the
///   argument.
/// - **Call ShareBytes() on an ArrayBuffer handle.** The method returns a Buffer for an ArrayBuffer
///   you already hold.
///
/// Access the bytes from native code:
///
/// ```
/// app["upload"] = [](RefPtr<Buffer> bytes) { Enqueue(bytes); };
///
/// js::ArrayBuffer save(ctx["saveData"]);  // empty if it isn't an ArrayBuffer
/// RefPtr<Buffer> save_bytes = save.ShareBytes();
/// ```
///
/// ## Detaching an ArrayBuffer
///
/// Call Detach() to take shared bytes back from the page early so JavaScript can no longer reach
/// the memory.
///
/// Once native code reads the bytes (by calling ShareBytes(), receiving a RefPtr<Buffer> or
/// std::span parameter in a bound function, or calling TypedArray::view()), the ArrayBuffer can no
/// longer be detached by native code or by the page, including an ArrayBuffer created from your own
/// Buffer. Calling Detach() then returns `false`.
///
/// ## Byte Lifetimes
///
/// A native Buffer shared with the page follows these lifetime rules:
///
/// - **Shared buffers stay referenced until collection or detachment.** The library releases its
///   reference during a later Renderer::Update() after the ArrayBuffer is garbage collected or
///   detached.
/// - **The destruction callback runs where the last reference is released.** That happens during
///   Renderer::Update() if the ArrayBuffer held the last reference, or wherever native code drops
///   its last one.
///
/// @warning  Bytes owned by the page are freed when the page goes away, even while native code
///           holds a Buffer over them. Use Buffer::CreateFromCopy() if the data must outlive the
///           page.
///
/// @see  js::Context::MakeArrayBuffer(), js::TypedArray, Buffer::Create(), Buffer::CreateFromCopy()
///
class ArrayBuffer : public Value {
 public:
  ///
  /// Create an empty ArrayBuffer wrapper.
  ///
  ArrayBuffer() = default;

  ///
  /// Wrap a value if it's an ArrayBuffer (detached ones included), otherwise create an empty
  /// wrapper.
  ///
  /// @param  value  The value to wrap.
  ///
  explicit ArrayBuffer(const Value& value)
      : Value(value.raw() && ulJSValueIsArrayBuffer(value.raw()) ? value : Value()) {}

  ///
  /// Share this ArrayBuffer's bytes with native code without copying.
  ///
  /// The Buffer is always a new one, even for an ArrayBuffer created from your own Buffer.
  /// Holding it keeps the ArrayBuffer alive while the page lives.
  ///
  /// @return  Returns a Buffer that views the bytes (a null RefPtr if the ArrayBuffer is
  ///          detached or the wrapper is empty).
  ///
  /// \parblock
  /// @note  The ArrayBuffer can no longer be detached afterwards.
  /// \endparblock
  ///
  /// \parblock
  /// @note  You can release the returned Buffer from any thread.
  /// \endparblock
  ///
  /// @warning  The bytes can be freed when the page goes away, even while you hold the Buffer (see
  ///           "Byte Lifetimes" in the class overview).
  ///
  RefPtr<ultralight::Buffer> ShareBytes() const {
    return detail::AdoptULBuffer(ulJSArrayBufferGetBuffer(raw()));
  }

  ///
  /// Get the length in bytes. The ArrayBuffer can still be detached.
  ///
  /// @return  Returns the byte length (0 if the ArrayBuffer is detached or the wrapper is
  ///          empty).
  ///
  size_t byte_length() const { return ulJSArrayBufferGetByteLength(raw()); }

  ///
  /// Detach this ArrayBuffer so JavaScript can no longer reach its bytes.
  ///
  /// Afterwards its `byteLength` is 0, typed arrays over it have length 0 (element reads
  /// return `undefined`), and creating a new typed array over it throws a TypeError. If it was
  /// created from a Buffer, its reference to that Buffer is released during a later
  /// Renderer::Update().
  ///
  /// @return  Returns true if the ArrayBuffer is detached on return (including one that was
  ///          already detached). Returns false if it can no longer be detached or the wrapper
  ///          is empty.
  ///
  [[nodiscard]] bool Detach() const { return ulJSArrayBufferDetach(raw()); }
};

///
/// Handle to a JavaScript typed array on a page.
///
/// TypedArray wraps a JavaScript typed array (such as a Float32Array or Uint8Array) on a page,
/// letting native code read and write its elements directly. It doesn't copy the underlying bytes,
/// so changes appear on the page right away.
///
/// Create a typed array for the page, or read elements from one that script created:
///
/// ```
/// js::TypedArray<float> samples = ctx.MakeTypedArray<float>(4);
/// samples.view()[0] = 0.5f;
/// ctx["samples"] = samples;
///
/// js::TypedArray<float> weights(ctx["weights"]);  // empty unless a Float32Array
/// float total = 0;
/// for (float weight : weights.view())
///   total += weight;
/// ```
///
/// ## Element Types
///
/// Each supported C++ type pairs with a matching JavaScript typed array:
///
/// | C++ Type   | JavaScript Typed Array |
/// |------------|------------------------|
/// | `int8_t`   | Int8Array              |
/// | `uint8_t`  | Uint8Array             |
/// | `int16_t`  | Int16Array             |
/// | `uint16_t` | Uint16Array            |
/// | `int32_t`  | Int32Array             |
/// | `uint32_t` | Uint32Array            |
/// | `int64_t`  | BigInt64Array          |
/// | `uint64_t` | BigUint64Array         |
/// | `float`    | Float32Array           |
/// | `double`   | Float64Array           |
///
/// A Uint8ClampedArray also counts as `uint8_t` when read from the page.
///
/// \parblock
/// @note  A typed array's bytes belong to an underlying ArrayBuffer, and view() and data() follow
///        the byte rules there (see js::ArrayBuffer).
/// \endparblock
///
/// \parblock
/// @note  A bound function takes a typed array as a std::span<T> parameter instead of a TypedArray
///        wrapper.
/// \endparblock
///
/// @see  js::ArrayBuffer, js::Context::MakeTypedArray()
///
template <typename T>
class TypedArray : public Value {
  static_assert(detail::kTypedArrayTypeOf<T> != kULJSTypedArrayType_None,
                "TypedArray<T>: T must be one of the fixed-width typed-array element types "
                "(intN_t / uintN_t, float, double)");

 public:
  ///
  /// Create an empty TypedArray wrapper.
  ///
  TypedArray() = default;

  ///
  /// Wrap a value if it's a typed array with elements of type `T`, otherwise create an empty
  /// wrapper. A Uint8ClampedArray counts as `uint8_t`.
  ///
  /// @param  value  The value to wrap.
  ///
  explicit TypedArray(const Value& value)
      : Value(value.raw()
                      && detail::ElementAccepts(ulJSValueGetTypedArrayType(value.raw()),
                                                detail::kTypedArrayTypeOf<T>)
                  ? value
                  : Value()) {}

  ///
  /// Create a typed array over part of an existing ArrayBuffer without copying. `buffer` can
  /// still be detached.
  ///
  /// To create a new typed array, use Context::MakeTypedArray().
  ///
  /// @param  buffer       The ArrayBuffer to view.
  ///
  /// @param  byte_offset  The byte offset of the first element (must be a multiple of
  ///                      `sizeof(T)`).
  ///
  /// @param  length       The number of elements (not bytes).
  ///
  /// @return  Returns a new TypedArray (an empty wrapper if `byte_offset` isn't a multiple of
  ///          `sizeof(T)` or the range doesn't fit in `buffer`).
  ///
  static TypedArray Create(const ArrayBuffer& buffer, size_t byte_offset, size_t length) {
    ULJSContext ctx = ulJSValueGetContext(buffer.raw());
    if (!ctx)
      return TypedArray();
    TypedArray result(Value::Adopt(ulCreateJSTypedArrayFromArrayBuffer(
        ctx, detail::kTypedArrayTypeOf<T>, buffer.raw(), byte_offset, length)));
    ulDestroyJSContext(ctx);
    return result;
  }

  ///
  /// Get a span over the elements without copying.
  ///
  /// The span stays valid while this wrapper lives and the page lives (see "Byte Lifetimes" on
  /// js::ArrayBuffer for what ends it early).
  ///
  /// @return  Returns the elements (an empty span if the array is detached or the wrapper is
  ///          empty).
  ///
  /// @note  The ArrayBuffer can no longer be detached afterwards.
  ///
  std::span<T> view() const {
    ULJSTypedArrayInfo info;
    if (!ulJSTypedArrayGetInfo(raw(), &info) || !info.data
        || !detail::ElementAccepts(info.type, detail::kTypedArrayTypeOf<T>))
      return {};  // The element re-check guards wrappers reassigned through a Value&.
    return std::span<T>(static_cast<T*>(info.data), info.length);
  }

  ///
  /// Get a pointer to the first element. The ArrayBuffer can no longer be detached afterwards,
  /// like view().
  ///
  /// @return  Returns the pointer (NULL if the array is detached or the wrapper is empty).
  ///
  T* data() const { return view().data(); }

  ///
  /// Get the number of elements. The ArrayBuffer can still be detached.
  ///
  /// @return  Returns the element count (0 if the array is detached or the wrapper is empty).
  ///
  size_t size() const {
    // The element check guards wrappers reassigned through a Value&, as in view().
    if (!detail::ElementAccepts(ulJSValueGetTypedArrayType(raw()), detail::kTypedArrayTypeOf<T>))
      return 0;
    return ulJSTypedArrayGetLength(raw());
  }

  ///
  /// Get the ArrayBuffer this typed array views. The ArrayBuffer can still be detached.
  ///
  /// @return  Returns the ArrayBuffer (an empty wrapper if this wrapper is empty).
  ///
  ArrayBuffer buffer() const {
    return ArrayBuffer(Value::Adopt(ulJSTypedArrayGetBuffer(raw())));
  }
};

///
/// Marshals RefPtr<Buffer> as a JavaScript ArrayBuffer without copying in either direction.
///
///  - **Returned to JavaScript**: a RefPtr<Buffer> becomes an ArrayBuffer, like
///    Context::MakeArrayBuffer(). A null RefPtr becomes `null`.
///  - **Received as a parameter**: an ArrayBuffer argument becomes a new Buffer that views its
///    bytes, like ArrayBuffer::ShareBytes(). `null` becomes a null RefPtr. Any other argument (a
///    detached ArrayBuffer or a typed array included) is rejected with a TypeError.
///
/// @note  A received ArrayBuffer can no longer be detached afterwards, and its bytes can be
///        freed when the page goes away even while you hold the Buffer (see js::ArrayBuffer).
///
// Buffer is binary data, not a bound class: this full specialization takes over from the
// canonical-holder marshaling that a RefCounted RefPtr would otherwise select.
template <>
struct TypeTraits<RefPtr<ultralight::Buffer>> {
  static bool FromJS(ULJSContext, ULJSValue value, RefPtr<ultralight::Buffer>* out,
                     ULJSValue*) {
    if (ulJSValueGetType(value) == kULJSType_Null) {
      *out = nullptr;
      return true;
    }
    ULBuffer handle = ulJSArrayBufferGetBuffer(value);
    if (!handle)
      return false;
    *out = detail::AdoptULBuffer(handle);
    return *out != nullptr;
  }

  static ULJSValue ToJS(ULJSContext ctx, const RefPtr<ultralight::Buffer>& value) {
    return detail::WrapBufferThrough(ctx, value, [&](ULBuffer handle) {
      return ulCreateJSArrayBufferFromBuffer(ctx, handle);
    });
  }

  static constexpr const char* SchemaType() { return "ArrayBuffer"; }

  // SchemaType() is TypeError-message casing; the schema expression is the lowercase token.
  static void WriteSchemaExpr(std::string& out) { out += "\"arraybuffer\""; }
};

///
/// Marshals a typed-array argument as a std::span<T> without copying.
///
/// A bound-function parameter of type std::span<T> (or std::span<const T>) accepts a typed
/// array with elements of type `T` (a Uint8ClampedArray counts as `uint8_t`).
/// std::span<uint8_t> and std::span<const uint8_t> also accept an ArrayBuffer. A detached
/// argument gives an empty span, and any other argument is rejected with a TypeError.
///
/// The span is valid only until the bound function returns (for a js::Task coroutine, until
/// the Task finishes), so never keep the span or its pointers. A span works only as a
/// parameter: returning one, or using one where the value outlives the call (eg, an aggregate
/// field, a container element, Value::To(), or js::API::Emit()), fails to compile. Copy the
/// elements into a std::vector, or use RefPtr<Buffer> to share the bytes.
///
/// @note  An accepted argument's ArrayBuffer can no longer be detached afterwards. A rejected
///        argument's ArrayBuffer can still be detached.
///
template <typename T>
struct TypeTraits<
    std::span<T>,
    std::enable_if_t<detail::kTypedArrayTypeOf<std::remove_const_t<T>>
                     != kULJSTypedArrayType_None>> {
 private:
  using Element = std::remove_const_t<T>;

 public:
  static bool FromJS(ULJSContext, ULJSValue value, std::span<T>* out, ULJSValue*) {
    // Classify first, without pinning: reading element pointers pins the underlying
    // ArrayBuffer permanently, and a rejected argument must leave the caller's buffer
    // detachable/transferable.
    ULJSTypedArrayType have = ulJSValueGetTypedArrayType(value);
    if (have != kULJSTypedArrayType_None) {
      if (!detail::ElementAccepts(have, detail::kTypedArrayTypeOf<Element>))
        return false;
      ULJSTypedArrayInfo info;
      if (!ulJSTypedArrayGetInfo(value, &info))
        return false;
      // A detached view converts as an empty span (JavaScript length-0 semantics).
      *out = info.data ? std::span<T>(static_cast<T*>(info.data), info.length)
                       : std::span<T>();
      return true;
    }
    if constexpr (std::is_same_v<Element, uint8_t>) {
      if (ulJSValueIsArrayBuffer(value)) {
        size_t length = 0;
        void* bytes = ulJSArrayBufferGetBytes(value, &length);
        // A detached ArrayBuffer converts as an empty span, matching detached views.
        *out = bytes ? std::span<T>(static_cast<T*>(bytes), length) : std::span<T>();
        return true;
      }
    }
    return false;
  }

  static constexpr const char* SchemaType() { return detail::TypedArrayNameOf<Element>(); }

  // SchemaType() is TypeError-message casing ("Float32Array"); the schema expression is
  // the typed-array composite with the lowercase element token.
  static void WriteSchemaExpr(std::string& out) {
    out += "{\"typedarray\":\"";
    out += detail::TypedArrayElementToken<Element>();
    out += "\"}";
  }
};

}  // namespace js
}  // namespace ultralight
