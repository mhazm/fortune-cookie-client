///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/*
* Portions of the below code are derived from 'RefPtr.h' and
* 'ThreadSafeWeakPtr.h' from Apple's WTF, with the following license header:
*
* Copyright (C) 2013-2024 Apple Inc. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
* 1. Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
*
* THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
* THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
* PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
* THE POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once
#include <Ultralight/Defines.h>
#include <Ultralight/Lock.h>
#include <Ultralight/platform/Allocator.h>
#include <assert.h>
#include <type_traits>
#include <utility>

namespace ultralight {

class WeakControlBlock;

///
/// @brief  Interface for all ref-counted objects that will be managed using
///         the RefPtr<> smart pointer.
///
class UExport RefCounted {
 public:
  /// Increment the reference count (thread-safe).
  virtual void AddRef() const = 0;

  /// Decrement the reference count (thread-safe). The object is destroyed when the count reaches
  /// zero.
  virtual void Release() const = 0;

  /// Get the current reference count.
  virtual int ref_count() const = 0;

  ///
  /// Get the control block used to track weak references to this object.
  ///
  /// @return  Returns the control block, or nullptr if the concrete type does not support weak
  ///          references (only types derived from WeakRefCounted do).
  ///
  /// @note  This is internal bookkeeping for WeakPtr; you should never need to call it directly.
  ///
  virtual WeakControlBlock* weak_control_block() const { return nullptr; }

 protected:
  virtual ~RefCounted();
};

///
/// @brief  Interface for ref-counted objects that can additionally be observed via the
///         WeakPtr<> smart pointer.
///
/// Types deriving this interface support weak references: a WeakPtr can observe the object's
/// lifetime without owning it. WeakPtr<T> requires T to derive WeakRefCounted; using it with a
/// plain RefCounted type is a compile error.
///
class UExport WeakRefCounted : public RefCounted {
 public:
  /// Get the control block used to track weak references to this object (never nullptr).
  virtual WeakControlBlock* weak_control_block() const override = 0;

 protected:
  virtual ~WeakRefCounted() = default;
};

template<typename T> class RefPtr;

///
/// @brief  Helper for wrapping new objects with the RefPtr smart pointer.
///
/// All ref-counted object are created with an initial ref-count of '1'.
/// The AdoptRef() helper returns a RefPtr<T> without calling AddRef().
/// This is used for creating new objects, like so:
///
///   RefPtr<Object> ref = AdoptRef(*new ObjectImpl());
///
template<typename T>
RefPtr<T> AdoptRef(T& reference)
{
  return RefPtr<T>(reference, RefPtr<T>::Adopt);
}

///
/// @brief  Construct a new object of type T and wrap it in a RefPtr.
///
/// Equivalent to `AdoptRef(*new T(args...))`.
///
template<typename T, typename... Args>
RefPtr<T> MakeRefPtr(Args&&... args)
{
  return AdoptRef(*new T(std::forward<Args>(args)...));
}

///
/// @brief  A nullable smart pointer.
///
/// This smart pointer automatically manages the lifetime of a RefCounted
/// object. The managed instance may be NULL.
///
template<typename T> class RefPtr {
  // Cross-type friendship: lets `RefPtr<Base>(const RefPtr<Derived>&)`
  // and the matching move constructor read the other instantiation's
  // `instance_` pointer during upcasting conversions.
  template <typename U> friend class RefPtr;
 public:
  ///
  /// Construct a NULL ref-pointer.
  ///
  constexpr RefPtr()
    : instance_(nullptr)
  {
  }

  ///
  /// Construct a NULL ref-pointer.
  ///
  inline RefPtr(std::nullptr_t)
    : instance_(nullptr)
  {
  }

  ///
  /// Construct from a pointer. (Will increment ref-count by one)
  ///
  inline RefPtr(T* other)
    : instance_(other)
  {
    if (instance_)
      instance_->AddRef();
  }

  ///
  /// Copy constructor.
  ///
  inline RefPtr(const RefPtr& other) 
    : instance_(other.instance_) 
  {
    if (instance_)
      instance_->AddRef();
  }

  ///
  /// Copy constructor with internal type conversion.
  ///
  template<typename U>
  RefPtr(const RefPtr<U>& other) 
    : instance_(other.instance_) 
  {
    if (instance_)
      instance_->AddRef();
  }

  ///
  /// Move constructor.
  ///
  inline RefPtr(RefPtr&& other) 
    : instance_(other.LeakRef())
  {
  }

  ///
  /// Move constructor.
  ///
  template<typename U>
  RefPtr(RefPtr<U>&& other) 
    : instance_(other.LeakRef())
  {
  }

  ///
  /// Destroy RefPtr (will decrement ref-count by one)
  ///
  inline ~RefPtr()
  {
    T* old_value = std::move(instance_);
    instance_ = std::forward<T*>(nullptr);
    if (old_value)
      old_value->Release();
  }

  ///
  /// Get a pointer to wrapped object.
  ///
  inline T* get() const { return instance_; }

  /// Release ownership and return the raw pointer without decrementing the reference count.
  ///
  /// @warning  The caller is responsible for eventually calling Release() on the returned pointer
  ///           to avoid a memory leak, or passing it to AdoptRef().
  ///
  T* LeakRef() {
    T* result = std::move(instance_);
    instance_ = std::forward<T*>(nullptr);
    return result;
  }

  T& operator*() const { assert(instance_); return *instance_; }
  inline T* operator->() const { return instance_; }

  bool operator!() const { return !instance_; }

  ///
  /// Whether this RefPtr holds a non-null object.
  ///
  explicit operator bool() const { return instance_ != nullptr; }

  RefPtr& operator=(const RefPtr&);
  RefPtr& operator=(T*);
  RefPtr& operator=(std::nullptr_t);
  template<typename U> RefPtr& operator=(const RefPtr<U>&);
  RefPtr& operator=(RefPtr&&);
  template<typename U> RefPtr& operator=(RefPtr<U>&&);

  friend inline bool operator==(const RefPtr& a, const RefPtr& b) {
    return a.instance_ == b.instance_;
  }

  friend inline bool operator!=(const RefPtr& a, const RefPtr& b) {
    return a.instance_ != b.instance_;
  }

  friend inline bool operator<(const RefPtr& a, const RefPtr& b) {
    return a.instance_ < b.instance_;
  }

  ///
  /// Releases the ownership of the managed object, if any
  ///
  void reset();

  ///
  /// Replaces the managed object with another.
  ///
  void reset(T* obj);

  ///
  /// Exchanges the stored pointer values and the ownerships of *this and ptr.
  /// Reference counts, if any, are not adjusted.
  ///
  void swap(RefPtr& ptr);

protected:
  friend RefPtr AdoptRef<T>(T&);

  enum AdoptTag { Adopt };
  RefPtr(T& object, AdoptTag) : instance_(&object) { }

private:
  T* instance_;
};

template<typename T>
RefPtr<T>& RefPtr<T>::operator=(const RefPtr& other)
{
  RefPtr ptr = other;
  swap(ptr);
  return *this;
}

template<typename T>
template<typename U>
RefPtr<T>& RefPtr<T>::operator=(const RefPtr<U>& other)
{
  RefPtr ptr = other;
  swap(ptr);
  return *this;
}

template<typename T>
RefPtr<T>& RefPtr<T>::operator=(T* object)
{
  RefPtr ptr = object;
  swap(ptr);
  return *this;
}

template<typename T>
RefPtr<T>& RefPtr<T>::operator=(std::nullptr_t)
{
  T* old_instance = std::move(instance_);
  instance_ = std::forward<T*>(nullptr);
  if (old_instance)
    old_instance->Release();
  return *this;
}

template<typename T>
RefPtr<T>& RefPtr<T>::operator=(RefPtr&& other)
{
  RefPtr ptr = std::move(other);
  swap(ptr);
  return *this;
}

template<typename T>
template<typename U>
RefPtr<T>& RefPtr<T>::operator=(RefPtr<U>&& other)
{
  RefPtr ptr = std::move(other);
  swap(ptr);
  return *this;
}

template <typename T> void RefPtr<T>::reset() { *this = nullptr; }

template <typename T> void RefPtr<T>::reset(T* obj) { *this = obj; }

template<typename T>
void RefPtr<T>::swap(RefPtr& other)
{
  std::swap(instance_, other.instance_);
}

template<class T>
void swap(RefPtr<T>& a, RefPtr<T>& b)
{
  a.swap(b);
}

template <class T> class RefCountedImpl;

///
/// @brief  Internal bookkeeping object that tracks the liveness of a weakly-referenced object.
///
/// Allocated on demand the first time a weak reference to an object is created, and destroyed
/// when the last weak reference and the object itself are gone. You should never use this type
/// directly; use WeakPtr instead.
///
/// @note  The layout of this class is part of the library's binary interface and does not
///        change within a major version.
///
class WeakControlBlock {
 public:
  WeakControlBlock(const WeakControlBlock&) = delete;
  WeakControlBlock& operator=(const WeakControlBlock&) = delete;

  // Control blocks are created inside the library but may be released (and therefore freed)
  // from code inlined into the embedder's binary, so allocation must go through the library's
  // exported allocator pair regardless of which runtime the caller was built with.
  static void* operator new(size_t size) {
    void* p = ul_malloc(size);
    assert(p && "WeakControlBlock: allocation failed");
    return p;
  }
  static void operator delete(void* p) { ul_free(p); }

  /// Increment the weak reference count and return this control block.
  WeakControlBlock* WeakRef() {
    LockHolder locker(lock_);
    ++weak_ref_count_;
    return this;
  }

  /// Decrement the weak reference count. Destroys the control block when both counts are zero.
  void WeakDeref() {
    bool should_delete_control_block = false;
    {
      LockHolder locker(lock_);
      assert(weak_ref_count_);
      if (!--weak_ref_count_ && !strong_ref_count_)
        should_delete_control_block = true;
    }
    if (should_delete_control_block)
      delete this;
  }

  /// Increment the strong reference count. Only valid while the object is alive.
  void StrongRef() const {
    LockHolder locker(lock_);
    assert(object_);
    ++strong_ref_count_;
  }

  /// Decrement the strong reference count, destroying the object when it reaches zero.
  template <typename T>
  void StrongDeref() const {
    T* object;
    {
      LockHolder locker(lock_);
      assert(object_);
      if (UL_LIKELY(--strong_ref_count_ != 0))
        return;
      object = static_cast<T*>(object_);
      object_ = nullptr;
      // Take a temporary weak ref so this control block survives until the delete below
      // returns: the object's destructor may itself drop weak references to this block.
      ++weak_ref_count_;
    }

    // The object's destructor (arbitrary user code) must run outside the lock above.
    // Destruction routes through RefCountedImpl<T>: it is the only class the object's
    // (typically protected) destructor befriends.
    RefCountedImpl<T>::DeleteObject(object);

    bool has_other_weak_refs;
    {
      LockHolder locker(lock_);
      has_other_weak_refs = --weak_ref_count_ != 0;
    }
    if (!has_other_weak_refs)
      delete this;
  }

  ///
  /// Atomically take a strong reference if the object is still alive.
  ///
  /// @param  maybe_interior_pointer  The typed pointer held by the caller. This (rather than
  ///         the stored object pointer) is what gets returned so that interior pointers from
  ///         multiple inheritance resolve to the correct subobject.
  ///
  /// @return  Returns a strong reference to the object, or nullptr if it has been destroyed.
  ///
  template <typename U>
  RefPtr<U> MakeStrongRefIfPossible(const U* maybe_interior_pointer) const {
    LockHolder locker(lock_);
    if (object_) {
      // Constructing RefPtr<U>(ptr) would call AddRef -> StrongRef and self-deadlock on
      // lock_; bump the count directly and adopt.
      ++strong_ref_count_;
      return AdoptRef(*const_cast<U*>(maybe_interior_pointer));
    }
    return nullptr;
  }

  ///
  /// Whether the object has been destroyed.
  ///
  /// @warning  Racy hint: the object may be destroyed by another thread immediately after this
  ///           returns false. Never gate a dereference on it; use MakeStrongRefIfPossible().
  ///
  bool expired() const {
    LockHolder locker(lock_);
    return !object_;
  }

  /// Get the current strong reference count.
  int ref_count() const {
    LockHolder locker(lock_);
    return static_cast<int>(strong_ref_count_);
  }

  /// Get the current weak reference count.
  int weak_ref_count() const {
    LockHolder locker(lock_);
    return static_cast<int>(weak_ref_count_);
  }

 private:
  template <class T> friend class RefCountedImpl;

  explicit WeakControlBlock(void* object) : object_(object) { }

  // Called by the creating thread before the control block is published; no lock needed.
  void set_strong_ref_count_during_init(uint32_t count) { strong_ref_count_ = count; }

  mutable Lock lock_;
  mutable uint32_t strong_ref_count_ = 1;
  mutable uint32_t weak_ref_count_ = 0;
  mutable void* object_;
};

///
/// @brief  A weak (non-owning) smart pointer that observes a WeakRefCounted object.
///
/// A WeakPtr does not keep its object alive. Use Lock() to atomically obtain a strong
/// RefPtr to the object; the result is null if the object has already been destroyed.
///
/// ## Usage
///
/// ```
/// RefPtr<Bitmap> bitmap = Bitmap::Create(100, 100, BitmapFormat::BGRA8_UNORM_SRGB);
/// WeakPtr<Bitmap> weak(bitmap);
///
/// if (RefPtr<Bitmap> strong = weak.Lock()) {
///   // The object is alive and stays alive while 'strong' is in scope.
/// }
/// ```
///
/// Thread-safe: WeakPtr instances can be created, copied, and destroyed on any thread,
/// and Lock() is race-free against concurrent destruction of the object.
///
/// \parblock
/// @note  `T` must derive from WeakRefCounted; using WeakPtr with a plain RefCounted
///        type is a compile error.
/// \endparblock
///
/// \parblock
/// @note  Only construct a WeakPtr from a fully-constructed object: never from `this`
///        inside a constructor.
/// \endparblock
///
template<typename T> class WeakPtr {
  static_assert(std::is_base_of<WeakRefCounted, T>::value,
                "WeakPtr<T> requires T to derive from WeakRefCounted");
  template <typename U> friend class WeakPtr;
 public:
  ///
  /// Construct a null WeakPtr.
  ///
  constexpr WeakPtr() : instance_(nullptr), control_block_(nullptr) { }

  ///
  /// Construct a null WeakPtr.
  ///
  WeakPtr(std::nullptr_t) : instance_(nullptr), control_block_(nullptr) { }

  ///
  /// Construct a WeakPtr observing the object held by a RefPtr (may be null).
  ///
  template<typename U>
  WeakPtr(const RefPtr<U>& strong)
    : instance_(strong.get()),
      control_block_(strong ? strong->weak_control_block() : nullptr)
  {
    if (control_block_)
      control_block_->WeakRef();
  }

  ///
  /// Construct a WeakPtr observing a raw pointer (may be null).
  ///
  /// The object must currently be alive: the caller (or another owner) must hold a
  /// strong reference to it.
  ///
  template<typename U>
  explicit WeakPtr(U* instance)
    : instance_(instance),
      control_block_(instance ? instance->weak_control_block() : nullptr)
  {
    if (control_block_)
      control_block_->WeakRef();
  }

  ///
  /// Copy constructor.
  ///
  WeakPtr(const WeakPtr& other)
    : instance_(other.instance_),
      control_block_(other.control_block_ ? other.control_block_->WeakRef() : nullptr)
  {
  }

  ///
  /// Move constructor.
  ///
  WeakPtr(WeakPtr&& other)
    : instance_(other.instance_), control_block_(other.control_block_)
  {
    other.instance_ = nullptr;
    other.control_block_ = nullptr;
  }

  ~WeakPtr()
  {
    if (control_block_)
      control_block_->WeakDeref();
  }

  WeakPtr& operator=(const WeakPtr& other)
  {
    WeakPtr ptr = other;
    swap(ptr);
    return *this;
  }

  WeakPtr& operator=(WeakPtr&& other)
  {
    WeakPtr ptr = std::move(other);
    swap(ptr);
    return *this;
  }

  WeakPtr& operator=(std::nullptr_t)
  {
    Reset();
    return *this;
  }

  template<typename U>
  WeakPtr& operator=(const RefPtr<U>& strong)
  {
    WeakPtr ptr = strong;
    swap(ptr);
    return *this;
  }

  ///
  /// Atomically take a strong reference to the observed object.
  ///
  /// @return  Returns a RefPtr keeping the object alive, or a null RefPtr if the object has
  ///          been destroyed (or this WeakPtr is null).
  ///
  RefPtr<T> Lock() const
  {
    return control_block_ ? control_block_->MakeStrongRefIfPossible(instance_)
                          : RefPtr<T>(nullptr);
  }

  ///
  /// Whether the observed object has been destroyed (or this WeakPtr is null).
  ///
  /// @warning  Racy hint only: the object may be destroyed by another thread
  ///           immediately after this returns false. Never gate a dereference on it;
  ///           use Lock() instead.
  ///
  bool Expired() const { return !control_block_ || control_block_->expired(); }

  ///
  /// Stop observing the object and become a null WeakPtr.
  ///
  void Reset()
  {
    if (control_block_)
      control_block_->WeakDeref();
    control_block_ = nullptr;
    instance_ = nullptr;
  }

  ///
  /// Exchanges the observed objects of *this and other.
  ///
  void swap(WeakPtr& other)
  {
    std::swap(instance_, other.instance_);
    std::swap(control_block_, other.control_block_);
  }

 private:
  // Two-member storage: the typed pointer (not the control block's most-derived object
  // pointer) is what Lock() returns, so interior pointers from multiple inheritance
  // resolve to the correct subobject.
  T* instance_;
  WeakControlBlock* control_block_;
};

template<class T>
void swap(WeakPtr<T>& a, WeakPtr<T>& b)
{
  a.swap(b);
}

template<typename T, typename U>
bool operator==(const RefPtr<T>& a, const RefPtr<U>& b)
{
  return a.get() == b.get();
}

template<typename T, typename U>
bool operator==(const RefPtr<T>& a, const U* b)
{
  return a.get() == b;
}

template<typename T, typename U>
bool operator==(const T* a, const RefPtr<U>& b)
{
  return a == b.get();
}

template<typename T, typename U>
bool operator!=(const RefPtr<T>& a, const RefPtr<U>& b)
{
  return a.get() != b.get();
}

template<typename T, typename U>
bool operator!=(const RefPtr<T>& a, const U* b)
{
  return a.get() != b;
}

template<typename T, typename U>
bool operator!=(const T* a, const RefPtr<U>& b)
{
  return a != b.get();
}

}  // namespace ultralight
