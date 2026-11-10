///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/Defines.h>

#include <cstddef>
#include <initializer_list>
#include <span>
#include <type_traits>

namespace ultralight {

///
/// A list of origin patterns that controls which pages get an attached API.
///
/// When attaching a js::API, dom::Triggers, or dom::data::Context, you pass OriginRules to
/// AttachTo() to choose which web origins get access.
///
/// Pass origin rules when attaching an API to allow a remote domain, its subdomains, and local
/// files:
///
/// ```
/// if (app.AttachTo(view.get(),
///                  { .origin_rules = { "https://*.mygame.com", "file://*" } }))
///   view->LoadURL("https://play.mygame.com/");
/// ```
///
/// ## Rule Syntax
///
/// Each rule follows the `scheme://host[:port]` format. If a rule fails to parse, the attach call
/// fails and returns `false`.
///
/// | Part   | Accepts                                                                  |
/// |--------|--------------------------------------------------------------------------|
/// | Scheme | An exact scheme with no wildcards                                        |
/// | Host   | An exact host, `*`, or `*.example.com` for the domain and all subdomains |
/// | Port   | A specific port number, `:*` for any port, or default port if omitted    |
///
/// Common rules illustrate these patterns:
///
/// ```
/// "https://store.mygame.com"   // store.mygame.com on the default port (443)
/// "https://*.mygame.com"       // mygame.com and all of its subdomains
/// "http://localhost:*"         // localhost on any port
/// "file://*"                   // local files
/// ```
///
/// ## Default Rules
///
/// Attach calls restrict page access when you omit origin rules:
///
/// - **An empty list allows local content.** Content loaded from `file://` URLs or View::LoadHTML()
///   gets the API.
/// - **Passing any rule replaces the default list.** Include `"file://*"` in your list if you want
///   to keep local page access alongside remote domains.
///
/// ## Origin Matching
///
/// Origin rules match only the scheme, host, and port of a page's web origin. Path segments, query
/// strings, and fragment identifiers are never checked.
///
/// Origin matching depends on how a document is loaded:
///
/// - **Frames with inherited origins match by the origin they inherit.** Documents loaded in
///   `about:blank` or `srcdoc` frames evaluate against the origin of the document that created
///   them.
/// - **HTML loaded with an explicit URL matches that URL.** Passing a URL to View::LoadHTML()
///   assigns that URL's origin to the document, matching rules like any page.
/// - **Opaque origins never match any rule.** Sandboxed frames, `data:` URLs, and View::LoadHTML()
///   called without a URL produce opaque origins that fail all rule checks.
///
/// To allow an API on pages that cannot match an origin rule, set a filter on the View using
/// js::SetInjectionFilter() or dom::SetInjectionFilter(). A dom::data::Context has no filter
/// mechanism.
///
/// ## String Lifetime
///
/// OriginRules refers to rule strings without copying them.
///
/// @warning  An inline list's array is destroyed when that statement ends, leaving any variable
///           that stored it (such as an OriginRules or an AttachOptions) pointing at freed memory.
///           The attach call copies the strings, so pass the list directly into that call or keep
///           them in a std::vector until the call completes.
///
/// @see  js::API::AttachTo(), dom::Triggers::AttachTo(), dom::data::Context::AttachTo(),
///       js::SetInjectionFilter(), dom::SetInjectionFilter()
///
class OriginRules {
 public:
  ///
  /// Create an empty list (the default policy).
  ///
  constexpr OriginRules() = default;

// Storing the list's array without copying it is intended (see String Lifetime above).
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winit-list-lifetime"
#endif

  ///
  /// Create a list from an inline list of rules.
  ///
  /// @param  rules  The rules, as null-terminated UTF-8 strings.
  ///
  constexpr OriginRules(std::initializer_list<const char*> rules)
      : rules_(rules.begin()), size_(rules.size()) {}

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

  ///
  /// Create a list from an array of rules.
  ///
  /// @param  rules  An array of `size` rules, as null-terminated UTF-8 strings (can be nullptr
  ///                when `size` is 0).
  ///
  /// @param  size   The number of entries in `rules`.
  ///
  constexpr OriginRules(const char* const* rules, size_t size) : rules_(rules), size_(size) {}

  ///
  /// Create a list from a contiguous container of rules (eg, a std::vector, std::array, or
  /// std::span of `const char*`).
  ///
  /// @param  rules  The rules, as null-terminated UTF-8 strings.
  ///
  template <typename Rules>
    requires std::is_convertible_v<const Rules&, std::span<const char* const>>
  constexpr OriginRules(const Rules& rules) {
    std::span<const char* const> view = rules;
    rules_ = view.data();
    size_ = view.size();
  }

  ///
  /// Get the rules.
  ///
  /// @return  Returns a pointer to the first rule (nullptr for an empty list).
  ///
  constexpr const char* const* data() const { return size_ ? rules_ : nullptr; }

  ///
  /// Get the number of rules.
  ///
  constexpr size_t size() const { return size_; }

  ///
  /// Whether or not the list is empty.
  ///
  constexpr bool empty() const { return size_ == 0; }

 private:
  const char* const* rules_ = nullptr;
  size_t size_ = 0;
};

}  // namespace ultralight
