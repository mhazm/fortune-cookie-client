///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// Coroutines for async JavaScript bindings: js::Task, js::RunOnWorker, and js::Await.
/// `<Ultralight/JS.h>` includes this header (and with it `<coroutine>`).
///
/// ULTRALIGHT_JS_TASK_EH is 1 when the including file is compiled with C++ exceptions. Then an
/// exception that escapes a Task rejects its Promise.
///
#pragma once
#include <Ultralight/CAPI/CAPI_JSAPI.h>
#include <Ultralight/CAPI/CAPI_JSRuntime.h>
#include <Ultralight/js/Resolver.h>
#include <Ultralight/js/TypeTraits.h>
#include <Ultralight/js/Value.h>
#include <Ultralight/js/detail/Coroutine.h>

#include <cassert>
#include <coroutine>
#include <type_traits>
#include <utility>

namespace ultralight {
namespace js {

///
/// A coroutine that returns a JavaScript Promise to the page.
///
/// A Task is a C++20 coroutine. Returning one from a bound function gives the page a JavaScript
/// Promise that settles during a later Renderer::Update() after the coroutine finishes.
///
/// The coroutine executes sequentially from top to bottom, pausing at each `co_await` without
/// blocking the Renderer's thread.
///
/// Return a Task from a bound function to perform background work and return a Promise:
///
/// ```
/// app["loadSave"] = [](std::string slot) -> js::Task<SaveData> {
///   SaveData data =
///       co_await js::RunOnWorker([slot] { return ReadSaveFromDisk(slot); });
///   if (!data.valid)
///     co_return js::Error::Make(js::ErrorType::Error, "save is corrupt")
///         .WithCode("BAD_SAVE");
///   co_return data;
/// };
/// ```
///
/// Page script awaits the returned Promise normally:
///
/// ```js
/// const save = await app.loadSave("slot1");
/// ```
///
/// ## JavaScript Equivalents
///
/// Task maps standard JavaScript asynchronous syntax directly to C++ coroutine keywords:
///
/// | JavaScript          | C++                              |
/// |---------------------|----------------------------------|
/// | `async function`    | `[](...) -> js::Task<T>`         |
/// | `return value;`     | `co_return value;`               |
/// | `throw new Error()` | `co_return js::Error::Make(...)` |
/// | `await promise`     | `co_await js::Await<T>(promise)` |
///
/// ## Awaiting Page Promises
///
/// Pass a call to a page function to Await() to invoke it and await its returned Promise:
///
/// ```
/// app["confirm"] = [](js::Value ask) -> js::Task<bool> {
///   js::Result<bool> answer = co_await js::Await<bool>(ask("Overwrite?"));
///   co_return answer ? *answer : false;
/// };
/// ```
///
/// The returned Result holds the resolved value converted to the requested type, or an Error
/// holding the rejection reason, a type conversion failure, or a page-gone error if the page
/// navigates away first.
///
/// ## Threads and Timing
///
/// Coroutines follow these execution and threading rules:
///
/// - **The coroutine body runs on the Renderer's thread.** It starts synchronously inside the
///   page's call up to the first `co_await`, and resumes on the Renderer's thread after each await.
/// - **Worker threads must not touch JavaScript.** Use RunOnWorker() to run background operations
///   on the worker pool, passing plain C++ data in and out without touching Value or Context.
/// - **The Promise settles during a later Renderer::Update().** Completions arrive on the
///   Renderer's thread when your frame loop updates the renderer.
/// - **Parameters stay valid for the entire Task.** Arguments, references, and views like
///   `std::string_view` and `std::span` remain valid across every `co_await` until the task
///   completes.
///
/// ## Page Navigation and API Teardown
///
/// Waiting tasks handle navigation and destruction safely:
///
/// - **A navigating page lets waiting tasks resume.** Awaiting a Promise from that page yields a
///   page-gone error, operations on its values fail safely, and the library discards the task's
///   final result when it completes.
/// - **Destroying the API cancels waiting tasks.** Once the last owning handle to the API is
///   destroyed, waiting tasks don't resume after their next `co_await`, and their Promises reject
///   with a TypeError whose code is `"ULJS_DETACHED"`.
///
/// @note  A `Task<void>` can't co_return an error. Return `Task<bool>` instead or use a Resolver to
///        report failures from a void operation.
///
/// @see  Resolver, RunOnWorker(), Await()
///
template <typename T = void>
class [[nodiscard]] Task {
 public:
  static_assert(!detail::IsExpectedType<T>::value,
                "co_return the value or the js::Error directly from Task<U>; Task's promise "
                "already carries the error channel");
  static_assert(!std::is_same_v<T, Error>, "a Task cannot resolve with a js::Error");

  using promise_type = detail::TaskPromise<T>;
  using Handle = std::coroutine_handle<promise_type>;

  ///
  /// Move constructor (`other` becomes empty).
  ///
  Task(Task&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}
  Task(const Task&) = delete;
  Task& operator=(const Task&) = delete;

  ///
  /// Move assignment (discards this Task if it never started, and `other` becomes empty).
  ///
  Task& operator=(Task&& other) noexcept {
    if (this != &other) {
      if (handle_)
        handle_.destroy();
      handle_ = std::exchange(other.handle_, {});
    }
    return *this;
  }

  ///
  /// Destructor. A Task that never started is discarded without running its body.
  ///
  ~Task() {
    if (handle_)
      handle_.destroy();
  }

  ///
  /// Start the Task and send its result to `resolver`.
  ///
  /// This Task becomes empty, and the coroutine frees itself when it finishes.
  ///
  /// @param  resolver  The Resolver that settles with the result.
  ///
  /// @pre  Must be called on the Renderer's thread.
  ///
  void Start(Resolver resolver) && {
    Handle handle = std::exchange(handle_, {});
    assert(handle && "Start on an empty Task");
    handle.promise().resolver_.emplace(std::move(resolver));
    handle.promise().root_ = &handle.promise();
    handle.resume();
  }

  ///
  /// Run this Task from another Task and wait for it (eg, `co_await std::move(task)`).
  ///
  /// @return  Returns an awaitable that yields a Result<T>.
  ///
  auto operator co_await() && {
    assert(handle_ && "co_await on an empty Task");
    return Awaiter { handle_ };
  }

  ///
  /// Awaiting a Task by name is a compile error: a Task runs once, so write
  /// `co_await std::move(task)`.
  ///
  template <typename U = T>
  void operator co_await() & {
    static_assert(detail::kAlwaysFalse<U>,
                  "co_await std::move(task): awaiting a Task runs it once and consumes it");
  }

 private:
  friend promise_type;
  friend struct detail::TaskAccess;

  struct Awaiter {
    Handle handle;
    bool await_ready() const noexcept { return false; }
    template <typename P>
    std::coroutine_handle<> await_suspend(std::coroutine_handle<P> continuation) noexcept {
      handle.promise().continuation_ = continuation;
      // An awaited Task joins its parent's chain, so it stops when the parent's would.
      if (detail::TaskFrame* parent = detail::FrameOf(continuation))
        handle.promise().root_ = parent->root_;
      return handle;  // Symmetric transfer: starts the awaited coroutine now.
    }
    Result<T> await_resume() { return handle.promise().TakeResult(); }
  };

  explicit Task(Handle handle) : handle_(handle) {}
  Handle handle_;
};

/// \cond INTERNAL
namespace detail {

template <typename T>
Task<T> TaskPromise<T>::get_return_object() {
  auto handle = std::coroutine_handle<TaskPromise<T>>::from_promise(*this);
  this->handle_ = handle;
  return Task<T>(handle);
}

inline Task<void> TaskPromise<void>::get_return_object() {
  auto handle = std::coroutine_handle<TaskPromise<void>>::from_promise(*this);
  handle_ = handle;
  return Task<void>(handle);
}

}  // namespace detail
/// \endcond

///
/// Run a function on a background thread, then resume the coroutine on the Renderer's thread
/// with its return value:
///
/// ```
/// SaveData data = co_await js::RunOnWorker([&] { return ReadSaveFromDisk(slot); });
/// ```
///
/// Unlike js::Await() and awaiting a Task, this gives you the function's return value directly
/// (not a Result), so report failures inside that value or, with C++ exceptions enabled, by
/// throwing.
///
/// @param  fn  The function to run. It takes no arguments.
///
/// @return  Returns an awaitable that yields `fn`'s return value.
///
/// \parblock
/// @note  The background threads are a small pool shared by the whole library (up to 4
///        threads, fewer on machines with few cores), so don't block one for a long time.
/// \endparblock
///
/// \parblock
/// @note  If the Renderer is destroyed while `fn` is still waiting for a thread, `fn` never
///        runs and the coroutine never resumes (its locals are never destroyed). Let pending
///        Tasks finish before you destroy the Renderer.
/// \endparblock
///
/// \parblock
/// @note  With C++ exceptions enabled, an exception thrown by `fn` is rethrown in the coroutine
///        when it resumes, so it rejects the Task's Promise.
/// \endparblock
///
/// @warning  Don't touch JavaScript in `fn` (js::Value and js::Context only work on the
///           Renderer's thread). Pass plain C++ data in and out.
///
template <typename Fn>
auto RunOnWorker(Fn&& fn) {
  return detail::WorkerAwaitable<std::decay_t<Fn>>(std::forward<Fn>(fn));
}

///
/// Wait for a JavaScript promise from a Task:
///
/// ```
/// Result<std::string> text = co_await js::Await<std::string>(promise);
/// ```
///
/// The Task resumes on the Renderer's thread during a Renderer::Update() after the promise
/// settles. The Result holds the value converted to T, or an Error: the rejection reason, a
/// TypeError if the value doesn't convert, or a page-gone Error if the page goes away first.
///
/// Like JavaScript's `await`, this also takes a value that isn't a Promise. A value that isn't
/// an object resumes the Task right away with that value. Any other object (eg, a thenable)
/// goes through the page's `Promise.resolve()` first, which can run page code.
///
/// @param  promise  The promise (or other value) to wait for.
///
/// @return  Returns an awaitable that yields a Result<T>.
///
/// \parblock
/// @note  If `promise` is empty, the Task resumes right away with an empty-handle Error (or a
///        page-gone Error if its page is gone).
/// \endparblock
///
/// \parblock
/// @note  A rejection you wait for this way counts as handled, so the page doesn't report it
///        as unhandled.
/// \endparblock
///
template <typename T = Value>
  requires Marshalable<T>
auto Await(const Value& promise) {
  return detail::PromiseAwaitable<T>(promise);
}

///
/// Call a JavaScript function and wait for the Promise it returns, from a Task:
///
/// ```
/// Result<bool> ok = co_await js::Await<bool>(ask("Overwrite?"));
/// Result<double> saved = co_await js::Await<double>(game["save"]("slot1"));
/// ```
///
/// This waits for the call's result like the overload above. If the call itself fails (it
/// throws, or the function is empty or its page is gone), the Task resumes right away with
/// that Error.
///
/// @param  call  The result of the call (a js::Result<js::Value>).
///
/// @return  Returns an awaitable that yields a Result<T>.
///
template <typename T = Value, typename R>
  requires(Marshalable<T> && std::is_same_v<std::remove_cvref_t<R>, Result<Value>>)
auto Await(R&& call) {
  if (!call)
    return detail::PromiseAwaitable<T>(std::forward<R>(call).error());
  return detail::PromiseAwaitable<T>(std::forward<R>(call).value());
}

}  // namespace js
}  // namespace ultralight
