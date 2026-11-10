///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// Implementation detail of `<Ultralight/dom/data/Binding.h>`; include that header
/// instead. Everything here lives in ultralight::dom::data::detail and may change without
/// notice.
///
/// The receiver-form member wrapping (MemberCallableSig, WrapBorrowedMember,
/// WrapHolderMember) is family-shared machinery in `<Ultralight/dom/detail/Receivers.h>`
/// (the listener surfaces consume it too); this header re-exports it into
/// dom::data::detail and keeps the data-binding-specific half: the guard's type-erased
/// InstanceAccess closure, how the implicit-receiver member-pointer forms reach the
/// bound instance. Set by the Bind overloads that can grant mutable access; empty
/// otherwise (const binds, const-element holders, raw-handle guards), which makes the
/// implicit forms a benign no-op. Handlers capture a COPY, so a registered handler is
/// independent of the guard object's later moves or destruction (delivery only happens
/// while the underlying binding is alive, which keeps the borrowed instance, owned box,
/// or holder reachable).
///
/// \cond INTERNAL
#pragma once
#include <Ultralight/dom/data/Holders.h>
#include <Ultralight/dom/detail/Receivers.h>

#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

namespace ultralight {
namespace dom {
namespace data {
namespace detail {

using ultralight::dom::detail::MemberCallableSig;
using ultralight::dom::detail::WrapBorrowedMember;
using ultralight::dom::detail::WrapHolderMember;

// ---- Implicit-receiver instance access --------------------------------------------------

template <typename T>
using InstanceAccess = std::function<void(const std::function<void(T&)>&)>;

// Deliver one invocation to the bound instance through the guard's access closure (an
// empty access is the documented benign no-op).
template <typename T, typename M, typename... Args>
void DeliverToBound(const InstanceAccess<T>& access, M method, Args&&... args) {
  if (!access)
    return;
  access([&](T& instance) { (instance.*method)(std::forward<Args>(args)...); });
}

template <typename T, typename M, typename... Args>
auto WrapImplicitMemberImpl(InstanceAccess<T> access, M method, std::tuple<Args...>*) {
  constexpr bool kInvocable = std::is_invocable_v<M, T&, Args...>;
  static_assert(kInvocable,
                "dom::data: the member function is not callable on the bound type");
  if constexpr (kInvocable) {
    return [access = std::move(access), method](Args... args) {
      DeliverToBound<T>(access, method, std::forward<Args>(args)...);
    };
  } else {
    return [](Args...) {};
  }
}

template <typename T, typename M>
auto WrapImplicitMember(InstanceAccess<T> access, M method) {
  using Sig = MemberCallableSig<M>;
  static_assert(std::is_base_of_v<typename Sig::owner, T>,
                "dom::data: the member function must belong to the bound type (pass a "
                "receiver to deliver to another object)");
  return WrapImplicitMemberImpl<T>(std::move(access), method,
                                   static_cast<typename Sig::args*>(nullptr));
}

}  // namespace detail
}  // namespace data
}  // namespace dom
}  // namespace ultralight
/// \endcond
