///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// Implementation detail of the dom family's listener and data-binding headers; include
/// those instead. Everything here lives in ultralight::dom::detail and may change without
/// notice.
///
/// Receiver-form wrapping for member-function registration. Each receiver form lowers a
/// member function to a lambda with the member's EXACT parameter list, so callable form
/// selection sees the member's own signature (a generic forwarder would match every form
/// at once). Every wrapper is a NAMED function template re-deriving its types in its own
/// body, and nested lambdas use only their enclosing template's parameters: an enclosing
/// function template's LOCAL TYPE ALIAS used inside a nested lambda body miscompiles on
/// MSVC (the descriptor-thunk lowering discipline).
///
/// \cond INTERNAL
#pragma once
#include <Ultralight/dom/Event.h>
#include <Ultralight/dom/Holders.h>

#include <type_traits>
#include <tuple>
#include <utility>

namespace ultralight {
namespace dom {
namespace detail {

template <typename>
inline constexpr bool kReceiverAlwaysFalse = false;

template <typename M>
struct MemberCallableSig;
template <typename C, typename Rt, typename... Args>
struct MemberCallableSig<Rt (C::*)(Args...)> {
  using owner = C;
  using args = std::tuple<Args...>;
};
template <typename C, typename Rt, typename... Args>
struct MemberCallableSig<Rt (C::*)(Args...) noexcept> : MemberCallableSig<Rt (C::*)(Args...)> {};
template <typename C, typename Rt, typename... Args>
struct MemberCallableSig<Rt (C::*)(Args...) const> : MemberCallableSig<Rt (C::*)(Args...)> {};
template <typename C, typename Rt, typename... Args>
struct MemberCallableSig<Rt (C::*)(Args...) const noexcept>
    : MemberCallableSig<Rt (C::*)(Args...)> {};
// Lvalue-ref-qualified members deliver like their unqualified twins (delivery invokes on
// lvalue receivers); rvalue-ref-qualified members cannot, and fail with a named message.
template <typename C, typename Rt, typename... Args>
struct MemberCallableSig<Rt (C::*)(Args...) &> : MemberCallableSig<Rt (C::*)(Args...)> {};
template <typename C, typename Rt, typename... Args>
struct MemberCallableSig<Rt (C::*)(Args...) & noexcept>
    : MemberCallableSig<Rt (C::*)(Args...)> {};
template <typename C, typename Rt, typename... Args>
struct MemberCallableSig<Rt (C::*)(Args...) const&> : MemberCallableSig<Rt (C::*)(Args...)> {};
template <typename C, typename Rt, typename... Args>
struct MemberCallableSig<Rt (C::*)(Args...) const & noexcept>
    : MemberCallableSig<Rt (C::*)(Args...)> {};
template <typename C, typename Rt, typename... Args>
struct MemberCallableSig<Rt (C::*)(Args...) &&> {
  static_assert(kReceiverAlwaysFalse<C>,
                "dom: rvalue-ref-qualified member functions cannot receive delivery "
                "(deliveries invoke members on lvalue receivers)");
  using owner = C;
  using args = std::tuple<Args...>;
};
template <typename C, typename Rt, typename... Args>
struct MemberCallableSig<Rt (C::*)(Args...) && noexcept>
    : MemberCallableSig<Rt (C::*)(Args...) &&> {};

// An event can't be copied or moved, so a member taking one by value can't receive the
// wrapper's parameter. The wrapper takes the event by reference instead and hands the member a
// fresh Event (or typed view) over the same handle; every other parameter passes through.
template <typename P>
inline constexpr bool kEventByValue
    = !std::is_reference_v<P> && std::is_base_of_v<Event, std::remove_cv_t<P>>;

template <typename P>
using WrapperParam = std::conditional_t<kEventByValue<P>, const Event&, P>;

template <typename P, typename A>
decltype(auto) PassParam(A&& arg) {
  if constexpr (kEventByValue<P>)
    return std::remove_cv_t<P>(arg.raw());
  else
    return std::forward<A>(arg);
}

template <typename M, typename R, typename... Args>
concept MemberInvocable = requires(M method, R&& receiver, WrapperParam<Args>... args) {
  (static_cast<R&&>(receiver).*method)(
      PassParam<Args>(static_cast<WrapperParam<Args>&&>(args))...);
};

// Borrowed receiver: referenced, not owned.
template <typename C, typename M, typename... Args>
auto WrapBorrowedMemberImpl(C& receiver, M method, std::tuple<Args...>*) {
  constexpr bool kInvocable = MemberInvocable<M, C&, Args...>;
  static_assert(kInvocable,
                "dom: the member function is not callable on this receiver (a const "
                "receiver delivers through const member functions only)");
  if constexpr (kInvocable) {
    return [&receiver, method](WrapperParam<Args>... args) {
      (receiver.*method)(PassParam<Args>(static_cast<WrapperParam<Args>&&>(args))...);
    };
  } else {
    return [](WrapperParam<Args>...) {};
  }
}

// A receiver that dereferences like a pointer but doesn't own the member is almost always a
// smart pointer with no HolderTraits specialization, which lands here as a borrowed receiver.
template <typename C>
concept PointerLikeReceiver = requires(C& receiver) { *receiver; }
    || requires(C& receiver) { receiver.get(); };

template <typename C, typename M>
auto WrapBorrowedMember(C& receiver, M method) {
  using Sig = MemberCallableSig<M>;
  constexpr bool kOwnsMember = std::is_base_of_v<typename Sig::owner, std::remove_cv_t<C>>;
  static_assert(kOwnsMember || !PointerLikeReceiver<C>,
                "dom: this receiver looks like a smart pointer, but ultralight::HolderTraits "
                "isn't specialized for it; specialize ultralight::HolderTraits (see "
                "<Ultralight/HolderTraits.h>) or pass the object itself");
  static_assert(kOwnsMember || PointerLikeReceiver<C>,
                "dom: the member function must belong to the receiver's class");
  if constexpr (kOwnsMember)
    return WrapBorrowedMemberImpl(receiver, method, static_cast<typename Sig::args*>(nullptr));
  else
    return [] {};
}

// Holder receiver: captured by value, locked per delivery, silently skipped when expired.
template <typename H, typename M, typename... Args>
auto WrapHolderMemberImpl(H holder, M method, std::tuple<Args...>*) {
  using LockedRef = decltype(*ultralight::detail::LockHolder(std::declval<const H&>()));
  static_assert(std::is_base_of_v<typename MemberCallableSig<M>::owner,
                                  std::remove_cvref_t<LockedRef>>,
                "dom: the member function must belong to the holder's element type");
  constexpr bool kInvocable = MemberInvocable<M, LockedRef, Args...>;
  static_assert(kInvocable,
                "dom: the member function is not callable on the holder's element (a "
                "holder of const elements delivers through const member functions only)");
  if constexpr (kInvocable) {
    return [holder = std::move(holder), method](WrapperParam<Args>... args) {
      auto&& lock = ultralight::detail::LockHolder(holder);
      if (lock)
        ((*lock).*method)(PassParam<Args>(static_cast<WrapperParam<Args>&&>(args))...);
    };
  } else {
    return [](WrapperParam<Args>...) {};
  }
}

template <typename H, typename M>
auto WrapHolderMember(H holder, M method) {
  using Sig = MemberCallableSig<M>;
  return WrapHolderMemberImpl(std::move(holder), method,
                              static_cast<typename Sig::args*>(nullptr));
}

}  // namespace detail
}  // namespace dom
}  // namespace ultralight
/// \endcond
