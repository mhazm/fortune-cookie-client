///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/CAPI/CAPI_JSAPI.h>
#include <Ultralight/CAPI/CAPI_JSRuntime.h>
#include <Ultralight/CAPI/CAPI_JSValue.h>
#include <Ultralight/CAPI/CAPI_String.h>
#include <Ultralight/View.h>
#include <Ultralight/detail/Exceptions.h>
#include <Ultralight/js/API.h>
#include <Ultralight/js/Buffer.h>
#include <Ultralight/js/CallInfo.h>
#include <Ultralight/js/Error.h>
#include <Ultralight/js/Resolver.h>
#include <Ultralight/js/TypeTraits.h>
#include <Ultralight/js/Value.h>

#include <cstddef>
#include <initializer_list>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace ultralight {
namespace js {

///
/// Counts of the live handles on one context, for finding leaks (see Context::GetHandleStats()).
///
/// \parblock
/// @note  Copies of one js::Value count once, and the counts include a few handles the library
///        holds for you (eg, for a Promise made with Context::MakePromise()).
/// \endparblock
///
/// \parblock
/// @note  A destroyed handle can stay counted until the next Renderer::Update().
/// \endparblock
///
struct HandleStats {
  size_t total_values = 0;                      ///< Live value handles.
  size_t values_by_type[kULJSType_Count] = {};  ///< Live value handles by js::Type (see count()).
  size_t weak_refs = 0;  ///< Live weak references (0 once the context is gone).

  ///
  /// Get the number of live value handles of one type.
  ///
  /// @param  type  The value type.
  ///
  /// @return  Returns the number of live value handles of that type.
  ///
  size_t count(Type type) const {
    size_t index = static_cast<size_t>(type);
    return index < kULJSType_Count ? values_by_type[index] : 0;
  }

  ///
  /// Count the value handles that keep something from being garbage collected (strings,
  /// symbols, BigInts, and objects).
  ///
  /// @return  Returns the number of those handles.
  ///
  size_t ProtectedEstimate() const {
    return count(Type::String) + count(Type::Symbol) + count(Type::BigInt)
        + count(Type::Object);
  }
};

///
/// A new pending Promise and the Resolver that settles it (see Context::MakePromise()).
///
struct PendingPromise {
  Value promise;      ///< The Promise to hand to the page.
  Resolver resolver;  ///< Settles `promise` (from any thread).
};

///
/// JavaScript execution environment for a page.
///
/// Every call into a page's JavaScript environment goes through its Context. It represents the
/// execution state of a loaded document, providing access to the global object where page functions
/// and variables live.
///
/// Construct a Context from a View inside LoadListener::OnDOMReady() to call functions and read
/// global variables on the page:
///
/// ```
/// void MyApp::OnDOMReady(View* caller, uint64_t frame_id, bool is_main_frame,
///                        const String& url) {
///   if (!is_main_frame)
///     return;
///
///   js::Context ctx(caller);
///   ctx["ShowMessage"]("Howdy!");              // call a page function
///   double score = js::Or(ctx["score"], 0.0);  // read a global (0.0 if it fails)
///   double doubled = js::Or(ctx.Evaluate<double>("score * 2"), 0.0);
/// }
/// ```
///
/// ## Getting a Context
///
/// Constructing a Context with a View wraps the main frame's JavaScript environment. To access the
/// Context of a subframe or a specific document, pass that document to dom::GetJSContext().
///
/// You'll typically construct a Context inside a LoadListener callback:
///
/// - **Use LoadListener::OnDOMReady() for work that needs the document or its scripts.** It fires
///   once the DOM is fully parsed and page scripts have run.
/// - **Use LoadListener::OnWindowObjectReady() to set up global state early.** It fires before the
///   page's own scripts run.
///
/// Each page gets its own Context. When a View navigates to a new page, the old Context's page is
/// gone, along with every Value created from it (see js::Value for what that means), so you'll need
/// to obtain a new Context for the new page. Operations on a Context whose page is gone fail safely
/// instead of crashing.
///
/// ## Creating Values
///
/// Call Make() to convert a native C++ value into a JavaScript Value. The method converts any type
/// that js::TypeTraits supports, including numbers, strings, standard containers, reflected
/// structs, and enums.
///
/// Convert a reflected struct to a JavaScript object and pass it to a page function:
///
/// ```
/// struct HudSettings {
///   std::string theme;
///   double volume;
/// };
///
/// js::Value settings = ctx.Make(HudSettings { "dark", 0.8 });
/// ctx["applySettings"](settings);
/// ```
///
/// \parblock
/// @note  Context operations must run on the Renderer's thread. To dispatch work from another
///        thread, call PostTask() to schedule a callback that runs during a later
///        Renderer::Update().
/// \endparblock
///
/// \parblock
/// @note  js::Context and dom::data::Context are unrelated types (dom::data::Context manages
///        declarative data bindings).
/// \endparblock
///
/// @see  js::Value, js::CallInfo::context(), dom::GetJSContext(), LoadListener::OnDOMReady()
///
class Context {
 public:
  ///
  /// Create an empty Context.
  ///
  Context() = default;

  ///
  /// Get the context of a View's main frame.
  ///
  /// @param  view  The View. The Context is empty if `view` is NULL or its page can't run script
  ///               (JavaScript is disabled or the document is sandboxed).
  ///
  /// @note  A Context belongs to the page loaded now. After the View loads a new page, get a new
  ///        Context.
  ///
  explicit Context(View* view) : handle_(view ? view->GetJSContext() : nullptr) {}

  ///
  /// Get the context a value belongs to.
  ///
  /// @param  value  The value. An empty Value gives an empty Context, and a Value whose page is
  ///                gone gives a Context whose page is also gone.
  ///
  explicit Context(const Value& value)
      : handle_(value.raw() ? ulJSValueGetContext(value.raw()) : nullptr) {}

  // --- Interop with the C API (most embedders never touch raw handles) -------------------

  ///
  /// Take ownership of a context handle from the C API.
  ///
  /// @param  handle  The handle to take ownership of.
  ///
  /// @return  Returns a Context that owns `handle`.
  ///
  /// @warning  Don't pass a handle the library owns (eg, one a callback receives). Use
  ///           FromBorrowed() for that.
  ///
  static Context Adopt(ULJSContext handle) { return Context(handle); }

  ///
  /// Add a reference to a context handle the library owns (eg, the context a C API callback
  /// receives) so you can use it with this class and keep it after the callback.
  ///
  /// @param  handle  The borrowed handle.
  ///
  /// @return  Returns a Context with its own reference to `handle`.
  ///
  static Context FromBorrowed(ULJSContext handle) {
    return Context(handle ? ulCreateJSContextRef(handle) : nullptr);
  }

  ///
  /// Copy constructor (refers to the same context).
  ///
  Context(const Context& other)
      : handle_(other.handle_ ? ulCreateJSContextRef(other.handle_) : nullptr) {}

  ///
  /// Move constructor (`other` becomes empty).
  ///
  Context(Context&& other) noexcept : handle_(other.handle_) { other.handle_ = nullptr; }

  ///
  /// Assignment (copies or moves `other` into this Context).
  ///
  Context& operator=(Context other) noexcept {
    std::swap(handle_, other.handle_);
    return *this;
  }

  ///
  /// Destructor (releases this handle).
  ///
  ~Context() { ulDestroyJSContext(handle_); }

  ///
  /// Whether or not this Context is valid (it isn't empty and its page is still alive).
  ///
  explicit operator bool() const { return handle_ && ulJSContextIsAlive(handle_); }

  ///
  /// Whether or not this Context holds nothing.
  ///
  bool IsEmpty() const { return handle_ == nullptr; }

  ///
  /// Whether or not this Context's page is still alive (the same test as `operator bool`).
  ///
  /// @note  Safe to call from any thread, but off the Renderer's thread the result may already
  ///        be out of date when you act on it.
  ///
  bool IsAlive() const { return handle_ && ulJSContextIsAlive(handle_); }

  ///
  /// Run a script and get its completion value.
  ///
  /// @param  script      The script (UTF-8; embedded null characters are kept).
  ///
  /// @param  source_url  A URL for the script, used in error reports (may be NULL).
  ///
  /// @return  Returns the script's completion value. Fails with a JavaScript exception if the
  ///          script throws.
  ///
  [[nodiscard]] Result<Value> Evaluate(std::string_view script,
                                       const char* source_url = nullptr) const {
    if (!handle_)
      return detail::EmptyHandleError();
    ULString source = ulCreateStringUTF8(script.data(), script.size());
    ULJSValue exception = nullptr;
    ULJSValue result = ulJSContextEvaluate(handle_, source, source_url, &exception);
    ulDestroyString(source);
    if (!result) {
      if (exception)
        return Unexpected<Error>(Error::AdoptException(exception));
      return Unexpected<Error>(Error::PageGone());
    }
    return Value::Adopt(result);
  }

  ///
  /// Run a script and convert its completion value to a C++ type (see Value::To()).
  ///
  /// ```
  /// js::Result<double> health = ctx.Evaluate<double>("game.player.health");
  /// double speed = ctx.Evaluate<double>("game.player.speed").value_or(1.0);
  /// ```
  ///
  /// @param  script      The script (UTF-8; embedded null characters are kept).
  ///
  /// @param  source_url  A URL for the script, used in error reports (may be NULL).
  ///
  /// @return  Returns the converted value. Fails with a JavaScript exception if the script
  ///          throws, or with a TypeError if the value doesn't convert to T.
  ///
  template <typename T>
    requires Marshalable<T>
  [[nodiscard]] Result<T> Evaluate(std::string_view script,
                                   const char* source_url = nullptr) const {
    Result<Value> completion = Evaluate(script, source_url);
    if (!completion)
      return Unexpected<Error>(std::move(completion).error());
    return completion.value().To<T>();
  }

  ///
  /// Get the global object.
  ///
  /// @return  Returns the global object (an empty Value if the context is gone).
  ///
  Value GlobalObject() const {
    return Value::Adopt(ulJSContextGetGlobalObject(handle_));
  }

  ///
  /// Access a property of the global object (`ctx["fn"](args)` is the same as
  /// `ctx.GlobalObject()["fn"](args)`):
  ///
  /// ```
  /// ctx["ShowMessage"]("Howdy!");                       // `this` = the global object
  /// double score = js::Or(ctx["score"], 0.0);
  /// double total = js::Or(ctx["computeTotal"](3, 4), 0.0);
  /// ```
  ///
  /// @param  name  The property name.
  ///
  /// @return  Returns a Value::Ref for the property.
  ///
  Value::Ref operator[](const char* name) const {
    Value global = GlobalObject();
    Value::Ref ref = global[name];
    // The global object reads empty only when the context's page is gone.
    ref.source_gone_ = handle_ && global.IsEmpty();
    return ref;
  }

  ///
  /// Convert a C++ value to a JavaScript value through js::TypeTraits.
  ///
  /// This works for every supported type, including reflected structs, enums, containers,
  /// js::null, and js::undefined.
  ///
  /// @param  value  The C++ value to convert.
  ///
  /// @return  Returns the new value (an empty Value if the context is gone).
  ///
  template <typename T>
  Value Make(T&& value) const {
    using U = std::decay_t<T>;
    detail::AssertInteropHeader<U>();
    static_assert(Marshalable<U>,
                  "no js::TypeTraits for T: use a supported built-in, make it a plain "
                  "aggregate struct, or specialize js::TypeTraits for it");
    return Value::Adopt(handle_ ? TypeTraits<U>::ToJS(handle_, value) : nullptr);
  }

  ///
  /// Create a new empty JavaScript object.
  ///
  /// @return  Returns the new object (an empty Value if the context is gone).
  ///
  Value MakeObject() const { return Value::Adopt(ulCreateJSObject(handle_)); }

  ///
  /// Create a new JavaScript Array.
  ///
  /// @param  elements  The elements (an empty Value becomes `undefined`).
  ///
  /// @param  count     The number of entries in `elements`.
  ///
  /// @return  Returns the new Array (an empty Value if the context or an element's page is gone).
  ///
  Value MakeArray(const Value* elements, size_t count) const {
    if (!handle_)
      return Value();
    std::vector<ULJSValue> raw(count);
    for (size_t i = 0; i < count; i++)
      raw[i] = elements[i].raw();
    return Value::Adopt(ulCreateJSArray(handle_, count ? raw.data() : nullptr, count));
  }

  ///
  /// Create a new JavaScript Array from a brace list (eg, `ctx.MakeArray({ a, b })`).
  ///
  /// @param  elements  The elements.
  ///
  /// @return  Returns the new Array (an empty Value if the context or an element's page is gone).
  ///
  Value MakeArray(std::initializer_list<Value> elements) const {
    return MakeArray(elements.begin(), elements.size());
  }

  ///
  /// Create a new JavaScript Array from a range (eg, a `std::vector<js::Value>`).
  ///
  /// @param  elements  The elements.
  ///
  /// @return  Returns the new Array (an empty Value if the context or an element's page is gone).
  ///
  Value MakeArray(std::span<const Value> elements) const {
    return MakeArray(elements.data(), elements.size());
  }

  ///
  /// Create a value from JSON text (the reverse of Value::ToJSON()):
  ///
  /// ```
  /// js::Value config = js::OrEmpty(ctx.MakeFromJSON(R"({"volume": 5, "muted": false})"));
  /// ```
  ///
  /// @param  json  The JSON text (any JSON value).
  ///
  /// @return  Returns the new value. Fails with a SyntaxError (a JavaScript exception) if the
  ///          JSON is invalid.
  ///
  [[nodiscard]] Result<Value> MakeFromJSON(std::string_view json) const {
    if (!handle_)
      return detail::EmptyHandleError();
    ULString text = ulCreateStringUTF8(json.data(), json.length());
    ULJSValue exception = nullptr;
    ULJSValue result = ulCreateJSValueFromJSON(handle_, text, &exception);
    ulDestroyString(text);
    if (!result) {
      if (exception)
        return Unexpected<Error>(Error::AdoptException(exception));
      return Unexpected<Error>(Error::PageGone());
    }
    return Value::Adopt(result);
  }

  ///
  /// Run a function with this context during a later Renderer::Update().
  ///
  /// Use this to touch JavaScript from another thread:
  ///
  /// ```
  /// ctx.PostTask([](js::Context& live) {
  ///   live["progress"] = 0.5;
  /// });
  /// ```
  ///
  /// @param  task  The function to run. It can take the live js::Context or nothing.
  ///
  /// \parblock
  /// @note  Safe to call from any thread. Tasks run in the order they were posted.
  /// \endparblock
  ///
  /// \parblock
  /// @note  If the context goes away before the task runs, the task is destroyed without running.
  /// \endparblock
  ///
  template <typename F>
  void PostTask(F&& task) const {
    using Fn = std::decay_t<F>;
    static_assert(std::is_invocable_v<Fn&> || std::is_invocable_v<Fn&, Context&>,
                  "js::Context::PostTask takes a callable invocable with nothing or with "
                  "js::Context&");
    ulJSContextPostTask(handle_, &TaskThunk<Fn>, new Fn(std::forward<F>(task)),
                        &DeleteTaskCallable<Fn>);
  }

  ///
  /// Create a pending Promise and the Resolver that settles it, to hand a Promise to the page
  /// outside a bound function:
  ///
  /// ```
  /// auto [promise, resolver] = ctx.MakePromise();
  /// ctx["ready"] = promise;
  /// // later, from any thread:
  /// resolver.Resolve(std::string("ok"));
  /// ```
  ///
  /// @return  Returns the Promise and its Resolver (both empty if the context is gone).
  ///
  PendingPromise MakePromise() const {
    ULJSPromiseResolver raw = nullptr;
    ULJSValue promise = handle_ ? ulCreateJSPromise(handle_, &raw) : nullptr;
    if (!promise)
      return {};
    return PendingPromise { Value::Adopt(promise), Resolver::Adopt(raw) };
  }

  ///
  /// Create an ArrayBuffer that shares a Buffer's bytes with JavaScript without copying.
  ///
  /// The ArrayBuffer keeps a reference to `buffer` until it's garbage collected or detached
  /// (see "Byte Lifetimes" on js::ArrayBuffer).
  ///
  /// @param  buffer  The buffer to share. An empty buffer gives an ordinary empty ArrayBuffer
  ///                 that keeps no reference.
  ///
  /// @return  Returns a new ArrayBuffer over the buffer's bytes (an empty wrapper if `buffer`
  ///          is null or the context is gone).
  ///
  ArrayBuffer MakeArrayBuffer(const RefPtr<ultralight::Buffer>& buffer) const {
    if (!handle_)
      return ArrayBuffer();
    return ArrayBuffer(
        Value::Adopt(detail::WrapBufferThrough(handle_, buffer, [&](ULBuffer shared) {
          return ulCreateJSArrayBufferFromBuffer(handle_, shared);
        })));
  }

  ///
  /// Create an ArrayBuffer holding a copy of the given bytes.
  ///
  /// @param  bytes   The bytes to copy (may be NULL only when `length` is 0).
  ///
  /// @param  length  The number of bytes to copy (0 creates an empty ArrayBuffer).
  ///
  /// @return  Returns a new ArrayBuffer (an empty wrapper if `bytes` is NULL and `length`
  ///          isn't 0, or if the context is gone).
  ///
  ArrayBuffer MakeArrayBuffer(const void* bytes, size_t length) const {
    return ArrayBuffer(Value::Adopt(ulCreateJSArrayBuffer(handle_, bytes, length)));
  }

  ///
  /// Create a typed array of zero-filled elements of type `T` (eg, `MakeTypedArray<float>(4)`
  /// creates a Float32Array; see js::TypedArray for the element types).
  ///
  /// @param  length  The number of elements (not bytes).
  ///
  /// @return  Returns a new TypedArray (an empty wrapper if the context is gone).
  ///
  template <typename T>
  TypedArray<T> MakeTypedArray(size_t length) const {
    return TypedArray<T>(
        Value::Adopt(ulCreateJSTypedArray(handle_, detail::kTypedArrayTypeOf<T>, length)));
  }

  ///
  /// Create a typed array of elements of type `T` that shares a Buffer's bytes with JavaScript
  /// without copying.
  ///
  /// Lifetime works as for MakeArrayBuffer().
  ///
  /// @param  buffer  The buffer to share (its size must be a multiple of `sizeof(T)`). An empty
  ///                 buffer gives an ordinary empty typed array that keeps no reference.
  ///
  /// @return  Returns a new TypedArray over the buffer's bytes (an empty wrapper if `buffer`
  ///          is null, its size isn't a multiple of `sizeof(T)`, or the context is gone).
  ///
  template <typename T>
  TypedArray<T> MakeTypedArray(const RefPtr<ultralight::Buffer>& buffer) const {
    if (!handle_)
      return TypedArray<T>();
    return TypedArray<T>(
        Value::Adopt(detail::WrapBufferThrough(handle_, buffer, [&](ULBuffer shared) {
          return ulCreateJSTypedArrayFromBuffer(handle_, detail::kTypedArrayTypeOf<T>, shared);
        })));
  }

  ///
  /// Create a JavaScript function backed by a C++ callable, to hand to the page (eg, as a
  /// callback). Arguments convert the same way as for a js::API binding:
  ///
  /// ```
  /// js::Value cb = ctx.MakeFunction("onTick", [](double dt) { ... });
  /// ```
  ///
  /// @param  name  The function's `name`, also used in its conversion errors.
  ///
  /// @param  fn    The callable.
  ///
  /// @return  Returns the function (an empty Value if the context is gone).
  ///
  /// \parblock
  /// @note  Only synchronous callables work here (returning a value or a js::Result). Bind async
  ///        functions (a js::Task or a trailing js::Resolver) through js::API.
  /// \endparblock
  ///
  /// \parblock
  /// @note  The callable is destroyed on the Renderer's thread after the function is garbage
  ///        collected.
  /// \endparblock
  ///
  template <typename Fn>
  Value MakeFunction(const char* name, Fn&& fn) const {
    using F = std::decay_t<Fn>;
    using Traits = detail::CallableTraits<F>;
    using Shape = detail::ParamShape<typename Traits::Params>;
    static_assert(!Shape::kHasResolver
                      && !detail::kIsAsyncReturn<typename Traits::Return>,
                  "MakeFunction binds synchronous callables; bind async forms (js::Task, "
                  "trailing js::Resolver) through js::API");
    [&]<size_t... Is>(std::index_sequence<Is...>) {
      (detail::ValidateParam<std::tuple_element_t<Is, typename Shape::Converted>>(), ...);
    }(std::make_index_sequence<Shape::kCount> {});
    if (!handle_)
      return Value();
    auto* bound = new detail::BoundCallable<F> { std::forward<Fn>(fn),
                                                 std::string(name ? name : ""), {}, {} };
    return Value::Adopt(ulCreateJSFunction(handle_, name, &detail::SyncTrampoline<F>, bound,
                                           &detail::DestroyBoundCallable<F>));
  }

  ///
  /// Count the live handles on this context, for finding leaks.
  ///
  /// Value handles you never destroy keep counting after the page goes away.
  ///
  /// @return  Returns the counts (all zero for an empty Context).
  ///
  /// @see  HandleStats
  ///
  HandleStats GetHandleStats() const {
    HandleStats stats = {};
    ULJSHandleStats raw_stats = {};
    if (handle_ && ulJSContextGetHandleStats(handle_, &raw_stats)) {
      stats.total_values = raw_stats.total_values;
      for (size_t i = 0; i < kULJSType_Count; i++)
        stats.values_by_type[i] = raw_stats.values_by_type[i];
      stats.weak_refs = raw_stats.weak_refs;
    }
    return stats;
  }

  ///
  /// Get the C API handle without transferring ownership.
  ///
  /// @return  Returns the handle.
  ///
  ULJSContext raw() const { return handle_; }

  ///
  /// Give up ownership of the C API handle and return it.
  ///
  /// @return  Returns the handle. You must call ulDestroyJSContext() when finished.
  ///
  ULJSContext LeakRef() {
    ULJSContext handle = handle_;
    handle_ = nullptr;
    return handle;
  }

 private:
  explicit Context(ULJSContext handle) : handle_(handle) {}

  template <typename Fn>
  static void TaskThunk(void* user_data, ULJSContext ctx) {
    Fn& fn = *static_cast<Fn*>(user_data);
    // The task runs inside the library's update: an exception must not unwind into it.
    ultralight::detail::CallCatchingExceptions(
        [&] {
          if constexpr (std::is_invocable_v<Fn&, Context&>) {
            Context borrowed = Context::FromBorrowed(ctx);
            fn(borrowed);
          } else {
            fn();
          }
        },
        [](const char* what) { detail::LogNativeException("js::Context::PostTask", what); });
  }

  template <typename Fn>
  static void DeleteTaskCallable(void* user_data) {
    delete static_cast<Fn*>(user_data);
  }

  ULJSContext handle_ = nullptr;
};

inline Context CallInfo::context() const {
  return Context::FromBorrowed(ctx_);
}

}  // namespace js
}  // namespace ultralight
