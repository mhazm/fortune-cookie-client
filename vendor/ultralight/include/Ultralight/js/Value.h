///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
// X11's headers define None as a macro, which would break the None enumerators.
#pragma push_macro("None")
#undef None

#include <Ultralight/CAPI/CAPI_JSAPI.h>
#include <Ultralight/CAPI/CAPI_JSValue.h>
#include <Ultralight/CAPI/CAPI_String.h>
#include <Ultralight/detail/Exceptions.h>
#include <Ultralight/js/Error.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace ultralight {
namespace js {

///
/// Forward declaration.
///
/// @see  <Ultralight/js/TypeTraits.h>
///
template <typename T, typename = void>
struct TypeTraits;

///
/// Tag type for the JavaScript `null` value (see js::null).
///
struct NullType {};

///
/// The JavaScript `null` value.
///
/// Use it anywhere a typed value is accepted (eg, `obj["x"] = js::null` or a bound function's
/// return value).
///
inline constexpr NullType null {};

///
/// Tag type for the JavaScript `undefined` value (see js::undefined).
///
struct UndefinedType {};

///
/// The JavaScript `undefined` value. Use it the same way as js::null.
///
inline constexpr UndefinedType undefined {};

///
/// Attribute flags for a property created by Value::SetProperty(). Combine them with `|`:
///
/// ```
/// obj.SetProperty("id", ctx.Make(7.0),
///                 js::PropertyAttributes::ReadOnly | js::PropertyAttributes::DontDelete);
/// ```
///
/// @note  The flags only apply when SetProperty() creates the property. If the object (or its
///        prototype chain) already has one with that name, SetProperty() assigns the value and
///        ignores the flags.
///
enum class PropertyAttributes : unsigned {
  None = kULJSPropertyAttributes_None,              ///< No flags (a normal property).
  ReadOnly = kULJSPropertyAttributes_ReadOnly,      ///< Script can't change its value.
  DontEnum = kULJSPropertyAttributes_DontEnum,      ///< Hidden from for...in and Object.keys.
  DontDelete = kULJSPropertyAttributes_DontDelete,  ///< Script can't delete it.
};

///
/// Combine property attribute flags.
///
/// @return  Returns the union of both flag sets.
///
constexpr PropertyAttributes operator|(PropertyAttributes a, PropertyAttributes b) {
  return static_cast<PropertyAttributes>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
}

///
/// The type of a JavaScript value.
///
/// Later versions may add types, so handle a value you don't know (eg, with a `default` case).
///
/// @see  Value::type()
///
enum class Type : unsigned {
  Invalid = kULJSType_Invalid,  ///< An empty Value, or one whose page is gone.
  Undefined = kULJSType_Undefined,
  Null = kULJSType_Null,
  Boolean = kULJSType_Boolean,
  Number = kULJSType_Number,
  String = kULJSType_String,
  Symbol = kULJSType_Symbol,
  BigInt = kULJSType_BigInt,
  Object = kULJSType_Object,
};

static_assert(kULJSType_Count == 9, "js::Type must list every ULJSType");

///
/// A handle to a live JavaScript value.
///
/// A Value represents a live JavaScript value on a page. You use it to hold page objects and
/// functions across calls, interacting with them directly without converting them to C++ types.
///
/// This example interacts with an object on the page:
///
/// ```
/// js::Value player = ctx["game"]["player"];
///
/// double health = player["health"].Or(0.0);  // read (0.0 if anything fails)
/// player["score"] = 1200;                    // write
/// player["respawn"](10, 20);                 // call a method (`this` is player)
/// ```
///
/// ## Handle States
///
/// A handle is always in one of these states:
///
/// | State | When                                             | operator bool | Calls         |
/// |-------|--------------------------------------------------|---------------|---------------|
/// | Valid | On a live page                                   | `true`        | Work normally |
/// | Empty | Default-constructed, moved from, or failed read  | `false`       | Do nothing    |
/// | Gone  | Page navigated, frame removed, or View destroyed | `false`       | Do nothing    |
///
/// An empty Value differs from JavaScript `null` and `undefined`. A missing property reads as a
/// valid Value holding `undefined`, while an empty handle represents an uninitialized handle or a
/// failed operation.
///
/// Every operation is safe in all three states, allowing property reads and method calls to chain
/// without checking each step.
///
/// This call chains safely even if the HUD object doesn't exist:
///
/// ```
/// ctx["hud"]["showToast"]("Saved");  // does nothing if there's no hud
/// ```
///
/// When an operation returns a failed js::Result, inspect js::Error::is_empty() to check for an
/// empty handle or js::Error::is_page_gone() to check whether the page navigated away.
///
/// A handle whose page is gone never recovers, even when that page is restored from the
/// back-forward cache.
///
/// To interact with the new page, get a new Value from that page's js::Context. Every page-lifetime
/// handle across the JavaScript API follows these rules.
///
/// ## Copies and Lifetimes
///
/// Copying a Value creates another reference to the same JavaScript value.
///
/// Holding a Value keeps the underlying JavaScript value from being garbage-collected, but it
/// doesn't keep the page or its View alive.
///
/// For ownership cycles between bound instances and JavaScript objects, see js::ClassBuilder.
///
/// A `const Value` still lets you change the underlying JavaScript value, matching the behavior of
/// a `const RefPtr`.
///
/// ## Type Conversions
///
/// The conversion methods To(), Maybe(), and Or() are strict about the JavaScript type (the string
/// `"7"` doesn't convert to a number), differing only in how they report a failure.
///
/// To convert values using JavaScript's own coercion rules, call ToNumber(), ToString(),
/// ToBoolean(), or ToJSON() directly on the Value. For the full list of supported types, see
/// js::TypeTraits.
///
/// This example compares strict conversion against JavaScript coercion:
///
/// ```
/// js::Value level = player["level"];              // the string "7"
///
/// double strict = level.Or(0.0);                  // 0.0 (not a number)
/// double loose = level.ToNumber().value_or(0.0);  // 7.0 (JavaScript's rules)
/// ```
///
/// ## Values Across Pages
///
/// A Value belongs to the page that created it. You can pass a Value directly only between frames
/// of the same top-level page that share an exact origin, never across separate View%s.
///
/// Using a Value on a page that can't take it fails with a TypeError whose error code is
/// `ULJS_CROSS_CONTEXT`. To transfer data between separate pages or View%s, copy the data using
/// JSON or native C++ types.
///
/// This example transfers player data to a page in another View using JSON:
///
/// ```
/// js::Context bestiary(bestiary_view.get());  // a page in another View
///
/// std::string json = player.ToJSON().value_or("null");
/// bestiary["player"] = js::OrEmpty(bestiary.MakeFromJSON(json));
/// ```
///
/// @note  js::Value and dom::data::Value are unrelated types (dom::data::Value is the value type of
///        the data-binding API).
///
/// @see  js::Context, js::Error, js::TypeTraits, js::WeakValue
///
class Value {
 public:
  ///
  /// Create an empty Value.
  ///
  Value() : handle_(nullptr) {}

  ///
  /// Copy constructor (adds a reference to the same JavaScript value).
  ///
  Value(const Value& other)
      : handle_(other.handle_ ? ulCreateJSValueRef(other.handle_) : nullptr) {}

  ///
  /// Move constructor (`other` becomes empty).
  ///
  Value(Value&& other) noexcept : handle_(other.handle_) { other.handle_ = nullptr; }

  ///
  /// Assignment (copies or moves `other` into this Value).
  ///
  Value& operator=(Value other) noexcept {
    std::swap(handle_, other.handle_);
    return *this;
  }

  ///
  /// Destructor (releases this handle).
  ///
  ~Value() { ulDestroyJSValue(handle_); }

  ///
  /// Whether or not this Value is valid (it isn't empty and its page is still alive).
  ///
  explicit operator bool() const { return IsAlive(); }

  ///
  /// Whether or not this Value holds nothing (see "Handle States" above).
  ///
  bool IsEmpty() const { return handle_ == nullptr; }

  ///
  /// Whether or not this Value's page is still alive (the same test as `operator bool`).
  ///
  /// @note  Safe to call from any thread, but off the Renderer's thread the result may already
  ///        be out of date when you act on it.
  ///
  bool IsAlive() const { return handle_ && ulJSValueIsAlive(handle_); }

  ///
  /// Get the type of the value.
  ///
  /// @return  Returns the value's type (Type::Invalid for an empty Value or one whose page is
  ///          gone).
  ///
  Type type() const { return static_cast<Type>(ulJSValueGetType(handle_)); }

  ///
  /// Whether or not the value is `undefined`.
  ///
  bool IsUndefined() const { return type() == Type::Undefined; }

  ///
  /// Whether or not the value is `null`.
  ///
  bool IsNull() const { return type() == Type::Null; }

  ///
  /// Whether or not the value is `null` or `undefined`.
  ///
  bool IsNullish() const { return IsNull() || IsUndefined(); }

  ///
  /// Whether or not the value is a boolean.
  ///
  bool IsBoolean() const { return type() == Type::Boolean; }

  ///
  /// Whether or not the value is a number.
  ///
  bool IsNumber() const { return type() == Type::Number; }

  ///
  /// Whether or not the value is a BigInt (eg, `10n`).
  ///
  bool IsBigInt() const { return type() == Type::BigInt; }

  ///
  /// Whether or not the value is a string.
  ///
  bool IsString() const { return type() == Type::String; }

  ///
  /// Whether or not the value is an object.
  ///
  /// Arrays and functions are objects too (use IsArray() or IsFunction() to tell them apart).
  ///
  bool IsObject() const { return type() == Type::Object; }

  ///
  /// Whether or not the value is an Array.
  ///
  bool IsArray() const { return handle_ && ulJSValueIsArray(handle_); }

  ///
  /// Whether or not the value can be called (see Call() and Invoke()).
  ///
  bool IsCallable() const { return handle_ && ulJSValueIsCallable(handle_); }

  ///
  /// Whether or not the value is a function.
  ///
  /// @note  A few callable objects aren't functions (eg, the constructor of a native class). Use
  ///        IsCallable() to check whether you can call a value.
  ///
  bool IsFunction() const { return handle_ && ulJSValueIsFunction(handle_); }

  ///
  /// Whether or not the value is a Promise.
  ///
  /// This has no side effects: the promise isn't marked as handled.
  ///
  bool IsPromise() const { return handle_ && ulJSValueIsPromise(handle_); }

  ///
  /// Convert to a boolean using JavaScript's rules (this never runs script).
  ///
  /// @return  Returns the boolean.
  ///
  [[nodiscard]] Result<bool> ToBoolean() const {
    if (!handle_)
      return detail::EmptyHandleError();
    bool result;
    if (!ulJSValueToBoolean(handle_, &result))
      return Unexpected<Error>(Error::PageGone());
    return result;
  }

  ///
  /// Convert to a number using JavaScript's rules.
  ///
  /// @return  Returns the number. Fails with a JavaScript exception if the conversion throws
  ///          (eg, a valueOf() method throws or the value is a Symbol).
  ///
  [[nodiscard]] Result<double> ToNumber() const {
    if (!handle_)
      return detail::EmptyHandleError();
    double result;
    ULJSValue exception = nullptr;
    if (!ulJSValueToNumber(handle_, &result, &exception)) {
      if (exception)
        return Unexpected<Error>(Error::AdoptException(exception));
      return Unexpected<Error>(Error::PageGone());
    }
    return result;
  }

  ///
  /// Convert to a UTF-8 string using JavaScript's rules.
  ///
  /// @return  Returns the string. Fails with a JavaScript exception if the conversion throws
  ///          (eg, a toString() method throws or the value is a Symbol).
  ///
  [[nodiscard]] Result<std::string> ToString() const {
    if (!handle_)
      return detail::EmptyHandleError();
    ULJSValue exception = nullptr;
    if (ulJSValueGetType(handle_) == kULJSType_String) {
      // Already a string: reading its bytes runs no script, so the capacity retry is safe.
      char stack_buffer[256];
      size_t length = 0;
      if (ulJSValueGetUTF8(handle_, stack_buffer, sizeof(stack_buffer), &length, &exception))
        return std::string(stack_buffer, length);
      if (!exception && length > sizeof(stack_buffer)) {
        std::string heap(length, '\0');
        if (ulJSValueGetUTF8(handle_, heap.data(), heap.size(), &length, &exception))
          return heap;
      }
      if (exception)
        return Unexpected<Error>(Error::AdoptException(exception));
      return Unexpected<Error>(Error::PageGone());
    }
    ULString s = ulJSValueToString(handle_, &exception);
    if (!s) {
      if (exception)
        return Unexpected<Error>(Error::AdoptException(exception));
      return Unexpected<Error>(Error::PageGone());
    }
    std::string result(ulStringGetData(s), ulStringGetLength(s));
    ulDestroyString(s);
    return result;
  }

  ///
  /// Convert to JSON text like `JSON.stringify()` does.
  ///
  /// @param  indent  The number of spaces to indent each level by (0 for compact output, at
  ///                 most 10).
  ///
  /// @return  Returns the JSON text. Fails with a TypeError if the value has no JSON form
  ///          (`undefined`, a function, or a Symbol), or with a JavaScript exception if a
  ///          toJSON() method or a getter throws.
  ///
  [[nodiscard]] Result<std::string> ToJSON(unsigned indent = 0) const {
    if (!handle_)
      return detail::EmptyHandleError();
    ULJSValue exception = nullptr;
    ULString s = ulJSValueToJSON(handle_, indent, &exception);
    if (!s) {
      if (exception)
        return Unexpected<Error>(Error::AdoptException(exception));
      // JSON.stringify renders some live values as no JSON at all (undefined, functions,
      // symbols); report that as its own failure, not as a page-gone error.
      if (ulJSValueIsAlive(handle_))
        return Unexpected<Error>(Error::TypeError("The value has no JSON representation."));
      return Unexpected<Error>(Error::PageGone());
    }
    std::string result(ulStringGetData(s), ulStringGetLength(s));
    ulDestroyString(s);
    return result;
  }

  ///
  /// Get a property of this object (own or inherited).
  ///
  /// @param  name  The property name (NULL is treated as "").
  ///
  /// @return  Returns the property value (`undefined` if there's no such property). Fails with
  ///          a JavaScript exception if a getter throws.
  ///
  [[nodiscard]] Result<Value> GetProperty(const char* name) const {
    if (!handle_)
      return detail::EmptyHandleError();
    ULJSValue exception = nullptr;
    ULJSValue result = ulJSObjectGetProperty(handle_, name ? name : "", &exception);
    if (!result) {
      if (exception)
        return Unexpected<Error>(Error::AdoptException(exception));
      return Unexpected<Error>(Error::PageGone());
    }
    return Adopt(result);
  }

  ///
  /// Whether or not this object has a property (own or inherited).
  ///
  /// Unlike reading the property, this returns true for a property set to `undefined`. It
  /// doesn't run getters.
  ///
  /// @param  name  The property name (NULL is treated as "").
  ///
  /// @note  Any failure returns false. Use HasProperty() if you need the reason.
  ///
  bool Has(const char* name) const {
    return handle_ && ulJSObjectHasProperty(handle_, name ? name : "", nullptr);
  }

  ///
  /// Whether or not this object has a property (like Has(), but reports failures).
  ///
  /// @param  name  The property name (NULL is treated as "").
  ///
  /// @return  Returns whether the property exists. Fails with a JavaScript exception if the
  ///          check throws (eg, in a Proxy's `has` handler).
  ///
  [[nodiscard]] Result<bool> HasProperty(const char* name) const {
    if (!handle_)
      return detail::EmptyHandleError();
    ULJSValue exception = nullptr;
    bool present = ulJSObjectHasProperty(handle_, name ? name : "", &exception);
    if (exception)
      return Unexpected<Error>(Error::AdoptException(exception));
    if (!IsAlive())
      return Unexpected<Error>(Error::PageGone());
    return present;
  }

  ///
  /// Set a property of this object.
  ///
  /// ```
  /// obj.SetProperty("id", ctx.Make(7.0),
  ///                 js::PropertyAttributes::ReadOnly | js::PropertyAttributes::DontDelete);
  /// ```
  ///
  /// @param  name        The property name (NULL is treated as "").
  ///
  /// @param  value       The value to assign (an empty Value assigns `undefined`).
  ///
  /// @param  attributes  The flags combined with `|`. They only apply if this creates the
  ///                     property (see PropertyAttributes).
  ///
  /// @return  Returns success. Fails with a JavaScript exception if a setter throws.
  ///
  /// @note  Like an assignment in non-strict JavaScript, a write the object refuses (a frozen
  ///        object, or a read-only property) is silently ignored and still returns success.
  ///
  [[nodiscard]] Result<void> SetProperty(
      const char* name, const Value& value,
      PropertyAttributes attributes = PropertyAttributes::None) const {
    if (!handle_)
      return detail::EmptyHandleError();
    ULJSValue exception = nullptr;
    if (ulJSObjectSetProperty(handle_, name ? name : "", value.handle_,
                              static_cast<unsigned>(attributes), &exception))
      return {};
    if (exception)
      return Unexpected<Error>(Error::AdoptException(exception));
    return Unexpected<Error>(Error::PageGone());
  }

  ///
  /// Call this value as a function.
  ///
  /// @param  this_value  The `this` value for the call (an empty Value uses the global object).
  ///
  /// @param  args        The arguments (an empty Value passes `undefined`).
  ///
  /// @param  argc        The number of entries in `args`.
  ///
  /// @return  Returns the function's return value. Fails with a JavaScript exception if the
  ///          call throws or this value can't be called.
  ///
  [[nodiscard]] Result<Value> Call(const Value& this_value, const Value* args,
                                   size_t argc) const {
    if (!handle_)
      return detail::EmptyHandleError();
    ULJSValue raw_args[8];
    ULJSValue* arg_ptr = raw_args;
    if (argc > 8) {
      // Rare wide calls take one heap allocation; typical calls stay on the stack.
      arg_ptr = new ULJSValue[argc];
    }
    for (size_t i = 0; i < argc; i++)
      arg_ptr[i] = args[i].handle_;
    ULJSValue exception = nullptr;
    ULJSValue result = ulJSFunctionCall(handle_, this_value.handle_, arg_ptr, argc, &exception);
    if (arg_ptr != raw_args)
      delete[] arg_ptr;
    if (!result) {
      if (exception)
        return Unexpected<Error>(Error::AdoptException(exception));
      return Unexpected<Error>(Error::PageGone());
    }
    return Adopt(result);
  }

  ///
  /// Call this value as a function with the arguments in a range (eg, a
  /// `std::vector<js::Value>`).
  ///
  /// @param  this_value  The `this` value for the call (an empty Value uses the global object).
  ///
  /// @param  args        The arguments.
  ///
  /// @return  Returns the function's return value and fails the same way as the overload
  ///          above.
  ///
  [[nodiscard]] Result<Value> Call(const Value& this_value,
                                   std::span<const Value> args) const {
    return Call(this_value, args.data(), args.size());
  }

  ///
  /// Convert to a C++ type.
  ///
  /// This works for every type js::TypeTraits supports: numbers, strings, containers,
  /// optionals, variants, reflected structs and enums, and your own specializations. The value
  /// must already have the matching JavaScript type (see "Type Conversions" above).
  ///
  /// ```
  /// js::Result<Settings> s = payload["settings"].To<Settings>();
  ///
  /// // To walk an array or an object's entries, convert to a container of js::Value:
  /// auto items   = arr.To<std::vector<js::Value>>().value_or({});
  /// auto entries = obj.To<std::map<std::string, js::Value>>().value_or({});
  /// ```
  ///
  /// An integer type needs a whole number in its range, so `To<int>()` on 3.5 fails instead of
  /// truncating.
  ///
  /// @return  Returns the converted value. Fails with a TypeError (code `ULJS_BAD_ARG`) if the
  ///          value doesn't have the requested type. Its message says where and why, like a
  ///          bound function's (eg, "volume: expected number, got 'loud'").
  ///
  /// @note  Requires `<Ultralight/js/TypeTraits.h>` (or any header that includes it).
  ///
  template <typename T>
  [[nodiscard]] Result<T> To() const;

  ///
  /// Convert to a C++ type like To() but without the failure reason.
  ///
  /// @return  Returns the converted value or nullopt on any failure.
  ///
  template <typename T>
  [[nodiscard]] std::optional<T> Maybe() const;

  ///
  /// Convert to a C++ type like To() but with a fallback.
  ///
  /// @param  fallback  The value to return if the conversion fails.
  ///
  /// @return  Returns the converted value or `fallback` on any failure.
  ///
  /// @note  The fallback's type is the type converted to, so pass `0.0` (not `0`) to read a
  ///        number that can have a fraction: `Or(0)` converts to `int` and fails on 3.5.
  ///
  template <typename T>
  [[nodiscard]] T Or(T fallback) const;

  ///
  /// Convert to a std::string like To() but with a string-literal fallback.
  ///
  /// @param  fallback  The text to return if the conversion fails.
  ///
  /// @return  Returns the converted string or `fallback` on any failure.
  ///
  [[nodiscard]] std::string Or(const char* fallback) const;

  ///
  /// Call this value as a function with typed arguments and result.
  ///
  /// The arguments and the result (when you give R) convert through js::TypeTraits:
  ///
  /// ```
  /// js::Result<double> total = fn.Invoke<double>(3, "x");
  /// fn.Invoke(path);                    // result left as a js::Value
  /// ```
  ///
  /// @param  args  The arguments to pass to the function.
  ///
  /// @return  Returns the call's result converted to R. Fails with a JavaScript exception if
  ///          the call throws, or with a TypeError if the result doesn't convert to R.
  ///
  /// @note  The function's `this` is the global object. To call a method on an object, call
  ///        through operator[] (`obj["m"](x)`) or use InvokeOn().
  ///
  template <typename R = Value, typename... A>
  Result<R> Invoke(A&&... args) const;

  ///
  /// Call this value as a function with an explicit `this` (otherwise the same as Invoke()).
  ///
  /// ```
  /// js::Result<js::Value> row = fn.InvokeOn(receiver, 42);
  /// ```
  ///
  /// @param  this_value  The `this` value for the call.
  ///
  /// @param  args        The arguments to pass to the function.
  ///
  /// @return  Returns the call's result and fails the same way as Invoke().
  ///
  template <typename R = Value, typename... A>
  Result<R> InvokeOn(const Value& this_value, A&&... args) const;

  ///
  /// Call this value as a function: `on_save(path)` is the same as `on_save.Invoke(path)`.
  ///
  /// @param  args  The arguments to pass to the function.
  ///
  /// @return  Returns the call's result and fails the same way as Invoke().
  ///
  template <typename... A>
  Result<Value> operator()(A&&... args) const {
    return Invoke<Value>(std::forward<A>(args)...);
  }

  ///
  /// Wait for this promise to settle without a coroutine.
  ///
  /// `on_settled` runs once on the Renderer's thread during a later Renderer::Update(). Its
  /// Result holds the promise's value or an Error with the rejection reason. If the page goes
  /// away first, it gets a page-gone Error instead, so it always runs.
  ///
  /// ```
  /// // Page:  myApp.nativeReady(fetch('/level.json').then(r => r.json()));
  /// api["nativeReady"] = [](js::Value promise) {
  ///   bool waiting = promise.Then([](js::Result<js::Value> r) {
  ///     if (r)
  ///       LoadLevel(*r);                      // fulfilled: r holds the value
  ///     else if (!r.error().is_page_gone())
  ///       Log(r.error().message());           // rejected: r.error() holds the reason
  ///   });
  ///   if (!waiting)
  ///     Log("nativeReady expects a Promise");
  /// };
  /// ```
  ///
  /// @param  on_settled  A callable that takes a js::Result<js::Value>.
  ///
  /// @return  Returns true if this value is a Promise and `on_settled` will run. Otherwise it
  ///          returns false and destroys `on_settled` without running it.
  ///
  /// \parblock
  /// @note  A rejection you wait for this way counts as handled, so the page doesn't report it
  ///        as unhandled.
  /// \endparblock
  ///
  /// \parblock
  /// @note  In a coroutine, use `co_await js::Await<T>(promise)` instead (see
  ///        `<Ultralight/js/Task.h>`).
  /// \endparblock
  ///
  template <typename F>
  [[nodiscard]] bool Then(F&& on_settled) const {
    using Fn = std::decay_t<F>;
    static_assert(std::is_invocable_v<Fn&, Result<Value>>,
                  "js::Value::Then takes a callable invocable with js::Result<js::Value>");
    auto* fn = new Fn(std::forward<F>(on_settled));
    // On registration failure the library destroys the observer inline, freeing `fn`.
    return ulJSPromiseThen(handle_, &ThenThunk<Fn>, fn, &DeleteThenCallable<Fn>);
  }

  ///
  /// Wait for this promise to settle and convert its value to a C++ type.
  ///
  /// ```
  /// bool waiting = promise.Then<std::string>([](js::Result<std::string> r) {
  ///   if (r)
  ///     ApplyName(*r);
  /// });
  /// ```
  ///
  /// @param  on_settled  A callable that takes a js::Result<T>.
  ///
  /// @return  Returns true if this value is a Promise and `on_settled` will run (see Then()).
  ///
  /// @note  If the value doesn't convert to T (see To()), `on_settled` gets the TypeError.
  ///
  template <typename T, typename F>
    requires(!std::is_same_v<T, Value>)
  [[nodiscard]] bool Then(F&& on_settled) const {
    using Fn = std::decay_t<F>;
    static_assert(std::is_invocable_v<Fn&, Result<T>>,
                  "js::Value::Then<T> takes a callable invocable with js::Result<T>");
    return Then(TypedSettleAdapter<T, Fn> { std::forward<F>(on_settled) });
  }

  class Ref;

  ///
  /// Access a property to read, assign, or call it.
  ///
  /// ```
  /// double volume = settings["volume"].Or(0.0);
  /// settings["volume"] = 5.0;
  /// settings["nested"]["deep"] = js::null;
  /// settings["save"]("slot1");                       // method call: `this` = settings
  /// ```
  ///
  /// A read that fails (the Value is empty or its page is gone, it holds `null` or `undefined`,
  /// or a getter throws) gives you an empty Value instead of an error, like JavaScript's
  /// optional chaining. Use GetProperty() or SetProperty() when you need the reason.
  ///
  /// @param  name  The property name.
  ///
  /// @return  Returns a Value::Ref for the property.
  ///
  /// \parblock
  /// @note  A missing property reads as a valid `undefined` Value (`operator bool` returns
  ///        true). Use Has() to check whether a property exists.
  /// \endparblock
  ///
  /// \parblock
  /// @note  You can also index with any integer type (eg, `arr[2]`). A negative index reads the
  ///        property with that name as JavaScript does (`arr[-1]` reads "-1").
  /// \endparblock
  ///
  Ref operator[](const char* name) const;
  template <typename I>
    requires(std::is_integral_v<I> && !std::is_same_v<I, bool>)
  Ref operator[](I index) const;

  // --- Interop with the C API (most embedders never touch raw handles) -------------------

  ///
  /// Take ownership of a handle from the C API without adding a reference.
  ///
  /// Use this for a handle you'd otherwise destroy with ulDestroyJSValue() (eg, the return value
  /// of a C function).
  ///
  /// @param  handle  The handle to take ownership of.
  ///
  /// @return  Returns a Value that owns `handle`.
  ///
  /// @note  For a handle the library owns (eg, a callback argument), use FromBorrowed().
  ///
  static Value Adopt(ULJSValue handle) { return Value(handle); }

  ///
  /// Add a reference to a handle the library owns (eg, a callback argument) so it can outlive
  /// the callback.
  ///
  /// @param  handle  The borrowed handle.
  ///
  /// @return  Returns a Value with its own reference to `handle`.
  ///
  /// @note  For a handle you own, use Adopt().
  ///
  static Value FromBorrowed(ULJSValue handle) {
    return Value(handle ? ulCreateJSValueRef(handle) : nullptr);
  }

  ///
  /// Get the C API handle without transferring ownership.
  ///
  /// @return  Returns the handle for use with the `<Ultralight/CAPI/CAPI_JSValue.h>` functions.
  ///
  ULJSValue raw() const { return handle_; }

  ///
  /// Give up ownership of the C API handle and return it (like RefPtr::LeakRef()).
  ///
  /// @return  Returns the handle. You must destroy it with ulDestroyJSValue() when finished.
  ///
  ULJSValue LeakRef() {
    ULJSValue handle = handle_;
    handle_ = nullptr;
    return handle;
  }

 protected:
  explicit Value(ULJSValue handle) : handle_(handle) {}

 private:
  // Decimal property name for integer indexes JavaScript treats as string keys.
  struct IndexKey {
    char text[24];
  };

  static IndexKey MakeIndexKey(long long value) {
    IndexKey key;
    snprintf(key.text, sizeof(key.text), "%lld", value);
    return key;
  }

  static IndexKey MakeIndexKey(unsigned long long value) {
    IndexKey key;
    snprintf(key.text, sizeof(key.text), "%llu", value);
    return key;
  }

  template <typename I>
  Ref MakeIndexRef(I index) const;

  // Named functor (never a lambda: the MSVC alias-in-lambda discipline) bridging the
  // untyped settlement to Then<T>'s typed callback.
  template <typename T, typename Fn>
  struct TypedSettleAdapter {
    Fn fn;
    void operator()(Result<Value> settled) {
      if (!settled)
        fn(Result<T>(Unexpected<Error>(std::move(settled).error())));
      else
        fn(settled.value().To<T>());
    }
  };

  template <typename Fn>
  static void ThenThunk(void* user_data, ULJSContext, ULJSValue result, bool rejected) {
    Fn& fn = *static_cast<Fn*>(user_data);
    // The callback runs inside the library's update: an exception must not unwind into it.
    ultralight::detail::CallCatchingExceptions(
        [&] {
          if (!result)
            fn(Result<Value>(Unexpected<Error>(Error::PageGone())));
          else if (rejected)
            fn(Result<Value>(
                Unexpected<Error>(Error::AdoptException(ulCreateJSValueRef(result)))));
          else
            fn(Result<Value>(Value::FromBorrowed(result)));
        },
        [](const char* what) { detail::LogNativeException("js::Value::Then", what); });
  }

  template <typename Fn>
  static void DeleteThenCallable(void* user_data) {
    delete static_cast<Fn*>(user_data);
  }

  ULJSValue handle_;
};

///
/// A reference to a property of a Value (returned by Value::operator[]).
///
/// Reading a Ref gives you the property's value, assigning to it sets the property, and calling
/// it calls the property as a method of the object. Refs chain (`obj["a"]["b"] = 1`).
///
/// Use a Ref within one expression. To keep a property's value, read it into a Value.
///
/// @note  A Ref has To(), Maybe(), and Or(). For the other conversions, read the property first
///        (eg, `obj["volume"].Get().ToNumber()`).
///
class Value::Ref {
 public:
  ///
  /// Read the property.
  ///
  /// @return  Returns the property value (an empty Value if the read fails, see
  ///          Value::operator[]()).
  ///
  [[nodiscard]] Value Get() const {
    detail::SwallowedException swallow;
    if (by_index_)
      return Value::Adopt(
          object_.raw() ? ulJSObjectGetPropertyAtIndex(object_.raw(), index_, swallow.out())
                        : nullptr);
    return Value::Adopt(
        object_.raw() ? ulJSObjectGetProperty(object_.raw(), name_.c_str(), swallow.out())
                      : nullptr);
  }

  ///
  /// Read the property (the same as Get()).
  ///
  operator Value() const { return Get(); }

  ///
  /// Set the property to `value` (converted through js::TypeTraits).
  ///
  /// @param  value  The value to assign.
  ///
  /// @return  Returns this Ref.
  ///
  /// @note  Failures are ignored (eg, the page is gone or a setter throws). Use
  ///        Value::SetProperty() if you need to check.
  ///
  template <typename T>
  Ref& operator=(T&& value);

  ///
  /// Read this property and access a property of its value.
  ///
  /// @param  name  The property name.
  ///
  /// @return  Returns a Ref for the nested property.
  ///
  Ref operator[](const char* name) const {
    Value value = Get();
    return Chained(value, value[name]);
  }
  template <typename I>
    requires(std::is_integral_v<I> && !std::is_same_v<I, bool>)
  Ref operator[](I index) const {
    Value value = Get();
    return Chained(value, value[index]);
  }

  ///
  /// Call the property as a method of its object (otherwise the same as Value::Invoke()).
  ///
  /// ```
  /// global["JSON"]["stringify"](payload);
  /// js::Result<double> total = cart["total"].Invoke<double>();
  /// ```
  ///
  /// @param  args  The arguments to pass to the method.
  ///
  /// @return  Returns the call's result converted to R and fails the same way as
  ///          Value::Invoke().
  ///
  template <typename R = Value, typename... A>
  Result<R> Invoke(A&&... args) const;

  ///
  /// Call the property as a method: `obj["save"](path)` is the same as `obj["save"].Invoke(path)`.
  ///
  /// @param  args  The arguments to pass to the method.
  ///
  /// @return  Returns the call's result and fails the same way as Invoke().
  ///
  template <typename... A>
  Result<Value> operator()(A&&... args) const {
    return Invoke<Value>(std::forward<A>(args)...);
  }

  ///
  /// Read the property and convert it to a C++ type (see Value::To()).
  ///
  /// @return  Returns the converted value and fails the same way as Value::To().
  ///
  template <typename T>
  [[nodiscard]] Result<T> To() const;

  ///
  /// Read the property and convert it to a C++ type like To() but without the failure reason.
  ///
  /// @return  Returns the converted value or nullopt on any failure.
  ///
  template <typename T>
  [[nodiscard]] std::optional<T> Maybe() const;

  ///
  /// Read the property and convert it to a C++ type with a fallback.
  ///
  /// @param  fallback  The value to return if the read or the conversion fails.
  ///
  /// @return  Returns the converted value or `fallback` on any failure.
  ///
  template <typename T>
  [[nodiscard]] T Or(T fallback) const;

  ///
  /// Read the property and convert it to a std::string with a string-literal fallback.
  ///
  /// @param  fallback  The text to return if the read or the conversion fails.
  ///
  /// @return  Returns the converted string or `fallback` on any failure.
  ///
  [[nodiscard]] std::string Or(const char* fallback) const { return Get().Or(fallback); }

  ///
  /// Copy constructor (refers to the same property).
  ///
  Ref(const Ref&) = default;

  ///
  /// Set this property to the value of another Ref's property.
  ///
  /// @param  other  The Ref to read the value from.
  ///
  /// @return  Returns this Ref.
  ///
  /// @note  A Ref always refers to the same property: assigning to it sets the property's value
  ///        and never changes which property it refers to (like std::map's operator[]).
  ///
  Ref& operator=(const Ref& other) { return (*this) = other.Get(); }

 private:
  friend class Value;
  friend class Context;
  Ref(Value object, const char* name)
      : object_(std::move(object)), name_(name ? name : ""), by_index_(false), index_(0) {}
  Ref(Value object, size_t index)
      : object_(std::move(object)), by_index_(true), index_(index) {}

  // Whether a failed read here is because the page is gone: this Ref's object belongs to a page
  // that's gone, or the chain it came from started at one.
  bool SourceGone() const { return source_gone_ || (object_.raw() && !object_.IsAlive()); }

  // A chain continued through a failed read keeps reporting a gone page as page-gone, not as an
  // empty handle.
  Ref Chained(const Value& value, Ref next) const {
    next.source_gone_ = value.IsEmpty() && SourceGone();
    return next;
  }

  Value object_;
  std::string name_;
  bool by_index_;
  size_t index_;
  bool source_gone_ = false;
};

inline Value::Ref Value::operator[](const char* name) const {
  return Ref(*this, name);
}

template <typename I>
Value::Ref Value::MakeIndexRef(I index) const {
  if constexpr (std::is_signed_v<I>) {
    if (index < 0)
      return Ref(*this, MakeIndexKey(static_cast<long long>(index)).text);
  }
  unsigned long long wide = static_cast<unsigned long long>(index);
  if (wide > SIZE_MAX)
    return Ref(*this, MakeIndexKey(wide).text);
  return Ref(*this, static_cast<size_t>(wide));
}

template <typename I>
  requires(std::is_integral_v<I> && !std::is_same_v<I, bool>)
Value::Ref Value::operator[](I index) const {
  return MakeIndexRef(index);
}

///
/// Compare against the JavaScript `null` literal: `if (v == js::null)`.
///
/// @return  Returns whether the value is JavaScript `null`.
///
inline bool operator==(const Value& value, NullType) {
  return value.IsNull();
}

///
/// Compare against the JavaScript `undefined` literal: `if (v == js::undefined)`.
///
/// @return  Returns whether the value is JavaScript `undefined`.
///
inline bool operator==(const Value& value, UndefinedType) {
  return value.IsUndefined();
}

///
/// An argument passed to a bound function.
///
/// An Arg is only valid during the callback that received it (it adds no reference, so reading
/// it is cheap). Call ToValue() to keep the argument after the callback returns.
///
/// An Arg has To(), Maybe(), and Or(). Other conversions need a Value first (eg,
/// `arg.ToValue().ToNumber()`).
///
/// @see  CallInfo::arg()
///
class Arg {
 public:
  ///
  /// Wrap a borrowed handle.
  ///
  /// @param  borrowed  The handle (valid only during the callback).
  ///
  /// @note  You normally get an Arg from CallInfo::arg() instead of creating one.
  ///
  explicit Arg(ULJSValue borrowed) : borrowed_(borrowed) {}

  ///
  /// Whether or not the argument is absent (the call passed fewer arguments than the index
  /// given to CallInfo::arg()).
  ///
  /// @note  An absent argument converts only to an optional type (as an empty optional). Any
  ///        other To() fails with a TypeError, and ToValue() returns an empty Value.
  ///
  bool IsEmpty() const { return borrowed_ == nullptr; }

  ///
  /// Get the argument's type (Type::Invalid if the argument is absent).
  ///
  Type type() const { return static_cast<Type>(ulJSValueGetType(borrowed_)); }

  ///
  /// Convert to a C++ type (see Value::To()).
  ///
  /// @return  Returns the converted value and fails the same way as Value::To().
  ///
  /// @note  Requires `<Ultralight/js/TypeTraits.h>` (or any header that includes it).
  ///
  template <typename T>
  [[nodiscard]] Result<T> To() const;

  ///
  /// Convert to a C++ type like To() but without the failure reason.
  ///
  /// @return  Returns the converted value or nullopt on any failure.
  ///
  template <typename T>
  [[nodiscard]] std::optional<T> Maybe() const;

  ///
  /// Convert to a C++ type like To() but with a fallback.
  ///
  /// @param  fallback  The value to return if the conversion fails.
  ///
  /// @return  Returns the converted value or `fallback` on any failure.
  ///
  template <typename T>
  [[nodiscard]] T Or(T fallback) const;

  ///
  /// Convert to a std::string like To() but with a string-literal fallback.
  ///
  /// @param  fallback  The text to return if the conversion fails.
  ///
  /// @return  Returns the converted string or `fallback` on any failure.
  ///
  [[nodiscard]] std::string Or(const char* fallback) const;

  ///
  /// Get a Value that keeps the argument after the callback returns.
  ///
  /// @return  Returns the new Value.
  ///
  Value ToValue() const { return Value::FromBorrowed(borrowed_); }

  ///
  /// Get the C API handle.
  ///
  /// @return  Returns the handle (valid only during the callback).
  ///
  ULJSValue raw() const { return borrowed_; }

 private:
  ULJSValue borrowed_;
};

///
/// Convert a value to a C++ type with a fallback (the free-function form of Value::Or()).
///
/// This takes a Value, a Value::Ref, a js::Arg, or the Result of a call. The value must already
/// have the requested JavaScript type (see Value::To()). Any failure returns `fallback` (eg, a
/// missing property or a failed call):
///
/// ```
/// double score = js::Or(ctx["score"], 0.0);
/// double total = js::Or(ctx["computeTotal"](3, 4), 0.0);
/// std::string name = js::Or(ctx["playerName"](), "guest");
/// ```
///
/// @param  value     The value to convert.
///
/// @param  fallback  The value to return if the conversion fails.
///
/// @return  Returns the converted value or `fallback` on any failure.
///
/// @note  Requires `<Ultralight/js/TypeTraits.h>` (or any header that includes it).
///
template <typename T>
[[nodiscard]] T Or(const Value& value, T fallback) {
  return value.Or(std::move(fallback));
}

///
/// Convert a value to a std::string with a string-literal fallback.
///
/// @param  value     The value to convert.
///
/// @param  fallback  The text to return if the conversion fails.
///
/// @return  Returns the converted string or `fallback` on any failure.
///
[[nodiscard]] inline std::string Or(const Value& value, const char* fallback) {
  return value.Or(fallback);
}

///
/// Convert a callback argument to a C++ type with a fallback.
///
/// @param  arg       The argument to convert.
///
/// @param  fallback  The value to return if the conversion fails.
///
/// @return  Returns the converted value or `fallback` on any failure.
///
template <typename T>
[[nodiscard]] T Or(const Arg& arg, T fallback) {
  return arg.Or(std::move(fallback));
}

///
/// Convert a callback argument to a std::string with a string-literal fallback.
///
/// @param  arg       The argument to convert.
///
/// @param  fallback  The text to return if the conversion fails.
///
/// @return  Returns the converted string or `fallback` on any failure.
///
[[nodiscard]] inline std::string Or(const Arg& arg, const char* fallback) {
  return arg.Or(fallback);
}

///
/// Convert the value a call returned to a C++ type with a fallback.
///
/// @param  result    The call's result.
///
/// @param  fallback  The value to return if the call or the conversion failed.
///
/// @return  Returns the converted value or `fallback` on any failure.
///
template <typename R, typename T>
  requires std::is_same_v<std::remove_cvref_t<R>, Result<Value>>
[[nodiscard]] T Or(const R& result, T fallback) {
  return result ? result.value().Or(std::move(fallback)) : fallback;
}

///
/// Convert the value a call returned to a std::string with a string-literal fallback.
///
/// @param  result    The call's result.
///
/// @param  fallback  The text to return if the call or the conversion failed.
///
/// @return  Returns the converted string or `fallback` on any failure.
///
template <typename R>
  requires std::is_same_v<std::remove_cvref_t<R>, Result<Value>>
[[nodiscard]] std::string Or(const R& result, const char* fallback) {
  return result ? result.value().Or(fallback) : std::string(fallback ? fallback : "");
}

///
/// Get a typed Result's value or `fallback` if it holds an error.
///
/// This is the same as `result.value_or(fallback)` and lets a typed call read like an untyped
/// one: `js::Or(fn.Invoke<double>(3, 4), 0.0)`.
///
/// @param  result    The typed result.
///
/// @param  fallback  The value to return if the result holds an error.
///
/// @return  Returns the result's value or `fallback`.
///
template <typename T, typename U>
  requires(!std::is_same_v<T, Value>)
[[nodiscard]] T Or(const Result<T>& result, U&& fallback) {
  return result.value_or(std::forward<U>(fallback));
}

///
/// Get the value a JavaScript exception threw, to keep it or pass it back to the page.
///
/// ```
/// js::Result<js::Value> r = ctx.Evaluate("JSON.parse('{')");
/// if (!r && r.error().is_exception())
///   ctx["lastError"] = js::ExceptionValue(r.error());
/// ```
///
/// @param  error  The error.
///
/// @return  Returns the thrown value (an empty Value if `error` doesn't hold a JavaScript
///          exception).
///
inline Value ExceptionValue(const Error& error) {
  return Value::FromBorrowed(error.exception());
}

}  // namespace js
}  // namespace ultralight

#pragma pop_macro("None")
