///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/HolderTraits.h>

#include <memory>
#include <type_traits>

namespace ultralight {
namespace dom {

///
/// Whether or not the DOM API can hold an object through H (ignoring const and references), ie.
/// ultralight::HolderTraits is specialized for it.
///
/// Some DOM calls can take a smart pointer to your object instead of a reference. These are the
/// listener overloads that take an object and a member function (eg,
/// Element::addEventListener() and Element::On()) and the data-binding API
/// (ultralight::dom::data):
///
/// ```
/// auto player = std::make_shared<Player>();
/// button.addEventListener("click", std::weak_ptr(player), &Player::OnClick);
/// ```
///
/// RefPtr, WeakPtr, std::shared_ptr, std::weak_ptr, and std::unique_ptr work without any setup:
///
///  - **An owning pointer keeps your object alive** (RefPtr, std::shared_ptr, or a
///    std::unique_ptr you move in) for as long as the listener (or binding) exists.
///  - **A weak pointer doesn't keep it alive** (WeakPtr or std::weak_ptr). The library locks it
///    for each call and skips each call that comes after your object is destroyed. A data
///    binding then keeps the values it last published.
///
/// To use your engine's own smart pointer, specialize ultralight::HolderTraits for it (see
/// `<Ultralight/HolderTraits.h>`).
///
template <typename H>
concept LockableHolder
    = requires { typename HolderTraits<std::remove_cvref_t<H>>::element_type; }
    && (ultralight::detail::HolderHasLock<std::remove_cvref_t<H>>
        || ultralight::detail::HolderHasGet<std::remove_cvref_t<H>>);

}  // namespace dom
}  // namespace ultralight
