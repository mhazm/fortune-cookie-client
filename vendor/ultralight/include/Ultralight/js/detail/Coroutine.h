///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// Implementation detail of `<Ultralight/js/Task.h>`; include that header instead.
/// The coroutine promise machinery behind js::Task and the awaitables behind js::RunOnWorker
/// and js::Await. Defines ULTRALIGHT_JS_TASK_EH (documented in `<Ultralight/js/Task.h>`).
///
#pragma once
#include <Ultralight/CAPI/CAPI_JSAPI.h>
#include <Ultralight/CAPI/CAPI_JSRuntime.h>
#include <Ultralight/detail/Exceptions.h>
#include <Ultralight/js/Error.h>
#include <Ultralight/js/Resolver.h>
#include <Ultralight/js/Value.h>
#include <Ultralight/js/detail/Marshal.h>

#include <coroutine>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

#if defined(__cpp_exceptions) || defined(_CPPUNWIND) || defined(__EXCEPTIONS)
#define ULTRALIGHT_JS_TASK_EH 1
#else
#define ULTRALIGHT_JS_TASK_EH 0
#endif

#if ULTRALIGHT_JS_TASK_EH
#include <exception>
#endif

/// \cond INTERNAL

namespace ultralight {
namespace js {

template <typename T>
class Task;

namespace detail {

template <typename T>
struct IsTask : std::false_type {};
template <typename T>
struct IsTask<Task<T>> : std::true_type {};

template <typename T>
struct IsExpectedType : std::false_type {};
template <typename T, typename E>
struct IsExpectedType<Expected<T, E>> : std::true_type {};

// State a binding attaches to a started Task so it lives exactly as long as the coroutine
// frame: converted argument storage, owned argument handles, a protected class instance, a
// locked holder, the bound callable. Attachments chain; the most recent is destroyed first.
struct TaskKeepAlive {
  virtual ~TaskKeepAlive() = default;
  std::unique_ptr<TaskKeepAlive> next;
};

// The part of a Task's promise that doesn't depend on T: what the library needs to stop a
// chain of Tasks (a started Task and the Tasks it awaits, recursively).
struct TaskFrame {
  // The frame whose Resolver settles the page's Promise for the chain this frame runs in:
  // itself once Start() gives it a Resolver, the awaiting parent's root for an awaited Task,
  // and null for a Task awaited by a coroutine that isn't a Task.
  TaskFrame* root_ = nullptr;
  std::coroutine_handle<> handle_;
  std::optional<Resolver> resolver_;  // Detached (Start) mode.
  // The bound path, for the error a stopped chain rejects with. It points into the bound
  // callable, which the chain keeps alive.
  const char* call_path_ = nullptr;
};

// The binding layer's access to a Task it is about to start (a friend of Task).
struct TaskAccess {
  // Ties `attachment` (and anything already chained behind it) to the frame's lifetime. Call
  // it before Task::Start(): the body has not run yet (initial_suspend is suspend_always), so
  // it sees the attachment from its first line.
  template <typename T>
  static void KeepAlive(Task<T>& task, std::unique_ptr<TaskKeepAlive> attachment) {
    if (!task.handle_)
      return;
    TaskKeepAlive* tail = attachment.get();
    while (tail->next)
      tail = tail->next.get();
    tail->next = std::move(task.handle_.promise().keep_alive_);
    task.handle_.promise().keep_alive_ = std::move(attachment);
  }

  template <typename T>
  static void SetCallPath(Task<T>& task, const char* path) {
    if (task.handle_)
      task.handle_.promise().call_path_ = path;
  }
};

// The Task frame behind a coroutine handle, or null when the coroutine isn't a Task.
template <typename P>
TaskFrame* FrameOf(std::coroutine_handle<P> handle) {
  if constexpr (std::is_base_of_v<TaskFrame, P>)
    return &handle.promise();
  else
    return nullptr;
}

// Runs at the library's asynchronous resume points (a RunOnWorker completion, an Await
// settle), before resuming `frame`. When the chain it runs in was started by a binding whose
// API's last owner is gone, the chain must not run again: the object that owned the API (which
// a `[this]` capture or a js::Bind(this, ...) method refers to) may already be destroyed.
// Rejects the page's Promise with ULJS_DETACHED, destroys the chain from its root (every
// frame's locals, never its body), and returns true.
inline bool StopIfDetached(TaskFrame* frame) {
  TaskFrame* root = frame ? frame->root_ : nullptr;
  if (!root || !root->resolver_ || !ulJSPromiseResolverIsDetached(root->resolver_->raw()))
    return false;
  std::string message = root->call_path_ ? std::string(root->call_path_) + ": " : std::string();
  message += "the call stopped because its JavaScript API was destroyed";
  root->resolver_->Reject(Error::TypeError(std::move(message)).WithCode("ULJS_DETACHED"));
  root->handle_.destroy();
  return true;
}

// The types below differ with ULTRALIGHT_JS_TASK_EH (a member body or a data member), so each
// selection gets its own inline namespace. That keeps these types apart; the typed layer's
// templates outside them are still shared by name (see <Ultralight/js/Task.h>).
#if ULTRALIGHT_JS_TASK_EH
inline namespace task_eh_on {
#else
inline namespace task_eh_off {
#endif

template <typename T>
struct TaskPromise;

///
/// Shared coroutine promise state. The whole Task machinery runs on the Renderer's thread
/// (creation, awaiting, delivery, resumption); only the payloads it awaits (worker results,
/// promise settlements) cross threads, through the deferred queues that already own that
/// problem. That is what keeps this state lock-free.
///
template <typename T>
struct TaskPromiseBase : TaskFrame {
  std::optional<Result<T>> result_;
  std::coroutine_handle<> continuation_;     // Awaited mode.
  // Declared last so it is destroyed first; the frame's own locals are already gone by then.
  std::unique_ptr<TaskKeepAlive> keep_alive_;

  // Lazy start: the caller attaches a resolver (Start) or a continuation (co_await) before
  // the body runs, so awaitables inside the body always see a fully wired promise.
  std::suspend_always initial_suspend() noexcept { return {}; }

  struct FinalAwaiter {
    bool await_ready() noexcept { return false; }
    std::coroutine_handle<> await_suspend(std::coroutine_handle<TaskPromise<T>> h) noexcept {
      TaskPromiseBase& promise = h.promise();
      if (promise.continuation_)
        return promise.continuation_;  // Awaited: the owning Task destroys the frame.
      if (promise.resolver_)
        promise.DeliverToResolver();
      // Detached: self-destroy (legal here; the coroutine is suspended at its final point).
      h.destroy();
      return std::noop_coroutine();
    }
    void await_resume() noexcept {}
  };
  FinalAwaiter final_suspend() noexcept { return {}; }

  void DeliverToResolver() {
    // A coroutine that ran off the end without co_return (possible only for Task<void>)
    // still has a result; non-void bodies are required by the language to co_return.
    Result<T>& result = *result_;
    if (result.has_value()) {
      if constexpr (std::is_void_v<T>)
        resolver_->Resolve();
      else
        resolver_->Resolve(std::move(result).value());
    } else {
      resolver_->Reject(std::move(result).error());
    }
  }

  Result<T> TakeResult() { return std::move(*result_); }

#if ULTRALIGHT_JS_TASK_EH
  void unhandled_exception() noexcept {
    try {
      throw;
    } catch (const std::exception& e) {
      result_.emplace(Unexpected<Error>(NativeExceptionError(nullptr, e.what())));
    } catch (...) {
      result_.emplace(Unexpected<Error>(NativeExceptionError(nullptr, nullptr)));
    }
  }
#else
  void unhandled_exception() noexcept {}  // Exceptions disabled: unreachable.
#endif
};

template <typename T>
struct TaskPromise : TaskPromiseBase<T> {
  Task<T> get_return_object();
  void return_value(T value) { this->result_.emplace(std::move(value)); }
  void return_value(Error error) {
    this->result_.emplace(Unexpected<Error>(std::move(error)));
  }
  void return_value(Result<T> result) { this->result_.emplace(std::move(result)); }
};

template <>
struct TaskPromise<void> : TaskPromiseBase<void> {
  Task<void> get_return_object();
  // The language permits exactly one of return_void/return_value, so a Task<void> body
  // cannot `co_return Error`; fallible operations use Task<T> or the callback-style
  // Resolver (or throw, when compiled with exceptions).
  void return_void() { this->result_.emplace(); }
};

///
/// Awaitable behind js::RunOnWorker. The callable runs on the library's background pool; the
/// coroutine resumes on the Renderer's thread with its result. The result lives in the
/// coroutine frame (this awaitable); the heap Job only carries the resume ticket, and its
/// destroy (which runs after the completion) frees it.
///
template <typename Fn>
class WorkerAwaitable {
 public:
  using R = std::invoke_result_t<Fn&>;

  explicit WorkerAwaitable(Fn fn) : fn_(std::move(fn)) {}

  bool await_ready() const noexcept { return false; }

  template <typename P>
  void await_suspend(std::coroutine_handle<P> handle) {
    auto* job = new Job { this, handle, FrameOf(handle) };
    ulJSRunOnWorker(&RunWork, &RunComplete, job, &DestroyJob);
  }

  R await_resume() {
#if ULTRALIGHT_JS_TASK_EH
    if (exception_)
      std::rethrow_exception(exception_);
#endif
    if constexpr (!std::is_void_v<R>)
      return std::move(*result_);
  }

 private:
  struct Job {
    WorkerAwaitable* self;  // Valid while the coroutine is suspended here.
    std::coroutine_handle<> handle;
    TaskFrame* frame;  // Null when the awaiting coroutine isn't a Task.
  };

  static void RunWork(void* user_data) {
    Job* job = static_cast<Job*>(user_data);
#if ULTRALIGHT_JS_TASK_EH
    // An exception escaping onto the pool thread would terminate the process; carry it back
    // and rethrow during resumption, where the Task's promise converts it to a rejection.
    try {
#endif
      if constexpr (std::is_void_v<R>) {
        job->self->fn_();
        job->self->result_ = true;
      } else {
        job->self->result_.emplace(job->self->fn_());
      }
#if ULTRALIGHT_JS_TASK_EH
    } catch (...) {
      job->self->exception_ = std::current_exception();
    }
#endif
  }

  static void RunComplete(void* user_data) {
    // After this resume (or a stop) the coroutine (and this awaitable) may already be gone;
    // only the Job may be touched afterwards, and only by DestroyJob.
    Job* job = static_cast<Job*>(user_data);
    if (StopIfDetached(job->frame))
      return;
    job->handle.resume();
  }

  static void DestroyJob(void* user_data) { delete static_cast<Job*>(user_data); }

  Fn fn_;
  std::conditional_t<std::is_void_v<R>, bool, std::optional<R>> result_ {};
#if ULTRALIGHT_JS_TASK_EH
  std::exception_ptr exception_;
#endif
};

///
/// Awaitable behind js::Await: suspends on a JavaScript promise via the waist observer and
/// resumes on the Renderer's thread when it settles (or its context goes away), yielding a
/// Result<T>. A value that isn't a Promise is adopted the way `await` adopts it. The Job
/// outlives the settle callback (its destroy runs after), which is what makes reading the
/// result during resumption safe even when the coroutine frame, and this awaitable with it, is
/// destroyed before the callback returns.
///
template <typename T>
class PromiseAwaitable {
  static_assert(!IsCallScopedView<std::remove_cvref_t<T>>::value,
                "an awaited promise result cannot be a std::span (the view would dangle "
                "when the coroutine resumes); await std::vector<E> or RefPtr<Buffer>");

 public:
  explicit PromiseAwaitable(Value value) : value_(std::move(value)) {}
  explicit PromiseAwaitable(Error immediate) {
    immediate_.emplace(Unexpected<Error>(std::move(immediate)));
  }

  bool await_ready() const noexcept { return false; }

  template <typename P>
  bool await_suspend(std::coroutine_handle<P> handle) {
    if (immediate_)
      return false;  // Resume immediately; await_resume reports the result.
    if (value_.IsEmpty()) {
      immediate_.emplace(Unexpected<Error>(ErrorAccess::Empty()));
      return false;
    }
    if (!value_.IsAlive()) {
      immediate_.emplace(Unexpected<Error>(Error::PageGone()));
      return false;
    }
    if (!value_.IsPromise()) {
      if (!value_.IsObject()) {
        immediate_.emplace(ConvertBorrowed<std::remove_cvref_t<T>>(value_.raw()));
        return false;
      }
      Result<Value> adopted = PageResolve(value_);
      if (!adopted) {
        immediate_.emplace(Unexpected<Error>(std::move(adopted).error()));
        return false;
      }
      value_ = std::move(adopted).value();
    }
    auto* job = new Job { handle, FrameOf(handle), std::nullopt };
    if (!ulJSPromiseThen(value_.raw(), &OnSettled, job, &DestroyJob)) {
      // The registration's inline destroy freed the job.
      immediate_.emplace(Unexpected<Error>(value_.IsAlive()
                                               ? Error::TypeError("The value is not a Promise.")
                                               : Error::PageGone()));
      return false;
    }
    job_ = job;
    return true;
  }

  Result<T> await_resume() {
    if (immediate_)
      return std::move(*immediate_);
    return std::move(*job_->result);
  }

 private:
  struct Job {
    std::coroutine_handle<> handle;
    TaskFrame* frame;  // Null when the awaiting coroutine isn't a Task.
    std::optional<Result<T>> result;
  };

  // `await x` on an object runs the page's own Promise.resolve(x), which calls a thenable's
  // then(); doing the same keeps the page code that runs identical.
  static Result<Value> PageResolve(const Value& value) {
    ULJSContext ctx = ulJSValueGetContext(value.raw());
    Value global = Value::Adopt(ulJSContextGetGlobalObject(ctx));
    ulDestroyJSContext(ctx);
    Value promise_ctor = global["Promise"];
    Value resolve = promise_ctor["resolve"];
    if (!resolve.IsCallable())
      return Unexpected<Error>(Error::TypeError("Promise.resolve is not a function."));
    return resolve.InvokeOn(promise_ctor, value);
  }

  static void OnSettled(void* user_data, ULJSContext, ULJSValue result, bool rejected) {
    Job* job = static_cast<Job*>(user_data);
    if (StopIfDetached(job->frame))
      return;
    if (!result) {
      job->result.emplace(Unexpected<Error>(Error::PageGone()));
    } else if (rejected) {
      job->result.emplace(Unexpected<Error>(Error::AdoptException(ulCreateJSValueRef(result))));
    } else {
      // A converter that throws must not unwind into the library's settle sweep.
      ultralight::detail::CallCatchingExceptions(
          [&] { job->result.emplace(ConvertBorrowed<std::remove_cvref_t<T>>(result)); },
          [&](const char* what) {
            job->result.emplace(Unexpected<Error>(NativeExceptionError(nullptr, what)));
          });
    }
    job->handle.resume();
  }

  static void DestroyJob(void* user_data) { delete static_cast<Job*>(user_data); }

  Value value_;
  Job* job_ = nullptr;
  std::optional<Result<T>> immediate_;
};

}  // inline namespace task_eh_on / task_eh_off

}  // namespace detail
}  // namespace js
}  // namespace ultralight
/// \endcond
