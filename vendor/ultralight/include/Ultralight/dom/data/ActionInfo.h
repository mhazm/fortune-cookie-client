///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/CAPI/CAPI_DOMData.h>

#include <cstdint>
#include <string_view>

namespace ultralight {
namespace dom {
namespace data {

///
/// Details of a delivered action (its name and the list row it fired from).
///
/// Take one as the second parameter of a generic action handler:
///
/// ```
/// binding.OnAction<"roster.kick">([&](dd::Value payload, dd::ActionInfo info) {
///   if (info.has_row())
///     KickPlayer(info.row_int_key());
/// });
/// ```
///
/// An action fired from `ul-on:*` markup never has a payload, and it doesn't say which View's
/// page fired it.
///
class ActionInfo {
 public:
  ///
  /// Get the action's name.
  ///
  /// @return  Returns the name from the schema. For a row action this is its name in the row
  ///          type's schema (eg, `kick` for `roster.kick`).
  ///
  const char* name() const { return name_; }

  ///
  /// Whether or not the action fired from a list row.
  ///
  bool has_row() const { return has_row_; }

  ///
  /// Whether or not the row's key is a string (see row_string_key()).
  ///
  bool row_key_is_string() const { return row_is_string_; }

  ///
  /// Get the row's integer key.
  ///
  /// @return  Returns the key (for a list without keys, the row's position when the page fired
  ///          the action), or 0 when the key is a string or there's no row.
  ///
  int64_t row_int_key() const { return int_key_; }

  ///
  /// Get the row's string key.
  ///
  /// @return  Returns the key, or an empty view when the key isn't a string or there's no row.
  ///
  /// @note  The view is valid only during the handler call. Copy it to keep it.
  ///
  std::string_view row_string_key() const { return string_key_; }

  /// \cond INTERNAL
  ActionInfo(const char* name, const ULDOMDataActionSource* source) : name_(name) {
    if (source) {
      has_row_ = true;
      row_is_string_ = source->is_string_key;
      int_key_ = source->int_key;
      if (source->is_string_key && source->string_key)
        string_key_ = std::string_view(source->string_key, source->string_key_length);
    }
  }
  /// \endcond

 private:
  const char* name_ = "";
  bool has_row_ = false;
  bool row_is_string_ = false;
  int64_t int_key_ = 0;
  std::string_view string_key_;
};

}  // namespace data
}  // namespace dom
}  // namespace ultralight
