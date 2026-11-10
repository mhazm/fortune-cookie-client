///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
// X11's headers define None as a macro, which would break the None enumerators.
#pragma push_macro("None")
#undef None

#include <Ultralight/CAPI/CAPI_JSAPI.h>
#include <Ultralight/CAPI/CAPI_String.h>
#include <Ultralight/DiagnosticsLevel.h>
#include <Ultralight/OriginRules.h>
#include <Ultralight/RefPtr.h>
#include <Ultralight/View.h>
#include <Ultralight/detail/InjectionFilter.h>
#include <Ultralight/js/CallInfo.h>
#include <Ultralight/js/Error.h>
#include <Ultralight/js/Resolver.h>
#include <Ultralight/js/TypeTraits.h>
#include <Ultralight/js/Value.h>
#include <Ultralight/js/detail/Binding.h>
#include <Ultralight/js/detail/Json.h>
#include <Ultralight/js/detail/Schema.h>

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace ultralight {
namespace js {

template <typename T>
class ClassBuilder;

///
/// The flags for API::AttachTo() (the C++ form of ULJSAPIAttachFlags).
///
enum class AttachFlags : unsigned {
  None = 0,             ///< Main frame only, with frozen namespace objects.
  AllFrames = 1u << 1,  ///< Also add the bindings to subframes.
  Mutable = 1u << 2,    ///< Don't freeze the namespace objects (page script can change or
                        ///< replace the bindings).
};

///
/// Combine attach flags.
///
/// @return  Returns the union of both flag sets.
///
constexpr AttachFlags operator|(AttachFlags a, AttachFlags b) {
  return static_cast<AttachFlags>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
}

///
/// Add attach flags to `a`.
///
/// @return  Returns `a`.
///
constexpr AttachFlags& operator|=(AttachFlags& a, AttachFlags b) {
  return a = a | b;
}

///
/// Shorthand for AttachFlags::AllFrames.
///
inline constexpr AttachFlags AllFrames = AttachFlags::AllFrames;

///
/// Shorthand for AttachFlags::Mutable.
///
inline constexpr AttachFlags Mutable = AttachFlags::Mutable;

static_assert(static_cast<unsigned>(AttachFlags::AllFrames) == kULJSAPIAttachFlags_AllFrames
                  && static_cast<unsigned>(AttachFlags::Mutable) == kULJSAPIAttachFlags_Mutable,
              "js::AttachFlags must mirror ULJSAPIAttachFlags");

///
/// Options for API::AttachTo().
///
/// ```
/// api.AttachTo(view.get(), { .flags = js::AllFrames,
///                            .origin_rules = { "https://*.mygame.com" } });
/// ```
///
struct AttachOptions {
  ///
  /// The attach flags (see AttachFlags).
  ///
  AttachFlags flags = AttachFlags::None;

  ///
  /// The origin rules for the pages that get the bindings. Leave it empty for the default
  /// policy (your application's own content). See ultralight::OriginRules.
  ///
  OriginRules origin_rules = {};
};

///
/// The diagnostics levels for an API.
///
/// Diagnostics are development warnings, written to the native logger and to the page's
/// console. Each level does everything the level below it does.
///
/// **Warn** reports mistakes that still run:
///
///  - **Typos**: reading a missing property of the API's namespace objects logs the closest
///    existing name (`myApp.addd is not defined; did you mean add?`).
///  - **Unknown events**: subscribing to or emitting an event the API never declared (checked
///    once the API declares at least one event, see DefineEvent()).
///  - **Lossy numbers**: a fractional or out-of-range argument truncated to an integer
///    parameter, or a 64-bit integer return value that a JavaScript number can't hold exactly.
///  - **Withheld bindings**: a page that the origin rules or the View's filter kept the
///    bindings from.
///  - **Reserved member names**: a member named `then`, `toJSON`, `constructor`, or
///    `__proto__`, which changes how JavaScript treats the object.
///
/// **Strict** also turns two caller mistakes into TypeErrors:
///
///  - **A lossy numeric argument** (error code `ULJS_LOSSY`).
///  - **More arguments than the binding declares** (error code `ULJS_EXTRA_ARGS`). A callable
///    that takes a CallInfo is exempt, since it reads the raw argument list.
///
/// A lossy return value still only warns, since the page can't fix a native return type.
///
/// **Auto** (the default) uses the JavaScript level in Config::diagnostics, which is Warn in
/// developer mode and Off otherwise (see DiagnosticsConfig).
///
/// Two warnings follow the Config's JavaScript level instead: an operation on a handle whose
/// page is gone, and a JavaScript exception that nothing read. Mistakes found once at
/// registration (an invalid or reserved root path, an invalid binding path, a missing callback)
/// always warn, whatever the level.
///
/// \parblock
/// @note  Warn and Strict make property reads on the API's namespace objects slower, and
///        attaching the API to a View also attaches the library's `ul.diagnostics` API.
/// \endparblock
///
/// \parblock
/// @note  The UL_JS_DIAGNOSTICS environment variable ("off", "warn", or "strict", any case,
///        read once per process) overrides the level of every API while developer mode is on,
///        so you can switch diagnostics without recompiling.
/// \endparblock
///
using Diagnostics = DiagnosticsLevel;

///
/// Options for creating an API.
///
/// ```
/// js::API api("myApp", { .diagnostics = js::Diagnostics::Strict });
/// ```
///
struct APIOptions {
  ///
  /// The diagnostics level.
  ///
  /// Diagnostics::Auto uses the JavaScript level in Config::diagnostics, so you can switch
  /// every API from one place.
  ///
  Diagnostics diagnostics = Diagnostics::Auto;
};

///
/// A parameter name for a registration (shown in API::schema() and in error messages).
///
/// API::Bind(), API::DefineEvent(), ClassBuilder::Method(), and ClassBuilder::StaticMethod()
/// accept these after the callable. Name every parameter (except a CallInfo or js::Resolver) or
/// none. The text is read during the call, so it doesn't need to outlive it.
///
/// ```
/// api.Bind("setConfig", [](double volume, bool muted) { ... },
///          js::Param("volume"), js::Param("muted"));
/// ```
///
/// A wrong call from the page then reports the names:
///
/// ```
/// myApp.setConfig('loud', true);
/// // TypeError: myApp.setConfig(volume: number, muted: boolean): argument 1 (volume):
/// //            expected number, got 'loud'
/// ```
///
/// \parblock
/// @note  The assignment form (`api["x"] = fn`) takes no annotations. Use Bind() to add them.
/// \endparblock
///
/// \parblock
/// @note  Don't confuse this with js::Arg, which is an argument value a callback reads.
/// \endparblock
///
/// @see  js::Doc
///
struct Param {
  ///
  /// The parameter name.
  ///
  const char* name;

  ///
  /// Create a parameter name annotation.
  ///
  /// @param  declared_name  The parameter name (NULL becomes an empty name).
  ///
  constexpr explicit Param(const char* declared_name)
      : name(declared_name ? declared_name : "") {}
};

///
/// A documentation string for a registration (shown in API::schema()).
///
/// The text becomes the entry's `doc` field in API::schema(). Pass at most one, to the same calls
/// that take js::Param. The text is read during the call, so it doesn't need to outlive it.
///
/// ```
/// api.Bind("save", &SaveGame, js::Doc("Saves the current session to disk."));
/// ```
///
struct Doc {
  ///
  /// The documentation text.
  ///
  const char* text;

  ///
  /// Create a documentation annotation.
  ///
  /// @param  doc_text  The documentation text.
  ///
  constexpr explicit Doc(const char* doc_text) : text(doc_text) {}
};

namespace detail {

// The trailing annotation pack accepted by the explicit registration forms. A concept
// (rather than accepting anything) keeps receiver overloads unambiguous.
template <typename A>
concept Annotation = std::is_same_v<std::remove_cvref_t<A>, Param>
    || std::is_same_v<std::remove_cvref_t<A>, Doc>;

template <typename... Anns>
inline constexpr size_t kArgAnnotationCount
    = ((std::is_same_v<std::remove_cvref_t<Anns>, Param> ? size_t { 1 } : size_t { 0 }) + ...
       + size_t { 0 });

template <typename... Anns>
inline constexpr size_t kDocAnnotationCount
    = ((std::is_same_v<std::remove_cvref_t<Anns>, Doc> ? size_t { 1 } : size_t { 0 }) + ...
       + size_t { 0 });

inline void CollectAnnotation(AnnotationSet& set, const Param& annotation) {
  if (set.name_count < AnnotationSet::kMaxNames)
    set.names[set.name_count++] = annotation.name;
}

inline void CollectAnnotation(AnnotationSet& set, const Doc& annotation) {
  set.doc = annotation.text;
}

template <typename... Anns>
AnnotationSet CollectAnnotations(const Anns&... annotations) {
  AnnotationSet set;
  (CollectAnnotation(set, annotations), ...);
  return set;
}

}  // namespace detail

///
/// A global JavaScript namespace for native functions and data.
///
/// A js::API gives page scripts access to your application's functions and data, much like browsers
/// add built-in Web APIs such as `console` and `document` on top of core JavaScript.
///
/// Once attached to a View, the API provides its global namespace to your pages.
///
/// Define an API with a constant and a function, attach it to a View, and load a page:
///
/// ```
/// js::API app("app");
/// app["version"] = "2.1.0";
/// app["add"] = [](double a, double b) { return a + b; };
///
/// if (app.AttachTo(view.get()))
///   view->LoadURL("file:///app.html");
/// ```
///
/// Page script accesses the namespace on `window`:
///
/// ```js
/// console.log(app.version);  // "2.1.0"
/// app.add(2, 3);             // 5
/// ```
///
/// ## Binding Functions and Values
///
/// The C++ type you bind decides what the page gets:
///
/// | Binding           | What the Page Gets             |
/// |-------------------|--------------------------------|
/// | Callable          | Function                       |
/// | Plain value       | Read-only constant             |
/// | Getter and setter | Live property                  |
/// | Async callable    | Function returning a Promise   |
/// | Bound class       | Class constructible with `new` |
///
/// A dot in a path string (such as `"fs.readFile"`) creates a child namespace object on the page.
///
/// Bind nested paths using dot notation and live properties using getters and setters:
///
/// ```
/// app["fs.readFile"] = [](std::string path) { return ReadFile(path); };
/// app.BindProperty("volume", [] { return g_volume; },
///                  [](double v) { g_volume = v; });
/// ```
///
/// ## Attaching to a View
///
/// Call AttachTo() with a View before loading content. Every page the View loads from then on gets
/// the API automatically, so you don't need to attach again after a navigation.
///
/// If you add, replace, or remove bindings after a page is loaded, that page keeps its current
/// bindings until its next navigation. To adjust frame scope or mutability, pass AttachOptions to
/// AttachTo() (see AttachFlags).
///
/// ## Allowed Pages and Origin Rules
///
/// By default, only local `file://` pages and content loaded with View::LoadHTML() get the API.
///
/// Passing origin rules replaces the default policy completely. Only pages matching your rules get
/// the API, so you must include `"file://*"` if you want local pages to retain access (see
/// ultralight::OriginRules). For decisions origin rules can't express (such as checking a user
/// setting at runtime), set a callback with js::SetInjectionFilter() to allow or withhold the API
/// per page.
///
/// Specify origin rules in AttachOptions to allow a local development server alongside local pages:
///
/// ```
/// // A local development server, plus your local pages:
/// if (app.AttachTo(view.get(),
///                  { .origin_rules = { "http://localhost:*", "file://*" } }))
///   view->LoadURL("http://localhost:5173/");
/// ```
///
/// ## Emitting Events
///
/// Native code can push events to attached pages without polling. Events are delivered on the
/// Renderer's thread during a later Renderer::Update().
///
/// Call Emit() from any thread with an event name and payload arguments:
///
/// ```
/// app.Emit("saved", std::string("slot1.dat"));
/// ```
///
/// Subscribe on the page using on() or once(), and remove listeners with off():
///
/// ```js
/// app.on("saved", (path) => refreshList(path));
/// ```
///
/// ## Managing API Lifetime
///
/// A View keeps an API attached only while the js::API instance exists. Destroying the js::API
/// detaches it from every View. Events stop reaching pages, and any calls to bound functions or
/// class constructors that a page still holds throw a TypeError with code `ULJS_DETACHED`.
///
/// Hold the API as a class member to tie its lifetime to an owner and bind member functions with
/// js::Bind():
///
/// ```
/// class Game {
///  public:
///   explicit Game(View* view) {
///     api_["showMenu"] = js::Bind(this, &Game::ShowMenu);
///     if (api_.AttachTo(view))
///       view->LoadURL("file:///app.html");
///   }
///
///   void ShowMenu();
///
///  private:
///   js::API api_{"app"};  // detaches from every View when the Game goes away
/// };
/// ```
///
/// @note  Once an API is attached to a View, you must bind and unbind only on the Renderer's
///        thread.
///
/// @see  ultralight::OriginRules, js::SetInjectionFilter(), js::BindingGuard, js::ClassBuilder,
///       js::TypeTraits, js::Diagnostics
///
class API {
 public:
  ///
  /// Create an API rooted at a global namespace path.
  ///
  /// @param  root_path  The dot-separated namespace path (eg, "myApp" or "myApp.native"). Every
  ///                    segment must be non-empty. The roots `ul` and `ultralight` (and every
  ///                    path under them) are reserved for the library.
  ///
  /// @param  options    The creation options (see APIOptions).
  ///
  /// @note  An invalid or reserved `root_path` creates an empty API (`operator bool` returns
  ///        false) and logs a warning saying why.
  ///
  explicit API(const char* root_path, const APIOptions& options = {})
      : root_(root_path ? root_path : ""), api_(ulCreateJSAPI(root_path)) {
    if (api_ && options.diagnostics != Diagnostics::Auto)
      ulJSAPISetDiagnostics(api_, static_cast<ULJSDiagnosticsLevel>(options.diagnostics));
  }

  ///
  /// Destructor (releases this handle).
  ///
  /// @note  Destroying the API detaches it from every View (see Managing API Lifetime in the class
  ///        overview), unless it came from FromBorrowed() or another owning handle to it exists.
  ///
  ~API() { ulDestroyJSAPI(api_); }

  ///
  /// Move constructor (`other` becomes empty).
  ///
  API(API&& other) noexcept : root_(std::move(other.root_)), api_(other.api_) {
    other.api_ = nullptr;
  }

  ///
  /// Move assignment (releases the current handle, and `other` becomes empty).
  ///
  API& operator=(API&& other) noexcept {
    if (this != &other) {
      ulDestroyJSAPI(api_);
      root_ = std::move(other.root_);
      api_ = other.api_;
      other.api_ = nullptr;
    }
    return *this;
  }
  API(const API&) = delete;
  API& operator=(const API&) = delete;

  // --- Interop with the C API (most embedders never touch raw handles) -------------------

  ///
  /// Wrap a handle you own from the C API, taking ownership of it (eg, the result of
  /// ulCreateJSAPI()).
  ///
  /// @param  api  The handle to take ownership of.
  ///
  /// @return  Returns an API that owns `api`.
  ///
  /// @note  For a handle someone else owns, use FromBorrowed().
  ///
  static API Adopt(ULJSAPI api) { return API(api); }

  ///
  /// Wrap a handle someone else owns, without owning the API (eg, the `api` member of a
  /// js::InjectionRequest).
  ///
  /// The result works like any API, but destroying it never detaches the API, and it doesn't
  /// keep the API attached once its owners are gone.
  ///
  /// @param  api  The borrowed handle.
  ///
  /// @return  Returns an API with its own (non-owning) reference to `api`. Its raw() is a
  ///          different handle than `api`.
  ///
  /// @note  For a handle you own, use Adopt().
  ///
  static API FromBorrowed(ULJSAPI api) { return API(ulCreateJSAPIBorrowedRef(api)); }

  ///
  /// Whether or not this API holds a handle (false if the root path was invalid or reserved, and
  /// after a move or LeakRef()).
  ///
  explicit operator bool() const { return api_ != nullptr; }

  ///
  /// The assignable path that operator[] returns (see Binding Functions and Values in the class
  /// overview).
  ///
  /// @warning  Use a Binder only in the expression that created it (`api["path"] = ...`). It
  ///           can't be copied, and a named Binder can't bind, since the API could move or be
  ///           destroyed while you hold it. Keep the path string instead.
  ///
  class Binder {
   public:
    Binder(const Binder&) = delete;
    Binder& operator=(const Binder&) = delete;

    ///
    /// Bind a callable or value at this path (the type of `rhs` decides what's bound, see Binding
    /// Functions and Values in the API class overview).
    ///
    /// @param  rhs  The callable or value to bind.
    ///
    /// @return  Returns this Binder.
    ///
    template <typename T>
    Binder& operator=(T&& rhs) && {
      using U = std::decay_t<T>;
      if constexpr (detail::IsIntrospectableCallable<U>::value) {
        api_->Bind(path_.c_str(), std::forward<T>(rhs));
      } else if constexpr (Holder<U>) {
        // A holder of a bound class exposes the INSTANCE: a read-only live property whose
        // every read returns the instance's wrapper (one wrapper per page, so `===` holds).
        static_assert(HolderTraits<U>::kind == HolderKind::Shared,
                      "an exclusive holder cannot back a live instance binding (every page "
                      "read needs its own stake): bind a shared holder such as RefPtr<T>, "
                      "std::shared_ptr<T>, or js::Eternal<T>");
        static_assert(Marshalable<U>,
                      "bind the class first: include <Ultralight/js/Class.h> and call "
                      "DefineClass<T>() before exposing an instance of it");
        api_->BindProperty(path_.c_str(), [value = U(std::forward<T>(rhs))] { return value; });
      } else if constexpr (LockableHolder<U>) {
        static_assert(detail::kAlwaysFalse<U>,
                      "a weak holder cannot back a live instance binding (every page read "
                      "needs a strong ownership stake): bind a shared holder such as "
                      "RefPtr<T>, or bind a method with js::Bind(weak, &T::Method)");
      } else if constexpr (std::is_null_pointer_v<U>) {
        static_assert(detail::kAlwaysFalse<U>,
                      "nullptr is not a JavaScript value: use js::null for a JavaScript null");
      } else if constexpr (detail::kIsJsonConstant<U>
                           || std::is_convertible_v<U, std::string_view>) {
        api_->SetConstant(path_.c_str(), std::forward<T>(rhs));
      } else if constexpr (Marshalable<U>) {
        static_assert(detail::kAlwaysFalse<U>,
                      "this type cannot be a constant: constants are serialized to JSON "
                      "once at registration (no live context exists yet), and JSON cannot "
                      "represent it. Bind a function or property instead (js::undefined "
                      "has no JSON form; use js::null)");
      } else {
        static_assert(detail::kAlwaysFalse<U>,
                      "this value can be neither bound nor stored as a constant. Callables "
                      "must have exactly one non-template operator() (generic lambdas with "
                      "'auto' parameters cannot be introspected); constants must be one of "
                      "the JSON-serializable types (see API::SetConstant())");
      }
      return *this;
    }

    ///
    /// Get the Binder for a child path (`api["fs"]["readFile"]` is `api["fs.readFile"]`).
    ///
    /// @param  segment  The child path segment.
    ///
    /// @return  Returns the Binder for the combined path.
    ///
    Binder operator[](const char* segment) && {
      return Binder(api_, path_ + "." + (segment ? segment : ""));
    }

    ///
    /// Bind a property with a getter (and optional setter) at this path (see
    /// API::BindProperty()):
    ///
    /// ```
    /// api["player"]["health"].BindProperty([&] { return player.health; });
    /// ```
    ///
    /// @param  get  The getter, called on every JavaScript read.
    ///
    /// @param  set  The setter, or nullptr for a read-only property.
    ///
    /// @note  Assigning a callable always binds a function the page calls with parentheses,
    ///        even one that takes no arguments. Use this to bind a property instead.
    ///
    template <typename Getter, typename Setter = std::nullptr_t>
    void BindProperty(Getter&& get, Setter&& set = nullptr) && {
      api_->BindProperty(path_.c_str(), std::forward<Getter>(get),
                         std::forward<Setter>(set));
    }

   private:
    friend class API;
    Binder(API* api, std::string path) : api_(api), path_(std::move(path)) {}
    API* api_;
    std::string path_;
  };

  ///
  /// Get the Binder for a path relative to the root (see Binder).
  ///
  /// @param  path  The dot-separated path (eg, "greet" or "fs.readFile").
  ///
  /// @return  Returns the Binder for `path`.
  ///
  Binder operator[](const char* path) { return Binder(this, path ? path : ""); }

  ///
  /// Bind a callable at a path (the explicit form of `api[path] = fn`, which also takes
  /// annotations).
  ///
  /// @param  path         The dot-separated path relative to the root (eg, "fs.readFile").
  ///
  /// @param  fn           The callable to bind (see Binding Functions and Values in the class
  ///                      overview).
  ///
  /// @param  annotations  Optional js::Param names (one per parameter, or none) and at most one
  ///                      js::Doc.
  ///
  /// @note  Binding a path that's already bound replaces the old binding and removes the path's
  ///        metadata (so call SetMetadata() after binding).
  ///
  template <typename Fn, typename... Anns>
    requires(detail::Annotation<Anns> && ...)
  void Bind(const char* path, Fn&& fn, const Anns&... annotations) {
    using F = std::decay_t<Fn>;
    using Traits = detail::CallableTraits<F>;
    using Shape = detail::ParamShape<typename Traits::Params>;
    using R = typename Traits::Return;

    [&]<size_t... Is>(std::index_sequence<Is...>) {
      (detail::ValidateParam<std::tuple_element_t<Is, typename Shape::Converted>>(), ...);
    }(std::make_index_sequence<Shape::kCount> {});
    constexpr size_t kNames = detail::kArgAnnotationCount<Anns...>;
    static_assert(kNames == 0 || kNames == Shape::kCount,
                  "js::Param annotations must name every payload parameter or none (one "
                  "Param per converted parameter; CallInfo and Resolver take no name)");
    static_assert(detail::kDocAnnotationCount<Anns...> <= 1,
                  "at most one js::Doc annotation per registration");

    detail::AnnotationSet set = detail::CollectAnnotations(annotations...);
    auto* bound
        = new detail::BoundCallable<F> { std::forward<Fn>(fn), FullPath(path), {}, {} };
    bound->display
        = bound->path + detail::RenderSignature<typename Shape::Converted>(set);
    bound->names.assign(set.names, set.names + set.name_count);
    if constexpr (detail::HasSettableBoundPath<F>)
      bound->fn.SetBoundPath(bound->path);
    bool bound_ok;
    if constexpr (Shape::kHasResolver || detail::kIsAsyncReturn<R>) {
      bound_ok = ulJSAPIBindAsyncFunction(api_, path, &detail::AsyncTrampoline<F>, bound,
                                          &detail::DestroyBoundCallable<F>);
    } else {
      bound_ok = ulJSAPIBindFunction(api_, path, &detail::SyncTrampoline<F>, bound,
                                     &detail::DestroyBoundCallable<F>);
    }
    if (!bound_ok)
      return;

    detail::SchemaShapes shapes;
    std::string fragment
        = detail::BuildCallableFragment<typename Shape::Converted, R, Shape::kHasResolver>(
            shapes, set);
    ulJSAPISetMetadata(api_, path, fragment.c_str());
    detail::EmitSchemaShapes(api_, shapes);
  }

  ///
  /// Bind a member function of an object at a path (the same as binding
  /// `js::Bind(receiver, method)`):
  ///
  /// ```
  /// api.Bind("save", this, &App::OnSave);
  /// ```
  ///
  /// @param  path         The dot-separated path relative to the root (eg, "fs.readFile").
  ///
  /// @param  receiver     The object to call `method` on. It must outlive every call through the
  ///                      binding.
  ///
  /// @param  method       The member function pointer.
  ///
  /// @param  annotations  Optional js::Param names (one per parameter, or none) and at most one
  ///                      js::Doc.
  ///
  /// @note  To have the binding keep the object alive, bind `js::Bind(holder, method)` with a
  ///        shared holder instead.
  ///
  template <typename T, typename M, typename... Anns>
    requires(std::is_member_function_pointer_v<M> && (detail::Annotation<Anns> && ...))
  void Bind(const char* path, T* receiver, M method, const Anns&... annotations) {
    Bind(path, detail::MakeBoundMethod(receiver, method, detail::MemberFnTraits<M> {}),
         annotations...);
  }

  ///
  /// Set a constant at a path (the explicit form of `api[path] = value`).
  ///
  /// The value is copied when you call this, and every page gets that copy. Changing your C++
  /// value later doesn't change the constant (use BindProperty() for a live value). Page script
  /// can't modify a constant, and objects and arrays in it are frozen too.
  ///
  /// @param  path   The dot-separated path relative to the root.
  ///
  /// @param  value  The value (see the supported types below).
  ///
  /// \parblock
  /// @note  Constants support the JSON-serializable types: numbers, booleans, strings, js::null,
  ///        std::optional, std::vector, std::map<std::string, T>, and (when
  ///        ULTRALIGHT_REFLECTION is 1) enums and plain aggregate structs of these.
  ///        js::undefined has no JSON form, so use js::null instead.
  /// \endparblock
  ///
  /// \parblock
  /// @note  Structs convert field by field, so a custom js::TypeTraits specialization doesn't
  ///        apply to a constant. Bind a function or property for a custom conversion.
  /// \endparblock
  ///
  /// \parblock
  /// @note  A null `const char*` binds `null` (and logs a diagnostics warning). For `null` on
  ///        purpose, pass js::null.
  /// \endparblock
  ///
  template <typename T>
  void SetConstant(const char* path, T&& value) {
    using U = std::decay_t<T>;
    if constexpr (std::is_same_v<U, const char*> || std::is_same_v<U, char*>) {
      if (!value) {
        ulJSAPISetConstantJSON(api_, path, "null");
        if (ulJSAPIGetDiagnostics(api_) >= kULJSDiagnostics_Warn) {
          std::string message = FullPath(path)
              + ": the constant's string is a null pointer, so it was bound as null.";
          ulJSAPIEmitDiagnostic(nullptr, message.c_str());
        }
        return;
      }
    }
    if constexpr (std::is_null_pointer_v<U>) {
      static_assert(detail::kAlwaysFalse<U>,
                    "nullptr is not a JavaScript value: use js::null for a JavaScript null");
    } else if constexpr (std::is_same_v<U, bool>) {
      ulJSAPISetConstantBoolean(api_, path, value);
    } else if constexpr (std::is_arithmetic_v<U>) {
      bool set = ulJSAPISetConstantNumber(api_, path, static_cast<double>(value));
      // The generated constant entry says "number"; an integral C++ source refines it.
      if constexpr (std::is_integral_v<U>) {
        if (set)
          ulJSAPISetMetadata(api_, path, "{\"type\":\"integer\"}");
      }
    } else if constexpr (std::is_convertible_v<U, std::string_view>) {
      std::string text { std::string_view(value) };
      ulJSAPISetConstantString(api_, path, text.c_str());
    } else {
      std::string json;
      detail::WriteJson(json, value);
      bool set = ulJSAPISetConstantJSON(api_, path, json.c_str());
      // A JSON constant's generated entry carries the parsed value but no type; the typed
      // layer knows it (js::null stays untyped: "null" is not a schema expression).
      if constexpr (!std::is_same_v<U, NullType>) {
        if (!set)
          return;
        detail::SchemaShapes shapes;
        std::string fragment = detail::BuildTypeFragment<U>(shapes);
        ulJSAPISetMetadata(api_, path, fragment.c_str());
        detail::EmitSchemaShapes(api_, shapes);
      }
    }
  }

  ///
  /// Define a native class at a path, so page script can call its methods, read its properties,
  /// and construct it with `new` (when you declare a constructor).
  ///
  /// ```
  /// api.DefineClass<Database>("Database")
  ///    .Constructor<std::string>()
  ///    .Method("query", &Database::Query);
  /// ```
  ///
  /// @param  path  The dot-separated path relative to the root. The first DefineClass() for a
  ///               type also takes the JavaScript class name from the last path segment
  ///               ("db.Database" gives a class called "Database").
  ///
  /// @return  Returns the builder to declare members on. The class is registered when the
  ///          builder goes out of scope (normally at the end of the statement).
  ///
  /// @note  Requires `<Ultralight/js/Class.h>`. A type binds to one class for the life of the
  ///        process, so a later DefineClass() for the same type reuses that class and its
  ///        members (see ClassBuilder).
  ///
  /// @see  ClassBuilder
  ///
  template <typename T>
  ClassBuilder<T> DefineClass(const char* path);

  ///
  /// Bind a property with a getter and an optional setter.
  ///
  /// Unlike a constant, the property is live: every JavaScript read calls `get`, and every
  /// assignment calls `set`.
  ///
  /// ```
  /// api.BindProperty("volume", [] { return g_volume; },
  ///                  [](double v) { g_volume = v; });
  /// // Page:  myApp.volume        -> calls the getter
  /// //        myApp.volume = 0.5  -> calls the setter
  /// ```
  ///
  /// @param  path  The dot-separated path relative to the root.
  ///
  /// @param  get   The getter. It takes no parameters and returns a type js::TypeTraits
  ///               converts, or a js::Result<T> to throw an error into the page.
  ///
  /// @param  set   The setter, which takes one parameter of a type js::TypeTraits converts
  ///               (return a js::Result<void> to reject the assignment with an error). Pass
  ///               nullptr for a read-only property: assigning to it then throws in strict-mode
  ///               code and does nothing otherwise.
  ///
  /// @note  Assigning a value of the wrong type throws the standard TypeError, and the setter
  ///        isn't called.
  ///
  template <typename Getter, typename Setter = std::nullptr_t>
  void BindProperty(const char* path, Getter&& get, Setter&& set = nullptr) {
    using G = std::decay_t<Getter>;
    using GTraits = detail::CallableTraits<G>;
    static_assert(std::tuple_size_v<typename GTraits::Params> == 0,
                  "a property getter takes no parameters");
    static_assert(!std::is_void_v<typename GTraits::Return>,
                  "a property getter must return a value (a Marshalable type or "
                  "js::Result<T>)");
    if constexpr (std::is_same_v<std::decay_t<Setter>, std::nullptr_t>) {
      struct ReadOnly {
        void operator()() const {}
      };
      using Bound = detail::BoundProperty<G, ReadOnly>;
      auto* bound = new Bound { std::forward<Getter>(get), ReadOnly {}, FullPath(path) };
      if constexpr (detail::HasSettableBoundPath<G>)
        bound->get.SetBoundPath(bound->path);
      if (!ulJSAPIBindProperty(api_, path, &detail::PropertyGetterThunk<Bound>, nullptr, bound,
                               &detail::DestroyBoundProperty<Bound>))
        return;
    } else {
      using S = std::decay_t<Setter>;
      using Bound = detail::BoundProperty<G, S>;
      auto* bound = new Bound { std::forward<Getter>(get), std::forward<Setter>(set),
                                FullPath(path) };
      if constexpr (detail::HasSettableBoundPath<G>)
        bound->get.SetBoundPath(bound->path);
      if constexpr (detail::HasSettableBoundPath<S>)
        bound->set.SetBoundPath(bound->path);
      if (!ulJSAPIBindProperty(api_, path, &detail::PropertyGetterThunk<Bound>,
                               &detail::PropertySetterThunk<Bound>, bound,
                               &detail::DestroyBoundProperty<Bound>))
        return;
    }

    detail::SchemaShapes shapes;
    std::string fragment = detail::BuildTypeFragment<typename GTraits::Return>(shapes);
    ulJSAPISetMetadata(api_, path, fragment.c_str());
    detail::EmitSchemaShapes(api_, shapes);
  }

  ///
  /// Emit an event to the page listeners subscribed with `on` or `once` (see Emitting Events in
  /// the class overview).
  ///
  /// ```
  /// api.Emit("saved", std::string("slot1.dat"));
  /// // Page:  myApp.on('saved', (path) => { ... });
  /// ```
  ///
  /// @param  event_path  The event path relative to the root (eg, "saved" or "fs.changed").
  ///                     A page can subscribe to "fs.changed" on the root with the full path
  ///                     (`myApp.on('fs.changed', fn)`), or as `myApp.fs.on('changed', fn)`
  ///                     when something is bound under `fs` (only then does `myApp.fs` exist).
  ///
  /// @param  args        Up to 16 argument values of types js::TypeTraits converts.
  ///
  /// \parblock
  /// @note  Safe to call from any thread. The arguments are copied here (strings included), so
  ///        nothing you pass needs to outlive the call.
  /// \endparblock
  ///
  /// \parblock
  /// @note  The event is delivered later, on the Renderer's thread: during a later
  ///        Renderer::Update(), the listeners run once in each page that had the bindings when
  ///        you called Emit() (each frame, for an API attached with AllFrames).
  /// \endparblock
  ///
  /// @see  dom::Element::dispatchEvent()
  ///
  template <typename... Args>
    requires(Marshalable<detail::CapturedArg<Args>> && ...)
  void Emit(const char* event_path, Args&&... args) {
    static_assert(sizeof...(Args) <= 16, "events support at most 16 arguments");
    static_assert((!detail::kIsClassInstancePointer<Args> && ...),
                  "a raw instance pointer cannot cross a deferred boundary (delivery "
                  "happens later, on the Renderer's thread): capture an owning holder (the "
                  "class's canonical holder) or js::Eternal<T> instead");
    static_assert((!detail::IsCallScopedView<std::decay_t<Args>>::value && ...),
                  "a std::span cannot cross a deferred boundary (delivery happens later; "
                  "the view would dangle): copy into a std::vector<E> or RefPtr<Buffer>");
    static_assert((!std::is_base_of_v<Value, std::decay_t<Args>> && ...),
                  "a js::Value cannot be emitted (Emit delivers to every attached page, and "
                  "a value belongs to one page): emit native values instead, such as "
                  "numbers, strings, containers, or structs. A js::ArrayBuffer or "
                  "js::TypedArray from the page can't be emitted either: emit a "
                  "RefPtr<Buffer> or the bytes (eg, a std::vector<uint8_t>)");
    using Payload = std::tuple<detail::CapturedArg<Args>...>;
    auto* payload = new Payload(detail::CaptureArg(std::forward<Args>(args))...);
    ulJSAPIEmitEvent(api_, event_path, &detail::EmitArgsThunk<Payload>, payload,
                     &detail::DeleteEmitPayload<Payload>);
  }

  ///
  /// Declare an event and its payload types.
  ///
  /// Declared events appear in schema(), and the generated TypeScript declarations give them
  /// typed `on`, `once`, and `off` overloads. The declaration only describes the event, so Emit()
  /// doesn't check its arguments against the declared types.
  ///
  /// ```
  /// api.DefineEvent<std::string>("saved", js::Param("path"), js::Doc("A save finished."));
  /// api.Emit("saved", path);  // Page:  myApp.on('saved', (path) => ...)
  /// ```
  ///
  /// @param  event_path   The event path relative to the root, in the form Emit() takes.
  ///
  /// @param  annotations  Optional js::Param names (one per payload type, or none) and at most
  ///                      one js::Doc.
  ///
  /// \parblock
  /// @note  Once an API declares an event, diagnostics (Warn and Strict) warn about every
  ///        subscription and every Emit() for an event it didn't declare. You should declare
  ///        every event the API emits.
  /// \endparblock
  ///
  /// \parblock
  /// @note  Declaring an event again replaces its payload types. Declarations are permanent
  ///        (Unbind() never removes them).
  /// \endparblock
  ///
  /// \parblock
  /// @note  This follows the registration thread rules (see the class overview).
  /// \endparblock
  ///
  template <typename... Args, typename... Anns>
    requires(detail::Annotation<Anns> && ...)
  void DefineEvent(const char* event_path, const Anns&... annotations) {
    static_assert(sizeof...(Args) <= 16, "events support at most 16 arguments");
    static_assert((Marshalable<std::remove_cvref_t<Args>> && ...),
                  "an event payload type has no js::TypeTraits: use a supported built-in, "
                  "make it a plain aggregate struct, or specialize js::TypeTraits for it");
    constexpr size_t kNames = detail::kArgAnnotationCount<Anns...>;
    static_assert(kNames == 0 || kNames == sizeof...(Args),
                  "js::Param annotations must name every event parameter or none");
    static_assert(detail::kDocAnnotationCount<Anns...> <= 1,
                  "at most one js::Doc annotation per registration");
    detail::AnnotationSet set = detail::CollectAnnotations(annotations...);
    detail::SchemaShapes shapes;
    std::string fragment = detail::BuildEventFragment<Args...>(shapes, set, event_path);
    ulJSAPISetMetadata(api_, "", fragment.c_str());
    detail::EmitSchemaShapes(api_, shapes);
  }

  ///
  /// Remove the binding at a path, along with its metadata.
  ///
  /// Pages see the removal at their next navigation. A page that's already loaded (or restored
  /// from the back-forward cache) keeps the binding, and functions the page got from it keep
  /// working. A js::Task one of them already started finishes normally. The library destroys
  /// the bound callable once the last of those functions and Tasks is gone (right away if
  /// there's none).
  ///
  /// To tie a binding to a C++ object's lifetime instead, see js::BindingGuard.
  ///
  /// @param  path  The binding's path relative to the root, as you bound it.
  ///
  /// @return  Returns whether a binding or metadata entry was removed (false for an empty path
  ///          or a path with neither). You can ignore it, since removing an absent path is
  ///          harmless.
  ///
  /// \parblock
  /// @note  Event declarations and root metadata (the top-level `meta`, `events`, and `types`
  ///        sections) are never removed.
  /// \endparblock
  ///
  /// \parblock
  /// @note  This follows the registration thread rules (see the class overview).
  /// \endparblock
  ///
  bool Unbind(const char* path) { return ulJSAPIUnbind(api_, path); }

  ///
  /// Get the API's schema as JSON.
  ///
  /// The JSON has a `format` ("ul-schema"), an `api` ("js"), and a `version`, then an entry for
  /// each binding with its parameter and return types, the named types they use, and the
  /// metadata you added (js::Param, js::Doc, and SetMetadata()).
  ///
  /// A reader should ignore keys and types it doesn't know. The `version` changes only for a
  /// change that would break an existing reader.
  ///
  /// @return  Returns the JSON text (an empty string for an empty API).
  ///
  /// @note  Safe to call from any thread.
  ///
  std::string schema() const {
    ULString text = ulJSAPIGetSchema(api_);
    std::string result
        = text ? std::string(ulStringGetData(text), ulStringGetLength(text)) : std::string();
    ulDestroyString(text);
    return result;
  }

  ///
  /// Write the JSON from schema() to a file.
  ///
  /// The library writes the file before this returns. A tool such as `gen-typescript.py` can
  /// then read it.
  ///
  /// @param  utf8_path  The file's path as a UTF-8 string (on Windows too). An existing file is
  ///                    replaced.
  ///
  /// @return  Returns whether or not the file was written (false for an empty API, or a NULL or
  ///          empty `utf8_path`). A write that fails also logs a warning with the path.
  ///
  /// @note  Safe to call from any thread.
  ///
  bool DumpSchema(const char* utf8_path) { return ulJSAPIDumpSchema(api_, utf8_path); }

  ///
  /// Add schema metadata (eg, documentation) to the entry at a path.
  ///
  /// schema() merges each fragment over the entry: objects merge field by field, other values
  /// replace, and later fragments win.
  ///
  /// Binding a path again (replacing its binding) removes that path's metadata, so call this
  /// after the Bind() it describes.
  ///
  /// ```
  /// api.SetMetadata("add", "{\"doc\": \"Adds two numbers.\"}");
  /// ```
  ///
  /// @param  path           The dot-separated path relative to the root, or "" for the document
  ///                        root (its top-level `meta`, `events`, and `types` sections). Root
  ///                        `events` entries declare events the way DefineEvent() does.
  ///
  /// @param  metadata_json  A JSON object. Anything else is ignored with a logged warning.
  ///
  /// @note  This follows the registration thread rules (see the class overview).
  ///
  void SetMetadata(const char* path, const char* metadata_json) {
    ulJSAPISetMetadata(api_, path, metadata_json);
  }

  ///
  /// Set the diagnostics level (see Diagnostics).
  ///
  /// Most warnings follow the new level right away. Typo warnings (reads of missing properties)
  /// start only on pages that get the bindings after you turn them on, but stop right away when
  /// you set Diagnostics::Off.
  ///
  /// @param  level  The new level (Diagnostics::Auto goes back to the Config's JavaScript
  ///                level).
  ///
  /// \parblock
  /// @note  You should set the level before attaching the API to a View (or pass it in
  ///        APIOptions), since attaching at Warn or higher is what also attaches the library's
  ///        `ul.diagnostics` API. Raising the level afterward doesn't add it.
  /// \endparblock
  ///
  /// \parblock
  /// @note  The UL_JS_DIAGNOSTICS environment variable overrides this level while developer
  ///        mode is on (see diagnostics()).
  /// \endparblock
  ///
  /// \parblock
  /// @note  This follows the registration thread rules (see the class overview).
  /// \endparblock
  ///
  void set_diagnostics(Diagnostics level) {
    ulJSAPISetDiagnostics(api_, static_cast<ULJSDiagnosticsLevel>(level));
  }

  ///
  /// Get the effective diagnostics level.
  ///
  /// @return  Returns the level in effect: the UL_JS_DIAGNOSTICS environment override when it
  ///          applies, else the level set on this API, with Diagnostics::Auto replaced by the
  ///          Config's JavaScript level (this is never Diagnostics::Auto).
  ///
  Diagnostics diagnostics() const {
    return static_cast<Diagnostics>(ulJSAPIGetDiagnostics(api_));
  }

  ///
  /// Attach this API to a View.
  ///
  /// The bindings are added to the View's current page (if the origin policy allows it) and to
  /// every page the View loads afterwards. The View keeps the API attached until you detach it
  /// or destroy this API. Attaching again updates the flags and origin rules.
  ///
  /// ```
  /// api.AttachTo(view.get());                                                // your own content
  /// api.AttachTo(view.get(), { .flags = js::AllFrames });                    // and subframes
  /// api.AttachTo(view.get(), { .origin_rules = { "https://*.mygame.com" } });  // the game's site
  /// ```
  ///
  /// @param  view     The View to attach to.
  ///
  /// @param  options  The attach flags and the origin rules for the pages that get the bindings
  ///                  (see AttachOptions).
  ///
  /// @return  Returns true on success, or false if `view` is nullptr, this API is empty, a flag
  ///          is unknown, a rule failed to parse, or every owning handle to the API is gone (for
  ///          an API from FromBorrowed()). Nothing changes then. An unknown flag, a rule that
  ///          failed to parse, or a destroyed API also logs a warning that says why.
  ///
  /// \parblock
  /// @note  Call this on the Renderer's thread.
  /// \endparblock
  ///
  /// \parblock
  /// @note  APIs that share a namespace prefix (eg, roots `myApp.fs` and `myApp.net`) should all
  ///        be attached before the page loads. An API attached later can't add to a frozen
  ///        namespace the page already has, so its bindings appear after the page's next
  ///        navigation.
  /// \endparblock
  ///
  [[nodiscard]] bool AttachTo(View* view, const AttachOptions& options = {}) {
    return view && api_
        && view->AttachJSAPI(api_, static_cast<unsigned>(options.flags),
                             options.origin_rules.data(), options.origin_rules.size());
  }

  ///
  /// Detach this API from a View.
  ///
  /// Events stop reaching the View's pages right away. The current page keeps the bindings it
  /// already has until it navigates (destroying the API instead makes them throw).
  ///
  /// @param  view  The View to detach from.
  ///
  /// @note  Call this on the Renderer's thread.
  ///
  void DetachFrom(View* view) {
    if (view && api_)
      view->DetachJSAPI(api_);
  }

  ///
  /// Get the underlying handle, for calls into the C API.
  ///
  /// @return  Returns the handle (this API keeps ownership).
  ///
  ULJSAPI raw() const { return api_; }

  ///
  /// Give up ownership of the underlying handle without releasing it (this API becomes empty).
  ///
  /// @return  Returns the handle. You must call ulDestroyJSAPI() when finished.
  ///
  ULJSAPI LeakRef() {
    ULJSAPI api = api_;
    api_ = nullptr;
    return api;
  }

 private:
  // The Adopt / FromBorrowed route: wrap an existing handle and recover its root path so
  // path-qualified diagnostics keep working.
  explicit API(ULJSAPI adopted) : api_(adopted) {
    if (!api_)
      return;
    ULString root = ulJSAPIGetRootPath(api_);
    if (root) {
      root_.assign(ulStringGetData(root), ulStringGetLength(root));
      ulDestroyString(root);
    }
  }

  std::string FullPath(const char* path) const {
    std::string full = root_;
    full += '.';
    full += path ? path : "";
    return full;
  }

  std::string root_;
  ULJSAPI api_;
};

///
/// A binding tied to a C++ object's lifetime.
///
/// When an armed guard is destroyed, it removes the binding at its path (see API::Unbind()), so
/// a feature can keep a binding for exactly as long as its owner lives:
///
/// ```
/// api.Bind("debug.dump", [] { DumpState(); });
/// js::BindingGuard dump_guard(api, "debug.dump");
/// // ~dump_guard removes debug.dump (pages see the removal at their next navigation).
/// ```
///
/// The guard keeps its own reference to the API, so it stays safe to destroy after the js::API
/// that created it. It doesn't own the API, so destroying the js::API still detaches it. The
/// guard removes by path: if you bind something else at the path later, the guard removes that.
///
/// \parblock
/// @note  Destroying or move-assigning over an armed guard removes the binding, so it follows
///        the registration thread rules (see the js::API class overview): once the API is
///        attached, do it on the Renderer's thread. To give up a guard on another thread, call
///        Release().
/// \endparblock
///
/// \parblock
/// @note  `operator bool` only says whether the guard is armed. It never reflects whether the
///        binding or any page still exists.
/// \endparblock
///
class [[nodiscard]] BindingGuard {
 public:
  ///
  /// Create an unarmed guard (removes nothing).
  ///
  BindingGuard() = default;

  ///
  /// Create an armed guard that removes the binding at `path` when destroyed.
  ///
  /// @param  api   The API the binding belongs to (an empty API gives an unarmed guard).
  ///
  /// @param  path  The binding's path relative to the root, as you bound it.
  ///
  BindingGuard(const API& api, const char* path)
      : api_(ulCreateJSAPIBorrowedRef(api.raw())), path_(path ? path : "") {}

  ///
  /// Destructor (removes the binding if the guard is armed).
  ///
  ~BindingGuard() {
    if (api_) {
      ulJSAPIUnbind(api_, path_.c_str());
      ulDestroyJSAPI(api_);
    }
  }

  ///
  /// Move constructor (`other` becomes unarmed).
  ///
  BindingGuard(BindingGuard&& other) noexcept
      : api_(other.api_), path_(std::move(other.path_)) {
    other.api_ = nullptr;
  }

  ///
  /// Move assignment (removes the current binding first if armed, and `other` becomes unarmed).
  ///
  BindingGuard& operator=(BindingGuard&& other) noexcept {
    if (this != &other) {
      if (api_) {
        ulJSAPIUnbind(api_, path_.c_str());
        ulDestroyJSAPI(api_);
      }
      api_ = other.api_;
      path_ = std::move(other.path_);
      other.api_ = nullptr;
    }
    return *this;
  }
  BindingGuard(const BindingGuard&) = delete;
  BindingGuard& operator=(const BindingGuard&) = delete;

  ///
  /// Disarm the guard without removing the binding (it stays until you unbind it or the API is
  /// destroyed).
  ///
  /// Safe to call from any thread.
  ///
  void Release() {
    if (api_) {
      ulDestroyJSAPI(api_);
      api_ = nullptr;
    }
    path_.clear();
  }

  ///
  /// Whether or not the guard is armed (holds a path to remove).
  ///
  explicit operator bool() const { return api_ != nullptr; }

 private:
  ULJSAPI api_ = nullptr;
  std::string path_;
};

///
/// The details an API injection filter decides on (see SetInjectionFilter()).
///
/// New members may be added at the end in later versions, so read the members by name.
///
struct InjectionRequest {
  ///
  /// The View loading the page (the View you set the filter on).
  ///
  View* view;

  ///
  /// The API being decided on. It's the handle your API's raw() returns, so compare the two to
  /// tell your APIs apart. Wrap it with API::FromBorrowed() to use it as an API.
  ///
  ULJSAPI api;

  ///
  /// The page's security origin, serialized (eg, "https://example.com", or "null" for an opaque
  /// origin).
  ///
  const char* origin;

  ///
  /// Whether or not the page is in the View's main frame.
  ///
  bool is_main_frame;

  ///
  /// Whether or not the origin rules allow the page.
  ///
  bool rules_allow;
};

}  // namespace js

/// \cond INTERNAL
namespace detail {
template <>
struct InjectionRequestTraits<ULJSAPIInjectionRequest> {
  static js::InjectionRequest ToRequest(View* view, const ULJSAPIInjectionRequest& request) {
    return { view, request.api, request.origin, request.is_main_frame, request.rules_allow };
  }
  static void ReportException(const char* what) {
    std::string message = "js::SetInjectionFilter: the filter threw a C++ exception, so the "
                          "JavaScript API was withheld from the page";
    if (what) {
      message += ": ";
      message += what;
    }
    ulJSAPIEmitDiagnostic(nullptr, message.c_str());
  }
};
}  // namespace detail
/// \endcond

namespace js {

///
/// Set a View's API filter with a C++ callable (the typed form of
/// View::SetJSAPIInjectionFilter()).
///
/// The View calls the filter each time it's about to add an attached API's bindings to a page:
/// when a page loads, when you attach an API, and when a page returns from the back-forward
/// cache. The origin rules run first and their verdict arrives in `rules_allow`. Return true to
/// add the bindings or false to withhold them, whatever the rules decided. Use it for decisions
/// that origin rules can't express, such as a user setting or allowing a page whose origin is
/// opaque.
///
/// ```
/// js::SetInjectionFilter(view.get(), [&](const js::InjectionRequest& request) {
///   if (request.api == debug_api.raw())
///     return settings.debug_tools_enabled && request.rules_allow;
///   return request.rules_allow;
/// });
/// ```
///
/// @param  view    The View to set the filter on (nothing happens if it's nullptr).
///
/// @param  filter  A callable invocable as `bool(const js::InjectionRequest&)`. The new filter
///                 replaces (and destroys) the previous one.
///
/// \parblock
/// @note  Call this on the Renderer's thread. The filter runs there too.
/// \endparblock
///
/// \parblock
/// @note  The filter is called for subframes only for APIs attached with AllFrames. The request
///        is valid only during the call.
/// \endparblock
///
/// \parblock
/// @note  A filter that throws a C++ exception withholds the API from that page (and the
///        library logs a warning).
/// \endparblock
///
/// @see  ClearInjectionFilter()
///
template <typename F>
void SetInjectionFilter(View* view, F&& filter) {
  using Fn = std::decay_t<F>;
  static_assert(std::is_invocable_r_v<bool, Fn&, const InjectionRequest&>,
                "js::SetInjectionFilter takes a callable invocable as "
                "bool(const js::InjectionRequest&)");
  if (!view)
    return;
  using Adapter = ultralight::detail::InjectionFilterAdapter<Fn, ULJSAPIInjectionRequest>;
  view->SetJSAPIInjectionFilter(&Adapter::Invoke, new Adapter { std::forward<F>(filter), view },
                                &Adapter::Destroy);
}

///
/// Remove a View's API filter, so the origin rules alone decide which pages get the bindings.
///
/// @param  view  The View to clear the filter on (nothing happens if it's nullptr).
///
/// @note  Call this on the Renderer's thread.
///
inline void ClearInjectionFilter(View* view) {
  if (view)
    view->SetJSAPIInjectionFilter(nullptr, nullptr, nullptr);
}

///
/// Bind a member function to an instance without writing a lambda.
///
/// How you pass the instance decides who keeps it alive:
///
/// ```
/// api["save"] = js::Bind(this, &App::OnSave);            // raw pointer: you do
/// api["query"] = js::Bind(database_, &Database::Query);  // shared holder: the binding does
/// api["tick"] = js::Bind(std::weak_ptr(widget_),         // weak holder: nobody, and calls
///                        &Widget::OnTick);               // fail safely once it's gone
/// ```
///
/// The result is an ordinary callable that you can bind anywhere a lambda works. A method that
/// returns js::Task<T> or takes a trailing js::Resolver binds as an async function in every
/// form.
///
/// @param  instance  The object to call the method on. It must outlive every call through the
///                   binding.
///
/// @param  method    The member function pointer.
///
/// @return  Returns a callable that calls `method` on `instance`.
///
template <typename T, typename M>
  requires std::is_member_function_pointer_v<M>
auto Bind(T* instance, M method) {
  return detail::MakeBoundMethod(instance, method, detail::MemberFnTraits<M> {});
}

///
/// Bind a member function to an instance kept by a holder.
///
/// Any smart pointer with an ultralight::HolderTraits specialization works, built-in or your own:
///
///  - **A shared holder** (RefPtr, std::shared_ptr, js::Eternal, or your own shared
///    specialization) is kept by the binding, so the instance lives as long as the binding.
///  - **A weak holder** (WeakPtr, std::weak_ptr, or your own specialization with Lock()) is
///    locked for each call and never keeps the instance alive between calls.
///
/// A method that returns js::Task<T> keeps the instance alive until its Task finishes, in both
/// forms (the weak form keeps the locked holder), even if the binding is removed meanwhile. A
/// method that takes a trailing js::Resolver binds in both forms too, but the weak form keeps
/// the instance only during the call, not until the Resolver settles.
///
/// @param  holder  The holder to call the method through (copied into the binding).
///
/// @param  method  The member function pointer.
///
/// @return  Returns a callable that calls `method` on the held instance.
///
/// \parblock
/// @note  When the instance is gone (an empty shared holder, or a weak holder whose object was
///        destroyed), a call throws a TypeError with code `ULJS_DETACHED` into the page instead
///        of touching the missing object. An async binding rejects its Promise with that error.
/// \endparblock
///
/// \parblock
/// @note  std::unique_ptr isn't accepted, since passing one would move ownership into the
///        binding. Keep ownership and bind `js::Bind(ptr.get(), &T::Method)` instead.
/// \endparblock
///
template <typename H, typename M>
  requires((Holder<H> || LockableHolder<H>) && std::is_member_function_pointer_v<M>)
auto Bind(H holder, M method) {
  using T = typename HolderTraits<H>::element_type;
  static_assert(std::is_base_of_v<typename detail::MemberFnTraits<M>::Class, T>,
                "the member pointer must belong to the holder's element type (or a base "
                "of it)");
  if constexpr (Holder<H> && !LockableHolder<H>) {
    static_assert(HolderTraits<H>::kind == HolderKind::Shared,
                  "js::Bind does not take exclusive holders: passing a unique_ptr would "
                  "silently move ownership into the binding. Keep ownership and borrow "
                  "with js::Bind(ptr.get(), &T::Method), or bind a shared holder");
  }
  return detail::MakeHolderBoundMethod<H, T>(std::move(holder), method,
                                             detail::MemberFnTraits<M> {});
}

}  // namespace js
}  // namespace ultralight

#pragma pop_macro("None")
