///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/dom/Holders.h>
#include <Ultralight/dom/data/TypeTraits.h>

#include <memory>

namespace ultralight {
namespace dom {
namespace data {

///
/// Whether or not the library can hold an object through H (the same concept as
/// dom::LockableHolder).
///
/// RefPtr, WeakPtr, std::shared_ptr, std::weak_ptr, and std::unique_ptr qualify without setup.
/// To use your own smart pointer, specialize ultralight::HolderTraits (see
/// `<Ultralight/HolderTraits.h>`).
///
using ultralight::dom::LockableHolder;

///
/// Whether or not Context::Bind() can bind an instance through H.
///
/// H must be a LockableHolder whose object type is described (see TypeTraits), eg,
/// `std::shared_ptr<HUD>`, `RefPtr<HUD>`, or `std::weak_ptr<HUD>`. Each Context::Sync() locks the
/// holder while it reads the instance. Once the object is gone, the binding stops updating and
/// the page keeps the values it last got.
///
/// @note  The smart-pointer receiver forms of Binding::OnChange() and Binding::OnAction() need
///        only a LockableHolder.
///
template <typename H>
concept BindableHolder = LockableHolder<H>
    && Described<typename HolderTraits<std::remove_cvref_t<H>>::element_type>;

}  // namespace data
}  // namespace dom
}  // namespace ultralight
