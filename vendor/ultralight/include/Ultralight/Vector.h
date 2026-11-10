///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once

#include <Ultralight/Defines.h>
#include <Ultralight/platform/Allocator.h>
#include <cassert>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <new>
#include <type_traits>
#include <utility>

namespace ultralight {

/// @cond INTERNAL
namespace detail {

template<typename T, size_t N>
struct VectorStorage {
  T* data_;
  size_t size_ = 0;
  size_t capacity_ = N;
  alignas(T) unsigned char inline_storage_[sizeof(T) * N];

  bool is_inline() const { return data_ == reinterpret_cast<const T*>(inline_storage_); }
  T* inline_buffer() { return reinterpret_cast<T*>(inline_storage_); }
  const T* inline_buffer() const { return reinterpret_cast<const T*>(inline_storage_); }

  VectorStorage() : data_(reinterpret_cast<T*>(inline_storage_)) {}
};

template<typename T>
struct VectorStorage<T, 0> {
  T* data_ = nullptr;
  size_t size_ = 0;
  size_t capacity_ = 0;

  bool is_inline() const { return false; }
  T* inline_buffer() { return nullptr; }
  const T* inline_buffer() const { return nullptr; }
};

} // namespace detail
/// @endcond

///
/// @brief  A std::vector-like container with small buffer optimization and ABI-safe allocation.
///
/// This container can be used safely in public API boundaries where `std::vector` cannot, because
/// STL types are not guaranteed to be ABI-compatible across different compilers, standard library
/// implementations, or even different build configurations.
///
/// All heap allocation is routed through UltralightCore's exported allocator functions
/// (`ul_malloc` / `ul_free`), ensuring that memory allocated by the library is always freed by
/// the library -- regardless of which runtime the caller was built with.
///
/// ## Small Buffer Optimization (SBO)
///
/// When `InlineCapacity > 0` (the default), the first N elements are stored inline within the
/// Vector object itself, avoiding heap allocation for common small lists. Once the size exceeds
/// `InlineCapacity`, the Vector transparently switches to heap storage.
///
/// ## Type support
///
/// Works with both trivially-copyable types (uses `memcpy`/`realloc` fast paths) and non-trivial
/// types like `RefPtr<T>` (proper construction, destruction, and move semantics).
///
/// ## Example
///
/// ```
/// Vector<int> v = {1, 2, 3};       // stored inline (default InlineCapacity = 4)
/// v.push_back(4);                   // still inline
/// v.push_back(5);                   // transitions to heap
///
/// Vector<int, 0> heap_only;         // pure heap, no inline storage
/// Vector<int, 16> large_inline;     // 16 elements inline before heap
/// ```
///
/// @tparam T              Element type.
/// @tparam InlineCapacity Number of elements stored inline before heap allocation (default 4).
///                        Use 0 for pure heap storage with no inline overhead.
///
template<typename T, size_t InlineCapacity = 4>
class Vector : private detail::VectorStorage<T, InlineCapacity> {
  using Storage = detail::VectorStorage<T, InlineCapacity>;
  using Storage::data_;
  using Storage::size_;
  using Storage::capacity_;

  template<typename U, size_t M>
  friend class Vector;

public:
  using value_type = T;
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using reference = T&;
  using const_reference = const T&;
  using pointer = T*;
  using const_pointer = const T*;
  using iterator = T*;
  using const_iterator = const T*;

  // ===== Constructors =====

  ///
  /// Construct an empty vector. No heap allocation is performed.
  ///
  Vector() = default;

  ///
  /// Construct a vector with @p count value-initialized elements.
  ///
  explicit Vector(size_t count) {
    if (count > 0) {
      ensure_capacity(count);
      default_construct_range(data_, count);
      size_ = count;
    }
  }

  ///
  /// Construct a vector with @p count copies of @p value.
  ///
  Vector(size_t count, const T& value) {
    if (count > 0) {
      ensure_capacity(count);
      fill_construct_range(data_, count, value);
      size_ = count;
    }
  }

  Vector(std::initializer_list<T> init) {
    if (init.size() > 0) {
      ensure_capacity(init.size());
      copy_construct_range(data_, init.begin(), init.size());
      size_ = init.size();
    }
  }

  template<typename InputIt,
           typename = typename std::enable_if<!std::is_integral<InputIt>::value>::type>
  Vector(InputIt first, InputIt last) {
    size_t count = static_cast<size_t>(std::distance(first, last));
    if (count > 0) {
      ensure_capacity(count);
      copy_construct_from_iter(data_, first, count);
      size_ = count;
    }
  }

  Vector(const Vector& other) {
    if (other.size_ > 0) {
      ensure_capacity(other.size_);
      copy_construct_range(data_, other.data_, other.size_);
      size_ = other.size_;
    }
  }

  template<size_t OtherInline>
  Vector(const Vector<T, OtherInline>& other) {
    if (other.size_ > 0) {
      ensure_capacity(other.size_);
      copy_construct_range(data_, other.data_, other.size_);
      size_ = other.size_;
    }
  }

  Vector(Vector&& other)
      noexcept(InlineCapacity == 0 || std::is_nothrow_move_constructible<T>::value) {
    move_from(std::move(other));
  }

  template<size_t OtherInline>
  Vector(Vector<T, OtherInline>&& other)
      noexcept(std::is_nothrow_move_constructible<T>::value) {
    if (other.size_ > 0) {
      ensure_capacity(other.size_);
      move_construct_range(data_, other.data_, other.size_);
      size_ = other.size_;
    }
    other.clear();
  }

  ~Vector() {
    destroy_range(data_, size_);
    free_heap();
  }

  // ===== Assignment =====

  Vector& operator=(const Vector& other) {
    if (this != &other)
      assign_from_copy(other.data_, other.size_);
    return *this;
  }

  template<size_t OtherInline>
  Vector& operator=(const Vector<T, OtherInline>& other) {
    assign_from_copy(other.data_, other.size_);
    return *this;
  }

  Vector& operator=(Vector&& other)
      noexcept(InlineCapacity == 0 || std::is_nothrow_move_constructible<T>::value) {
    if (this != &other) {
      destroy_range(data_, size_);
      free_heap();
      reset_to_inline();
      move_from(std::move(other));
    }
    return *this;
  }

  template<size_t OtherInline>
  Vector& operator=(Vector<T, OtherInline>&& other)
      noexcept(std::is_nothrow_move_constructible<T>::value) {
    destroy_range(data_, size_);
    size_ = 0;
    if (other.size_ > 0) {
      ensure_capacity(other.size_);
      move_construct_range(data_, other.data_, other.size_);
      size_ = other.size_;
    }
    other.clear();
    return *this;
  }

  Vector& operator=(std::initializer_list<T> init) {
    assign(init);
    return *this;
  }

  void assign(size_t count, const T& value) {
    destroy_range(data_, size_);
    size_ = 0;
    ensure_capacity(count);
    fill_construct_range(data_, count, value);
    size_ = count;
  }

  template<typename InputIt,
           typename = typename std::enable_if<!std::is_integral<InputIt>::value>::type>
  void assign(InputIt first, InputIt last) {
    size_t count = static_cast<size_t>(std::distance(first, last));
    destroy_range(data_, size_);
    size_ = 0;
    ensure_capacity(count);
    copy_construct_from_iter(data_, first, count);
    size_ = count;
  }

  void assign(std::initializer_list<T> init) {
    destroy_range(data_, size_);
    size_ = 0;
    ensure_capacity(init.size());
    copy_construct_range(data_, init.begin(), init.size());
    size_ = init.size();
  }

  // ===== Element access =====

  ///
  /// Access element by index. No bounds checking.
  ///
  T& operator[](size_t index) { return data_[index]; }
  const T& operator[](size_t index) const { return data_[index]; }

  ///
  /// Access element by index with bounds checking (asserts in debug builds).
  ///
  T& at(size_t index) { assert(index < size_); return data_[index]; }
  const T& at(size_t index) const { assert(index < size_); return data_[index]; }

  T& front() { assert(size_ > 0); return data_[0]; }
  const T& front() const { assert(size_ > 0); return data_[0]; }

  T& back() { assert(size_ > 0); return data_[size_ - 1]; }
  const T& back() const { assert(size_ > 0); return data_[size_ - 1]; }

  T* data() { return data_; }
  const T* data() const { return data_; }

  // ===== Iterators =====

  iterator begin() { return data_; }
  const_iterator begin() const { return data_; }
  const_iterator cbegin() const { return data_; }

  iterator end() { return data_ + size_; }
  const_iterator end() const { return data_ + size_; }
  const_iterator cend() const { return data_ + size_; }

  // ===== Capacity =====

  bool empty() const { return size_ == 0; }
  size_t size() const { return size_; }

  ///
  /// Returns the number of elements that can be held without allocating. For a default-constructed
  /// vector with `InlineCapacity > 0`, this equals `InlineCapacity` (inline storage is always
  /// available without heap allocation).
  ///
  size_t capacity() const { return capacity_; }

  ///
  /// Ensure capacity for at least @p min_capacity elements, allocating if necessary.
  ///
  void reserve(size_t min_capacity) {
    if (min_capacity > capacity_)
      grow_to(min_capacity);
  }

  ///
  /// Reduce capacity to fit the current size. If `InlineCapacity > 0` and the current size fits
  /// within the inline buffer, storage is moved back to inline (freeing the heap allocation).
  ///
  void shrink_to_fit() {
    if (size_ == capacity_)
      return;

    if (size_ == 0) {
      free_heap();
      reset_to_inline();
      return;
    }

    if constexpr (InlineCapacity > 0) {
      if (size_ <= InlineCapacity && !this->is_inline()) {
        T* old_data = data_;
        size_t old_size = size_;
        data_ = this->inline_buffer();
        capacity_ = InlineCapacity;
        move_construct_range(data_, old_data, old_size);
        destroy_range(old_data, old_size);
        deallocate(old_data);
        return;
      }
    }

    if (!this->is_inline()) {
      if constexpr (std::is_trivially_copyable_v<T>) {
        void* p = reallocate(data_, size_ * sizeof(T));
        data_ = static_cast<T*>(p);
        capacity_ = size_;
      } else {
        T* new_data = static_cast<T*>(allocate(size_ * sizeof(T)));
        move_construct_range(new_data, data_, size_);
        destroy_range(data_, size_);
        deallocate(data_);
        data_ = new_data;
        capacity_ = size_;
      }
    }
  }

  // ===== Modifiers =====

  void clear() {
    destroy_range(data_, size_);
    size_ = 0;
  }

  void push_back(const T& value) {
    if (UL_LIKELY(size_ < capacity_)) {
      construct_at(data_ + size_, value);
      ++size_;
      return;
    }
    // Value may reference internal storage -- copy before grow
    T tmp(value);
    grow();
    construct_at(data_ + size_, std::move(tmp));
    ++size_;
  }

  void push_back(T&& value) {
    if (UL_LIKELY(size_ < capacity_)) {
      construct_at(data_ + size_, std::move(value));
      ++size_;
      return;
    }
    T tmp(std::move(value));
    grow();
    construct_at(data_ + size_, std::move(tmp));
    ++size_;
  }

  ///
  /// Construct an element in-place at the end of the vector.
  ///
  template<typename... Args>
  T& emplace_back(Args&&... args) {
    if (UL_LIKELY(size_ < capacity_)) {
      construct_at(data_ + size_, std::forward<Args>(args)...);
      return data_[size_++];
    }
    return emplace_back_slow(std::forward<Args>(args)...);
  }

  ///
  /// Append an element without checking capacity. Caller must ensure `size() < capacity()`.
  ///
  void unchecked_append(const T& value) {
    assert(size_ < capacity_);
    construct_at(data_ + size_, value);
    ++size_;
  }

  ///
  /// Construct an element in-place at the end without checking capacity.
  /// Caller must ensure `size() < capacity()`.
  ///
  template<typename... Args>
  T& unchecked_emplace_back(Args&&... args) {
    assert(size_ < capacity_);
    construct_at(data_ + size_, std::forward<Args>(args)...);
    return data_[size_++];
  }

  void pop_back() {
    assert(size_ > 0);
    --size_;
    destroy_at(data_ + size_);
  }

  ///
  /// Remove the last element and return it by move.
  ///
  T take_last() {
    assert(size_ > 0);
    --size_;
    T result(std::move(data_[size_]));
    destroy_at(data_ + size_);
    return result;
  }

  iterator insert(const_iterator pos, const T& value) {
    size_t index = static_cast<size_t>(pos - data_);
    assert(index <= size_);
    if (UL_UNLIKELY(size_ == capacity_))
      return insert_slow_path(index, value);
    T tmp(value);
    insert_no_grow(index, std::move(tmp));
    return data_ + index;
  }

  iterator insert(const_iterator pos, T&& value) {
    size_t index = static_cast<size_t>(pos - data_);
    assert(index <= size_);
    if (UL_UNLIKELY(size_ == capacity_))
      return insert_slow_path(index, std::move(value));
    T tmp(std::move(value));
    insert_no_grow(index, std::move(tmp));
    return data_ + index;
  }

  template<typename... Args>
  iterator emplace(const_iterator pos, Args&&... args) {
    size_t index = static_cast<size_t>(pos - data_);
    assert(index <= size_);
    if (UL_UNLIKELY(size_ == capacity_))
      return insert_slow_path(index, std::forward<Args>(args)...);
    if constexpr (std::is_trivially_copyable_v<T>) {
      std::memmove(data_ + index + 1, data_ + index, (size_ - index) * sizeof(T));
      construct_at(data_ + index, std::forward<Args>(args)...);
    } else if (index == size_) {
      construct_at(data_ + index, std::forward<Args>(args)...);
    } else {
      construct_at(data_ + size_, std::move(data_[size_ - 1]));
      for (size_t i = size_ - 1; i > index; --i)
        data_[i] = std::move(data_[i - 1]);
      destroy_at(data_ + index);
      construct_at(data_ + index, std::forward<Args>(args)...);
    }
    ++size_;
    return data_ + index;
  }

  iterator erase(const_iterator pos) {
    size_t index = static_cast<size_t>(pos - data_);
    assert(index < size_);
    if constexpr (std::is_trivially_copyable_v<T>) {
      std::memmove(data_ + index, data_ + index + 1, (size_ - index - 1) * sizeof(T));
    } else {
      for (size_t i = index; i < size_ - 1; ++i)
        data_[i] = std::move(data_[i + 1]);
      destroy_at(data_ + size_ - 1);
    }
    --size_;
    return data_ + index;
  }

  iterator erase(const_iterator first, const_iterator last) {
    if (first == last)
      return const_cast<iterator>(first);
    size_t start = static_cast<size_t>(first - data_);
    size_t count = static_cast<size_t>(last - first);
    assert(start + count <= size_);
    size_t remaining = size_ - start - count;
    if constexpr (std::is_trivially_copyable_v<T>) {
      std::memmove(data_ + start, data_ + start + count, remaining * sizeof(T));
    } else {
      for (size_t i = 0; i < remaining; ++i)
        data_[start + i] = std::move(data_[start + count + i]);
      destroy_range(data_ + size_ - count, count);
    }
    size_ -= count;
    return data_ + start;
  }

  void resize(size_t count) {
    if (count < size_) {
      destroy_range(data_ + count, size_ - count);
      size_ = count;
    } else if (count > size_) {
      ensure_capacity(count);
      default_construct_range(data_ + size_, count - size_);
      size_ = count;
    }
  }

  void resize(size_t count, const T& value) {
    if (count < size_) {
      destroy_range(data_ + count, size_ - count);
      size_ = count;
    } else if (count > size_) {
      ensure_capacity(count);
      fill_construct_range(data_ + size_, count - size_, value);
      size_ = count;
    }
  }

  void swap(Vector& other) {
    if constexpr (InlineCapacity == 0) {
      std::swap(data_, other.data_);
      std::swap(size_, other.size_);
      std::swap(capacity_, other.capacity_);
    } else {
      bool a_inline = this->is_inline();
      bool b_inline = other.is_inline();

      if (!a_inline && !b_inline) {
        std::swap(data_, other.data_);
        std::swap(size_, other.size_);
        std::swap(capacity_, other.capacity_);
      } else if (a_inline && b_inline) {
        swap_both_inline(other);
      } else if (a_inline) {
        swap_inline_with_heap(other);
      } else {
        other.swap_inline_with_heap(*this);
      }
    }
  }

  // ===== Bulk operations =====

  ///
  /// Append all elements from another vector. Works across different InlineCapacity values.
  ///
  template<size_t OtherInline>
  void append_vector(const Vector<T, OtherInline>& other) {
    if (other.size_ == 0)
      return;
    ensure_capacity(size_ + other.size_);
    copy_construct_range(data_ + size_, other.data_, other.size_);
    size_ += other.size_;
  }

  ///
  /// Move-append all elements from another vector, leaving it empty.
  ///
  template<size_t OtherInline>
  void append_vector(Vector<T, OtherInline>&& other) {
    if (other.size_ == 0)
      return;
    ensure_capacity(size_ + other.size_);
    move_construct_range(data_ + size_, other.data_, other.size_);
    size_ += other.size_;
    other.clear();
  }

  // ===== Comparison =====

  friend bool operator==(const Vector& a, const Vector& b) {
    if (a.size_ != b.size_)
      return false;
    if constexpr (std::is_trivially_copyable_v<T>) {
      return std::memcmp(a.data_, b.data_, a.size_ * sizeof(T)) == 0;
    } else {
      for (size_t i = 0; i < a.size_; ++i) {
        if (!(a.data_[i] == b.data_[i]))
          return false;
      }
      return true;
    }
  }

  friend bool operator!=(const Vector& a, const Vector& b) { return !(a == b); }

private:
  // ===== Allocation helpers =====

  static void* allocate(size_t bytes) {
    void* p;
    if constexpr (alignof(T) > alignof(std::max_align_t))
      p = ul_aligned_malloc(bytes, alignof(T));
    else
      p = ul_malloc(bytes);
    assert(p && "Vector: allocation failed");
    return p;
  }

  static void deallocate(void* ptr) {
    if constexpr (alignof(T) > alignof(std::max_align_t))
      ul_aligned_free(ptr);
    else
      ul_free(ptr);
  }

  static void* reallocate(void* ptr, size_t bytes) {
    void* p;
    if constexpr (alignof(T) > alignof(std::max_align_t))
      p = ul_aligned_realloc(ptr, bytes, alignof(T));
    else
      p = ul_realloc(ptr, bytes);
    assert(p && "Vector: reallocation failed");
    return p;
  }

  // ===== Construction / destruction helpers =====

  template<typename... Args>
  static void construct_at(T* ptr, Args&&... args) {
    ::new (static_cast<void*>(ptr)) T(std::forward<Args>(args)...);
  }

  static void destroy_at(T* ptr) {
    if constexpr (!std::is_trivially_destructible_v<T>)
      ptr->~T();
  }

  static void destroy_range(T* ptr, size_t count) {
    if constexpr (!std::is_trivially_destructible_v<T>) {
      for (size_t i = 0; i < count; ++i)
        ptr[i].~T();
    }
  }

  static void copy_construct_range(T* dst, const T* src, size_t count) {
    if constexpr (std::is_trivially_copyable_v<T>) {
      std::memcpy(dst, src, count * sizeof(T));
    } else {
      for (size_t i = 0; i < count; ++i)
        ::new (static_cast<void*>(dst + i)) T(src[i]);
    }
  }

  static void move_construct_range(T* dst, T* src, size_t count) {
    if constexpr (std::is_trivially_copyable_v<T>) {
      std::memcpy(dst, src, count * sizeof(T));
    } else {
      for (size_t i = 0; i < count; ++i)
        ::new (static_cast<void*>(dst + i)) T(std::move(src[i]));
    }
  }

  static void fill_construct_range(T* dst, size_t count, const T& value) {
    for (size_t i = 0; i < count; ++i)
      ::new (static_cast<void*>(dst + i)) T(value);
  }

  static void default_construct_range(T* dst, size_t count) {
    if constexpr (std::is_trivially_default_constructible_v<T>) {
      std::memset(dst, 0, count * sizeof(T));
    } else {
      for (size_t i = 0; i < count; ++i)
        ::new (static_cast<void*>(dst + i)) T();
    }
  }

  template<typename InputIt>
  static void copy_construct_from_iter(T* dst, InputIt first, size_t count) {
    for (size_t i = 0; i < count; ++i, ++first)
      ::new (static_cast<void*>(dst + i)) T(*first);
  }

  /// Move-construct elements to a new non-overlapping buffer and destroy originals.
  static void relocate_range(T* dst, T* src, size_t count) {
    if (count == 0)
      return;
    if constexpr (std::is_trivially_copyable_v<T>) {
      std::memcpy(dst, src, count * sizeof(T));
    } else {
      for (size_t i = 0; i < count; ++i) {
        ::new (static_cast<void*>(dst + i)) T(std::move(src[i]));
        src[i].~T();
      }
    }
  }

  // ===== Capacity helpers =====

  size_t next_capacity() const {
    size_t new_cap = capacity_ + capacity_ / 2 + 1;
    assert(new_cap > capacity_ && "Vector: capacity overflow");
    return new_cap;
  }

  void grow() { grow_to(next_capacity()); }

  void grow_to(size_t new_cap) {
    assert(new_cap > capacity_);
    assert(new_cap <= SIZE_MAX / sizeof(T) && "Vector: allocation size overflow");

    if constexpr (std::is_trivially_copyable_v<T>) {
      if (!this->is_inline()) {
        data_ = static_cast<T*>(reallocate(data_, new_cap * sizeof(T)));
      } else {
        T* new_data = static_cast<T*>(allocate(new_cap * sizeof(T)));
        std::memcpy(new_data, data_, size_ * sizeof(T));
        data_ = new_data;
      }
    } else {
      T* new_data = static_cast<T*>(allocate(new_cap * sizeof(T)));
      for (size_t i = 0; i < size_; ++i) {
        ::new (static_cast<void*>(new_data + i)) T(std::move(data_[i]));
        data_[i].~T();
      }
      if (!this->is_inline())
        deallocate(data_);
      data_ = new_data;
    }
    capacity_ = new_cap;
  }

  void ensure_capacity(size_t min_cap) {
    if (min_cap > capacity_)
      grow_to(min_cap);
  }

  // ===== Buffer management =====

  void free_heap() {
    if (!this->is_inline() && data_)
      deallocate(data_);
  }

  void reset_to_inline() {
    if constexpr (InlineCapacity > 0) {
      data_ = this->inline_buffer();
      capacity_ = InlineCapacity;
    } else {
      data_ = nullptr;
      capacity_ = 0;
    }
    size_ = 0;
  }

  void move_from(Vector&& other) {
    if constexpr (InlineCapacity == 0) {
      data_ = other.data_;
      size_ = other.size_;
      capacity_ = other.capacity_;
      other.data_ = nullptr;
      other.size_ = 0;
      other.capacity_ = 0;
    } else {
      if (other.is_inline()) {
        if constexpr (std::is_trivially_copyable_v<T>) {
          std::memcpy(data_, other.data_, other.size_ * sizeof(T));
        } else {
          move_construct_range(data_, other.data_, other.size_);
          destroy_range(other.data_, other.size_);
        }
        size_ = other.size_;
        other.size_ = 0;
      } else {
        // Steal heap buffer
        data_ = other.data_;
        size_ = other.size_;
        capacity_ = other.capacity_;
        other.data_ = other.inline_buffer();
        other.size_ = 0;
        other.capacity_ = InlineCapacity;
      }
    }
  }

  void assign_from_copy(const T* src, size_t count) {
    destroy_range(data_, size_);
    size_ = 0;
    ensure_capacity(count);
    copy_construct_range(data_, src, count);
    size_ = count;
  }

  // ===== Insert / erase helpers =====

  void insert_no_grow(size_t index, T&& val) {
    if constexpr (std::is_trivially_copyable_v<T>) {
      std::memmove(data_ + index + 1, data_ + index, (size_ - index) * sizeof(T));
      std::memcpy(data_ + index, &val, sizeof(T));
    } else if (index == size_) {
      construct_at(data_ + index, std::move(val));
    } else {
      construct_at(data_ + size_, std::move(data_[size_ - 1]));
      for (size_t i = size_ - 1; i > index; --i)
        data_[i] = std::move(data_[i - 1]);
      data_[index] = std::move(val);
    }
    ++size_;
  }

  /// Grow + insert: allocate new buffer, construct element at index, relocate rest around it.
  /// Safe when args reference internal storage (old buffer is valid during construct).
  template<typename... Args>
  iterator insert_slow_path(size_t index, Args&&... args) {
    size_t new_cap = next_capacity();
    T* new_data = static_cast<T*>(allocate(new_cap * sizeof(T)));
    // Construct new element first while old buffer is still valid
    construct_at(new_data + index, std::forward<Args>(args)...);
    // Relocate existing elements around it
    relocate_range(new_data, data_, index);
    relocate_range(new_data + index + 1, data_ + index, size_ - index);
    if (!this->is_inline())
      deallocate(data_);
    data_ = new_data;
    capacity_ = new_cap;
    ++size_;
    return data_ + index;
  }

  template<typename... Args>
  T& emplace_back_slow(Args&&... args) {
    size_t new_cap = next_capacity();
    T* new_data = static_cast<T*>(allocate(new_cap * sizeof(T)));
    construct_at(new_data + size_, std::forward<Args>(args)...);
    relocate_range(new_data, data_, size_);
    if (!this->is_inline())
      deallocate(data_);
    data_ = new_data;
    capacity_ = new_cap;
    return data_[size_++];
  }

  // ===== Swap helpers =====

  void swap_both_inline(Vector& other) {
    size_t a_size = size_;
    size_t b_size = other.size_;

    if constexpr (std::is_trivially_copyable_v<T>) {
      alignas(T) unsigned char tmp[sizeof(T) * InlineCapacity];
      std::memcpy(tmp, data_, a_size * sizeof(T));
      std::memcpy(data_, other.data_, b_size * sizeof(T));
      std::memcpy(other.data_, tmp, a_size * sizeof(T));
    } else {
      size_t min_size = a_size < b_size ? a_size : b_size;
      for (size_t i = 0; i < min_size; ++i) {
        using std::swap;
        swap(data_[i], other.data_[i]);
      }
      if (a_size > b_size) {
        move_construct_range(other.data_ + min_size, data_ + min_size, a_size - b_size);
        destroy_range(data_ + min_size, a_size - b_size);
      } else if (b_size > a_size) {
        move_construct_range(data_ + min_size, other.data_ + min_size, b_size - a_size);
        destroy_range(other.data_ + min_size, b_size - a_size);
      }
    }

    size_ = b_size;
    other.size_ = a_size;
  }

  /// this is inline, other is heap. Swap storage.
  void swap_inline_with_heap(Vector& other) {
    T* heap_data = other.data_;
    size_t heap_size = other.size_;
    size_t heap_cap = other.capacity_;

    // Move our inline elements to other's inline buffer
    other.data_ = other.inline_buffer();
    other.capacity_ = InlineCapacity;
    other.size_ = 0;

    if constexpr (std::is_trivially_copyable_v<T>) {
      std::memcpy(other.data_, data_, size_ * sizeof(T));
    } else {
      move_construct_range(other.data_, data_, size_);
      destroy_range(data_, size_);
    }
    other.size_ = size_;

    // Steal the heap buffer
    data_ = heap_data;
    size_ = heap_size;
    capacity_ = heap_cap;
  }
};

template<typename T, size_t N>
void swap(Vector<T, N>& a, Vector<T, N>& b) { a.swap(b); }

} // namespace ultralight
