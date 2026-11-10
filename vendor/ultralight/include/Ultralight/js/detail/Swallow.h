///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// Implementation detail of the `<Ultralight/js/*.h>` headers; include those instead.
/// Everything here lives in ultralight::js::detail and may change without notice.
///
/// \cond INTERNAL
#pragma once
#include <Ultralight/CAPI/CAPI_JSValue.h>

namespace ultralight {
namespace js {
namespace detail {

// Owns the exception a documented best-effort read chooses to discard. Passing a real
// out-parameter (instead of NULL) marks the exception as observed by the caller, so the
// diagnostics layer's unobserved-exception log stays reserved for exceptions nobody could
// ever have examined.
struct SwallowedException {
  ULJSValue handle = nullptr;

  SwallowedException() = default;
  SwallowedException(const SwallowedException&) = delete;
  SwallowedException& operator=(const SwallowedException&) = delete;
  ~SwallowedException() {
    if (handle)
      ulDestroyJSValue(handle);
  }

  ULJSValue* out() { return &handle; }
};

}  // namespace detail
}  // namespace js
}  // namespace ultralight
/// \endcond
