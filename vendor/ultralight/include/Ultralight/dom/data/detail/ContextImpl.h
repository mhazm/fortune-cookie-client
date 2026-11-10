///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// Implementation detail of `<Ultralight/dom/data/Context.h>`; include that header
/// instead. Everything here lives in ultralight::dom::data::detail and may change without
/// notice.
///
/// Bind-side ownership boxes, the walk thunk, the guard's instance-access makers, and the
/// callable-bridging trampolines behind Context's Bind/PostTask/DefineFormat.
///
/// \cond INTERNAL
#pragma once
#include <Ultralight/CAPI/CAPI_DOMData.h>
#include <Ultralight/dom/Document.h>
#include <Ultralight/dom/data/Holders.h>
#include <Ultralight/dom/data/Model.h>
#include <Ultralight/dom/data/Value.h>
#include <Ultralight/dom/data/detail/CallGuard.h>
#include <Ultralight/dom/data/detail/Receivers.h>

#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

namespace ultralight {
namespace dom {
namespace data {
namespace detail {

// The lowered schema walk for one described type, instantiated in the embedder's TU and
// carried across the waist as a plain function pointer.
template <typename T>
void SyncWalkThunk(ULDOMDataRecord record, const void* instance) {
  // A throwing read (a custom Sync function or a reflected getter) stops this instance's pass
  // where it threw; the rest of the Sync continues.
  CallHandler(nullptr, "Sync", [&] {
    SyncType(reinterpret_cast<RecordOps*>(record), *static_cast<const T*>(instance));
  });
}

inline const void* BorrowedAcquire(void* holder_state) {
  return holder_state;
}

// By-value bindings own their instance.
template <typename T>
struct ValueHolderBox {
  T value;
  static const void* Acquire(void* state) {
    return std::addressof(static_cast<ValueHolderBox*>(state)->value);
  }
  static void Destroy(void* state) { delete static_cast<ValueHolderBox*>(state); }
};

// Holder-managed bindings keep the holder and lock it around each Sync pass's read (an owning
// holder's "lock" is its pointee, kept alive by the holder itself).
template <typename H>
struct SmartHolderBox {
  using LockResult = std::remove_cvref_t<decltype(ultralight::detail::LockHolder(
      std::declval<const H&>()))>;

  H holder;
  std::optional<LockResult> guard;

  // A holder whose lock throws is treated like an expired one: the binding sits out this Sync.
  static const void* Acquire(void* state) {
    auto* box = static_cast<SmartHolderBox*>(state);
    const void* instance = nullptr;
    CallHandler(nullptr, "holder lock", [&] { instance = LockInstance(box); });
    return instance;
  }
  static void Release(void* state) {
    CallHandler(nullptr, "holder unlock",
                [&] { static_cast<SmartHolderBox*>(state)->guard.reset(); });
  }
  static const void* LockInstance(SmartHolderBox* box) {
    box->guard.emplace(ultralight::detail::LockHolder(box->holder));
    if (*box->guard)
      return std::addressof(**box->guard);
    box->guard.reset();
    return nullptr;
  }
  static void Destroy(void* state) { delete static_cast<SmartHolderBox*>(state); }
};

template <typename T>
InstanceAccess<T> MakeBorrowedAccess(T* instance) {
  return [instance](const std::function<void(T&)>& deliver) { deliver(*instance); };
}

template <typename T>
InstanceAccess<T> MakeValueAccess(ValueHolderBox<T>* box) {
  return [box](const std::function<void(T&)>& deliver) { deliver(box->value); };
}

// Reaches the holder through the binding's box rather than a captured copy: an extra
// strong holder in the access closure would observably extend a shared model's lifetime
// past its unbind whenever a guard outlives the binding.
template <typename H, typename T>
InstanceAccess<T> MakeHolderAccess(SmartHolderBox<H>* box) {
  return [box](const std::function<void(T&)>& deliver) {
    auto&& lock = ultralight::detail::LockHolder(box->holder);
    if (lock)
      deliver(*lock);
  };
}

template <typename Fn>
void InvokePostedCallable(void* user_data, ULDOMDocument document) {
  Fn& fn = *static_cast<Fn*>(user_data);
  CallHandler(nullptr, "posted task", [&] {
    if constexpr (std::is_invocable_v<Fn&, Document>) {
      fn(Document::FromBorrowed(document));
    } else {
      (void)document;
      fn();
    }
  });
}

template <typename Fn>
void DeletePostedCallable(void* user_data) {
  delete static_cast<Fn*>(user_data);
}

template <typename Fn>
void RunFormatCallable(Fn& fn, const ULDOMDataLeafValue* value, ULString output) {
  auto result = fn(Value::FromLeaf(*value));
  using R = std::remove_cvref_t<decltype(result)>;
  ultralight::String text;
  if constexpr (std::is_same_v<R, ultralight::String>)
    text = std::move(result);
  else if constexpr (std::is_same_v<R, std::string>)
    text = ultralight::String(result.data(), result.size());
  else
    text = ultralight::String(result);
  const ultralight::String8& utf8 = text.utf8();
  ULString owned = ulCreateStringUTF8(utf8.data(), utf8.length());
  ulStringAssignString(output, owned);
  ulDestroyString(owned);
}

template <typename Fn>
void InvokeFormatCallable(void* user_data, const ULDOMDataLeafValue* value,
                          ULString output) {
  Fn& fn = *static_cast<Fn*>(user_data);
  // A formatter that throws leaves `output` as the library passed it in.
  CallHandler(nullptr, "formatter", [&] { RunFormatCallable(fn, value, output); });
}

}  // namespace detail
}  // namespace data
}  // namespace dom
}  // namespace ultralight
/// \endcond
