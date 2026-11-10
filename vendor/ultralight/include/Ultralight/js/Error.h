///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/CAPI/CAPI_JSAPI.h>
#include <Ultralight/CAPI/CAPI_JSValue.h>
#include <Ultralight/CAPI/CAPI_String.h>
#include <Ultralight/detail/Expected.h>
#include <Ultralight/js/detail/Swallow.h>

#include <string>
#include <type_traits>
#include <utility>

// The Expected/Unexpected implementation is shared across the typed header families and
// lives in <Ultralight/detail/Expected.h>; the js/ names below alias it. The reader-facing
// story lives on the js::Result alias doc at the end of this header.

namespace ultralight {
namespace js {

/// \cond INTERNAL
namespace detail {
struct ErrorAccess;

// The details of a thrown value, read once and released on destruction. Everything reads as
// empty when there's no exception or its page is gone.
class ExceptionDetails {
 public:
  explicit ExceptionDetails(ULJSValue exception) {
    if (exception && ulJSValueIsAlive(exception))
      ulJSValueGetErrorDetails(exception, &raw_);
  }
  ~ExceptionDetails() { ulJSErrorDetailsRelease(&raw_); }
  ExceptionDetails(const ExceptionDetails&) = delete;
  ExceptionDetails& operator=(const ExceptionDetails&) = delete;

  const ULJSErrorDetails& raw() const { return raw_; }

  static std::string Text(ULString s) {
    return s ? std::string(ulStringGetData(s), ulStringGetLength(s)) : std::string();
  }

 private:
  ULJSErrorDetails raw_ {};
};
}  // namespace detail
/// \endcond

///
/// The type of a JavaScript error, matching the standard error constructors.
///
enum class ErrorType : unsigned {
  Error = 0,    ///< A plain Error, or an error type with no dedicated constant.
  TypeError,
  RangeError,
  SyntaxError,
  ReferenceError,
};

///
/// An error from a JavaScript operation.
///
/// The library never throws C++ exceptions. Operations that can fail return a js::Result, and
/// its Error is one of four kinds:
///
///  - **A JavaScript exception** (is_exception() is true): script threw, or the call was
///    invalid in JavaScript (eg, calling something that isn't a function). exception() holds
///    the thrown value, and type(), stack(), source_url(), line(), and column() describe it.
///  - **Empty handle** (is_empty() is true): the Value or Context you called it on holds nothing
///    (see js::Value). This usually means a bug in your code.
///  - **Page gone** (is_page_gone() is true): the handle's page is gone (see js::Value).
///  - **A native error** (all three are false): an error created in C++, by you (TypeError(),
///    RangeError(), Make()) or by the library (eg, when To() gets the wrong type).
///
/// A page can go away at any time, so a page-gone error usually needs no report. Report the rest
/// with message():
///
/// ```
/// js::Result<std::string> title = value.ToString();
/// if (!title) {
///   if (title.error().is_page_gone())
///     return;                              // the page went away; nothing to report
///   Log(title.error().message());          // script threw or the Value was empty: report it
/// }
/// ```
///
/// A bound function throws an Error into the page by returning it in a js::Result. Use
/// WithCode() to give it a `code` property, so page script can check the kind of error without
/// parsing the message.
///
/// ## Error Codes
///
/// The TypeErrors the library creates have a `code` property (read it with code() in native
/// code, or `error.code` in page script). Check the code rather than the message, since message
/// text is written for people and can change in any release. The codes below stay the same
/// (new ones may be added).
///
/// | Code                 | Meaning                                                             |
/// |----------------------|---------------------------------------------------------------------|
/// | `ULJS_BAD_ARG`       | A value doesn't have the expected type, or its text doesn't parse   |
/// | `ULJS_LOSSY`         | A number doesn't fit the parameter's type (Strict diagnostics only) |
/// | `ULJS_EXTRA_ARGS`    | A call passed more arguments than the binding takes (Strict only)   |
/// | `ULJS_DETACHED`      | The native object or API behind a binding or instance is gone       |
/// | `ULJS_NOT_INSTANCE`  | A class method was called on an object that isn't an instance       |
/// | `ULJS_NO_CTOR`       | Page script used `new` on a class that has no constructor           |
/// | `ULJS_CROSS_CONTEXT` | A value was used on a page it can't be shared with (see js::Value)  |
///
/// @note  When JavaScript diagnostics are on (see js::Diagnostics; they're on by default in
///        developer mode), an Error that holds a JavaScript exception logs it if it's
///        destroyed before anything reads it (any accessor, such as message(), counts). So a
///        dropped Result never hides a script error during development.
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
  /// Create an error from a JavaScript exception, taking ownership of the handle.
  ///
  /// @param  exception  The thrown value. The Error destroys the handle when it's done with it.
  ///
  /// @return  Returns the new Error.
  ///
  static Error AdoptException(ULJSValue exception) {
    Error error(Kind::kException);
    error.exception_ = exception;
    return error;
  }

  ///
  /// Create a native TypeError.
  ///
  /// @param  message  The description, returned by message().
  ///
  /// @return  Returns the new Error.
  ///
  static Error TypeError(std::string message) {
    return Error(ErrorType::TypeError, std::move(message));
  }

  ///
  /// Create a native RangeError.
  ///
  /// @param  message  The description, returned by message().
  ///
  /// @return  Returns the new Error.
  ///
  static Error RangeError(std::string message) {
    return Error(ErrorType::RangeError, std::move(message));
  }

  ///
  /// Create a native error of any type.
  ///
  /// @param  type     The error type, returned by type().
  ///
  /// @param  message  The description, returned by message().
  ///
  /// @return  Returns the new Error.
  ///
  static Error Make(ErrorType type, std::string message) {
    return Error(type, std::move(message));
  }

  ///
  /// Move constructor (`other` no longer holds the exception).
  ///
  Error(Error&& other) noexcept
      : kind_(other.kind_), type_(other.type_), exception_(other.exception_),
        message_(std::move(other.message_)), code_(std::move(other.code_)),
        observed_(other.observed_) {
    other.exception_ = nullptr;
  }

  ///
  /// Move assignment (`other` no longer holds the exception).
  ///
  Error& operator=(Error&& other) noexcept {
    if (this != &other) {
      ReleaseException();
      kind_ = other.kind_;
      type_ = other.type_;
      exception_ = other.exception_;
      message_ = std::move(other.message_);
      code_ = std::move(other.code_);
      observed_ = other.observed_;
      other.exception_ = nullptr;
    }
    return *this;
  }

  ///
  /// Copy constructor (takes its own reference to the exception, if any).
  ///
  Error(const Error& other)
      : kind_(other.kind_), type_(other.type_),
        exception_(other.exception_ ? ulCreateJSValueRef(other.exception_) : nullptr),
        message_(other.message_), code_(other.code_) {
    other.observed_ = true;
  }

  ///
  /// Copy assignment (takes its own reference to the exception, if any).
  ///
  Error& operator=(const Error& other) {
    if (this != &other)
      *this = Error(other);
    return *this;
  }

  ///
  /// Destructor (logs the exception first if nothing read it; see the class notes).
  ///
  ~Error() { ReleaseException(); }

  ///
  /// Add a code that page script gets as the error's `code` property when it's thrown.
  ///
  /// ```
  /// return js::Unexpected(js::Error::TypeError("bad slot").WithCode("BAD_SLOT"));
  /// ```
  ///
  /// @param  code  The code, returned by code().
  ///
  /// @return  Returns the Error with the code added.
  ///
  Error WithCode(std::string code) && {
    code_ = std::move(code);
    return std::move(*this);
  }

  ///
  /// Whether or not this error holds a JavaScript exception.
  ///
  bool is_exception() const {
    observed_ = true;
    return kind_ == Kind::kException;
  }

  ///
  /// Whether or not this error means the handle's page is gone.
  ///
  bool is_page_gone() const {
    observed_ = true;
    return kind_ == Kind::kPageGone;
  }

  ///
  /// Whether or not this error means the handle holds nothing (see the class notes).
  ///
  bool is_empty() const {
    observed_ = true;
    return kind_ == Kind::kEmpty;
  }

  ///
  /// Get the JavaScript exception (NULL if is_exception() is false).
  ///
  /// @note  The Error owns the handle. Call ulCreateJSValueRef() to keep it longer.
  ///
  ULJSValue exception() const {
    observed_ = true;
    return exception_;
  }

  ///
  /// Get the error's type.
  ///
  /// For a JavaScript exception, this is the class of the thrown error. It reads as
  /// ErrorType::Error for any other class, for a thrown value that isn't an error object, and
  /// once the exception's page is gone.
  ///
  /// @note  For a thrown value that isn't an error object, this converts it to a string, which
  ///        can run script.
  ///
  ErrorType type() const {
    observed_ = true;
    if (kind_ != Kind::kException)
      return type_;
    return static_cast<ErrorType>(detail::ExceptionDetails(exception_).raw().type);
  }

  ///
  /// Get the stack trace of a JavaScript exception.
  ///
  /// @return  Returns the stack trace (an empty string for a native error, a thrown value that
  ///          isn't an error object, or once the exception's page is gone).
  ///
  /// @note  For a thrown value that isn't an error object, this converts it to a string, which
  ///        can run script.
  ///
  std::string stack() const {
    observed_ = true;
    detail::ExceptionDetails details(exception_);
    return detail::ExceptionDetails::Text(details.raw().stack);
  }

  ///
  /// Get the URL of the script that created a JavaScript exception.
  ///
  /// @return  Returns the URL (an empty string if it's unknown, for a native error, or once the
  ///          exception's page is gone).
  ///
  /// @note  For a thrown value that isn't an error object, this converts it to a string, which
  ///        can run script.
  ///
  std::string source_url() const {
    observed_ = true;
    detail::ExceptionDetails details(exception_);
    return detail::ExceptionDetails::Text(details.raw().source_url);
  }

  ///
  /// Get the 1-based line where a JavaScript exception was created.
  ///
  /// @return  Returns the line (0 if it's unknown, for a native error, or once the exception's
  ///          page is gone).
  ///
  /// @note  For a thrown value that isn't an error object, this converts it to a string, which
  ///        can run script.
  ///
  unsigned line() const {
    observed_ = true;
    return detail::ExceptionDetails(exception_).raw().line;
  }

  ///
  /// Get the 1-based column where a JavaScript exception was created.
  ///
  /// @return  Returns the column (0 if it's unknown, for a native error, or once the
  ///          exception's page is gone).
  ///
  /// @note  For a thrown value that isn't an error object, this converts it to a string, which
  ///        can run script.
  ///
  unsigned column() const {
    observed_ = true;
    return detail::ExceptionDetails(exception_).raw().column;
  }

  ///
  /// Get the error's code (an empty string if it has none).
  ///
  /// For a native error, this is the code from WithCode(). For a JavaScript exception, it's the
  /// thrown value's `code` property, if that's a string of up to 64 bytes. The errors the
  /// library creates use the codes listed in the class notes.
  ///
  /// @note  Reading a JavaScript exception's property can run a getter.
  ///
  std::string code() const {
    observed_ = true;
    if (kind_ == Kind::kException && exception_ && ulJSValueIsAlive(exception_)) {
      detail::SwallowedException swallow;
      ULJSValue prop = ulJSObjectGetProperty(exception_, "code", swallow.out());
      if (prop) {
        char buffer[64];
        size_t length = 0;
        bool ok = ulJSValueGetType(prop) == kULJSType_String
            && ulJSValueGetUTF8(prop, buffer, sizeof(buffer), &length, swallow.out());
        ulDestroyJSValue(prop);
        if (ok)
          return std::string(buffer, length);
      }
    }
    return code_;
  }

  ///
  /// Convert this error to a JavaScript value.
  ///
  /// For a JavaScript exception whose page is alive, you get the thrown value. Otherwise you get a
  /// new error object with type(), message(), and the code from WithCode().
  ///
  /// @param  ctx  The context to create the value in.
  ///
  /// @return  Returns a new handle you must destroy with ulDestroyJSValue() (NULL if `ctx` is
  ///          gone).
  ///
  /// @pre  Must be called on the Renderer's thread.
  ///
  ULJSValue ToJS(ULJSContext ctx) const {
    observed_ = true;
    if (kind_ == Kind::kException && exception_ && ulJSValueIsAlive(exception_))
      return ulCreateJSValueRef(exception_);
    static_assert(static_cast<unsigned>(ErrorType::Error) == kULJSErrorType_Error
                      && static_cast<unsigned>(ErrorType::TypeError) == kULJSErrorType_TypeError
                      && static_cast<unsigned>(ErrorType::RangeError)
                          == kULJSErrorType_RangeError
                      && static_cast<unsigned>(ErrorType::SyntaxError)
                          == kULJSErrorType_SyntaxError
                      && static_cast<unsigned>(ErrorType::ReferenceError)
                          == kULJSErrorType_ReferenceError,
                  "js::ErrorType must mirror ULJSErrorType");
    ULJSValue error = ulCreateJSError(ctx, static_cast<ULJSErrorType>(type_),
                                      message().c_str());
    if (error && !code_.empty()) {
      detail::SwallowedException swallow;
      ULJSValue code_value = ulCreateJSValueStringUTF8(ctx, code_.data(), code_.size());
      ulJSObjectSetProperty(error, "code", code_value, kULJSPropertyAttributes_None,
                            swallow.out());
      ulDestroyJSValue(code_value);
    }
    return error;
  }

  ///
  /// Get a readable description of the error.
  ///
  /// For a JavaScript exception, this converts the thrown value to a string (which can run
  /// script) and falls back to "JavaScript exception" if that fails.
  ///
  std::string message() const {
    observed_ = true;
    if (kind_ == Kind::kException) {
      detail::SwallowedException swallow;
      if (ULString s = ulJSValueToString(exception_, swallow.out())) {
        std::string result(ulStringGetData(s), ulStringGetLength(s));
        ulDestroyString(s);
        return result;
      }
      return "A JavaScript exception was thrown.";
    }
    if (kind_ == Kind::kPageGone)
      return "The page behind this handle is gone (it navigated away, its frame was removed, or "
             "its View was destroyed).";
    if (kind_ == Kind::kEmpty)
      return "The handle is empty (it was never assigned, was moved from, or came from a read "
             "that failed).";
    return message_;
  }

 private:
  friend struct detail::ErrorAccess;

  enum class Kind { kNative, kException, kPageGone, kEmpty };

  explicit Error(Kind kind) : kind_(kind) {}
  Error(ErrorType type, std::string message)
      : kind_(Kind::kNative), type_(type), message_(std::move(message)) {}

  void ReleaseException() {
    if (exception_ && !observed_)
      ulJSValueReportUnobservedException(exception_);
    ulDestroyJSValue(exception_);
    exception_ = nullptr;
  }

  Kind kind_ = Kind::kNative;
  ErrorType type_ = ErrorType::Error;
  ULJSValue exception_ = nullptr;
  std::string message_;
  std::string code_;
  // Set by every accessor, so the destructor can tell a read error from a dropped one.
  mutable bool observed_ = false;
};

/// \cond INTERNAL
namespace detail {

struct ErrorAccess {
  static Error Empty() { return Error(Error::Kind::kEmpty); }
};

// The typed layer checks for an empty handle before the C call, since the C API reports an
// empty handle and a page that's gone as the same failure.
inline ultralight::detail::Unexpected<Error> EmptyHandleError() {
  return ultralight::detail::Unexpected<Error>(ErrorAccess::Empty());
}

// Logs a C++ exception that escaped native code the library called. `where` names the call
// (null when there's nothing to name); `what` is what() for a std::exception, else null.
inline void LogNativeException(const char* where, const char* what) {
  std::string log = std::string("C++ exception in ") + (where ? where : "native code") + ": "
      + (what ? what : "unknown exception") + ". The library caught it and continued.";
  ulJSAPIEmitDiagnostic(nullptr, log.c_str());
}

// The error for a C++ exception that escaped native code called from the page (a bound
// callable, `path` naming it, or a Task body, `path` null). The page gets a generic message,
// since what() can hold native details the page shouldn't see; the native log gets what().
inline Error NativeExceptionError(const char* path, const char* what) {
  LogNativeException(path, what);
  std::string prefix = path ? std::string(path) + ": " : std::string();
  return Error::Make(ErrorType::Error, prefix + "the native code failed");
}

}  // namespace detail
/// \endcond

///
/// Wraps an error to return it through a js::Result (like std::unexpected).
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
/// The result of a JavaScript operation that can fail: a T or a js::Error.
///
/// Result is js::Expected<T, js::Error>, the library's own type with the members of
/// std::expected (has_value(), value(), error(), value_or(), and_then(), transform(), and the
/// rest). It's the same type in every file of your program, whichever C++ standard each file is
/// compiled with.
///
template <typename T>
using Result = Expected<T, Error>;

///
/// Get a Result's value, or a default-constructed T if it holds an error.
///
/// Use this where an empty value is a fine way to mark failure (every operation on an empty
/// Value fails safely):
///
/// ```
/// js::Value fn = js::OrEmpty(ctx.Evaluate("(x => x * 2)"));
/// double n = js::OrEmpty(ctx.Evaluate("rawAdd(2, 3)")).Or(0.0);
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

}  // namespace js
}  // namespace ultralight
