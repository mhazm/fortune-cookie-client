///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/Defines.h>
#include <Ultralight/String.h>
#include <functional>

namespace ultralight {

///
/// Parsed web address.
///
/// URL represents a parsed web address conforming to the WHATWG specification. You'll use it to
/// inspect address components and resolve relative paths when loading pages into a View.
///
/// The object converts to String automatically, so you can pass it directly to View::LoadURL().
///
/// Construct a URL from a string and verify that parsing succeeded before loading the address:
///
/// ```
/// URL link(input);  // eg, "https://example.com/docs"
/// if (link)
///   view->LoadURL(link);
/// ```
///
/// ## Parsing Addresses
///
/// Testing a URL in a conditional evaluates to `true` when parsing succeeds and `false` when
/// parsing fails.
///
/// The library stores addresses in canonical form-- str() returns the lowercased scheme and host
/// rather than the raw input text.
///
/// ## Reading Components
///
/// Inspect individual parts of an address with component accessors:
///
/// ```
/// URL link("https://example.com/search?q=maps#top");
///
/// String site = link.host();                 // "example.com"
/// String route = link.path();                // "/search"
/// String terms = link.query_parameter("q");  // "maps"
/// ```
///
/// Accessors return an empty String when a component is absent.
///
/// Call query_parameter() with a key name to read a single parameter-- it returns the form-decoded
/// value, or an empty String when the key isn't present.
///
/// ## Resolving Relative References
///
/// Resolve a relative path against an existing base URL using Resolve():
///
/// ```
/// URL base(view->url());
/// URL logo = base.Resolve("../images/logo.png");
/// ```
///
/// Resolve() resolves references using the same rules a web browser applies to page links.
///
/// ## Building Query Strings
///
/// Append key-value pairs to an address using AppendQueryParameter():
///
/// ```
/// URL search("https://www.google.com/search");
/// search.AppendQueryParameter("q", "C++ URL & more");
/// // https://www.google.com/search?q=C%2B%2B+URL+%26+more
/// ```
///
/// You can add parameters individually or replace the entire query at once:
///
/// - **AppendQueryParameter() encodes both keys and values automatically.** This makes it safe to
///   build addresses from user input.
/// - **set_query() applies raw query text as-is.** You'll need to encode the string beforehand.
///
/// ## Converting File Paths
///
/// Convert a local file path to a `file:` URL with FromFilePath():
///
/// ```
/// URL menu_url = URL::FromFilePath("C:\\Games\\ui\\menu.html");  // a Windows path
/// String native_path = menu_url.ToFilePath();  // back to the native path
/// ```
///
/// Both methods use the host OS's native path format, converting conventions such as Windows drive
/// letters and backslashes automatically.
///
/// \parblock
/// @note  Parsing requires an absolute address with a scheme (input like `"example.com"` fails to
///        parse).
/// \endparblock
///
/// \parblock
/// @note  You must initialize the object directly because copy initialization from a string doesn't
///        compile.
/// \endparblock
///
/// @see  View::LoadURL(), View::url()
///
class UExport URL {
 public:
  ///
  /// Create an empty, invalid URL.
  ///
  URL();

  ///
  /// Parse a string as an absolute URL (WHATWG-standard parsing).
  ///
  /// Input without a scheme (eg, "example.com") does not parse; the result is invalid. To
  /// resolve a relative reference, use the two-argument constructor taking a base URL.
  ///
  /// @param  url_string  The string to parse.
  ///
  /// @warning  A host with non-ASCII characters (an international domain name) needs the
  ///           library's Unicode data, which the Renderer loads. Parse such a URL only after
  ///           you've called Renderer::Create() or App::Create().
  ///
  explicit URL(const String& url_string);

  ///
  /// Resolve a (possibly relative) reference against a base URL.
  ///
  /// This follows the same rules a browser uses to resolve links on a page. An invalid base
  /// yields an invalid result unless `relative` is itself an absolute URL.
  ///
  /// @param  base      The base URL to resolve against.
  ///
  /// @param  relative  The reference to resolve. This may be a path ("../images/logo.png"), an
  ///                   absolute path ("/index.html"), a fragment ("#top"), or a full absolute
  ///                   URL (which ignores the base).
  ///
  URL(const URL& base, const String& relative);

  ///
  /// Create a file: URL from a native file-system path.
  ///
  /// Platform path conventions are handled for you (eg, Windows drive letters and backslashes).
  /// Use ToFilePath() for the reverse conversion.
  ///
  /// @param  file_path  An absolute native file-system path.
  ///
  /// @return  Returns a file: URL, or an invalid URL if the path cannot be represented.
  ///
  static URL FromFilePath(const String& file_path);

  ///
  /// Percent-encode a string for safe embedding inside a URL component.
  ///
  /// This matches JavaScript's `encodeURIComponent`: every character except letters, digits, and
  /// `- _ . ! ~ * ' ( )` is replaced by the percent-encoding of its UTF-8 bytes.
  ///
  /// @param  component  The text to encode.
  ///
  /// @return  Returns the encoded text.
  ///
  static String EncodeComponent(const String& component);

  ///
  /// Decode a percent-encoded URL component.
  ///
  /// This matches JavaScript's `decodeURIComponent` for well-formed input: `%XX` escapes
  /// (case-insensitive hex) are decoded as UTF-8 bytes.
  ///
  /// Unlike its JavaScript counterpart this function never fails. A malformed escape (truncated,
  /// or with non-hex digits) is passed through unchanged.
  ///
  /// @param  encoded  The text to decode.
  ///
  /// @return  Returns the decoded text.
  ///
  static String DecodeComponent(const String& encoded);

  ///
  /// Copy constructor.
  ///
  URL(const URL& other);

  ///
  /// Move constructor.
  ///
  URL(URL&& other);

  ///
  /// Destructor.
  ///
  ~URL();

  ///
  /// Assignment operator.
  ///
  URL& operator=(const URL& other);

  ///
  /// Move assignment operator.
  ///
  URL& operator=(URL&& other);

  ///
  /// Whether or not this URL parsed successfully.
  ///
  bool is_valid() const;

  ///
  /// Whether or not this URL parsed successfully (same as is_valid()).
  ///
  explicit operator bool() const { return is_valid(); }

  ///
  /// Whether or not this URL is empty (equivalent to !is_valid()).
  ///
  bool empty() const;

  ///
  /// Get the canonical serialization of the whole URL ("" if invalid).
  ///
  String str() const;

  ///
  /// Implicit conversion to the canonical serialization.
  ///
  operator String() const;

  ///
  /// Resolve a relative or absolute reference against this URL (same as the two-argument
  /// constructor).
  ///
  /// @param  relative  The reference to resolve.
  ///
  /// @return  Returns the resolved URL. The result is invalid when this URL is invalid, unless
  ///          `relative` is itself an absolute URL.
  ///
  URL Resolve(const String& relative) const;

  ///
  /// Get the origin serialization, like JavaScript's `URL.origin`.
  ///
  /// For hierarchical schemes this is "scheme://host" plus the port when one is explicitly
  /// present (eg, "https://example.com:8080").
  ///
  /// @note  Opaque origins (eg, data: URLs) serialize as "null".
  ///
  String origin() const;

  ///
  /// Get the scheme, lowercased, without the trailing colon (eg, "https"; "" if invalid).
  ///
  String scheme() const;

  ///
  /// Get the host, without the port (eg, "example.com"; "" if invalid or hostless).
  ///
  String host() const;

  ///
  /// Get the host with the explicit port appended, if one is present (eg, "example.com:8080").
  ///
  String host_with_port() const;

  ///
  /// Whether or not the URL carries an explicit port.
  ///
  /// A default port (eg, 443 for https) is elided during canonicalization and does not count as
  /// explicit.
  ///
  bool has_port() const;

  ///
  /// Get the explicit port number (0 when has_port() is false).
  ///
  unsigned short port() const;

  ///
  /// Get the username portion of the URL's credentials, percent-decoded ("" if none).
  ///
  String username() const;

  ///
  /// Get the password portion of the URL's credentials, percent-decoded ("" if none).
  ///
  String password() const;

  ///
  /// Get the path (eg, "/a/b"; "" if invalid).
  ///
  String path() const;

  ///
  /// Get the query string, without the leading '?' ("" if none).
  ///
  /// Values inside the query are stored encoded. Use query_parameter() to read a decoded value.
  ///
  String query() const;

  ///
  /// Get the fragment, without the leading '#' ("" if none).
  ///
  String fragment() const;

  ///
  /// Set the scheme.
  ///
  /// The new scheme is validated and canonicalized (lowercased).
  ///
  /// @param  scheme  The new scheme, with or without the trailing colon.
  ///
  /// @return  Returns whether or not the scheme was accepted. On failure the URL is unchanged.
  ///
  bool set_scheme(const String& scheme);

  ///
  /// Set the host (without port).
  ///
  /// Invalid input (eg, a stray colon) and empty input are ignored, leaving the URL unchanged.
  /// This matches the JavaScript `URL.host` setter, which reports no acceptance signal either.
  ///
  /// @note  This has no effect on non-hierarchical URLs (eg, data:).
  ///
  void set_host(const String& host);

  ///
  /// Set an explicit port.
  ///
  /// A scheme-default port (eg, 443 for https) is elided from the serialization.
  ///
  /// @note  This has no effect on non-hierarchical URLs.
  ///
  void set_port(unsigned short port);

  ///
  /// Remove the explicit port, if any.
  ///
  void clear_port();

  ///
  /// Set the username portion of the URL's credentials (percent-encoded as needed).
  ///
  /// Pass an empty string to remove it.
  ///
  /// @note  This has no effect on non-hierarchical URLs.
  ///
  void set_username(const String& username);

  ///
  /// Set the password portion of the URL's credentials (percent-encoded as needed).
  ///
  /// Pass an empty string to remove it.
  ///
  /// @note  This has no effect on non-hierarchical URLs.
  ///
  void set_password(const String& password);

  ///
  /// Set the path.
  ///
  /// The path is encoded as needed. On http(s) URLs an empty path becomes "/".
  ///
  void set_path(const String& path);

  ///
  /// Set the raw query string (without the leading '?').
  ///
  /// Pass an empty string to remove the query entirely.
  ///
  /// The value is used as-is, so you are responsible for encoding it. To build a query from
  /// arbitrary text safely, use AppendQueryParameter() instead.
  ///
  void set_query(const String& query);

  ///
  /// Set the fragment (without the leading '#').
  ///
  /// Pass an empty string to remove the fragment entirely.
  ///
  void set_fragment(const String& fragment);

  ///
  /// Whether or not the query contains a parameter with the given key (form-decoded match).
  ///
  bool has_query_parameter(const String& key) const;

  ///
  /// Get the first query-parameter value for the given key, form-decoded ("" if absent).
  ///
  String query_parameter(const String& key) const;

  ///
  /// Append a key-value pair to the query, form-encoding both.
  ///
  /// Spaces become `+` and reserved characters are percent-encoded.
  ///
  /// @param  key    The parameter name (unencoded).
  ///
  /// @param  value  The parameter value (unencoded).
  ///
  void AppendQueryParameter(const String& key, const String& value);

  ///
  /// Get the native file-system path for a file: URL.
  ///
  /// @return  Returns the native path, or "" for any other scheme or if this URL is invalid.
  ///
  /// @see  FromFilePath()
  ///
  String ToFilePath() const;

  ///
  /// Equality over the canonical serialization.
  ///
  bool operator==(const URL& other) const;

  ///
  /// Inequality over the canonical serialization.
  ///
  bool operator!=(const URL& other) const;

  ///
  /// Lexicographic ordering over the canonical serialization (for ordered containers).
  ///
  bool operator<(const URL& other) const;

  ///
  /// Compare the canonical serialization against a raw string.
  ///
  /// @note  The comparison is textual, so the string must already be in canonical form to match
  ///        (eg, `URL("HTTP://example.com") == "http://example.com/"`).
  ///
  bool operator==(const String& other) const;

  ///
  /// Inequality against a raw string (see the equality operator taking a String).
  ///
  bool operator!=(const String& other) const;

  ///
  /// Equality with the string on the left-hand side.
  ///
  inline friend bool operator==(const String& lhs, const URL& rhs) { return rhs == lhs; }

  ///
  /// Inequality with the string on the left-hand side.
  ///
  inline friend bool operator!=(const String& lhs, const URL& rhs) { return rhs != lhs; }

 private:
  String string_;
};

}  // namespace ultralight

namespace std {

///
/// Hash support so URL works as a key in unordered containers.
///
template <>
struct hash<ultralight::URL> {
  size_t operator()(const ultralight::URL& url) const { return url.str().Hash(); }
};

}  // namespace std
