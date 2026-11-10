///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// Implementation detail shared by the typed C++ header families (`<Ultralight/js/*.h>`,
/// `<Ultralight/dom/*.h>`); include those instead. Everything here lives in
/// ultralight::detail and may change without notice.
///
/// \cond INTERNAL
#pragma once

#if defined(__cpp_exceptions) || defined(_CPPUNWIND) || defined(__EXCEPTIONS)
#define ULTRALIGHT_DETAIL_EH 1
#include <exception>
#else
#define ULTRALIGHT_DETAIL_EH 0
#endif

namespace ultralight {
namespace detail {

// The body differs with ULTRALIGHT_DETAIL_EH, so each selection gets its own inline namespace
// (as in js/detail/Coroutine.h). That keeps only this function's two definitions apart: the
// typed-layer templates that call it have one name for both settings, and the linker keeps
// one definition of each. So files compiled with different exception settings can share a
// program only if they don't use the typed layers with the same types or callable signatures.
#if ULTRALIGHT_DETAIL_EH
inline namespace eh_on {
#else
inline namespace eh_off {
#endif

// Every trampoline the library calls user code through runs it here. Without exceptions this
// is a plain call. With exceptions, an exception escaping `body` must not unwind into the
// library's C frames (undefined behavior): `on_exception` runs instead, receiving what() for
// a std::exception and nullptr for anything else.
template <typename Body, typename OnException>
void CallCatchingExceptions(Body&& body, OnException&& on_exception) {
#if ULTRALIGHT_DETAIL_EH
  try {
    body();
  } catch (const std::exception& e) {
    on_exception(e.what());
  } catch (...) {
    on_exception(static_cast<const char*>(nullptr));
  }
#else
  (void)on_exception;
  body();
#endif
}

}  // inline namespace eh_on / eh_off

}  // namespace detail
}  // namespace ultralight
/// \endcond
