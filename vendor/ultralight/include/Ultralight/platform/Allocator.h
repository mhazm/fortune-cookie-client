///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#ifndef ULTRALIGHT_ALLOCATOR_H
#define ULTRALIGHT_ALLOCATOR_H

#include <stddef.h>
#include <stdbool.h>
#include <Ultralight/Exports.h>

#ifdef __cplusplus
extern "C" {
#endif

///
/// User-defined allocator interface.
///
/// @pre  Requires a Windows build with the `UL_ENABLE_ALLOCATOR_OVERRIDE` build option enabled.
///       The option has no effect on macOS or Linux, and no published Ultralight binary enables
///       it.
///
/// The library uses this to allocate memory. You can override the default allocator functions
/// by setting the ulAllocator object with your own functions.
///
/// This should be done before calling any other library functions.
///
/// @see ulAllocator
///
typedef struct {
  ///
  /// Allocate a block of memory of at least |bytes| size.
  ///
  void* (*malloc)(size_t bytes);

  ///
  /// Reallocate a block of memory to at least |bytes| size.
  ///
  void* (*realloc)(void* address, size_t bytes);

  ///
  /// Free a block of memory allocated with malloc or realloc.
  ///
  void (*free)(void* address);

  ///
  /// Allocate a block of memory of at least |bytes| size, aligned to |alignment|.
  ///
  void* (*aligned_malloc)(size_t bytes, size_t alignment);

  ///
  /// Reallocate a block of memory to at least |bytes| size, aligned to |alignment|.
  ///
  void* (*aligned_realloc)(void* address, size_t bytes, size_t alignment);

  ///
  /// Free a block of memory allocated with aligned_malloc or aligned_realloc.
  ///
  void (*aligned_free)(void* address);

  ///
  /// Get the size of the memory block that backs the allocation at |address|. The memory
  /// block size is always at least as large as the allocation it backs, and may be larger.
  ///  * Windows equivalent: _msize
  ///  * POSIX equivalent: malloc_size
  ///
  size_t (*get_size_estimate)(void* address);

} ULAllocator;

///
/// Get the allocator interface object for the library.
///
/// @pre  Requires a Windows build with the `UL_ENABLE_ALLOCATOR_OVERRIDE` build option enabled.
///       The option has no effect on macOS or Linux, and no published Ultralight binary enables
///       it.
///
/// The C functions set in this object will be used for allocating memory inside the library.
///
/// Default functions are already set for all of these but you can override them with your own.
///
extern UCExport ULAllocator ulAllocator;

#ifdef _WIN32
///
/// Get the handle to the private heap used by the library.
///
/// @return  Returns the heap the library allocates from when a build with the
///          `UL_ENABLE_ALLOCATOR_OVERRIDE` option creates one, and NULL when the library uses the
///          process heap (every published binary does). If non-NULL, destroy it with
///          `HeapDestroy()` after unloading the library.
///
UCExport void* ulGetHeapHandle();
#endif

/// @cond INTERNAL

// Library-heap allocation functions. Backing the ulAllocator override table and used
// directly by public header-only templates (Vector, etc.) to ensure allocations are
// freed by the same heap that created them.
UCExport void* ul_malloc(size_t bytes);
UCExport void* ul_aligned_malloc(size_t bytes, size_t alignment);
UCExport void* ul_realloc(void* ptr, size_t bytes);
UCExport void* ul_aligned_realloc(void* ptr, size_t bytes, size_t alignment);
UCExport void  ul_free(void* ptr);
UCExport void  ul_aligned_free(void* ptr);
/// @endcond

#ifdef __cplusplus
}
#endif

#endif // ULTRALIGHT_ALLOCATOR_H