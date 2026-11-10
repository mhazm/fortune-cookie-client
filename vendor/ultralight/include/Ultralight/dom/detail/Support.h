///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// Implementation detail shared by the `<Ultralight/dom/*.h>` headers; include those
/// instead. Everything here lives in ultralight::dom::detail and may change without notice.
///
/// \cond INTERNAL
#pragma once
#include <Ultralight/detail/FixedString.h>
#include <Ultralight/detail/Marshal.h>

// Property proxies collapse their empty members onto one address where the ABI supports it
// (MSVC and clang-cl honor only the msvc:: spelling and silently ignore the standard one;
// Itanium-ABI compilers the reverse). Where neither applies the members simply keep
// distinct addresses and the enclosing objects grow; behavior is identical.
#if defined(_MSC_VER)
#define ULTRALIGHT_DOM_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#elif defined(__has_cpp_attribute)
#if __has_cpp_attribute(no_unique_address)
#define ULTRALIGHT_DOM_NO_UNIQUE_ADDRESS [[no_unique_address]]
#else
#define ULTRALIGHT_DOM_NO_UNIQUE_ADDRESS
#endif
#else
#define ULTRALIGHT_DOM_NO_UNIQUE_ADDRESS
#endif

namespace ultralight {
namespace dom {
namespace detail {

// The string adapters and the FixedString template parameter literal live in the
// family-neutral ultralight::detail; these re-exports keep the established dom::detail
// spellings working.
using ultralight::detail::CString;
using ultralight::detail::FixedString;
using ultralight::detail::TakeString;

}  // namespace detail
}  // namespace dom
}  // namespace ultralight
/// \endcond
