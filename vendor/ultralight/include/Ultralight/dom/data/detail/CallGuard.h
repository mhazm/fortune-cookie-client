///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// Implementation detail of `<Ultralight/dom/data/Context.h>`; include that header
/// instead. Everything here lives in ultralight::dom::data::detail and may change without
/// notice.
///
/// \cond INTERNAL
#pragma once
#include <Ultralight/CAPI/CAPI_DOMData.h>
#include <Ultralight/detail/Exceptions.h>

#include <string>
#include <utility>

namespace ultralight {
namespace dom {
namespace data {
namespace detail {

// Runs user code the library calls (a handler, formatter, posted task, validator, Sync read,
// or holder lock). With exceptions enabled, an exception must not unwind into the library:
// it's reported under `name` through ulDOMDataContextReportDrop() (`context` can be null),
// and the call returns false so the caller can fall back. Returns true when `body` finished.
//
// This body (and every thunk that calls it) differs with the exception setting under one
// name, so files compiled with and without exceptions may share a program only if they don't
// instantiate the typed layer for the same types or callables (the linker keeps only one of the
// two bodies). One setting everywhere is the safe choice.
template <typename Body>
bool CallHandler(ULDOMDataContext context, const char* name, Body&& body) {
  bool finished = true;
  ultralight::detail::CallCatchingExceptions(std::forward<Body>(body), [&](const char* what) {
    finished = false;
    std::string reason = "the call was dropped because it threw a C++ exception";
    if (what) {
      reason += ": ";
      reason += what;
    }
    ulDOMDataContextReportDrop(context, name, reason.c_str());
  });
  return finished;
}

}  // namespace detail
}  // namespace data
}  // namespace dom
}  // namespace ultralight
/// \endcond
