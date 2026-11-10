///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// Implementation detail shared by the typed injection filters (`<Ultralight/js/API.h>` and
/// `<Ultralight/dom/Triggers.h>`); include those instead. Everything here lives in
/// ultralight::detail and may change without notice.
///
/// \cond INTERNAL
#pragma once
#include <Ultralight/detail/Exceptions.h>

namespace ultralight {

class View;

namespace detail {

// Per family: builds the typed filter request from the C request (ToRequest) and reports a
// filter that threw (ReportException). Specialized beside each family's InjectionRequest.
template <typename CRequest>
struct InjectionRequestTraits;

// Named functor (never a lambda: the MSVC alias-in-lambda discipline) bridging a View's
// C-shaped filter slot to a typed callable. It keeps the View the filter was set on: the C
// request names a ULView only for filters set through the C API. A filter that throws
// withholds the page (unwinding into the library is undefined behavior).
template <typename Fn, typename CRequest>
struct InjectionFilterAdapter {
  Fn fn;
  View* view;

  static bool Invoke(void* user_data, const CRequest* request) {
    auto& self = *static_cast<InjectionFilterAdapter*>(user_data);
    bool allow = false;
    CallCatchingExceptions(
        [&] {
          allow = static_cast<bool>(
              self.fn(InjectionRequestTraits<CRequest>::ToRequest(self.view, *request)));
        },
        [](const char* what) { InjectionRequestTraits<CRequest>::ReportException(what); });
    return allow;
  }

  static void Destroy(void* user_data) { delete static_cast<InjectionFilterAdapter*>(user_data); }
};

}  // namespace detail
}  // namespace ultralight
/// \endcond
