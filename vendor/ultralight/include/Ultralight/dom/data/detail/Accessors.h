///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// Implementation detail of the `<Ultralight/dom/data/*.h>` headers; include those instead.
/// Everything here lives in ultralight::dom::data::detail and may change without notice.
///
/// Accessor deduction for schema tokens: given a field accessor (member data pointer, const
/// member function pointer, or a callable taking `const Owner&`), resolves the owner type,
/// the field's decayed value type, and a uniform const-reading invoke. The read direction
/// is the only one that exists: bound instances are const to the library (the Const Law),
/// so mutating accessor shapes are rejected by the IsAccessor concept rather than deduced.
///
/// \cond INTERNAL
#pragma once

#include <type_traits>
#include <utility>

namespace ultralight {
namespace dom {
namespace data {
namespace detail {

template <typename A>
struct AccessorTraits;

template <typename O, typename V>
struct AccessorTraits<V O::*> {
  using owner = O;
  using value = std::remove_cvref_t<V>;
};

template <typename O, typename V>
struct AccessorTraits<V (O::*)() const> {
  using owner = O;
  using value = std::remove_cvref_t<V>;
};

template <typename O, typename V>
struct AccessorTraits<V (O::*)() const noexcept> {
  using owner = O;
  using value = std::remove_cvref_t<V>;
};

// Callables (captureless lambdas taking `const Owner&`), deduced through operator(). The
// four call-operator shapes cover const/noexcept; capturing lambdas are rejected later at
// descriptor lowering (thunks default-construct the closure type), not here.
template <typename L, typename V, typename O>
struct AccessorTraits<V (L::*)(const O&) const> {
  using owner = O;
  using value = std::remove_cvref_t<V>;
};

template <typename L, typename V, typename O>
struct AccessorTraits<V (L::*)(const O&) const noexcept> {
  using owner = O;
  using value = std::remove_cvref_t<V>;
};

template <typename L>
  requires requires { &L::operator(); }
struct AccessorTraits<L> : AccessorTraits<decltype(&L::operator())> {};

///
/// Whether or not A is a usable field accessor: a member pointer, or a callable with an
/// unambiguous const call operator.
///
template <typename A>
concept IsAccessor = std::is_member_pointer_v<std::remove_cvref_t<A>>
    || requires { &std::remove_cvref_t<A>::operator(); };

template <typename A>
using AccessorOwner = typename AccessorTraits<std::remove_cvref_t<A>>::owner;

template <typename A>
using AccessorValue = typename AccessorTraits<std::remove_cvref_t<A>>::value;

/// Uniform const read: invokes any accessor shape against a const instance. Preserves the
/// accessor's value category (a member read yields a const lvalue, never a copy): walks
/// must not duplicate containers or sub-objects, and move-only members (a unique_ptr
/// nullable) cannot be read by value at all. AccessorValue stays decayed for marshalling.
template <typename A, typename Owner>
constexpr decltype(auto) ReadAccessor(const A& accessor, const Owner& instance) {
  using Plain = std::remove_cvref_t<A>;
  if constexpr (std::is_member_object_pointer_v<Plain>) {
    return instance.*accessor;
  } else if constexpr (std::is_member_function_pointer_v<Plain>) {
    return (instance.*accessor)();
  } else {
    return accessor(instance);
  }
}

}  // namespace detail
}  // namespace data
}  // namespace dom
}  // namespace ultralight
/// \endcond
