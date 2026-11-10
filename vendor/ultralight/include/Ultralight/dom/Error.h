///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
// X11's headers define None as a macro, which would break the None enumerators.
#pragma push_macro("None")
#undef None

#include <Ultralight/CAPI/CAPI_DOMDocument.h>
#include <Ultralight/CAPI/CAPI_String.h>
#include <Ultralight/detail/Expected.h>
#include <Ultralight/dom/detail/Support.h>

#include <string>
#include <type_traits>
#include <utility>

// The Expected/Unexpected implementation is shared across the typed header families and
// lives in <Ultralight/detail/Expected.h>; the dom/ names below alias it. The reader-facing
// story lives on the dom::Result alias doc at the end of this header.

namespace ultralight {
namespace dom {

/// \cond INTERNAL
namespace detail {
class ErrorScope;
}  // namespace detail
/// \endcond

///
/// The kinds of DOM exception a dom::Error can hold (the web's DOMException names, plus TypeError).
///
/// The values match ULDOMErrorCode in the C API. They stay the same across releases. Later
/// versions may add types, so handle a value you don't know (eg, with a `default` case).
///
enum class ErrorType : unsigned {
  None = 0,                    ///< Not a DOM exception (eg, a page-gone error).
  Unknown,                     ///< A DOM exception with no code of its own here.
  SyntaxError,                 ///< A malformed selector, or an invalid position for
                               ///< Element::insertAdjacentHTML() and its siblings.
  InvalidCharacterError,       ///< An invalid tag, attribute, or class name (only the C API
                               ///< reports it, since the typed calls that take these names
                               ///< have no dom::Checked form).
  HierarchyRequestError,       ///< The tree would contain a cycle, or the node can't go there
                               ///< (eg, text directly under the document).
  NotFoundError,               ///< A node isn't where the call needs it (eg, a reference child
                               ///< that isn't a child).
  NoModificationAllowedError,  ///< The target can't be changed (eg, it has no parent).
  InvalidStateError,           ///< The call isn't valid in the target's current state.
  IndexSizeError,              ///< An index or offset is out of range.
  NotSupportedError,           ///< The operation isn't supported.
  TypeError,                   ///< An argument has the wrong type (eg, a submitter passed to
                               ///< HTMLFormElement::requestSubmit() that isn't a submit
                               ///< button).
  InvalidNodeTypeError,        ///< The node is the wrong kind for the call (eg, selecting a
                               ///< node that has no parent).
  WrongDocumentError,          ///< A node or range belongs to a different document.
};

///
/// The reason a DOM operation failed.
///
/// dom::Error holds the details of a failed DOM operation, such as a DOM exception or a handle
/// whose page is gone.
///
/// Passing dom::Checked returns a dom::Result you can inspect on failure:
///
/// ```
/// dom::Result<dom::Element> total =
///     document.querySelector("#scores td.total", dom::Checked);
/// if (!total) {
///   if (total.error().is_page_gone())
///     return;                          // the page went away (nothing to report)
///   Log(total.error().message());      // eg, a malformed selector
/// }
/// ```
///
/// ## Getting an Error
///
/// DOM operations return an empty value on failure instead of throwing C++ exceptions. Most code
/// never needs dom::Error because operations on empty values fail safely without extra checks.
///
/// To inspect a failure, pass dom::Checked as an extra argument to receive a dom::Result holding
/// either the value or a dom::Error.
///
/// When you've inspected the error and want to continue with an empty value, pass the dom::Result
/// to dom::OrEmpty().
///
/// ## Kinds of Errors
///
/// Specific query methods identify each failure condition:
///
/// | Kind          | How to Check                                                |
/// |---------------|-------------------------------------------------------------|
/// | DOM exception | is_dom_exception() is true, and type() gives the code       |
/// | Empty handle  | is_empty() is true                                          |
/// | Page gone     | is_page_gone() is true                                      |
/// | Native error  | Created by Make() or the library, other checks return false |
///
/// How you handle an error depends on its condition:
///
/// - **Ignore page-gone errors in routine code.** A page can navigate away at any moment, leaving
///   nothing to report.
/// - **Treat empty-handle errors as application bugs.** They indicate that native code operated on
///   an unassigned handle, a moved-from handle, or a lookup that found nothing.
/// - **Inspect type() rather than message() in code.** The message text is meant for human
///   diagnostics and can change between releases, while type() returns stable dom::ErrorType
///   values.
///
/// @note  An error you get through dom::Checked isn't logged by DOM diagnostics.
///
/// @see  dom::Checked, dom::Result, dom::ErrorType, dom::OrEmpty()
///
class Error {
 public:
  ///
  /// Create a page-gone error (is_page_gone() returns true).
  ///
  /// @return  Returns the new Error.
  ///
  static Error PageGone() { return Error(Kind::kPageGone); }

  ///
  /// Create an error from a DOM exception reported by the C API.
  ///
  /// @param  error  The ULDOMError a C function filled in. The new Error takes its message, and
  ///                `error` is reset to zero (so don't destroy the message yourself).
  ///
  /// @return  Returns the new Error (is_dom_exception() returns true).
  ///
  static Error AdoptDOMException(ULDOMError& error) {
    Error result(Kind::kDOMException);
    result.type_ = static_cast<ErrorType>(error.code);
    result.message_ = detail::TakeString(error.message);
    error.code = kULDOMErrorCode_None;
    error.message = nullptr;
    return result;
  }

  ///
  /// Create a native error.
  ///
  /// @param  type     The error type, returned by type().
  ///
  /// @param  message  The description, returned by message().
  ///
  /// @return  Returns the new Error.
  ///
  static Error Make(ErrorType type, std::string message) {
    Error result(Kind::kNative);
    result.type_ = type;
    result.message_ = std::move(message);
    return result;
  }

  ///
  /// Whether or not this error means the handle's page is gone.
  ///
  bool is_page_gone() const { return kind_ == Kind::kPageGone; }

  ///
  /// Whether or not this error means a handle holds nothing (see the class notes).
  ///
  bool is_empty() const { return kind_ == Kind::kEmpty; }

  ///
  /// Whether or not this error holds a DOM exception.
  ///
  bool is_dom_exception() const { return kind_ == Kind::kDOMException; }

  ///
  /// Get the error's type.
  ///
  /// @return  Returns the kind of DOM exception, the type passed to Make(), or ErrorType::None
  ///          for a page-gone or empty-handle error.
  ///
  ErrorType type() const { return type_; }

  ///
  /// Get a readable description of the error.
  ///
  /// A DOM exception without a message of its own reads as its type (eg, `SyntaxError`). The text
  /// is written for people and can change in any release, so check type() in code instead.
  ///
  std::string message() const {
    if (kind_ == Kind::kPageGone)
      return "The page behind this handle is gone (it navigated away, its frame was removed, or "
             "its View was destroyed).";
    if (kind_ == Kind::kEmpty)
      return "A handle used in this call is empty (it was never assigned, was moved from, or "
             "came from a lookup that found nothing).";
    if (message_.empty() && kind_ == Kind::kDOMException)
      return TypeName(type_);
    return message_;
  }

 private:
  friend class detail::ErrorScope;

  enum class Kind { kNative, kDOMException, kPageGone, kEmpty };

  explicit Error(Kind kind) : kind_(kind) {}

  static const char* TypeName(ErrorType type) {
    switch (type) {
      case ErrorType::SyntaxError: return "SyntaxError";
      case ErrorType::InvalidCharacterError: return "InvalidCharacterError";
      case ErrorType::HierarchyRequestError: return "HierarchyRequestError";
      case ErrorType::NotFoundError: return "NotFoundError";
      case ErrorType::NoModificationAllowedError: return "NoModificationAllowedError";
      case ErrorType::InvalidStateError: return "InvalidStateError";
      case ErrorType::IndexSizeError: return "IndexSizeError";
      case ErrorType::NotSupportedError: return "NotSupportedError";
      case ErrorType::TypeError: return "TypeError";
      case ErrorType::InvalidNodeTypeError: return "InvalidNodeTypeError";
      case ErrorType::WrongDocumentError: return "WrongDocumentError";
      default: return "A DOM exception occurred.";
    }
  }

  Kind kind_ = Kind::kNative;
  ErrorType type_ = ErrorType::None;
  std::string message_;
};

static_assert(static_cast<unsigned>(ErrorType::None) == kULDOMErrorCode_None
                  && static_cast<unsigned>(ErrorType::Unknown) == kULDOMErrorCode_Unknown
                  && static_cast<unsigned>(ErrorType::SyntaxError) == kULDOMErrorCode_SyntaxError
                  && static_cast<unsigned>(ErrorType::InvalidCharacterError)
                      == kULDOMErrorCode_InvalidCharacterError
                  && static_cast<unsigned>(ErrorType::HierarchyRequestError)
                      == kULDOMErrorCode_HierarchyRequestError
                  && static_cast<unsigned>(ErrorType::NotFoundError)
                      == kULDOMErrorCode_NotFoundError
                  && static_cast<unsigned>(ErrorType::NoModificationAllowedError)
                      == kULDOMErrorCode_NoModificationAllowedError
                  && static_cast<unsigned>(ErrorType::InvalidStateError)
                      == kULDOMErrorCode_InvalidStateError
                  && static_cast<unsigned>(ErrorType::IndexSizeError)
                      == kULDOMErrorCode_IndexSizeError
                  && static_cast<unsigned>(ErrorType::NotSupportedError)
                      == kULDOMErrorCode_NotSupportedError
                  && static_cast<unsigned>(ErrorType::TypeError) == kULDOMErrorCode_TypeError
                  && static_cast<unsigned>(ErrorType::InvalidNodeTypeError)
                      == kULDOMErrorCode_InvalidNodeTypeError
                  && static_cast<unsigned>(ErrorType::WrongDocumentError)
                      == kULDOMErrorCode_WrongDocumentError,
              "dom::ErrorType must mirror ULDOMErrorCode");

///
/// Wraps an error to return it through a dom::Result (like std::unexpected).
///
using ultralight::detail::Unexpected;

///
/// A value of type T or an error of type E (like std::expected).
///
/// @warning  Don't call value() when has_value() is false. That's undefined behavior, since the
///           library never throws C++ exceptions. Check first or use value_or().
///
template <typename T, typename E = Error>
using Expected = ultralight::detail::Expected<T, E>;

///
/// The result of a DOM operation that can fail (either a T or a dom::Error).
///
/// DOM calls return one when you pass dom::Checked.
///
/// Result is dom::Expected<T, dom::Error>, the library's own type with the members of
/// std::expected (has_value(), value(), error(), value_or(), and_then(), transform(), and the
/// rest). It's the same type in every file of your program, whichever C++ standard each file is
/// compiled with.
///
template <typename T>
using Result = Expected<T, Error>;

///
/// The type of dom::Checked.
///
struct Checked_t {
  ///
  /// Create the tag (use dom::Checked instead).
  ///
  explicit constexpr Checked_t() = default;
};

///
/// Pass to a DOM call to get a dom::Result holding the reason for a failure instead of an empty
/// value.
///
/// ```
/// dom::Result<dom::Element> title = doc.querySelector(".panel h1", dom::Checked);
/// if (!title)
///   Log(title.error().message());   // eg, a SyntaxError for a malformed selector
/// ```
///
inline constexpr Checked_t Checked{};

///
/// Get a Result's value, or a default-constructed T if it holds an error.
///
/// Use this when you've handled the error and want to carry on with an empty value (every
/// operation on an empty Element does nothing):
///
/// ```
/// dom::Result<dom::Element> row = doc.querySelector(selector, dom::Checked);
/// if (!row && !row.error().is_page_gone())
///   Log(row.error().message());
/// dom::Element cell = dom::OrEmpty(row);
/// ```
///
/// @param  result  The Result to read.
///
/// @return  Returns the value or a default-constructed T.
///
template <typename T>
  requires std::is_default_constructible_v<T>
[[nodiscard]] T OrEmpty(Result<T> result) {
  return std::move(result).value_or(T());
}

/// \cond INTERNAL
namespace detail {

// Wraps the zero-initialize-then-check ULDOMError protocol of the C API: the struct is
// written only on DOM-exception-level failure, so a nonzero code afterward reliably means
// a DOM exception occurred. Destroys an unconsumed message.
class ErrorScope {
 public:
  ErrorScope() = default;
  ErrorScope(const ErrorScope&) = delete;
  ErrorScope& operator=(const ErrorScope&) = delete;
  ~ErrorScope() { ulDestroyString(error_.message); }

  ULDOMError* out() { return &error_; }

  bool has_exception() const { return error_.code != kULDOMErrorCode_None; }

  // On DOM-exception failure, the exception; otherwise the engine-level failure.
  Error TakeError() {
    if (has_exception())
      return Error::AdoptDOMException(error_);
    return Error::PageGone();
  }

  static Error EmptyHandle() { return Error(Error::Kind::kEmpty); }

 private:
  ULDOMError error_ = {};
};

// The typed layer checks the receiver and the handle arguments a call needs before the C call,
// since the C API reports an empty handle and a page that's gone as the same failure.
inline ultralight::detail::Unexpected<Error> EmptyHandleError() {
  return ultralight::detail::Unexpected<Error>(ErrorScope::EmptyHandle());
}

// Whether a ULDOMNodeOrString list holds an empty node handle (an item with neither a node nor
// text).
template <typename Items>
bool HasEmptyItem(const Items& items) {
  for (const auto& item : items) {
    if (!item.node && !item.text)
      return true;
  }
  return false;
}

}  // namespace detail
/// \endcond

}  // namespace dom
}  // namespace ultralight

#pragma pop_macro("None")
