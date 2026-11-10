///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// Implementation detail shared by the typed C++ header families (`<Ultralight/js/*.h>`,
/// `<Ultralight/dom/*.h>`); include those instead. Everything here lives in
/// ultralight::detail and may change without notice. Each family re-exports this surface
/// under its own namespace (for example js::Expected / js::Result and dom::Expected /
/// dom::Result) with its own error type; the reader-facing documentation lives on those
/// aliases.
///
/// \cond INTERNAL
#pragma once

#include <cassert>
#include <new>
#include <type_traits>
#include <utility>

// Always this implementation, never std::expected, so the families' Result types are the same
// type in every translation unit whatever its -std level (files that disagreed would otherwise
// merge two layouts under one name). The surface mirrors std::expected, monadic operations
// included, so most code written against std::expected compiles unchanged.

namespace ultralight {
namespace detail {

// Carrier for an error value being returned through an Expected (mirrors std::unexpected).
template <typename E>
class Unexpected {
 public:
  explicit Unexpected(E error) : error_(std::move(error)) {}

  E& error() & { return error_; }
  const E& error() const& { return error_; }
  E&& error() && { return std::move(error_); }

 private:
  E error_;
};

template <typename E>
Unexpected(E) -> Unexpected<E>;

// A value of type T or an error of type E (mirrors the std::expected surface). Never throws C++
// exceptions: accessing value() when has_value() is false is undefined behavior (asserted
// in debug builds), so check first or use value_or().
template <typename T, typename E>
class Expected {
 public:
  Expected(T value) : has_value_(true) { new (&storage_.value) T(std::move(value)); }

  template <typename G>
  Expected(Unexpected<G> unexpected) : has_value_(false) {
    new (&storage_.error) E(std::move(unexpected).error());
  }

  Expected(const Expected& other) : has_value_(other.has_value_) {
    if (has_value_)
      new (&storage_.value) T(other.storage_.value);
    else
      new (&storage_.error) E(other.storage_.error);
  }

  Expected(Expected&& other) noexcept : has_value_(other.has_value_) {
    if (has_value_)
      new (&storage_.value) T(std::move(other.storage_.value));
    else
      new (&storage_.error) E(std::move(other.storage_.error));
  }

  Expected& operator=(Expected other) noexcept {
    this->~Expected();
    new (this) Expected(std::move(other));
    return *this;
  }

  ~Expected() {
    if (has_value_)
      storage_.value.~T();
    else
      storage_.error.~E();
  }

  bool has_value() const { return has_value_; }
  explicit operator bool() const { return has_value_; }

  T& value() & { assert(has_value_); return storage_.value; }
  const T& value() const& { assert(has_value_); return storage_.value; }
  T&& value() && { assert(has_value_); return std::move(storage_.value); }

  E& error() & { assert(!has_value_); return storage_.error; }
  const E& error() const& { assert(!has_value_); return storage_.error; }
  E&& error() && { assert(!has_value_); return std::move(storage_.error); }

  T& operator*() & { return value(); }
  const T& operator*() const& { return value(); }
  T* operator->() { return &value(); }
  const T* operator->() const { return &value(); }

  template <typename U>
  T value_or(U&& fallback) const& {
    return has_value_ ? storage_.value : static_cast<T>(std::forward<U>(fallback));
  }

  template <typename U>
  T value_or(U&& fallback) && {
    return has_value_ ? std::move(storage_.value) : static_cast<T>(std::forward<U>(fallback));
  }

  template <typename G>
  E error_or(G&& fallback) const& {
    return has_value_ ? static_cast<E>(std::forward<G>(fallback)) : storage_.error;
  }

  template <typename G>
  E error_or(G&& fallback) && {
    return has_value_ ? static_cast<E>(std::forward<G>(fallback)) : std::move(storage_.error);
  }

  // The monadic operations, with std::expected's semantics: and_then / transform run on a
  // value and pass an error through; or_else / transform_error run on an error and pass a
  // value through. A callable given to and_then returns an Expected with this error type,
  // one given to or_else returns an Expected with this value type.
  template <typename F>
  auto and_then(F&& f) & {
    using R = std::remove_cvref_t<std::invoke_result_t<F, T&>>;
    if (has_value_)
      return R(std::forward<F>(f)(storage_.value));
    return R(Unexpected<E>(storage_.error));
  }

  template <typename F>
  auto and_then(F&& f) const& {
    using R = std::remove_cvref_t<std::invoke_result_t<F, const T&>>;
    if (has_value_)
      return R(std::forward<F>(f)(storage_.value));
    return R(Unexpected<E>(storage_.error));
  }

  template <typename F>
  auto and_then(F&& f) && {
    using R = std::remove_cvref_t<std::invoke_result_t<F, T&&>>;
    if (has_value_)
      return R(std::forward<F>(f)(std::move(storage_.value)));
    return R(Unexpected<E>(std::move(storage_.error)));
  }

  template <typename F>
  auto or_else(F&& f) & {
    using R = std::remove_cvref_t<std::invoke_result_t<F, E&>>;
    if (has_value_)
      return R(storage_.value);
    return R(std::forward<F>(f)(storage_.error));
  }

  template <typename F>
  auto or_else(F&& f) const& {
    using R = std::remove_cvref_t<std::invoke_result_t<F, const E&>>;
    if (has_value_)
      return R(storage_.value);
    return R(std::forward<F>(f)(storage_.error));
  }

  template <typename F>
  auto or_else(F&& f) && {
    using R = std::remove_cvref_t<std::invoke_result_t<F, E&&>>;
    if (has_value_)
      return R(std::move(storage_.value));
    return R(std::forward<F>(f)(std::move(storage_.error)));
  }

  template <typename F>
  auto transform(F&& f) & {
    return TransformImpl<std::invoke_result_t<F, T&>>(std::forward<F>(f), storage_.value,
                                                      has_value_, storage_.error);
  }

  template <typename F>
  auto transform(F&& f) const& {
    return TransformImpl<std::invoke_result_t<F, const T&>>(std::forward<F>(f), storage_.value,
                                                            has_value_, storage_.error);
  }

  template <typename F>
  auto transform(F&& f) && {
    return TransformImpl<std::invoke_result_t<F, T&&>>(std::forward<F>(f),
                                                       std::move(storage_.value), has_value_,
                                                       std::move(storage_.error));
  }

  template <typename F>
  auto transform_error(F&& f) & {
    using G = std::remove_cv_t<std::invoke_result_t<F, E&>>;
    if (has_value_)
      return Expected<T, G>(storage_.value);
    return Expected<T, G>(Unexpected<G>(std::forward<F>(f)(storage_.error)));
  }

  template <typename F>
  auto transform_error(F&& f) const& {
    using G = std::remove_cv_t<std::invoke_result_t<F, const E&>>;
    if (has_value_)
      return Expected<T, G>(storage_.value);
    return Expected<T, G>(Unexpected<G>(std::forward<F>(f)(storage_.error)));
  }

  template <typename F>
  auto transform_error(F&& f) && {
    using G = std::remove_cv_t<std::invoke_result_t<F, E&&>>;
    if (has_value_)
      return Expected<T, G>(std::move(storage_.value));
    return Expected<T, G>(Unexpected<G>(std::forward<F>(f)(std::move(storage_.error))));
  }

 private:
  template <typename U, typename F, typename V, typename W>
  static auto TransformImpl(F&& f, V&& value, bool has_value, W&& error) {
    using Mapped = std::remove_cv_t<U>;
    if constexpr (std::is_void_v<Mapped>) {
      if (has_value) {
        std::forward<F>(f)(std::forward<V>(value));
        return Expected<void, E>();
      }
      return Expected<void, E>(Unexpected<E>(std::forward<W>(error)));
    } else {
      if (has_value)
        return Expected<Mapped, E>(std::forward<F>(f)(std::forward<V>(value)));
      return Expected<Mapped, E>(Unexpected<E>(std::forward<W>(error)));
    }
  }

  union Storage {
    Storage() {}
    ~Storage() {}
    T value;
    E error;
  };

  Storage storage_;
  bool has_value_;
};

// Specialization for operations that produce no value (mirrors std::expected<void, E>).
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4324)  // Padding after the alignas storage is intended.
#endif
template <typename E>
class Expected<void, E> {
 public:
  Expected() : has_value_(true) {}

  template <typename G>
  Expected(Unexpected<G> unexpected) : has_value_(false) {
    new (&error_) E(std::move(unexpected).error());
  }

  bool has_value() const { return has_value_; }
  explicit operator bool() const { return has_value_; }

  void value() const { assert(has_value_); }

  E& error() & { assert(!has_value_); return *error_ptr(); }
  const E& error() const& { assert(!has_value_); return *error_ptr(); }
  E&& error() && { assert(!has_value_); return std::move(*error_ptr()); }

  Expected(const Expected& other) : has_value_(other.has_value_) {
    if (!has_value_)
      new (&error_) E(*other.error_ptr());
  }

  Expected(Expected&& other) noexcept : has_value_(other.has_value_) {
    if (!has_value_)
      new (&error_) E(std::move(*other.error_ptr()));
  }

  Expected& operator=(Expected other) noexcept {
    this->~Expected();
    new (this) Expected(std::move(other));
    return *this;
  }

  ~Expected() {
    if (!has_value_)
      error_ptr()->~E();
  }

  template <typename G>
  E error_or(G&& fallback) const& {
    return has_value_ ? static_cast<E>(std::forward<G>(fallback)) : *error_ptr();
  }

  template <typename G>
  E error_or(G&& fallback) && {
    return has_value_ ? static_cast<E>(std::forward<G>(fallback)) : std::move(*error_ptr());
  }

  // The monadic operations (see the value-carrying specialization); the callables given to
  // and_then and transform take no argument.
  template <typename F>
  auto and_then(F&& f) const& {
    using R = std::remove_cvref_t<std::invoke_result_t<F>>;
    if (has_value_)
      return R(std::forward<F>(f)());
    return R(Unexpected<E>(*error_ptr()));
  }

  template <typename F>
  auto and_then(F&& f) && {
    using R = std::remove_cvref_t<std::invoke_result_t<F>>;
    if (has_value_)
      return R(std::forward<F>(f)());
    return R(Unexpected<E>(std::move(*error_ptr())));
  }

  template <typename F>
  auto or_else(F&& f) const& {
    using R = std::remove_cvref_t<std::invoke_result_t<F, const E&>>;
    if (has_value_)
      return R();
    return R(std::forward<F>(f)(*error_ptr()));
  }

  template <typename F>
  auto or_else(F&& f) && {
    using R = std::remove_cvref_t<std::invoke_result_t<F, E&&>>;
    if (has_value_)
      return R();
    return R(std::forward<F>(f)(std::move(*error_ptr())));
  }

  template <typename F>
  auto transform(F&& f) const& {
    using Mapped = std::remove_cv_t<std::invoke_result_t<F>>;
    if constexpr (std::is_void_v<Mapped>) {
      if (has_value_) {
        std::forward<F>(f)();
        return Expected<void, E>();
      }
      return Expected<void, E>(Unexpected<E>(*error_ptr()));
    } else {
      if (has_value_)
        return Expected<Mapped, E>(std::forward<F>(f)());
      return Expected<Mapped, E>(Unexpected<E>(*error_ptr()));
    }
  }

  template <typename F>
  auto transform(F&& f) && {
    return static_cast<const Expected&>(*this).transform(std::forward<F>(f));
  }

  template <typename F>
  auto transform_error(F&& f) const& {
    using G = std::remove_cv_t<std::invoke_result_t<F, const E&>>;
    if (has_value_)
      return Expected<void, G>();
    return Expected<void, G>(Unexpected<G>(std::forward<F>(f)(*error_ptr())));
  }

  template <typename F>
  auto transform_error(F&& f) && {
    using G = std::remove_cv_t<std::invoke_result_t<F, E&&>>;
    if (has_value_)
      return Expected<void, G>();
    return Expected<void, G>(Unexpected<G>(std::forward<F>(f)(std::move(*error_ptr()))));
  }

 private:
  E* error_ptr() { return reinterpret_cast<E*>(&error_); }
  const E* error_ptr() const { return reinterpret_cast<const E*>(&error_); }

  bool has_value_;
  alignas(E) unsigned char error_[sizeof(E)];
};
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

}  // namespace detail
}  // namespace ultralight
/// \endcond
