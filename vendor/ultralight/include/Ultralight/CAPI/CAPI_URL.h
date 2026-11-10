///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// @file CAPI_URL.h
///
/// Parsed web addresses for C.
///
/// `#include <Ultralight/CAPI/CAPI_URL.h>`
///
/// ULURL represents a parsed web address conforming to the WHATWG specification. You'll use it to
/// validate web links and resolve relative paths before loading them into a View.
///
/// Parse an address string and load it into a View:
///
/// ```c
/// void LoadAddress(const char* input) {
///   ULString text = ulCreateString(input);  // eg, "https://example.com/docs"
///   ULURL link = ulCreateURL(text);
///   ulDestroyString(text);
///
///   if (ulURLIsValid(link)) {
///     ULString address = ulURLGetString(link);
///     ulViewLoadURL(view, address);
///     ulDestroyString(address);
///   }
///
///   ulDestroyURL(link);
/// }
/// ```
///
/// ## Creating and Destroying URLs
///
/// Ownership rules for URL handles and returned strings:
///
/// - **Destroy every created URL handle.** Functions in the ulCreateURL() family return owned
///   handles that you must release with ulDestroyURL().
/// - **Destroy handles even when parsing fails.** A failed parse still returns an allocated ULURL,
///   so check ulURLIsValid() before using the address.
/// - **Destroy strings returned by URL functions.** Getters like ulURLGetString() and
///   ulURLGetPath() return owned ULString handles rather than borrowed strings.
///
/// Resolve a relative path against the View's current URL and load the resolved address:
///
/// ```c
/// void OpenHelpPage(void) {
///   // Borrowed from the View, so we don't destroy it.
///   ULString current = ulViewGetURL(view);
///   ULURL base = ulCreateURL(current);
///
///   ULString relative = ulCreateString("../help/index.html");
///   ULURL help = ulCreateURLRelative(base, relative);
///   ulDestroyString(relative);
///
///   // Returned by a URL function, so we destroy it.
///   ULString help_address = ulURLGetString(help);
///   ulViewLoadURL(view, help_address);
///   ulDestroyString(help_address);
///
///   ulDestroyURL(help);
///   ulDestroyURL(base);
/// }
/// ```
///
/// ## Parsing and Encoding Rules
///
/// URL parsing, component access, and query building follow the same rules as the C++ interface
/// (see `<Ultralight/URL.h>`).
///
/// @note  Passing a NULL ULURL handle crashes every function that takes one, except ulDestroyURL(),
///        which accepts NULL and does nothing.
///
/// @see  `<Ultralight/URL.h>`, ulCreateURL(), ulDestroyURL(), ulViewLoadURL()
///
#ifndef ULTRALIGHT_CAPI_URL_H
#define ULTRALIGHT_CAPI_URL_H

#include <Ultralight/CAPI/CAPI_Defines.h>

#ifdef __cplusplus
extern "C" {
#endif

///
/// Opaque handle to a URL object.
///
/// @see  ulCreateURL(), ulDestroyURL()
///
typedef struct C_URL* ULURL;

/******************************************************************************
 * URL: creation and destruction
 *****************************************************************************/

///
/// Parse a string as an absolute URL (WHATWG-standard parsing).
///
/// Input without a scheme (eg, "example.com") does not parse. The result is then an invalid URL
/// object, which you can test with ulURLIsValid().
///
/// @param  url_string  The string to parse.
///
/// @return  Returns a new ULURL instance (never NULL). You must call ulDestroyURL() when
///          finished.
///
/// @warning  A host with non-ASCII characters (an international domain name) needs the library's
///           Unicode data, which the renderer loads. Parse such a URL only after you've called
///           ulCreateRenderer() or ulCreateApp().
///
ULExport ULURL ulCreateURL(ULString url_string);

///
/// Resolve a (possibly relative) reference against a base URL, following the same rules a
/// browser uses to resolve links on a page.
///
/// @param  base      The base URL to resolve against.
///
/// @param  relative  The reference to resolve: a path, an absolute path, a fragment, or a full
///                   absolute URL (which ignores the base).
///
/// @return  Returns a new ULURL instance (never NULL). The result is invalid when the base is
///          invalid, unless `relative` is itself an absolute URL. You must call ulDestroyURL()
///          when finished.
///
ULExport ULURL ulCreateURLRelative(ULURL base, ULString relative);

///
/// Create a file: URL from a native file-system path.
///
/// Platform path conventions are handled for you (eg, Windows drive letters and backslashes).
/// Use ulURLGetFilePath() for the reverse conversion.
///
/// @param  file_path  An absolute native file-system path.
///
/// @return  Returns a new ULURL instance (never NULL), invalid if the path cannot be
///          represented. You must call ulDestroyURL() when finished.
///
ULExport ULURL ulCreateURLFromFilePath(ULString file_path);

///
/// Create a copy of an existing URL.
///
/// @return  Returns a new ULURL instance. You must call ulDestroyURL() when finished.
///
ULExport ULURL ulCreateURLCopy(ULURL url);

///
/// Destroy a URL previously created with ulCreateURL(), ulCreateURLRelative(),
/// ulCreateURLFromFilePath(), or ulCreateURLCopy().
///
ULExport void ulDestroyURL(ULURL url);

/******************************************************************************
 * URL: validity and serialization
 *****************************************************************************/

///
/// Whether or not the URL parsed successfully.
///
ULExport bool ulURLIsValid(ULURL url);

///
/// Whether or not the URL holds nothing (equivalent to !ulURLIsValid()).
///
/// @note  A valid URL's canonical serialization is never empty.
///
ULExport bool ulURLIsEmpty(ULURL url);

///
/// Get the canonical serialization of the whole URL ("" if invalid).
///
/// @return  Returns a new string. You must call ulDestroyString() when finished.
///
ULExport ULString ulURLGetString(ULURL url);

///
/// Whether two URLs have the same canonical serialization.
///
/// @note  The comparison is textual, not a semantic equivalence test (two URLs that differ
///        only in fragment compare unequal).
///
ULExport bool ulURLEquals(ULURL url, ULURL other);

/******************************************************************************
 * URL: component reads
 *****************************************************************************/

///
/// Get the origin serialization, like JavaScript's `URL.origin`.
///
/// @return  Returns a new string. You must call ulDestroyString() when finished.
///
/// @note  Opaque origins (eg, data: URLs) serialize as "null".
///
ULExport ULString ulURLGetOrigin(ULURL url);

///
/// Get the scheme, lowercased, without the trailing colon (eg, "https"; "" if invalid).
///
/// @return  Returns a new string. You must call ulDestroyString() when finished.
///
ULExport ULString ulURLGetScheme(ULURL url);

///
/// Get the host, without the port (eg, "example.com"; "" if invalid or hostless).
///
/// @return  Returns a new string. You must call ulDestroyString() when finished.
///
ULExport ULString ulURLGetHost(ULURL url);

///
/// Get the host with the explicit port appended, if one is present (eg, "example.com:8080").
///
/// @return  Returns a new string. You must call ulDestroyString() when finished.
///
ULExport ULString ulURLGetHostWithPort(ULURL url);

///
/// Whether or not the URL has an explicit port.
///
/// @note  A scheme-default port is elided during canonicalization and does not count as
///        explicit.
///
ULExport bool ulURLHasPort(ULURL url);

///
/// Get the explicit port number (0 when ulURLHasPort() is false).
///
ULExport unsigned short ulURLGetPort(ULURL url);

///
/// Get the username portion of the URL's credentials, percent-decoded ("" if none).
///
/// @return  Returns a new string. You must call ulDestroyString() when finished.
///
ULExport ULString ulURLGetUsername(ULURL url);

///
/// Get the password portion of the URL's credentials, percent-decoded ("" if none).
///
/// @return  Returns a new string. You must call ulDestroyString() when finished.
///
ULExport ULString ulURLGetPassword(ULURL url);

///
/// Get the path (eg, "/a/b"; "" if invalid).
///
/// @return  Returns a new string. You must call ulDestroyString() when finished.
///
ULExport ULString ulURLGetPath(ULURL url);

///
/// Get the query string, without the leading '?' ("" if none).
///
/// Values inside the query are stored encoded. Use ulURLGetQueryParameter() to read a decoded
/// value.
///
/// @return  Returns a new string. You must call ulDestroyString() when finished.
///
ULExport ULString ulURLGetQuery(ULURL url);

///
/// Get the fragment, without the leading '#' ("" if none).
///
/// @return  Returns a new string. You must call ulDestroyString() when finished.
///
ULExport ULString ulURLGetFragment(ULURL url);

/******************************************************************************
 * URL: component writes
 *****************************************************************************/

///
/// Set the scheme (with or without the trailing colon).
///
/// The new scheme is validated and canonicalized (lowercased).
///
/// @return  Returns whether or not the scheme was accepted. On failure the URL is unchanged.
///
ULExport bool ulURLSetScheme(ULURL url, ULString scheme);

///
/// Set the host (without port).
///
/// Invalid input (eg, a stray colon) and empty input are ignored, leaving the URL unchanged.
///
/// @note  This has no effect on non-hierarchical URLs (eg, data:).
///
ULExport void ulURLSetHost(ULURL url, ULString host);

///
/// Set an explicit port.
///
/// A scheme-default port (eg, 443 for https) is elided from the serialization.
///
/// @note  This has no effect on non-hierarchical URLs.
///
ULExport void ulURLSetPort(ULURL url, unsigned short port);

///
/// Remove the explicit port, if any.
///
ULExport void ulURLClearPort(ULURL url);

///
/// Set the username portion of the URL's credentials (percent-encoded as needed).
///
/// Pass an empty string to remove it.
///
/// @note  This has no effect on non-hierarchical URLs.
///
ULExport void ulURLSetUsername(ULURL url, ULString username);

///
/// Set the password portion of the URL's credentials (percent-encoded as needed).
///
/// Pass an empty string to remove it.
///
/// @note  This has no effect on non-hierarchical URLs.
///
ULExport void ulURLSetPassword(ULURL url, ULString password);

///
/// Set the path.
///
/// The path is encoded as needed. On http(s) URLs an empty path becomes "/".
///
ULExport void ulURLSetPath(ULURL url, ULString path);

///
/// Set the raw query string (without the leading '?').
///
/// Pass an empty string to remove the query entirely. The value is used as-is, so you are
/// responsible for encoding it. To build a query from arbitrary text safely, use
/// ulURLAppendQueryParameter() instead.
///
ULExport void ulURLSetQuery(ULURL url, ULString query);

///
/// Set the fragment (without the leading '#').
///
/// Pass an empty string to remove the fragment entirely.
///
ULExport void ulURLSetFragment(ULURL url, ULString fragment);

/******************************************************************************
 * URL: query parameters
 *****************************************************************************/

///
/// Whether or not the query contains a parameter with the given key (form-decoded match).
///
ULExport bool ulURLHasQueryParameter(ULURL url, ULString key);

///
/// Get the first query-parameter value for the given key, form-decoded ("" if absent).
///
/// @return  Returns a new string. You must call ulDestroyString() when finished.
///
ULExport ULString ulURLGetQueryParameter(ULURL url, ULString key);

///
/// Append a key/value pair to the query, form-encoding both.
///
/// Spaces become `+` and reserved characters are percent-encoded, so this is the safe way to
/// build search or query URLs from arbitrary text.
///
/// @param  url    The URL handle.
///
/// @param  key    The parameter name (unencoded).
///
/// @param  value  The parameter value (unencoded).
///
ULExport void ulURLAppendQueryParameter(ULURL url, ULString key, ULString value);

/******************************************************************************
 * URL: file paths and component encoding
 *****************************************************************************/

///
/// Get the native file-system path for a file: URL.
///
/// @return  Returns a new string, "" for any other scheme or if the URL is invalid. You must
///          call ulDestroyString() when finished.
///
/// @see  ulCreateURLFromFilePath()
///
ULExport ULString ulURLGetFilePath(ULURL url);

///
/// Percent-encode a string for safe embedding inside a URL component.
///
/// This matches JavaScript's `encodeURIComponent`: every character except letters, digits, and
/// `- _ . ! ~ * ' ( )` is replaced by the percent-encoding of its UTF-8 bytes.
///
/// @return  Returns a new string. You must call ulDestroyString() when finished.
///
ULExport ULString ulURLEncodeComponent(ULString component);

///
/// Decode a percent-encoded URL component.
///
/// This matches JavaScript's `decodeURIComponent` for well-formed input. Unlike its JavaScript
/// counterpart this function never fails: a malformed escape is passed through unchanged.
///
/// @return  Returns a new string. You must call ulDestroyString() when finished.
///
ULExport ULString ulURLDecodeComponent(ULString encoded);

#ifdef __cplusplus
}
#endif

#endif  // ULTRALIGHT_CAPI_URL_H
