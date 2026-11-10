///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/String.h>
#include <Ultralight/detail/Css.h>

#include <cstddef>
#include <cstdint>

namespace ultralight {

class Flex;

///
/// A distance or size constraint in an AppCore layout.
///
/// You use Length to set container padding and gap, or to define minimum and maximum size
/// constraints on panels and containers.
///
/// Numbers and CSS string literals convert to Length automatically, so you can write values
/// directly in layout options.
///
/// This example sets a row's spacing and constrains a child panel:
///
/// ```
/// RefPtr<Container> body =
///     window->layout()->AddRow({ .gap = 8, .padding = "12px" });
/// body->AddPanel({ .min_size = "160px", .max_size = "40%" });
/// ```
///
/// ## Supported Units
///
/// A Length is measured in one of two units:
///
/// - **Logical pixels represent fixed distances.** Bare numbers, Length::Px(), and string literals
///   with `"px"` specify logical pixels. Container gap and padding accept logical pixels only.
/// - **Percentages are relative to the container's content box (inside its padding).** Unlike a
///   Size percent, which is a share of the container's free space, a Length percent uses the whole
///   box.
///
/// Length doesn't accept flex units-- a flex factor isn't a distance, and passing `"fr"` or a Flex
/// produces a compile error.
///
/// ## Literals and Unset Values
///
/// String literals and runtime strings follow the same rules as Size (see Size).
///
/// The default constructor creates an unset Length, which leaves the receiving field's default in
/// effect:
///
/// - **Size constraints impose no bounds.** An unset `min_size` leaves a node with no minimum size,
///   and an unset `max_size` leaves it with no maximum size.
/// - **Container spacing adds no space.** An unset `gap` leaves zero space between adjacent
///   children, and an unset `padding` applies zero inset around the container.
///
/// @see  Size, ContainerOptions, LayoutNode::SetMinSize()
///
class Length {
 public:
  ///
  /// The unit of a Length.
  ///
  enum class Unit : uint8_t {
    Default = 0,  ///< Unset: the receiving field's default applies.
    Px,           ///< Logical pixels.
    Percent,      ///< A percentage of the parent container's content box (inside its padding).
  };

  ///
  /// Create an unset Length (the receiving field's default applies).
  ///
  constexpr Length() = default;

  ///
  /// Create a Length in logical pixels (implicit, a bare number means px).
  ///
  constexpr Length(double px) : value_(px), unit_(Unit::Px) {}

  ///
  /// Create a Length from a CSS string literal, parsed and validated at compile time.
  ///
  /// Accepted forms are a number with `px`, a number with `%`, and a bare number (logical
  /// pixels). Any other format produces a compile error that explains the issue.
  ///
  /// @note  Only string literals bind here. A runtime string must go through TryParse().
  ///
  template <size_t N>
  UL_CSS_CONSTEVAL Length(const char (&s)[N]) {
    const detail::TrackValue t = detail::ResolveTrack(s, N - 1, false);
    if (t.status != detail::CssStatus::Ok) {
      if (std::is_constant_evaluated())
        detail::RaiseCssError(t.status);
      invalid_ = true;
      return;
    }
    value_ = t.value;
    unit_ = (t.unit_code == 1) ? Unit::Percent : Unit::Px;
  }

  ///
  /// Length does not accept `fr` units-- a flex factor is not a distance. Use Size instead.
  ///
  constexpr Length(Flex) = delete;

  ///
  /// Create a Length in logical pixels.
  ///
  /// @param  value  The distance, in logical pixels.
  ///
  static constexpr Length Px(double value) { return Length(value, Unit::Px); }

  ///
  /// Create a percentage Length.
  ///
  /// @param  value  The percentage, in percent points (`50` means 50%).
  ///
  static constexpr Length Pct(double value) { return Length(value, Unit::Percent); }

  ///
  /// Parse a runtime string with the same rules as the literal form.
  ///
  /// @param  text    UTF-8 text to parse.
  ///
  /// @param  length  Byte length of `text`.
  ///
  /// @param  out     Receives the parsed value on success (left unchanged on failure).
  ///
  /// @return  Returns whether or not the text parsed as a valid Length.
  ///
  static constexpr bool TryParse(const char* text, size_t length, Length& out) {
    const detail::TrackValue t = detail::ResolveTrack(text, length, false);
    if (t.status != detail::CssStatus::Ok)
      return false;
    out = Length(t.value, (t.unit_code == 1) ? Unit::Percent : Unit::Px);
    return true;
  }

  ///
  /// Parse a runtime string with the same rules as the literal form.
  ///
  /// @param  text  The text to parse.
  ///
  /// @param  out   Receives the parsed value on success (left unchanged on failure).
  ///
  /// @return  Returns whether or not the text parsed as a valid Length.
  ///
  static bool TryParse(const String& text, Length& out) {
    const String8& utf8 = text.utf8();
    return TryParse(utf8.data(), utf8.length(), out);
  }

  ///
  /// Get the numeric value (px or percent points, per unit()). Zero when unset.
  ///
  constexpr double value() const { return value_; }

  ///
  /// Get the unit. `Unit::Default` means unset (the field's default applies).
  ///
  constexpr Unit unit() const { return unit_; }

  ///
  /// Whether or not this value was explicitly set (shorthand for `unit() != Unit::Default`).
  ///
  constexpr bool is_set() const { return unit_ != Unit::Default; }

  ///
  /// Whether or not this value came from a malformed literal on a compiler where literal
  /// validation degrades to a runtime flag.
  ///
  /// @note  The library treats an invalid value as unset and logs a warning that identifies the
  ///        panel or container it belongs to.
  ///
  constexpr bool invalid() const { return invalid_; }

  friend constexpr bool operator==(const Length&, const Length&) = default;

 private:
  constexpr Length(double value, Unit unit) : value_(value), unit_(unit) {}

  double value_ = 0.0;
  Unit unit_ = Unit::Default;
  bool invalid_ = false;
};

///
/// A share of a container's free space distributed among siblings.
///
/// Sibling Panel%s and Container%s divide whatever space remains along the container's axis after
/// deducting fixed and percentage sizes, proportional to their declared flex factors.
///
/// You'll use Flex when computing a flex factor dynamically in code to pass to a Size field.
///
/// You can construct a Size from an explicit Flex factor or use its literal form:
///
/// ```
/// Size wide = Flex(2);  // twice the share of a 1fr sibling
/// Size same = "2fr";    // the literal form
/// ```
///
/// ## Distinguishing Shares from Distances
///
/// In layout contexts, a bare number means logical pixels rather than a flex share, so the Flex()
/// constructor is explicit. You can construct a Flex from a computed number using Flex() or
/// Flex::Fr().
///
/// Layout fields that take a Length (such as `min_size`, `max_size`, `gap`, and `padding`) don't
/// accept a flex share because a flex factor isn't a distance (see Length).
///
/// @see  Size, Length
///
class Flex {
 public:
  ///
  /// Create a flex factor from a computed number.
  ///
  /// The conversion is explicit because a bare number means logical px in layout contexts, never
  /// a flex factor.
  ///
  /// @param  factor  The share of free space, relative to the sibling flex factors.
  ///
  constexpr explicit Flex(double factor) : value_(factor) {}

  ///
  /// Create a Flex from a CSS string literal, parsed and validated at compile time.
  ///
  /// Only the `fr` form is accepted (eg, `"2fr"`). Any other format produces a compile error that
  /// explains the issue.
  ///
  template <size_t N>
  UL_CSS_CONSTEVAL Flex(const char (&s)[N]) {
    const detail::CssNumber num = detail::ParseCssNumber(s, N - 1);
    const detail::CssStatus status
        = (num.status != detail::CssStatus::Ok)
              ? num.status
              : (num.unit == detail::CssUnit::Fr ? detail::CssStatus::Ok
                                                 : detail::CssStatus::ExpectedFr);
    if (status != detail::CssStatus::Ok) {
      if (std::is_constant_evaluated())
        detail::RaiseCssError(status);
      invalid_ = true;
      return;
    }
    value_ = num.value;
  }

  ///
  /// Create a flex factor.
  ///
  /// @param  factor  The share of free space, relative to the sibling flex factors.
  ///
  static constexpr Flex Fr(double factor) { return Flex(factor); }

  ///
  /// Parse a runtime string with the same rules as the literal form (only the `fr` form is
  /// accepted).
  ///
  /// @param  text    UTF-8 text to parse.
  ///
  /// @param  length  Byte length of `text`.
  ///
  /// @param  out     Receives the parsed value on success (left unchanged on failure).
  ///
  /// @return  Returns whether or not the text parsed as a valid Flex.
  ///
  static constexpr bool TryParse(const char* text, size_t length, Flex& out) {
    const detail::CssNumber num = detail::ParseCssNumber(text, length);
    if (num.status != detail::CssStatus::Ok || num.unit != detail::CssUnit::Fr)
      return false;
    out = Flex(num.value);
    return true;
  }

  ///
  /// Parse a runtime string with the same rules as the literal form.
  ///
  /// @param  text  The text to parse.
  ///
  /// @param  out   Receives the parsed value on success (left unchanged on failure).
  ///
  /// @return  Returns whether or not the text parsed as a valid Flex.
  ///
  static bool TryParse(const String& text, Flex& out) {
    const String8& utf8 = text.utf8();
    return TryParse(utf8.data(), utf8.length(), out);
  }

  ///
  /// Get the flex factor.
  ///
  constexpr double value() const { return value_; }

  ///
  /// Whether or not this value came from a malformed literal on a compiler where literal
  /// validation degrades to a runtime flag (see Length::invalid()).
  ///
  constexpr bool invalid() const { return invalid_; }

  friend constexpr bool operator==(const Flex&, const Flex&) = default;

 private:
  double value_ = 1.0;
  bool invalid_ = false;
};

///
/// Fixed, percentage, and flexible sizes for panels and containers.
///
/// Size specifies how much space a child node occupies inside a row or column. You assign it when
/// configuring panels and containers in layout options, or update it later with
/// LayoutNode::SetSize().
///
/// This row distributes its space among three panels using different sizing units:
///
/// ```
/// RefPtr<Container> body = window->layout()->AddRow();
/// body->AddPanel({ .size = "240px" });  // logical pixels
/// body->AddPanel({ .size = "30%" });    // percent of the free space
/// body->AddPanel({ .size = "2fr" });    // two flex shares of what's left
/// ```
///
/// ## Layout Units
///
/// Panels and containers accept different measurement forms along their parent container's axis:
///
/// - **Logical pixels specify a fixed distance.** A bare number or a string with `px` sets the
///   dimension in display-independent pixels.
/// - **Percentages claim a fraction of the container's free space.** A string with `%` reserves
///   that portion of the space remaining after fixed siblings.
/// - **Flex shares divide whatever space remains.** A string with `fr` takes a proportional cut of
///   the free space left after fixed and percentage allocations.
///
/// ## Computed Sizes
///
/// When sizes depend on runtime calculations rather than string constants, use factory methods or
/// the Flex helper type. Call Size::Fr() or Flex to specify flex shares.
///
/// Pass numeric values directly instead of formatting strings:
///
/// ```
/// Size half = Size::Pct(50);
/// Size wide = Flex(2);  // the same as "2fr"
/// ```
///
/// ## Literals and Runtime Strings
///
/// String literals passed to Size are parsed and validated at compile time. Any syntax typo or
/// unsupported unit produces a compiler error.
///
/// For strings read at runtime, use Size::TryParse():
///
/// ```
/// Size parsed;
/// if (Size::TryParse(saved_width, parsed))
///   sidebar->SetSize(parsed);
/// ```
///
/// ## Default and Unset Sizes
///
/// A default-constructed Size has no explicit unit or value. Leaving a size unset instructs the
/// receiving field to apply its own default, which gives panels and containers in a row or column a
/// single flex share (`1fr`) and floating panels `100%`.
///
/// Setting an explicit size of `0` or `"0px"` collapses the node to zero pixels along the layout
/// axis instead of expanding it.
///
/// @see  Length, Flex, PanelOptions, LayoutNode::SetSize()
///
class Size {
 public:
  ///
  /// The unit of a Size.
  ///
  enum class Unit : uint8_t {
    Default = 0,  ///< Unset: the receiving field's default applies.
    Px,           ///< Logical pixels.
    Percent,      ///< A percentage of the container's free space along the axis.
    Fr,           ///< A flex factor (share of the remaining free space).
  };

  ///
  /// Create an unset Size (the receiving field's default applies).
  ///
  constexpr Size() = default;

  ///
  /// Create a Size in logical pixels (implicit, a bare number means px).
  ///
  constexpr Size(double px) : value_(px), unit_(Unit::Px) {}

  ///
  /// Create a Size from a Length (implicit, px stays px and percent stays percent).
  ///
  constexpr Size(Length length)
      : value_(length.value()),
        unit_(length.unit() == Length::Unit::Percent ? Unit::Percent
              : length.unit() == Length::Unit::Px    ? Unit::Px
                                                     : Unit::Default),
        invalid_(length.invalid()) {}

  ///
  /// Create a Size from a flex factor (implicit).
  ///
  constexpr Size(Flex flex) : value_(flex.value()), unit_(Unit::Fr), invalid_(flex.invalid()) {}

  ///
  /// Create a Size from a CSS string literal, parsed and validated at compile time.
  ///
  /// Accepted forms are a number with `px`, `%`, or `fr`, and a bare number (logical pixels).
  /// Any other format produces a compile error that explains the issue.
  ///
  /// @note  Only string literals bind here. A runtime string must go through TryParse().
  ///
  template <size_t N>
  UL_CSS_CONSTEVAL Size(const char (&s)[N]) {
    const detail::TrackValue t = detail::ResolveTrack(s, N - 1, true);
    if (t.status != detail::CssStatus::Ok) {
      if (std::is_constant_evaluated())
        detail::RaiseCssError(t.status);
      invalid_ = true;
      return;
    }
    value_ = t.value;
    unit_ = (t.unit_code == 2) ? Unit::Fr : (t.unit_code == 1) ? Unit::Percent : Unit::Px;
  }

  ///
  /// Create a Size in logical pixels.
  ///
  /// @param  value  The size, in logical pixels.
  ///
  static constexpr Size Px(double value) { return Size(value, Unit::Px); }

  ///
  /// Create a percent-of-free-space Size.
  ///
  /// @param  value  The percentage, in percent points (`50` means 50%).
  ///
  static constexpr Size Pct(double value) { return Size(value, Unit::Percent); }

  ///
  /// Create a flex-factor Size.
  ///
  /// @param  value  The share of free space, relative to the sibling flex factors.
  ///
  static constexpr Size Fr(double value) { return Size(value, Unit::Fr); }

  ///
  /// Parse a runtime string with the same rules as the literal form.
  ///
  /// @param  text    UTF-8 text to parse.
  ///
  /// @param  length  Byte length of `text`.
  ///
  /// @param  out     Receives the parsed value on success (left unchanged on failure).
  ///
  /// @return  Returns whether or not the text parsed as a valid Size.
  ///
  static constexpr bool TryParse(const char* text, size_t length, Size& out) {
    const detail::TrackValue t = detail::ResolveTrack(text, length, true);
    if (t.status != detail::CssStatus::Ok)
      return false;
    out = Size(t.value, (t.unit_code == 2)   ? Unit::Fr
                        : (t.unit_code == 1) ? Unit::Percent
                                             : Unit::Px);
    return true;
  }

  ///
  /// Parse a runtime string with the same rules as the literal form.
  ///
  /// @param  text  The text to parse.
  ///
  /// @param  out   Receives the parsed value on success (left unchanged on failure).
  ///
  /// @return  Returns whether or not the text parsed as a valid Size.
  ///
  static bool TryParse(const String& text, Size& out) {
    const String8& utf8 = text.utf8();
    return TryParse(utf8.data(), utf8.length(), out);
  }

  ///
  /// Get the numeric value (px, percent points, or flex factor, per unit()). Zero when unset.
  ///
  constexpr double value() const { return value_; }

  ///
  /// Get the unit. `Unit::Default` means unset (the field's default applies).
  ///
  constexpr Unit unit() const { return unit_; }

  ///
  /// Whether or not this value was explicitly set (shorthand for `unit() != Unit::Default`).
  ///
  constexpr bool is_set() const { return unit_ != Unit::Default; }

  ///
  /// Whether or not this value came from a malformed literal on a compiler where literal
  /// validation degrades to a runtime flag (see Length::invalid()).
  ///
  constexpr bool invalid() const { return invalid_; }

  friend constexpr bool operator==(const Size&, const Size&) = default;

 private:
  constexpr Size(double value, Unit unit) : value_(value), unit_(unit) {}

  double value_ = 0.0;
  Unit unit_ = Unit::Default;
  bool invalid_ = false;
};

}  // namespace ultralight
