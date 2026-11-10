///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/dom/data/TypeTraits.h>
#include <Ultralight/dom/data/detail/EntryInfo.h>
#include <Ultralight/dom/data/detail/Record.h>

#include <iterator>

namespace ultralight {
namespace dom {
namespace data {

template <typename T>
class Model;

/// \cond INTERNAL
namespace detail {

template <typename T>
inline constexpr bool kHasCustomSync = requires(const T& instance, Model<T> model) {
  TypeTraits<std::remove_cvref_t<T>>::Sync(instance, model);
};

// The walk entry for one described type: the type's own Sync when declared, the
// synthesized full walk otherwise. Recursion into sub-objects and rows funnels through
// here so each type's skip logic owns its subtree.
template <typename T>
void SyncType(RecordOps* ops, const T& instance);

}  // namespace detail
/// \endcond

///
/// Read-only access to the synchronized values of a described type.
///
/// Snapshot lets you inspect the field values and CSS variables recorded for a described type
/// without touching the underlying C++ instance. Calling Get() with an entry name returns its
/// current synchronized value.
///
/// This schema uses a validator to check an edited bid against another field on the page:
///
/// ```
/// struct Shop {
///   int64_t gold = 500;
///   int64_t bid = 0;
///
///   static constexpr auto schema = dd::Schema(
///       dd::Field("gold", &Shop::gold),
///       dd::Field("bid", &Shop::bid, dd::Editable,
///                 dd::Validate([](int64_t bid, auto s) -> std::optional<int64_t> {
///                   if (bid > s.template Get<"gold">())
///                     return std::nullopt;  // can't bid more than you have
///                   return bid;
///                 })));
/// };
/// ```
///
/// ## Receiving a Snapshot
///
/// The library passes a Snapshot to functions that inspect page state:
///
/// - **A validator receives a Snapshot through its second parameter on the Renderer's thread.** It
///   provides the values the page currently shows, letting you inspect related fields before
///   accepting an edit.
/// - **A type's static `Sync` function receives a dom::data::Model, which inherits from Snapshot.**
///   Calling Get() returns the last synced values, including any fields that call has already
///   synced, so you can compare against native data and skip unchanged fields (see
///   dom::data::Model).
///
/// @see  dom::data::Model, dom::data::Validate()
///
template <typename T>
class Snapshot {
 public:
  using described_type = T;

  ///
  /// Get the type's schema.
  ///
  static constexpr const auto& schema() { return TypeTraits<T>::schema; }

  /// \cond INTERNAL
  Snapshot(detail::RecordOps* ops) : ops_(ops) {}
  /// \endcond

  ///
  /// Get the value of a field or Var by name.
  ///
  /// A misspelled name is a compile error. So is an object, a list, or an action (they have no
  /// single value).
  ///
  /// @return  Returns the value as a `bool`, `int64_t`, `double`, String, StyleValue, or Color.
  ///          Integer fields read as `int64_t` and floating-point fields as `double`. String
  ///          fields read as a String, and so do enum fields (as the enumerator's name).
  ///
  /// @note  Most fields compare directly with the value in your object (eg,
  ///        `s.rev != m.Get<"rev">()`). A `std::string`, enum, or StyleValue field can't be
  ///        compared that way, so gate on an integer revision counter instead.
  ///
  template <ultralight::detail::FixedString Name>
  auto Get() const {
    constexpr size_t idx = IndexOfChecked<Name>();
    using SchemaT = std::remove_cvref_t<decltype(schema())>;
    using Info = detail::EntryInfo<std::tuple_element_t<idx, SchemaT>>;
    static_assert(Info::kClass == detail::EntryClass::kLeaf
                      || Info::kClass == detail::EntryClass::kVar,
                  "Get reads leaf values; objects, lists, and actions have no slot value");
    return ReadSlot<Info::kKind>(static_cast<uint32_t>(idx));
  }

 protected:
  template <ultralight::detail::FixedString Name>
  static consteval size_t IndexOfChecked() {
    constexpr size_t idx = schema().IndexOf(Name.view());
    static_assert(idx != detail::kNpos, "no entry with this name in the type's schema");
    return idx;
  }

  template <ValueKind K>
  auto ReadSlot(uint32_t slot) const {
    if constexpr (K == ValueKind::Bool) {
      return (ops_ && ops_->get_bool) ? ops_->get_bool(ops_->state, slot) : false;
    } else if constexpr (K == ValueKind::Int64) {
      return (ops_ && ops_->get_int64) ? ops_->get_int64(ops_->state, slot) : int64_t(0);
    } else if constexpr (K == ValueKind::Double) {
      return (ops_ && ops_->get_double) ? ops_->get_double(ops_->state, slot) : 0.0;
    } else if constexpr (K == ValueKind::String) {
      String out;
      if (ops_ && ops_->get_string)
        ops_->get_string(ops_->state, slot, &out);
      return out;
    } else if constexpr (K == ValueKind::StyleValue) {
      return (ops_ && ops_->get_style) ? ops_->get_style(ops_->state, slot)
                                       : StyleValue { 0.0 };
    } else if constexpr (K == ValueKind::Color) {
      return (ops_ && ops_->get_color) ? ops_->get_color(ops_->state, slot) : Color {};
    } else {
      static_assert(detail::kAlwaysFalse<Snapshot>, "unsupported slot kind");
    }
  }

  detail::RecordOps* ops_;
};

///
/// Handle passed to a type's static `Sync` function to control which fields are read during
/// synchronization.
///
/// When Context::Sync() runs, the library normally reads every field of every bound instance
/// through its schema. You can customize this process for any described type by declaring a static
/// `Sync` function that takes your instance alongside a Model.
///
/// It provides access to previously synchronized values and lets you choose which fields to read.
/// This avoids reading large containers or calling expensive accessors on frames where the
/// underlying data hasn't changed.
///
/// This leaderboard reads its row collection only when its revision counter changes:
///
/// ```
/// struct Score {
///   int64_t player_id = 0;
///   int points = 0;
/// };
///
/// struct Leaderboard {
///   int rev = 1;  // bump it whenever rows change
///   std::string heading;
///   std::vector<Score> rows;
///
///   static constexpr auto schema = dd::Schema(
///       dd::Field("rev", &Leaderboard::rev, dd::Internal),
///       dd::Field("heading", &Leaderboard::heading),
///       dd::List("rows", &Leaderboard::rows, &Score::player_id));
///
///   static void Sync(const Leaderboard& b, dd::Model<Leaderboard> m) {
///     if (b.rev != m.Get<"rev">())  // compare before syncing rev
///       m.Sync<"rows">();
///     m.SyncAllExcept<"rows">();     // syncs rev and heading
///   }
/// };
/// ```
///
/// ## Writing a Sync Function
///
/// You declare a static `Sync` function beside the type's schema, either inside the type itself or
/// within its dom::data::TypeTraits specialization.
///
/// The library never resends unchanged values to attached pages, with or without a static `Sync`
/// function. Skipping a field saves only the work of reading it in native code.
///
/// When you skip syncing a field, the page keeps its last synchronized value. For collections gated
/// on a revision counter, tag the counter with dom::data::Internal in the schema. Markup can't bind
/// an Internal entry, and schema tools leave it out.
///
/// @note  Compare a revision counter before syncing it, and start the counter at 1 rather than 0.
///        Calling Sync() on the counter updates the recorded value, so comparing afterwards always
///        finds them equal. Before the first sync, Get() returns 0, so starting at 0 skips the
///        initial sync and leaves the list blank.
///
/// ## Three Kinds of Sync
///
/// Each function plays a distinct role during synchronization:
///
/// - **Context::Sync() coordinates the overall update cycle.** You call it once per frame on the
///   home thread to deliver page events to handlers and broadcast changed values to attached
///   View%s.
/// - **A type's static `Sync` function customizes the read step for that type.** The library
///   invokes it while inspecting an instance, passing the instance alongside a Model.
/// - **Model::Sync() reads a specific field from the instance and sends it if it changed.** You
///   call it inside your type's static `Sync` function, alongside SyncAll() to read every field and
///   SyncAllExcept() to read every field except the ones you list.
///
/// @warning  A Model is valid only during the call to your type's static `Sync` function. Never
///           store a Model or use it after the function returns.
///
/// @see  dom::data::Snapshot, dom::data::TypeTraits, dom::data::Internal,
///       dom::data::Context::Sync()
///
template <typename T>
class Model : public Snapshot<T> {
 public:
  /// \cond INTERNAL
  Model(detail::RecordOps* ops, const T* instance)
      : Snapshot<T>(ops), instance_(instance) {}
  /// \endcond

  ///
  /// Sync one field by name.
  ///
  /// Reads the field from your instance and sends it to the page if it changed. A nested object
  /// or a list syncs each object or row through its own type's Sync function.
  ///
  /// @note  Sync() on an action or on a field without an accessor is a compile error. Send a
  ///        field without an accessor with Set().
  ///
  template <ultralight::detail::FixedString Name>
  void Sync() {
    constexpr size_t idx = Base::template IndexOfChecked<Name>();
    using Tok = std::tuple_element_t<idx, std::remove_cvref_t<decltype(Base::schema())>>;
    static_assert(detail::EntryInfo<Tok>::kClass != detail::EntryClass::kAction,
                  "actions have no value to sync");
    static_assert(detail::EntryInfo<Tok>::kHasAccessor,
                  "accessorless fields are written with Set, not synced");
    SyncEntry<idx>();
  }

  ///
  /// Sync every field (what Context::Sync() does for a type without a Sync function).
  ///
  /// Actions and fields without an accessor are skipped.
  ///
  void SyncAll() { SyncRange(std::make_index_sequence<Base::schema().kCount> {}); }

  ///
  /// Sync every field except the ones you list (eg, `m.SyncAllExcept<"rows", "log">()`).
  ///
  /// Actions and fields without an accessor are skipped. A name that isn't in the schema is a
  /// compile error.
  ///
  template <ultralight::detail::FixedString... Names>
  void SyncAllExcept() {
    ((void)Base::template IndexOfChecked<Names>(), ...);
    SyncRangeExcept<Names...>(std::make_index_sequence<Base::schema().kCount> {});
  }

  ///
  /// Set a field's value by name.
  ///
  /// This is how you send a field declared without an accessor (eg, `dd::Field<int>("score")`).
  /// It also works on any other field or Var with a single value.
  ///
  /// @param  value  The value to send. It must convert to the field's declared type.
  ///
  template <ultralight::detail::FixedString Name, typename Arg>
  void Set(const Arg& value) {
    constexpr size_t idx = Base::template IndexOfChecked<Name>();
    using Tok = std::tuple_element_t<idx, std::remove_cvref_t<decltype(Base::schema())>>;
    using Info = detail::EntryInfo<Tok>;
    static_assert(Info::kClass == detail::EntryClass::kLeaf
                      || Info::kClass == detail::EntryClass::kVar,
                  "Set writes leaf values; objects, lists, and actions have no slot value");
    using Value = typename Tok::value_type;
    static_assert(std::is_convertible_v<const Arg&, Value>,
                  "value is not convertible to the field's declared value type");
    WriteSlot<Info::kKind>(static_cast<uint32_t>(idx),
                           ValueTraits<Value>::ToSlot(static_cast<Value>(value)));
  }

 private:
  using Base = Snapshot<T>;

  template <size_t... Is>
  void SyncRange(std::index_sequence<Is...>) {
    (SyncEntryIfSyncable<Is>(), ...);
  }

  template <ultralight::detail::FixedString... Names, size_t... Is>
  void SyncRangeExcept(std::index_sequence<Is...>) {
    (SyncEntryIfIncluded<Is, Names...>(), ...);
  }

  template <size_t I, ultralight::detail::FixedString... Names>
  void SyncEntryIfIncluded() {
    constexpr bool excluded = ((Base::schema().IndexOf(Names.view()) == I) || ...);
    if constexpr (!excluded)
      SyncEntryIfSyncable<I>();
  }

  template <size_t I>
  void SyncEntryIfSyncable() {
    using SchemaT = std::remove_cvref_t<decltype(Base::schema())>;
    using Info = detail::EntryInfo<std::tuple_element_t<I, SchemaT>>;
    if constexpr (Info::kClass != detail::EntryClass::kAction && Info::kHasAccessor)
      SyncEntry<I>();
  }

  template <size_t I>
  void SyncEntry() {
    const auto& token = Base::schema().template get<I>();
    using Tok = std::remove_cvref_t<decltype(token)>;
    using Info = detail::EntryInfo<Tok>;
    static_assert(requires(const T& t, const Tok& tk) { detail::ReadAccessor(tk.accessor, t); },
                  "schema accessor does not read this described type (accessor owner "
                  "mismatch at the TypeTraits pairing)");
    if (!instance_)
      return;
    constexpr uint32_t slot = static_cast<uint32_t>(I);
    detail::RecordOps* ops = this->ops_;
    if constexpr (Info::kClass == detail::EntryClass::kLeaf
                  || Info::kClass == detail::EntryClass::kVar) {
      using Value = typename Tok::value_type;
      WriteSlot<Info::kKind>(slot,
                             ValueTraits<Value>::ToSlot(
                                 detail::ReadAccessor(token.accessor, *instance_)));
    } else if constexpr (Info::kClass == detail::EntryClass::kNullableObject) {
      decltype(auto) value = detail::ReadAccessor(token.accessor, *instance_);
      const bool present = static_cast<bool>(value);
      if (ops && ops->set_present)
        ops->set_present(ops->state, slot, present);
      if (present) {
        if (detail::RecordOps* child = (ops && ops->child) ? ops->child(ops->state, slot)
                                                           : nullptr)
          detail::SyncType(child, *value);
      }
    } else if constexpr (Info::kClass == detail::EntryClass::kObject) {
      decltype(auto) value = detail::ReadAccessor(token.accessor, *instance_);
      if (detail::RecordOps* child = (ops && ops->child) ? ops->child(ops->state, slot)
                                                         : nullptr)
        detail::SyncType(child, value);
    } else if constexpr (Info::kClass == detail::EntryClass::kList) {
      SyncList<I>(token);
    }
  }

  template <size_t I, typename Tok>
  void SyncList(const Tok& token) {
    detail::RecordOps* ops = this->ops_;
    if (!ops)
      return;
    constexpr uint32_t slot = static_cast<uint32_t>(I);
    decltype(auto) rows = detail::ReadAccessor(token.accessor, *instance_);
    const uint64_t count
        = static_cast<uint64_t>(std::distance(std::begin(rows), std::end(rows)));
    if (ops->list_begin)
      ops->list_begin(ops->state, slot, count);
    constexpr bool keyed = [] {
      if constexpr (detail::IsListToken<Tok>::value)
        return Tok::kKeyed;
      else
        return false;
    }();
    uint64_t ordinal = 0;
    for (const auto& row : rows) {
      detail::RecordOps* row_ops = nullptr;
      if constexpr (keyed) {
        using Key = detail::AccessorValue<typename Tok::key_pointer_type>;
        constexpr ValueKind key_kind = ValueTraits<Key>::kind;
        if constexpr (key_kind == ValueKind::Int64) {
          if (ops->list_row_key_int64)
            row_ops = ops->list_row_key_int64(ops->state, slot, ordinal,
                                              ValueTraits<Key>::ToSlot(row.*token.key));
        } else {
          const String key = ValueTraits<Key>::ToSlot(row.*token.key);
          if (ops->list_row_key_string)
            row_ops = ops->list_row_key_string(ops->state, slot, ordinal, &key);
        }
      } else {
        if (ops->list_row)
          row_ops = ops->list_row(ops->state, slot, ordinal);
      }
      if (row_ops)
        detail::SyncType(row_ops, row);
      ordinal++;
    }
    if (ops->list_end)
      ops->list_end(ops->state, slot);
  }

  template <ValueKind K, typename SlotV>
  void WriteSlot(uint32_t slot, const SlotV& value) {
    detail::RecordOps* ops = this->ops_;
    if (!ops)
      return;
    if constexpr (K == ValueKind::Bool) {
      if (ops->set_bool)
        ops->set_bool(ops->state, slot, value);
    } else if constexpr (K == ValueKind::Int64) {
      if (ops->set_int64)
        ops->set_int64(ops->state, slot, value);
    } else if constexpr (K == ValueKind::Double) {
      if (ops->set_double)
        ops->set_double(ops->state, slot, value);
    } else if constexpr (K == ValueKind::String) {
      const String& stored = value;
      if (ops->set_string)
        ops->set_string(ops->state, slot, &stored);
    } else if constexpr (K == ValueKind::StyleValue) {
      if (ops->set_style)
        ops->set_style(ops->state, slot, value);
    } else if constexpr (K == ValueKind::Color) {
      if (ops->set_color)
        ops->set_color(ops->state, slot, value);
    } else {
      static_assert(detail::kAlwaysFalse<SlotV>, "unsupported slot kind");
    }
  }

  const T* instance_;
};

/// \cond INTERNAL
namespace detail {

template <typename T>
void SyncType(RecordOps* ops, const T& instance) {
  using Plain = std::remove_cvref_t<T>;
  Model<Plain> model(ops, &instance);
  if constexpr (kHasCustomSync<Plain>)
    TypeTraits<Plain>::Sync(instance, model);
  else
    model.SyncAll();
}

}  // namespace detail
/// \endcond

}  // namespace data
}  // namespace dom
}  // namespace ultralight
