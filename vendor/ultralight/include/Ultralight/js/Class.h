///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/CAPI/CAPI_JSClass.h>
#include <Ultralight/js/API.h>
#include <Ultralight/js/Holder.h>
#include <Ultralight/js/detail/ClassBinding.h>

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

namespace ultralight {
namespace js {

///
/// Select one overload of a member function for ClassBuilder::Method().
///
/// Write the overload's signature as the template argument:
///
/// ```
/// builder.Method("query", js::Overload<Row(const std::string&)>(&Database::Query));
/// builder.Method("size", js::Overload<size_t() const>(&Database::Size));
/// ```
///
/// @param  member  The overloaded member function.
///
/// @return  Returns the member-function pointer for that overload.
///
template <typename Signature, typename C>
constexpr auto Overload(Signature C::* member) {
  return member;
}

///
/// Converts a pointer to a bound class as a borrowed instance (you keep ownership).
///
/// As a return value it gives the page the instance's wrapper without giving it ownership. As a
/// parameter it gives you the instance for the duration of the call. NULL converts to `null`
/// and back. Passing a detached instance throws a TypeError with code `ULJS_DETACHED`.
///
/// @note  API::Emit() and Resolver::Resolve() don't accept raw instance pointers, because they
///        deliver later. Pass the class's canonical holder instead (see js::ClassHolder).
///
/// @see  js::ClassBuilder, js::TypeTraits
///
template <typename T>
struct TypeTraits<T*, std::enable_if_t<std::is_class_v<T>>> {
  static bool FromJS(ULJSContext ctx, ULJSValue value, T** out, ULJSValue* exception) {
    return detail::UnwrapInstance<T>(ctx, value, out, exception);
  }
  static ULJSValue ToJS(ULJSContext ctx, T* const& value) {
    return detail::WrapView(ctx, value);
  }
  static constexpr const char* SchemaType() { return detail::ClassSchemaName<T>(); }
};

///
/// Converts a bound class's canonical holder (see js::ClassHolder) with ownership.
///
/// Returning the holder makes the instance's wrapper an owner of the instance:
///
///  - **Exclusive holders** (std::unique_ptr) move ownership to the wrapper and leave your
///    holder empty, even when it's passed by const reference. One can't be a parameter, since
///    the page can't give up ownership through an argument. Take a `T*` instead, or take
///    ownership back with js::Detach().
///  - **Shared holders** (RefPtr, std::shared_ptr, js::Eternal) add the wrapper as another
///    owner. As a parameter you get a holder that shares ownership with the wrapper.
///
/// @note  A std::shared_ptr (or custom shared holder) parameter only accepts a wrapper that owns
///        its instance. A wrapper created from a `T*` return throws a TypeError there.
///
/// @see  js::ClassBuilder, ultralight::HolderTraits
///
template <typename H>
struct TypeTraits<H, std::enable_if_t<detail::kIsCanonicalHolder<H>>> {
 private:
  using Traits = HolderTraits<H>;
  using T = typename Traits::element_type;

 public:
  static bool FromJS(ULJSContext ctx, ULJSValue value, H* out, ULJSValue* exception) {
    static_assert(Traits::kind == HolderKind::Shared,
                  "an exclusive holder cannot be a parameter: JavaScript cannot relinquish "
                  "ownership through an argument. Borrow with T*, or take ownership "
                  "explicitly with js::Detach<T>()");
    T* instance = nullptr;
    if (!detail::UnwrapInstance<T>(ctx, value, &instance, exception))
      return false;
    if (!instance) {
      *out = H {};
      return true;
    }
    if constexpr (std::is_same_v<H, RefPtr<T>> || std::is_same_v<H, Eternal<T>>) {
      *out = H(instance);  // A new stake is constructible from the instance itself.
      return true;
    } else {
      // The stake must be copied from the wrapper's own holder (it cannot be reconstructed
      // from the instance pointer), so the wrapper must be bridge-owned. The magic check
      // keeps a holder attached through the C API from being misread as a typed stake.
      void* raw = ulJSObjectGetInstanceHolder(value, detail::BoundClass<T>());
      if (!raw || static_cast<detail::StakeBox<H>*>(raw)->magic != detail::kStakeBoxMagic) {
        if (exception) {
          std::string name = detail::ClassSchemaName<T>();
          std::string message = "this " + name
              + " has no shared owner this parameter can copy (declare the parameter as "
              + name + "* to borrow the instance)";
          *exception = Error::TypeError(message.c_str()).ToJS(ctx);
        }
        return false;
      }
      *out = static_cast<detail::StakeBox<H>*>(raw)->holder;
      return true;
    }
  }

  static ULJSValue ToJS(ULJSContext ctx, const H& value) {
    if constexpr (Traits::kind == HolderKind::Exclusive)
      return detail::WrapOwned<H>(ctx, std::move(const_cast<H&>(value)));
    else
      return detail::WrapOwned<H>(ctx, H(value));
  }

  static constexpr const char* SchemaType() { return detail::ClassSchemaName<T>(); }
};

///
/// Builder for exposing a C++ class to JavaScript.
///
/// Calling API::DefineClass() returns a ClassBuilder to bind a C++ class under an API namespace,
/// giving page scripts real objects backed by your C++ instances. Scripts can create instances with
/// `new` or receive them from bound functions.
///
/// Chain member declarations on the builder to define the class:
///
/// ```
/// class Database {
///  public:
///   explicit Database(std::string file);
///
///   std::string Query(std::string sql);
///   double timeout = 30;
///
///   int page_size() const;
///   void set_page_size(int bytes);
/// };
///
/// app.DefineClass<Database>("Database")
///     .Constructor<std::string>()
///     .Method("query", &Database::Query)
///     .Field("timeout", &Database::timeout)
///     .Property("pageSize", &Database::page_size, &Database::set_page_size);
/// ```
///
/// Page scripts can instantiate the class and call its methods:
///
/// ```js
/// const db = new app.Database("save.db");
/// db.timeout = 60;
/// const rows = db.query("SELECT * FROM saves");
///
/// db.query(42);  // TypeError: app.Database.query(string): argument 1:
///                //            expected string, got number
/// ```
///
/// ## Class Registration
///
/// The class registers when the ClassBuilder is destroyed, which usually happens at the end of the
/// statement.
///
/// The library registers each C++ class once per process. A later call to API::DefineClass() for
/// the same C++ type attaches the existing definition to another path and can't add or alter
/// members.
///
/// If you omit Constructor(), calling `new` on the page throws a TypeError with code
/// `ULJS_NO_CTOR`. This lets you require callers to obtain instances through static factory methods
/// bound with StaticMethod() or through functions that return an instance.
///
/// ## Instance Ownership
///
/// An instance crosses to JavaScript through either an owning holder or a borrowed pointer:
///
/// - **An owned instance uses the canonical holder.** The holder type is configured by specializing
///   js::ClassHolder, defaulting to `RefPtr<T>` for RefCounted types and `std::unique_ptr<T>`
///   otherwise.
/// - **A raw pointer borrows the instance.** Returning a `T*` gives the page a wrapper without
///   transferring ownership. You must keep the native object alive while pages can call into it, or
///   detach its wrapper before freeing it.
///
/// Functions bound to an API can return either owned instances or borrowed pointers:
///
/// ```
/// class Enemy {
///  public:
///   double health = 100;
/// };
///
/// void RegisterEnemyAPI(js::API& api, Enemy& boss) {
///   api.DefineClass<Enemy>("Enemy").Field("health", &Enemy::health);
///
///   // The page owns each new Enemy (std::unique_ptr is the default holder).
///   api["spawnEnemy"] = [] { return std::make_unique<Enemy>(); };
///
///   // A borrow: the page gets `boss`, and you keep ownership.
///   api["boss"] = [&boss]() -> Enemy* { return &boss; };
/// }
/// ```
///
/// The library releases owned instances on the Renderer's thread during a later Renderer::Update()
/// when their wrapper is garbage-collected or when their page navigates away.
///
/// @note  Navigating away releases owned instances even when the page enters the back-forward
///        cache. Calls on those instances after the page is restored throw a TypeError with code
///        `ULJS_DETACHED`.
///
/// ## Closing an Instance
///
/// JavaScript garbage collection runs unpredictably. Any class that manages a scarce resource, such
/// as an open file or network socket, should provide an explicit close method built on
/// js::Detach().
///
/// Calling js::Detach() on a wrapper releases the native instance and returns the canonical holder,
/// freeing the instance when that holder is destroyed. Later JavaScript calls on that detached
/// wrapper throw a TypeError with code `ULJS_DETACHED`.
///
/// Implement an explicit close method by detaching the wrapper during a method call:
///
/// ```
/// class Connection {
///  public:
///   void Disconnect(js::CallInfo info) {
///     js::Detach<Connection>(info.this_value().ToValue());  // frees this
///     // Don't touch members after the Detach.
///   }
/// };
///
/// void RegisterConnection(js::API& api) {
///   api.DefineClass<Connection>("Connection")
///       .Constructor<>()
///       .Method("disconnect", &Connection::Disconnect);
/// }
/// ```
///
/// Subsequent calls on the disconnected wrapper fail with a TypeError:
///
/// ```js
/// const conn = new app.Connection();
/// conn.disconnect();
/// conn.disconnect();  // TypeError with code "ULJS_DETACHED"
/// ```
///
/// Detaching an instance follows specific lifetime rules:
///
/// - **Pending async calls delay instance destruction.** If an async method on the instance is
///   still running when js::Detach() is called, the wrapper detaches immediately but js::Detach()
///   returns an empty holder. The library keeps the instance alive until that call settles,
///   releasing it during a later Renderer::Update().
/// - **Borrowed instances must be detached across all pages.** Because each page creates its own
///   wrapper for a borrowed instance, you must call js::Detach() on every page's wrapper before
///   freeing the native object.
///
/// ## Ownership Cycles
///
/// A native instance that stores a js::Value pointing back to its own JavaScript wrapper creates a
/// reference cycle that prevents garbage collection while the page is alive. To break the cycle,
/// reset the stored handle in an explicit close method, or store a js::WeakValue instead.
///
/// @see  js::API::DefineClass(), js::ClassHolder, js::Detach(), js::Eternal, js::Overload()
///
template <typename T>
class ClassBuilder {
 public:
  ///
  /// The class's canonical holder type (see js::ClassHolder).
  ///
  using HolderType = typename ClassHolder<std::remove_cv_t<T>>::type;

  ClassBuilder(ClassBuilder&& other) noexcept
      : api_(other.api_), cls_(other.cls_), path_(std::move(other.path_)),
        full_path_(std::move(other.full_path_)), shapes_(std::move(other.shapes_)),
        ctor_(std::move(other.ctor_)), methods_(std::move(other.methods_)),
        properties_(std::move(other.properties_)), statics_(std::move(other.statics_)) {
    other.api_ = nullptr;
  }
  ClassBuilder(const ClassBuilder&) = delete;
  ClassBuilder& operator=(const ClassBuilder&) = delete;
  ClassBuilder& operator=(ClassBuilder&&) = delete;

  ~ClassBuilder() {
    if (api_ && cls_ && ulJSAPIRegisterClass(api_, path_.c_str(), cls_))
      EmitShapeEnrichment();
  }

  ///
  /// Declare the constructor's parameter types.
  ///
  /// `new` converts its arguments to these types (throwing a TypeError on a mismatch), then
  /// constructs the instance into the class's canonical holder, owned by the new wrapper.
  ///
  /// @return  Returns this builder, so calls chain.
  ///
  /// @note  This works only when the canonical holder is RefPtr, std::unique_ptr, or
  ///        std::shared_ptr. For another holder (js::Eternal included), bind a StaticMethod()
  ///        that returns the holder instead.
  ///
  template <typename... Args>
  ClassBuilder& Constructor() {
    (detail::ValidateParam<Args>(), ...);
    static_assert(std::is_constructible_v<std::remove_cv_t<T>, detail::ParamStorage<Args>&&...>,
                  "T is not constructible from these argument types");
    if (cls_) {
      auto* bound = new detail::ClassCtor<T, Args...> { full_path_ };
      if (ulJSClassSetConstructor(cls_, &detail::ClassCtor<T, Args...>::Invoke, bound,
                                  &detail::DestroyPayload<detail::ClassCtor<T, Args...>>))
        ctor_ = detail::BuildCallableFragment<std::tuple<Args...>, void, true>(shapes_);
    }
    return *this;
  }

  ///
  /// Add an instance method.
  ///
  /// Arguments and the return value convert like those of a function bound with API::Bind().
  /// A method that returns a js::Task or takes a trailing js::Resolver returns a Promise to the
  /// page.
  ///
  /// @param  name         The method's name in JavaScript.
  ///
  /// @param  method       A member function of T (or of a base of T). Select one overload of an
  ///                      overloaded function with js::Overload().
  ///
  /// @param  annotations  Optional js::Param / js::Doc annotations for the parameter names and
  ///                      documentation (as on API::Bind()).
  ///
  /// @return  Returns this builder, so calls chain.
  ///
  template <typename M, typename... Anns>
    requires(std::is_member_function_pointer_v<M> && (detail::Annotation<Anns> && ...))
  ClassBuilder& Method(const char* name, M method, const Anns&... annotations) {
    using Traits = detail::MemberFnTraits<M>;
    static_assert(std::is_base_of_v<typename Traits::Class, T>,
                  "the member pointer must belong to T (or a base of T)");
    using Shape = detail::ParamShape<typename Traits::Params>;
    [&]<size_t... Is>(std::index_sequence<Is...>) {
      (detail::ValidateParam<std::tuple_element_t<Is, typename Shape::Converted>>(), ...);
    }(std::make_index_sequence<Shape::kCount> {});
    constexpr size_t kNames = detail::kArgAnnotationCount<Anns...>;
    static_assert(kNames == 0 || kNames == Shape::kCount,
                  "js::Param annotations must name every payload parameter or none");
    static_assert(detail::kDocAnnotationCount<Anns...> <= 1,
                  "at most one js::Doc annotation per registration");
    detail::AnnotationSet set = detail::CollectAnnotations(annotations...);
    if (cls_) {
      auto* bound
          = new detail::BoundClassMethod<T, M> { method, MemberPath(name), {}, {} };
      bound->display
          = bound->path + detail::RenderSignature<typename Shape::Converted>(set);
      bound->names.assign(set.names, set.names + set.name_count);
      if (!ulJSClassAddMethod(cls_, name, &detail::ClassMethodTrampoline<T, M>, bound,
                              &detail::DestroyPayload<detail::BoundClassMethod<T, M>>,
                              kULJSPropertyAttributes_DontEnum))
        return *this;
      using R = typename Traits::Return;
      methods_[name ? name : ""]
          = detail::BuildCallableFragment<typename Shape::Converted, R, Shape::kHasResolver,
                                          Shape::kHasResolver || detail::kIsAsyncReturn<R>>(
              shapes_, set);
    }
    return *this;
  }

  ///
  /// Expose a data member as a property.
  ///
  /// Reads return the member's current value. Assignments convert the value and store it
  /// (throwing a TypeError on a mismatch). A const member is read-only.
  ///
  /// @param  name    The property's name in JavaScript.
  ///
  /// @param  member  A pointer to a data member of T. For a member T inherits, convert the
  ///                 pointer first (eg, `static_cast<int Database::*>(&Base::count)`).
  ///
  /// @return  Returns this builder, so calls chain.
  ///
  template <typename F>
  ClassBuilder& Field(const char* name, F T::* member) {
    static_assert(Marshalable<std::remove_cv_t<F>>,
                  "this field type has no js::TypeTraits: use a supported built-in, make it "
                  "a plain aggregate struct, or specialize js::TypeTraits for it");
    static_assert(!detail::IsCallScopedView<std::remove_cv_t<F>>::value,
                  "a bound field cannot be a std::span (the instance would store a view "
                  "that dies with the bound call); use std::vector<E> or RefPtr<Buffer>");
    if (cls_) {
      auto* bound = new detail::BoundField<T, F> { member, MemberPath(name) };
      bool added;
      if constexpr (std::is_const_v<F>) {
        added = ulJSClassAddProperty(cls_, name, &detail::FieldGetterTrampoline<T, F>, nullptr,
                                     bound, &detail::DestroyPayload<detail::BoundField<T, F>>);
      } else {
        static_assert(detail::ConvertibleFromJS<detail::ParamStorage<F>>,
                      "this field type converts to JavaScript only; expose it through "
                      "Property() with a getter instead");
        added = ulJSClassAddProperty(cls_, name, &detail::FieldGetterTrampoline<T, F>,
                                     &detail::FieldSetterTrampoline<T, F>, bound,
                                     &detail::DestroyPayload<detail::BoundField<T, F>>);
      }
      if (added) {
        properties_[name ? name : ""]
            = detail::BuildTypeFragment<std::remove_cv_t<F>>(shapes_);
      }
    }
    return *this;
  }

  ///
  /// Expose a read-only property backed by a getter.
  ///
  /// @param  name    The property's name in JavaScript.
  ///
  /// @param  getter  A member function that takes no parameters and returns a convertible type
  ///                 (or a js::Result of one, whose error is thrown).
  ///
  /// @return  Returns this builder, so calls chain.
  ///
  template <typename GM>
    requires std::is_member_function_pointer_v<GM>
  ClassBuilder& Property(const char* name, GM getter) {
    return AddAccessor(name, getter, nullptr);
  }

  ///
  /// Expose a read-write property backed by a getter and a setter.
  ///
  /// @param  name    The property's name in JavaScript.
  ///
  /// @param  getter  A member function that takes no parameters and returns a convertible type
  ///                 (or a js::Result of one, whose error is thrown).
  ///
  /// @param  setter  A member function that takes one convertible parameter and returns void,
  ///                 or a js::Result<void> whose error is thrown to refuse the assignment (eg,
  ///                 a value out of range). Any other return type is a compile error.
  ///                 Assigning a value of the wrong type throws a TypeError.
  ///
  /// @return  Returns this builder, so calls chain.
  ///
  template <typename GM, typename SM>
    requires(std::is_member_function_pointer_v<GM> && std::is_member_function_pointer_v<SM>)
  ClassBuilder& Property(const char* name, GM getter, SM setter) {
    return AddAccessor(name, getter, setter);
  }

  ///
  /// Add a static method (a function on the constructor, eg, `myApp.Database.open(...)`).
  ///
  /// Any callable works, with the same conversions and async forms as API::Bind(). A static
  /// method that returns the canonical holder is the usual way to create instances of a class
  /// without a Constructor.
  ///
  /// @param  name         The method's name in JavaScript.
  ///
  /// @param  fn           The callable to bind. It must have exactly one non-template call
  ///                      signature.
  ///
  /// @param  annotations  Optional js::Param / js::Doc annotations for the parameter names and
  ///                      documentation (as on API::Bind()).
  ///
  /// @return  Returns this builder, so calls chain.
  ///
  template <typename Fn, typename... Anns>
    requires(detail::Annotation<Anns> && ...)
  ClassBuilder& StaticMethod(const char* name, Fn&& fn, const Anns&... annotations) {
    using F = std::decay_t<Fn>;
    static_assert(detail::IsIntrospectableCallable<F>::value,
                  "a static method must have exactly one non-template call signature "
                  "(generic lambdas cannot be introspected)");
    using Traits = detail::CallableTraits<F>;
    using Shape = detail::ParamShape<typename Traits::Params>;
    [&]<size_t... Is>(std::index_sequence<Is...>) {
      (detail::ValidateParam<std::tuple_element_t<Is, typename Shape::Converted>>(), ...);
    }(std::make_index_sequence<Shape::kCount> {});
    constexpr size_t kNames = detail::kArgAnnotationCount<Anns...>;
    static_assert(kNames == 0 || kNames == Shape::kCount,
                  "js::Param annotations must name every payload parameter or none");
    static_assert(detail::kDocAnnotationCount<Anns...> <= 1,
                  "at most one js::Doc annotation per registration");
    detail::AnnotationSet set = detail::CollectAnnotations(annotations...);
    if (cls_) {
      auto* bound
          = new detail::BoundCallable<F> { std::forward<Fn>(fn), MemberPath(name), {}, {} };
      bound->display
          = bound->path + detail::RenderSignature<typename Shape::Converted>(set);
      bound->names.assign(set.names, set.names + set.name_count);
      if constexpr (detail::HasSettableBoundPath<F>)
        bound->fn.SetBoundPath(bound->path);
      if (!ulJSClassAddStaticMethod(cls_, name, &detail::ClassStaticTrampoline<F>, bound,
                                    &detail::DestroyBoundCallable<F>))
        return *this;
      using R = typename detail::CallableTraits<F>::Return;
      statics_[name ? name : ""]
          = detail::BuildCallableFragment<typename Shape::Converted, R, Shape::kHasResolver,
                                          Shape::kHasResolver || detail::kIsAsyncReturn<R>>(
              shapes_, set);
    }
    return *this;
  }

  ///
  /// Add a static method that calls a member function on an object you own:
  ///
  /// ```
  /// api.DefineClass<Database>("Database")
  ///    .StaticMethod("defaultPath", settings, &Settings::DatabasePath);
  /// ```
  ///
  /// @param  name         The method's name in JavaScript.
  ///
  /// @param  receiver     The object the method is called on (borrowed, not copied).
  ///
  /// @param  method       A member function of the receiver's class (or of a base of it).
  ///
  /// @param  annotations  Optional js::Param / js::Doc annotations for the parameter names and
  ///                      documentation (as on API::Bind()).
  ///
  /// @return  Returns this builder, so calls chain.
  ///
  /// @warning  The receiver must stay alive for as long as any page can call the method, even
  ///           after API::Unbind() (class definitions are never removed). Pass a holder
  ///           instead (see the overload below) when you can't guarantee that.
  ///
  template <typename C, typename M, typename... Anns>
    requires(std::is_member_function_pointer_v<M> && !js::Holder<C>
             && !js::LockableHolder<C> && (detail::Annotation<Anns> && ...))
  ClassBuilder& StaticMethod(const char* name, C& receiver, M method,
                             const Anns&... annotations) {
    static_assert(std::is_base_of_v<typename detail::MemberFnTraits<M>::Class, C>,
                  "the member pointer must belong to the receiver's class (or a base of "
                  "it)");
    return StaticMethod(name, js::Bind(std::addressof(receiver), method), annotations...);
  }

  ///
  /// Add a static method that calls a member function through a holder (see js::Bind()).
  ///
  /// A shared holder keeps the receiver alive. A weak holder is locked for each call, and a
  /// call after the receiver is gone throws a TypeError with code `ULJS_DETACHED`.
  ///
  /// @param  name         The method's name in JavaScript.
  ///
  /// @param  holder       A shared or weak holder of the receiver (std::unique_ptr isn't
  ///                      accepted).
  ///
  /// @param  method       A member function of the holder's element type (or of a base of it).
  ///
  /// @param  annotations  Optional js::Param / js::Doc annotations for the parameter names and
  ///                      documentation (as on API::Bind()).
  ///
  /// @return  Returns this builder, so calls chain.
  ///
  template <typename H, typename M, typename... Anns>
    requires((js::Holder<H> || js::LockableHolder<H>)
             && std::is_member_function_pointer_v<M> && (detail::Annotation<Anns> && ...))
  ClassBuilder& StaticMethod(const char* name, H holder, M method,
                             const Anns&... annotations) {
    return StaticMethod(name, js::Bind(std::move(holder), method), annotations...);
  }

 private:
  friend class API;

  ClassBuilder(ULJSAPI api, ULJSClass cls, std::string full_path, std::string path)
      : api_(api), cls_(cls), path_(std::move(path)), full_path_(std::move(full_path)) {}

  std::string MemberPath(const char* name) const {
    return full_path_ + "." + (name ? name : "");
  }

  template <typename GM, typename SM>
  ClassBuilder& AddAccessor(const char* name, GM getter, SM setter) {
    using GTraits = detail::MemberFnTraits<GM>;
    static_assert(std::tuple_size_v<typename GTraits::Params> == 0,
                  "a property getter takes no parameters");
    static_assert(!std::is_void_v<typename GTraits::Return>,
                  "a property getter must return a value");
    if (!cls_)
      return *this;
    bool added;
    if constexpr (std::is_same_v<SM, std::nullptr_t>) {
      using Bound = detail::BoundAccessor<T, GM, std::nullptr_t>;
      auto* bound = new Bound { getter, nullptr, MemberPath(name) };
      added = ulJSClassAddProperty(cls_, name, &detail::AccessorGetterTrampoline<T, Bound>,
                                   nullptr, bound, &detail::DestroyPayload<Bound>);
    } else {
      using STraits = detail::MemberFnTraits<SM>;
      static_assert(std::tuple_size_v<typename STraits::Params> == 1,
                    "a property setter takes exactly one parameter");
      using Bound = detail::BoundAccessor<T, GM, SM>;
      auto* bound = new Bound { getter, setter, MemberPath(name) };
      added = ulJSClassAddProperty(cls_, name, &detail::AccessorGetterTrampoline<T, Bound>,
                                   &detail::AccessorSetterTrampoline<T, Bound>, bound,
                                   &detail::DestroyPayload<Bound>);
    }
    if (added) {
      properties_[name ? name : ""]
          = detail::BuildTypeFragment<typename GTraits::Return>(shapes_);
    }
    return *this;
  }

  // Emits one root fragment enriching this class's `types` entry with the typed member
  // signatures (the generated shape is skeletal: member names and readonly flags only),
  // plus any named shapes those signatures referenced. Fragments accumulate, so a later
  // DefineClass for an already-defined class that registers no members emits nothing.
  void EmitShapeEnrichment() {
    if (ctor_.empty() && methods_.empty() && properties_.empty() && statics_.empty())
      return;
    std::string shape_name = path_;
    std::replace(shape_name.begin(), shape_name.end(), '.', '_');
    std::string body = "{";
    bool first = true;
    if (!ctor_.empty()) {
      body += "\"ctor\":";
      body += ctor_;
      first = false;
    }
    auto add_section = [&](const char* key, const std::map<std::string, std::string>& members) {
      if (members.empty())
        return;
      if (!first)
        body += ',';
      first = false;
      body += '"';
      body += key;
      body += "\":{";
      bool member_first = true;
      for (const auto& [name, entry] : members) {
        if (!member_first)
          body += ',';
        member_first = false;
        detail::WriteJsonEscaped(body, name);
        body += ':';
        body += entry;
      }
      body += '}';
    };
    add_section("methods", methods_);
    add_section("properties", properties_);
    add_section("statics", statics_);
    body += '}';
    std::string fragment = "{\"types\":{";
    detail::WriteJsonEscaped(fragment, shape_name);
    fragment += ':';
    fragment += body;
    for (const auto& [name, shape] : shapes_.shapes) {
      if (name == shape_name)
        continue;  // A member type sharing the class's shape name would duplicate the key.
      fragment += ',';
      detail::WriteJsonEscaped(fragment, name);
      fragment += ':';
      fragment += shape;
    }
    fragment += "}}";
    ulJSAPISetMetadata(api_, "", fragment.c_str());
  }

  ULJSAPI api_ = nullptr;
  ULJSClass cls_ = nullptr;
  std::string path_;
  std::string full_path_;
  detail::SchemaShapes shapes_;
  std::string ctor_;
  std::map<std::string, std::string> methods_;
  std::map<std::string, std::string> properties_;
  std::map<std::string, std::string> statics_;
};

template <typename T>
ClassBuilder<T> API::DefineClass(const char* path) {
  static_assert(std::is_class_v<T>, "DefineClass binds class types");
  using Bare = std::remove_cv_t<T>;
  if (!api_ || !path || !*path)
    return ClassBuilder<T>(nullptr, nullptr, std::string(), std::string());
  ULJSClass& cls = detail::ClassRegistry<Bare>::cls;
  if (!cls) {
    // The class name is the last path segment ("db.Database" names the class "Database").
    const char* name = path;
    for (const char* p = path; *p; ++p) {
      if (*p == '.')
        name = p + 1;
    }
    cls = ulCreateJSClass(name);
  }
  return ClassBuilder<T>(api_, cls, FullPath(path), path);
}

///
/// Detach a bound-class instance from its wrapper and take ownership back.
///
/// After this, calls on the wrapper throw a TypeError with code `ULJS_DETACHED` instead of
/// reaching the instance. Use it to write close methods (see "Closing an Instance" in
/// js::ClassBuilder), and to detach a borrowed instance's wrapper before you free the instance.
///
/// Each page has its own wrapper for an instance, and this detaches only the one you pass. To
/// get an instance's wrapper on a page, convert its pointer there (`js::Detach<T>(ctx.Make(ptr))`),
/// so before you free a borrowed instance, detach its wrapper on every page you gave it to.
///
/// @param  object  The instance's wrapper.
///
/// @return  Returns the instance's canonical holder. Returns an empty holder if the wrapper
///          didn't own the instance (it came from a `T*`, so the instance is already yours),
///          was already detached, or isn't a wrapper of T. Also returns an empty holder while an
///          async method call on the instance is pending: the wrapper is still detached, and
///          the instance is released after that call's Promise settles (see "Closing an
///          Instance" in js::ClassBuilder).
///
/// @note  Call this on the Renderer's thread.
///
template <typename T>
typename ClassHolder<std::remove_cv_t<T>>::type Detach(const Value& object) {
  using Bare = std::remove_cv_t<T>;
  using H = typename ClassHolder<Bare>::type;
  ULJSClass cls = detail::ClassRegistry<Bare>::cls;
  if (!cls)
    return H {};
  // Peek before detaching: a stake owned by the C holder channel is not a StakeBox, and
  // refusing here (rather than after the detach) leaves the wrapper attached and its
  // ownership intact.
  void* peek = ulJSObjectGetInstanceHolder(object.raw(), cls);
  if (peek && static_cast<detail::StakeBox<H>*>(peek)->magic != detail::kStakeBoxMagic)
    return H {};
  void* holder = nullptr;
  ulJSObjectDetachInstance(object.raw(), cls, &holder);
  if (!holder)
    return H {};
  auto* box = static_cast<detail::StakeBox<H>*>(holder);
  H result = std::move(box->holder);
  delete box;
  return result;
}

}  // namespace js
}  // namespace ultralight
