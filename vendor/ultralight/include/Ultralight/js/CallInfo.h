///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/CAPI/CAPI_JSValue.h>
#include <Ultralight/js/Value.h>

namespace ultralight {
namespace js {

class Context;

///
/// Details of a call to a bound function: the context, `this`, and the raw arguments.
///
/// Add a CallInfo parameter to a bound function to receive it, either first or after the other
/// parameters. It isn't a JavaScript argument, so the other parameters still convert from the
/// page's arguments as usual:
///
/// ```
/// api["inspect"] = [](std::string name, js::CallInfo info) {
///   return (double)info.arg_count();     // `name` still converts from argument 1
/// };
/// ```
///
/// Declare only one. With a trailing js::Resolver the order is (arguments..., CallInfo,
/// Resolver).
///
/// \parblock
/// @note  A CallInfo and the js::Arg values it gives you are valid only during the call (in a
///        js::Task, until the Task finishes). Use Arg::ToValue() to keep an argument. You can
///        keep the Context that context() returns.
/// \endparblock
///
/// \parblock
/// @note  A function that takes a CallInfo receives every argument the page passes, so Strict
///        diagnostics don't reject extra arguments for it (see js::Diagnostics).
/// \endparblock
///
class CallInfo {
 public:
  /// @cond INTERNAL
  // Constructed by the binding layer; a bound callable receives a CallInfo, never builds one.
  CallInfo(ULJSContext ctx, ULJSValue this_value, const ULJSValue* args, size_t argc)
      : ctx_(ctx), this_value_(this_value), args_(args), argc_(argc) {}
  /// @endcond

  ///
  /// Get the calling context, to create values in it or to keep it after the call:
  ///
  /// ```
  /// api["build"] = [](js::CallInfo info) {
  ///   return info.context().Make(std::vector<double> { 1, 2, 3 });
  /// };
  /// ```
  ///
  /// @return  Returns a new reference to the context.
  ///
  /// @note  Requires `<Ultralight/js/Context.h>` (or `<Ultralight/JS.h>`).
  ///
  Context context() const;

  ///
  /// Get the call's `this` value.
  ///
  Arg this_value() const { return Arg(this_value_); }

  ///
  /// Get the number of arguments the page passed.
  ///
  size_t arg_count() const { return argc_; }

  ///
  /// Get an argument.
  ///
  /// @param  index  The zero-based argument position.
  ///
  /// @return  Returns the argument (an empty Arg if `index` is out of range).
  ///
  Arg arg(size_t index) const { return Arg(index < argc_ ? args_[index] : nullptr); }

 private:
  ULJSContext ctx_;
  ULJSValue this_value_;
  const ULJSValue* args_;
  size_t argc_;
};

}  // namespace js
}  // namespace ultralight
