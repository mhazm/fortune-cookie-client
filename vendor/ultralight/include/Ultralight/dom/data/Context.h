///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
// X11's headers define None as a macro, which would break the None enumerators.
#pragma push_macro("None")
#undef None

#include <Ultralight/CAPI/CAPI_DOMData.h>
#include <Ultralight/OriginRules.h>
#include <Ultralight/View.h>
#include <Ultralight/dom/Document.h>
#include <Ultralight/dom/data/Binding.h>
#include <Ultralight/dom/data/Holders.h>
#include <Ultralight/dom/data/Model.h>
#include <Ultralight/dom/data/Value.h>
#include <Ultralight/dom/data/detail/ContextImpl.h>
#include <Ultralight/dom/data/detail/Descriptors.h>
#include <Ultralight/dom/data/detail/Emission.h>

#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace ultralight {
namespace dom {

///
/// @namespace ultralight::dom::data
///
/// Data-binding API that connects native C++ data to HTML and CSS markup.
///
/// `#include <Ultralight/dom/data/Context.h>`
///
/// @note  This API is a preview and may still change after 2.0.
///
/// Data bindings connect native C++ objects directly to page markup. Your HTML and CSS declare
/// where values appear and which elements dispatch actions, while your data and application logic
/// remain in native code.
///
/// You update your native objects during your normal update loop and call
/// dom::data::Context::Sync() once per frame. The library reads every bound instance through its
/// schema and sends only the values that changed in a single batch.
///
/// A data context binds native instances and synchronizes their fields with an attached View:
///
/// ```
/// struct Player {
///   int health = 100;
///   int64_t gold = 0;
/// };
///
/// Player player;
/// dd::Context ctx = dd::Context::Create();
/// dd::Binding binding = ctx.Bind("player", player);  // "player" in markup
///
/// if (ctx.AttachTo(view.get()))
///   view->LoadURL("file:///hud.html");
///
/// // Once per frame:
/// player.health -= 10;
/// ctx.Sync();
/// ```
///
/// The page markup displays those values using placeholder syntax:
///
/// ```html
/// <p>Health: {{player.health}}</p>
/// <p>Gold: {{player.gold}}</p>
/// ```
///
/// ## Common Tasks
///
/// Select a type based on the task you need to perform:
///
/// | Task                            | Type                   |
/// |---------------------------------|------------------------|
/// | Bind instances and sync pages   | dom::data::Context     |
/// | Describe a type for markup      | dom::data::TypeTraits  |
/// | Handle actions and edits        | dom::data::Binding     |
/// | Skip unchanged data during sync | dom::data::Model       |
/// | Support custom value types      | dom::data::ValueTraits |
///
/// ## Header and Namespace
///
/// The data-binding types live in `<Ultralight/dom/data/Context.h>` and aren't included by
/// `<Ultralight/DOM.h>`. Including this header provides the full data-binding API.
///
/// The library provides `dd` as a namespace alias for `ultralight::dom::data`. Reference
/// documentation and code examples use this alias for brevity (`dd::Context`, `dd::Binding`).
///
/// ## Core Rules
///
/// - **Most operations belong to the Context's home thread.** You bind instances, register
///   handlers, modify bound objects, and call dom::data::Context::Sync() on the thread that created
///   the Context. Members safe to call from any thread say so in their documentation.
/// - **The library reads bound objects only during dom::data::Context::Sync() and never writes
///   them.** You can modify your instances between sync calls.
/// - **Page input arrives as requests that your handlers apply.** User edits and button clicks
///   don't modify your objects directly. Your dom::data::Binding::OnChange() and
///   dom::data::Binding::OnAction() callbacks decide whether to store or reject each change.
/// - **The API reports errors through return values rather than C++ exceptions.** Functions return
///   false or empty handles when an operation can't be completed. If a handler, formatter,
///   validator, or posted task throws, the library catches it, drops that call, and logs a warning.
///
/// @see  dom::data::Context, dom::data::Binding, dom::data::TypeTraits, dom::data::Schema()
///
namespace data {

///
/// When Context::DumpSchema() writes its file.
///
enum class DumpAt {
  NextSync,  ///< At the end of the next Context::Sync().
  Now,       ///< Right away.
};

static_assert(static_cast<int>(DumpAt::NextSync) == kULDOMDataDumpAt_NextSync
                  && static_cast<int>(DumpAt::Now) == kULDOMDataDumpAt_Now,
              "DumpAt must mirror ULDOMDataDumpAt");

///
/// The flags for Context::AttachTo() (the typed form of ULDOMDataContextAttachFlags). None are
/// defined yet.
///
enum class AttachFlags : unsigned {
  None = 0,  ///< No options.
};

static_assert(static_cast<unsigned>(AttachFlags::None) == kULDOMDataContextAttachFlags_None,
              "dom::data::AttachFlags must mirror ULDOMDataContextAttachFlags");

///
/// Options for Context::AttachTo().
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
/// Data-binding context for attached View%s.
///
/// A Context registers your C++ instances under names that HTML markup can reference, exposing
/// their fields and actions to the pages loaded by each attached View. Your markup declares where
/// values appear and which interactions trigger native actions, keeping page presentation separate
/// from your application's data.
///
/// Calling Sync() once per frame delivers input from the page to your native handlers and
/// broadcasts changed values in a single batch to keep those pages current.
///
/// This example binds a player instance to a context and attaches it to a View:
///
/// ```
/// struct Player {
///   int health = 80;
///   int potions = 3;
///   void UsePotion();
///
///   static constexpr auto schema = dd::Schema(
///       dd::Field("health", &Player::health),
///       dd::Field("potions", &Player::potions),
///       dd::Action("usePotion"));
/// };
///
/// Player player;
/// dd::Context ctx = dd::Context::Create();
/// dd::Binding binding = ctx.Bind("player", player)  // the page calls it "player"
///     .OnAction<"usePotion">(&Player::UsePotion);   // the button below calls this
///
/// if (!ctx.AttachTo(view.get()))
///   Log("couldn't attach the data context");
/// view->LoadURL("file:///hud.html");
///
/// // Once per frame:
/// ctx.Sync();  // runs UsePotion() if clicked, then updates the page
/// ```
///
/// The loaded page references bound fields and click actions directly in markup:
///
/// ```html
/// <p>Health: {{player.health}}</p>
/// <button type="button" ul-on:click="player.usePotion">
///   Drink ({{player.potions}} left)
/// </button>
/// ```
///
/// ## Binding Instances
///
/// Calling Bind() registers an instance under a binding name that forms the root of every markup
/// path on the page.
///
/// Always store the returned dom::data::Binding handle alongside your data, because destroying it
/// unbinds the instance immediately (see dom::data::Binding).
///
/// A context can bind an instance by reference, by value, or through a smart pointer:
///
/// ```
/// auto ally = std::make_shared<Player>();
///
/// dd::Binding b1 = ctx.Bind("player", player);      // borrows player
/// dd::Binding b2 = ctx.Bind("preview", Player {});  // owns its own Player
/// dd::Binding b3 = ctx.Bind("ally", ally);          // shares ownership of *ally
/// ```
///
/// Each binding form defines who keeps the bound instance alive:
///
/// - **Binding by reference borrows the instance.** Native code retains ownership and must keep the
///   instance alive while it remains bound.
/// - **Binding by value moves ownership into the binding.** The dom::data::Binding owns the
///   instance and destroys it when it unbinds.
/// - **Binding through a smart pointer manages lifetime automatically.** An owning pointer keeps
///   the instance alive, while an expired weak pointer skips updates and leaves the last values on
///   the page.
///
/// ## Attaching to Views
///
/// Call AttachTo() to connect a Context to a View. You only attach once-- every page that View
/// loads receives the bindings automatically, including pages restored from the back-forward cache.
///
/// A single Context can attach to multiple View%s, and a View can hold bindings from multiple
/// Context%s. One call to Sync() synchronizes all attached View%s at once.
///
/// Data bindings work only in a View's main frame.
///
/// ### Origin Rules
///
/// By default, only local `file://` pages and pages loaded with View::LoadHTML() receive bindings.
///
/// Passing custom origin rules in AttachOptions replaces the default policy completely. To allow
/// local files alongside remote origins, include `file://*` in the rule list (see OriginRules for
/// pattern syntax).
///
/// Pages loaded with View::LoadHTML() without a URL have an opaque origin that no custom rule
/// matches, so they receive bindings only under the default policy. When loaded with an explicit
/// URL, they match against that URL's origin.
///
/// ## Home Thread
///
/// The thread that calls Create() becomes the home thread for that Context. In a single-threaded
/// application, this is the Renderer's thread. You bind instances, register handlers, and call
/// Sync() on this thread. When you modify bound data, make those changes on the home thread between
/// calls to Sync().
///
/// When multiple worker threads need to modify bound data, give each thread its own Context. If the
/// thread that calls Sync() changes over time (such as across worker threads in a job system), call
/// CreateThreadSafe() instead so any thread can bind data, register handlers, and call Sync().
///
/// ## Finding Mistakes
///
/// A mistake in markup drops only the broken binding, allowing the rest of the page to bind
/// normally.
///
/// When developer mode is on (`Config::diagnostics`), the library outputs warnings for markup
/// mistakes to the native Logger and the page's console.
///
/// \parblock
/// @note  Keep calling Sync() while the page is visible, even when your game is paused, so actions
///        and change requests from the page continue to reach native handlers.
/// \endparblock
///
/// \parblock
/// @note  dom::data::Context is unrelated to js::Context.
/// \endparblock
///
/// @see  dom::data::Binding, dom::data::TypeTraits, dom::data::Schema(), PostTask(), DefineFormat()
///
class Context {
 public:
  ///
  /// Create a Context whose home thread is the calling thread.
  ///
  /// @return  Returns the new Context.
  ///
  static Context Create() { return Context(ulCreateDOMDataContext()); }

  ///
  /// Create a Context that any thread can use.
  ///
  /// Use this when the thread that calls Sync() changes over time (eg, a job system that syncs
  /// on different worker threads). Any thread can then bind, sync, and register handlers. The
  /// calls take turns (each one waits until the running one finishes), and handlers run on the
  /// thread that called Sync().
  ///
  /// @warning
  /// \parblock
  /// Only the Context's own calls are thread-safe, **not** your bound instances. Sync() reads
  /// them without any lock you can see, so don't change an instance while another thread may be
  /// syncing. To hand an instance to another thread, use your own synchronization.
  ///
  /// A handler that waits on another thread that needs this Context deadlocks. Calling back into
  /// the Context from the handler itself is fine.
  /// \endparblock
  ///
  /// @return  Returns the new Context.
  ///
  static Context CreateThreadSafe() { return Context(ulCreateDOMDataContextThreadSafe()); }

  Context(const Context&) = delete;
  Context& operator=(const Context&) = delete;

  ///
  /// Move constructor (`other` becomes empty).
  ///
  /// @param  other  The Context to move from.
  ///
  Context(Context&& other) noexcept : handle_(other.handle_) { other.handle_ = nullptr; }

  ///
  /// Move assignment (`other` becomes empty).
  ///
  /// This Context releases its own handle first.
  ///
  /// @param  other  The Context to move from.
  ///
  /// @return  Returns this Context.
  ///
  Context& operator=(Context&& other) noexcept {
    if (this != &other) {
      ulDestroyDOMDataContext(handle_);
      handle_ = other.handle_;
      other.handle_ = nullptr;
    }
    return *this;
  }

  ///
  /// Destroy this handle.
  ///
  /// Destroying the Context detaches it from every View (the pages drop its bindings the same
  /// way DetachFrom() does) and destroys its bindings, handlers, and formatters (and anything
  /// they capture) right here, on the calling thread. So destroy it on its home thread. A
  /// Binding that outlives it stays safe to use, but its instance is no longer read.
  ///
  /// @note  A Context from FromBorrowed() doesn't own the context, so destroying it changes
  ///        nothing, and neither does destroying one while another owning handle exists.
  ///
  ~Context() { ulDestroyDOMDataContext(handle_); }

  ///
  /// Whether or not this Context is valid (false after a move or LeakRef()).
  ///
  explicit operator bool() const { return handle_ != nullptr; }

  ///
  /// Bind an instance of a described type under a binding name.
  ///
  /// Pages reach the instance's fields through the binding name (eg, `hud.health`), and each
  /// Sync() sends your changes to them. The Binding borrows `instance`, so keep it alive while
  /// it's bound.
  ///
  /// Binding a name again replaces the earlier binding (see Binding).
  ///
  /// @param  name      The binding name pages use. It starts with a letter or an underscore,
  ///                   followed by letters, digits, and underscores (eg, `hud` or
  ///                   `main_menu`).
  ///
  /// @param  instance  The instance to bind. Ownership remains with the caller.
  ///
  /// @return  Returns the Binding (empty if `name` isn't a valid binding name, and the library
  ///          logs a warning).
  ///
  /// @warning  Destroying the Binding unbinds the instance, so a `ctx.Bind(...)` statement whose
  ///           result you don't keep binds and unbinds at once.
  ///
  template <typename T>
    requires(Described<T> && !std::is_const_v<T>)
  [[nodiscard]] Binding<T> Bind(std::string_view name, T& instance) {
    Binding<T> binding(BindHandle<T>(name, static_cast<void*>(std::addressof(instance)),
                                     &detail::BorrowedAcquire, nullptr, nullptr));
    if (binding)
      binding.access_ = detail::MakeBorrowedAccess<T>(std::addressof(instance));
    return binding;
  }

  ///
  /// Bind a const instance under a binding name.
  ///
  /// This works like the non-const overload, except that handlers which are member functions
  /// of the bound type (eg, `OnAction<"respawn">(&HUD::Respawn)`) do nothing, since they would
  /// change the instance. Register a lambda or another receiver instead.
  ///
  /// @param  name      The binding name pages use.
  ///
  /// @param  instance  The instance to bind. Ownership remains with the caller.
  ///
  /// @return  Returns the Binding (empty if `name` isn't a valid binding name).
  ///
  template <typename T>
    requires Described<T>
  [[nodiscard]] Binding<T> Bind(std::string_view name, const T& instance) {
    return Binding<T>(
        BindHandle<T>(name, const_cast<void*>(static_cast<const void*>(std::addressof(instance))),
                      &detail::BorrowedAcquire, nullptr, nullptr));
  }

  ///
  /// Bind an instance by value under a binding name.
  ///
  /// The Binding takes ownership of the moved-in instance and destroys it when it unbinds. Use
  /// this for data you build once and don't change afterwards.
  ///
  /// Handlers that are member functions of the bound type (eg,
  /// `OnAction<"respawn">(&HUD::Respawn)`) run on the owned instance. They're the only way to
  /// change it after binding.
  ///
  /// @param  name      The binding name pages use.
  ///
  /// @param  instance  The instance to move into the Binding.
  ///
  /// @return  Returns the Binding (empty if `name` isn't a valid binding name).
  ///
  template <typename T>
    requires(Described<std::remove_cvref_t<T>> && !std::is_lvalue_reference_v<T>)
  [[nodiscard]] Binding<std::remove_cvref_t<T>> Bind(std::string_view name, T&& instance) {
    using Plain = std::remove_cvref_t<T>;
    using Box = detail::ValueHolderBox<Plain>;
    auto* box = new Box { std::forward<T>(instance) };
    Binding<Plain> binding(
        BindHandle<Plain>(name, box, &Box::Acquire, nullptr, &Box::Destroy));
    if (binding)
      binding.access_ = detail::MakeValueAccess<Plain>(box);
    return binding;
  }

  ///
  /// Bind an instance through a smart pointer under a binding name.
  ///
  /// `holder` can be a std::shared_ptr, RefPtr, or moved std::unique_ptr. It can also be a
  /// std::weak_ptr, WeakPtr, or your own smart pointer type with an ultralight::HolderTraits
  /// specialization. The Binding keeps the holder (a copy or the moved std::unique_ptr)-- an owning
  /// pointer keeps the instance alive while it's bound. Sync() locks the holder while it reads the
  /// instance.
  ///
  /// Once a weak holder expires, Sync() skips the binding and pages keep its last values.
  /// Handlers that are member functions of the bound type skip it too, and do nothing at all
  /// when the holder points to a const type.
  ///
  /// @param  name    The binding name pages use.
  ///
  /// @param  holder  The smart pointer to the instance.
  ///
  /// @return  Returns the Binding (empty if `name` isn't a valid binding name).
  ///
  template <typename H>
    requires BindableHolder<H>
  [[nodiscard]] Binding<
      std::remove_cv_t<typename HolderTraits<std::remove_cvref_t<H>>::element_type>>
  Bind(std::string_view name, H&& holder) {
    using Plain = std::remove_cvref_t<H>;
    using T = std::remove_cv_t<typename HolderTraits<Plain>::element_type>;
    using Box = detail::SmartHolderBox<Plain>;
    auto* box = new Box { std::forward<H>(holder), std::nullopt };
    Binding<T> binding(
        BindHandle<T>(name, box, &Box::Acquire, &Box::Release, &Box::Destroy));
    if (binding) {
      using LockedRef
          = decltype(*ultralight::detail::LockHolder(std::declval<const Plain&>()));
      if constexpr (!std::is_const_v<std::remove_reference_t<LockedRef>>)
        binding.access_ = detail::MakeHolderAccess<Plain, T>(box);
    }
    return binding;
  }

  ///
  /// Attach this Context to a View.
  ///
  /// Every page the View loads then gets this Context's bindings, starting with the current page
  /// during a later Renderer::Update(). The View keeps the Context attached until you detach it
  /// or destroy its last owning Context.
  ///
  /// A page gets the bindings only if the origin rules allow it. With no rules, only your
  /// application's own content does (local pages and pages loaded with View::LoadHTML()). Rules
  /// have the `scheme://host[:port]` form (see OriginRules for the full syntax), and they replace
  /// that default, so add `file://*` if you still want local pages:
  ///
  /// ```
  /// if (!ctx.AttachTo(view.get(), { .origin_rules = { "https://*.mygame.com", "file://*" } }))
  ///   Log("an origin rule failed to parse");
  /// ```
  ///
  /// A page whose origin isn't allowed shows its authored markup, and its controls send no input. A
  /// page loaded with View::LoadHTML() without a URL has an opaque origin-- no rule matches it and
  /// no filter allows one, so it gets bindings only with the default rules. Attaching an already
  /// attached Context replaces its flags and rules.
  ///
  /// @param  view     The View to attach to.
  ///
  /// @param  options  The attach flags and the origin rules (see AttachOptions).
  ///
  /// @return  Returns true on success, or false if `view` is nullptr, this Context is empty, a
  ///          flag is unknown, or a rule failed to parse. An earlier attachment stays as it was.
  ///          An unknown flag or a rule that failed to parse also logs a warning that says why.
  ///
  /// @note  Safe to call from any thread.
  ///
  /// @see  DetachFrom()
  ///
  [[nodiscard]] bool AttachTo(View* view, const AttachOptions& options = {}) {
    return view && handle_
        && view->AttachDOMDataContext(handle_, static_cast<unsigned>(options.flags),
                                      options.origin_rules.data(), options.origin_rules.size());
  }

  ///
  /// Detach this Context from a View.
  ///
  /// During a later Renderer::Update(), the View's page rebuilds its bindings from the Contexts
  /// still attached. When none are left, the page gets its authored markup back (`{{path}}` text,
  /// `ul-if` templates, and no rows). Values already written by `ul-text`, classes, attributes,
  /// and styles stay.
  ///
  /// @param  view  The View to detach from (nullptr does nothing).
  ///
  /// @note  Safe to call from any thread.
  ///
  void DetachFrom(View* view) {
    if (view && handle_)
      view->DetachDOMDataContext(handle_);
  }

  ///
  /// Deliver input from the page to your handlers and send your changes to the pages.
  ///
  /// Call this once per tick on the home thread. Each call does three things in order:
  ///
  ///  1. **Delivers input from the page** to your handlers (changes first, then actions).
  ///  2. **Reads every bound instance** through its schema.
  ///  3. **Sends what changed** to the pages as one update.
  ///
  /// Pages show the update during a later Renderer::Update(), and they never show part of one.
  /// A Sync with nothing to send is cheap.
  ///
  /// A type can control how its instances are read with a static `Sync` function, which reads
  /// fields with Model::Sync() (see "Three Kinds of Sync" in the Model class description).
  ///
  /// @note  A Sync() called from inside one of your handlers does nothing (the library logs a
  ///        warning).
  ///
  void Sync() { ulDOMDataContextSync(handle_); }

  ///
  /// Send an action to a bound instance's handler as if the page fired it.
  ///
  /// Use this where you have no Binding at hand, eg, in a DOM listener that handles an
  /// interaction the markup can't express. Where you have the Binding, use
  /// Binding::PostAction() instead (it checks the action name at compile time).
  ///
  /// The action reaches its handler at the next Sync(), in order with the actions from the page.
  /// An action with an unknown binding or action name is dropped there, with a warning when
  /// DOM diagnostics are on (see "Finding Mistakes" in the class overview).
  ///
  /// @param  name  The binding name and the action name joined by a dot (eg, `"inv.sort"`).
  ///               Only actions of the bound type itself can be sent (not a row's actions).
  ///
  /// @return  Returns true if the action was queued, or false if `name` isn't in that form or
  ///          the action queue is full.
  ///
  /// \parblock
  /// @note  Safe to call from any thread.
  /// \endparblock
  ///
  /// \parblock
  /// @note  Each call takes one entry in the action queue, even for an action declared
  ///        `dd::OnlyLatest` (repeats merge only when Sync() delivers them).
  /// \endparblock
  ///
  /// @see  set_action_queue_capacity()
  ///
  bool PostAction(std::string_view name) { return PostActionByName(name, nullptr, 0); }

  ///
  /// Send an action with a payload struct (see the payloadless overload).
  ///
  /// ```
  /// ctx.PostAction("inv.moveItem", MoveItem { .from = a, .to = b });
  /// ```
  ///
  /// The payload is a described struct whose fields are all bool, integer, floating-point,
  /// string, or reflected enum values (checked at compile time). The action's handler receives
  /// it as its declared payload type (a handler whose payload type doesn't match doesn't receive
  /// it).
  ///
  /// @param  name     The binding name and the action name joined by a dot.
  ///
  /// @param  payload  The payload to send.
  ///
  /// @return  Returns true if the action was queued, or false if `name` isn't in that form or
  ///          the action queue is full.
  ///
  template <typename P>
    requires Described<std::remove_cvref_t<P>>
  bool PostAction(std::string_view name, const P& payload) {
    using Plain = std::remove_cvref_t<P>;
    static_assert(detail::PayloadMarshalable<Plain>(),
                  "dom::data: a payload type carries plain data fields only (readable "
                  "leaf members of boolean, integral, floating-point, string, or "
                  "reflected enum type)");
    std::vector<std::string> strings;
    std::vector<ULDOMDataPayloadEntry> entries;
    detail::MarshalPayload(payload, strings, entries);
    return PostActionByName(name, entries.empty() ? nullptr : entries.data(),
                            entries.size());
  }

  ///
  /// Send an action with a single-value payload (see the payloadless overload).
  ///
  /// Use this for an action declared with one bool, number, string, or reflected enum value (eg,
  /// `dd::Action<double>("seek")`):
  ///
  /// ```
  /// ctx.PostAction("player.seek", 0.5);
  /// ```
  ///
  /// @param  name     The binding name and the action name joined by a dot.
  ///
  /// @param  payload  The value to send.
  ///
  /// @return  Returns true if the action was queued, or false if `name` isn't in that form or
  ///          the action queue is full.
  ///
  template <typename P>
    requires(!Described<std::decay_t<P>> && ScalarPayload<std::decay_t<P>>)
  bool PostAction(std::string_view name, const P& payload) {
    std::string backing;
    ULDOMDataPayloadEntry entry {};
    detail::MarshalScalarPayload(payload, backing, entry);
    return PostActionByName(name, &entry, 1);
  }

  ///
  /// Run a callable on the Renderer's thread once the pages show your latest data.
  ///
  /// The callable runs during a later Renderer::Update(), after the pages have applied the data
  /// from:
  ///
  ///  - **This Sync**, when you post from a handler inside Sync().
  ///  - **The last Sync**, when you post from anywhere else.
  ///
  /// How often it runs depends on its parameters:
  ///
  ///  - **No parameters** runs once after every attached View has applied the data. When no View is
  ///    attached, it runs during a later Renderer::Update() without waiting for a page.
  ///  - **A dom::Document** runs once for each attached View and gets that View's document, so it
  ///    can read the page your data produced. With no attached Views, it runs once with an empty
  ///    Document.
  ///
  /// ```
  /// ctx.PostTask([] { SignalUIReady(); });  // once
  /// ctx.PostTask([](dom::Document doc) {    // once per View
  ///   dom::Element row = doc.querySelector("#roster li");
  ///   // ...
  /// });
  /// ```
  ///
  /// A View whose page is still loading holds it up until that page shows the data. A page that
  /// doesn't use this Context (eg, its origin isn't allowed) doesn't hold it up.
  ///
  /// @param  callback  The callable to run. Capture by value, since it runs later (and on
  ///                   another thread unless your home thread is the Renderer's thread).
  ///
  /// \parblock
  /// @note  Safe to call from any thread.
  /// \endparblock
  ///
  /// \parblock
  /// @note  The library destroys the callable after its last run. If no Renderer exists, or the
  ///        Renderer shuts down first, it's destroyed without running.
  /// \endparblock
  ///
  template <typename F>
    requires(std::is_invocable_v<std::decay_t<F>&>
             || std::is_invocable_v<std::decay_t<F>&, Document>)
  void PostTask(F&& callback) {
    using Fn = std::decay_t<F>;
    auto* fn = new Fn(std::forward<F>(callback));
    if constexpr (std::is_invocable_v<Fn&, Document>) {
      ulDOMDataContextPostTask(handle_, &detail::InvokePostedCallable<Fn>, fn,
                               &detail::DeletePostedCallable<Fn>);
    } else {
      ulDOMDataContextPostTaskOnce(handle_, &detail::InvokePostedCallable<Fn>, fn,
                                   &detail::DeletePostedCallable<Fn>);
    }
  }

  ///
  /// Define a formatter for `{{path|name}}` text.
  ///
  /// A formatter turns a bool, number, or string value into the text the page shows:
  ///
  /// ```
  /// ctx.DefineFormat("comma", [](dd::Value v) {
  ///   return GroupDigits(v.Or(int64_t(0)));
  /// });
  /// ```
  ///
  /// ```html
  /// <span>{{hud.gold|comma}} gold</span>
  /// ```
  ///
  /// Only `{{path}}` text uses formatters (`ul-text` doesn't take one), and only for the
  /// bindings of this Context. Defining a name again replaces its formatter. A new formatter
  /// shows the next time the text changes.
  ///
  /// @param  name  The formatter name, used after the pipe. It starts with a letter or an
  ///               underscore, followed by letters, digits, and underscores. A name that
  ///               doesn't defines nothing (the library logs a warning).
  ///
  /// @param  fn    The formatter. It takes the value as a dd::Value and returns the text (a
  ///               String, a std::string, or a C string).
  ///
  /// @return  Returns this Context, for chaining.
  ///
  /// @warning  Formatters run on the **Renderer's thread**. They must return the same text for
  ///           the same value, must not throw, and must not call the data-binding API.
  ///
  template <typename Fn>
  Context& DefineFormat(std::string_view name, Fn&& fn) {
    using Plain = std::decay_t<Fn>;
    static_assert(std::is_invocable_v<Plain&, Value>,
                  "dom::data: a formatter takes the published value as a dd::Value");
    if constexpr (std::is_invocable_v<Plain&, Value>) {
      using R = std::remove_cvref_t<std::invoke_result_t<Plain&, Value>>;
      static_assert(std::is_same_v<R, ultralight::String> || std::is_same_v<R, std::string>
                        || std::is_constructible_v<ultralight::String, R>,
                    "dom::data: a formatter returns display text (String, std::string, "
                    "or a C string)");
      auto* boxed = new Plain(std::forward<Fn>(fn));
      ulDOMDataContextDefineFormat(handle_, std::string(name).c_str(),
                                   &detail::InvokeFormatCallable<Plain>, boxed,
                                   &detail::DeletePostedCallable<Plain>);
    }
    return *this;
  }

  ///
  /// Remove a formatter.
  ///
  /// Text that uses it shows the plain value (and logs a warning) from the next time it changes.
  ///
  /// @param  name  The formatter name.
  ///
  /// @return  Returns this Context, for chaining.
  ///
  Context& UndefineFormat(std::string_view name) {
    ulDOMDataContextDefineFormat(handle_, std::string(name).c_str(), nullptr, nullptr, nullptr);
    return *this;
  }

  ///
  /// Get the generation of the last update Sync() sent to the pages.
  ///
  /// The generation goes up by one with each update.
  ///
  /// @return  Returns the generation (0 before the first update or for an empty Context).
  ///
  uint64_t generation() const { return ulDOMDataContextGetGeneration(handle_); }

  ///
  /// Set how many actions can wait for the next Sync(). (Default: 1024)
  ///
  /// When the queue is full, new actions are dropped (the library logs a warning). For an action
  /// the page fires continuously (eg, a scroll position), declare it `dd::OnlyLatest` so each new
  /// one replaces the waiting one instead of raising the limit.
  ///
  /// @param  capacity  The number of actions (values below 1 become 1).
  ///
  /// @note  Safe to call from any thread.
  ///
  void set_action_queue_capacity(uint32_t capacity) {
    ulDOMDataContextSetActionQueueCapacity(handle_, capacity);
  }

  ///
  /// Get how many actions can wait for the next Sync().
  ///
  /// @return  Returns the limit (0 for an empty Context).
  ///
  /// @note  Safe to call from any thread.
  ///
  uint32_t action_queue_capacity() const {
    return ulDOMDataContextGetActionQueueCapacity(handle_);
  }

  ///
  /// Get the schema of every bound instance as JSON, with their current values.
  ///
  /// The mock data tools read this JSON, so you can build and test pages in a browser with your
  /// app's real model shapes (see DumpSchema() to write it to a file). It has:
  ///
  ///  - **`format`, `api`, and `version`**: "ul-schema", "data", and 1.
  ///  - **`models`**: maps each binding name to its type in `types`.
  ///  - **`types`**: lists each type's entries in schema order (with their names, value types,
  ///    flags, and child types).
  ///  - **`values`**: maps each binding name to its current values, read through its schema in
  ///    the form mock data uses. A list holds at most its first 100 rows, and actions and
  ///    dd::Internal entries have no values.
  ///
  /// A reader should ignore keys and value types it doesn't know. The `version` changes only for
  /// a change that would break an existing reader.
  ///
  /// Since the values are your app's real data, you should only write them out from development
  /// builds.
  ///
  /// @return  Returns the JSON text (empty for an empty Context).
  ///
  /// @note  This reads every bound instance (like Sync() does), so it isn't free. Call it on the
  ///        home thread.
  ///
  std::string schema() const {
    ULString text = ulDOMDataContextGetSchema(handle_);
    if (!text)
      return std::string();
    std::string result(ulStringGetData(text), ulStringGetLength(text));
    ulDestroyString(text);
    return result;
  }

  ///
  /// Write the JSON from schema() to a file.
  ///
  /// With DumpAt::NextSync (the default), the library writes the file at the end of the next
  /// Sync() on the home thread, so you can call this from any thread (eg, from a debug hotkey).
  /// A write that fails then logs a warning with the path.
  ///
  /// With DumpAt::Now, the library writes the file before this returns. Call it on the home
  /// thread.
  ///
  /// ```
  /// if (debug_key_pressed)
  ///   ctx.DumpSchema("ui/schema.json");
  /// ```
  ///
  /// @param  utf8_path  The file's path as a UTF-8 string (on Windows too). An existing file is
  ///                    replaced.
  ///
  /// @param  when       When to write the file.
  ///
  /// @return  Returns false if `utf8_path` is NULL or empty, or for an empty Context. With
  ///          DumpAt::Now, also returns false if the write fails.
  ///
  bool DumpSchema(const char* utf8_path, DumpAt when = DumpAt::NextSync) {
    return ulDOMDataContextDumpSchema(handle_, utf8_path, static_cast<ULDOMDataDumpAt>(when));
  }

  // --- Interop with the C API (most embedders never touch raw handles) -------------------

  ///
  /// Wrap a C handle you own, taking ownership of it.
  ///
  /// @param  handle  A handle from the C API that you would otherwise destroy with
  ///                 ulDestroyDOMDataContext() (NULL gives an empty Context).
  ///
  /// @return  Returns a Context that destroys `handle` when it's done.
  ///
  static Context Adopt(ULDOMDataContext handle) { return Context(handle); }

  ///
  /// Wrap a C handle someone else owns, without owning the context.
  ///
  /// The result works like any Context, but destroying it never detaches the context or
  /// destroys its bindings, and it doesn't keep the context attached once its owners are gone.
  ///
  /// @param  handle  The borrowed handle (NULL gives an empty Context).
  ///
  /// @return  Returns a Context with its own (non-owning) reference, so you can keep it. Its
  ///          raw() is a different handle than `handle`.
  ///
  static Context FromBorrowed(ULDOMDataContext handle) {
    return Context(handle ? ulCreateDOMDataContextBorrowedRef(handle) : nullptr);
  }

  ///
  /// Get the C handle, for passing to the `<Ultralight/CAPI/CAPI_DOMData.h>` functions.
  ///
  /// @return  Returns the handle (NULL for an empty Context). This Context still owns it, so
  ///          don't destroy it.
  ///
  ULDOMDataContext raw() const { return handle_; }

  ///
  /// Give up ownership of the C handle and return it. This Context becomes empty.
  ///
  /// @return  Returns the handle. You must call ulDestroyDOMDataContext() when finished.
  ///
  ULDOMDataContext LeakRef() {
    ULDOMDataContext handle = handle_;
    handle_ = nullptr;
    return handle;
  }

 private:
  explicit Context(ULDOMDataContext handle) : handle_(handle) {}

  template <typename T>
  ULDOMDataBinding BindHandle(std::string_view name, void* holder_state,
                              ULDOMDataAcquireCallback acquire, ULDOMDataReleaseCallback release,
                              ULUserDataDestroyCallback destroy_holder) {
    return ulDOMDataContextBind(handle_, std::string(name).c_str(),
                                reinterpret_cast<ULDOMDataType>(detail::Descriptors<T>()),
                                &detail::SyncWalkThunk<T>, holder_state, acquire, release,
                                destroy_holder);
  }

  bool PostActionByName(std::string_view name, const ULDOMDataPayloadEntry* entries,
                        size_t count) {
    const size_t dot = name.find('.');
    if (dot == std::string_view::npos || dot == 0 || dot + 1 == name.size())
      return false;
    const std::string binding_name(name.substr(0, dot));
    const std::string action_name(name.substr(dot + 1));
    return ulDOMDataContextPostActionByName(handle_, binding_name.c_str(), action_name.c_str(),
                                            entries, count);
  }

  ULDOMDataContext handle_ = nullptr;
};

}  // namespace data
}  // namespace dom

///
/// Short alias for the Data Bindings namespace (`dd::Context`, `dd::Binding`, ...).
///
namespace dd = dom::data;

}  // namespace ultralight

#pragma pop_macro("None")
