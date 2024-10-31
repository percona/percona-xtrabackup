/* Copyright (c) 2008, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <assert.h>
#include <string.h>

#include "my_inttypes.h"
#include "my_sys.h"
#include "storage/perfschema/pfs_global.h"

bool pfs_initialized = false;
size_t pfs_allocated_memory_size = 0;
size_t pfs_allocated_memory_count = 0;

bool stub_alloc_always_fails = true;
bool stub_alloc_maybe_fails = true;
int stub_alloc_fails_after_count = 0;

void *pfs_malloc(PFS_builtin_memory_class *, size_t size, myf) {
  /*
    Catch non initialized sizing parameter in the unit tests.
  */
  assert(size <= 100 * 1024 * 1024);

  if (stub_alloc_always_fails) return nullptr;

  if (stub_alloc_maybe_fails) {
    if (--stub_alloc_fails_after_count <= 0) {
      return nullptr;
    }
  }

  void *ptr = nullptr;

#ifdef PFS_ALIGNMENT
#ifdef HAVE_POSIX_MEMALIGN
  /* Linux */
  if (unlikely(posix_memalign(&ptr, PFS_ALIGNMENT, size))) {
    return nullptr;
  }
#else
#ifdef HAVE_MEMALIGN
  /* Solaris */
  ptr = memalign(PFS_ALIGNMENT, size);
#else
#ifdef HAVE_ALIGNED_MALLOC
  /* Windows */
  ptr = _aligned_malloc(size, PFS_ALIGNMENT);
#else
#error "Missing implementation for PFS_ALIGNMENT"
#endif /* HAVE_ALIGNED_MALLOC */
#endif /* HAVE_MEMALIGN */
#endif /* HAVE_POSIX_MEMALIGN */
#else  /* PFS_ALIGNMENT */
  /* Everything else */
  ptr = malloc(size);
#endif

  if (ptr != nullptr) memset(ptr, 0, size);
  return ptr;
}

void pfs_free(PFS_builtin_memory_class *, size_t, void *ptr) {
  if (ptr == nullptr) {
    return;
  }

#ifdef HAVE_POSIX_MEMALIGN
  /* Allocated with posix_memalign() */
  free(ptr);
#else
#ifdef HAVE_MEMALIGN
  /* Allocated with memalign() */
  free(ptr);
#else
#ifdef HAVE_ALIGNED_MALLOC
  /* Allocated with _aligned_malloc() */
  _aligned_free(ptr);
#else
  /* Allocated with malloc() */
  free(ptr);
#endif /* HAVE_ALIGNED_MALLOC */
#endif /* HAVE_MEMALIGN */
#endif /* HAVE_POSIX_MEMALIGN */
}

void *pfs_malloc_array(PFS_builtin_memory_class *klass, size_t n, size_t size,
                       myf flags) {
  size_t array_size = n * size;
  /* Check for overflow before allocating. */
  if (is_overflow(array_size, n, size)) return nullptr;
  return pfs_malloc(klass, array_size, flags);
}

void pfs_free_array(PFS_builtin_memory_class *klass, size_t n, size_t size,
                    void *ptr) {
  if (ptr == nullptr) return;
  size_t array_size = n * size;
  return pfs_free(klass, array_size, ptr);
}

bool is_overflow(size_t product, size_t n1, size_t n2) {
  if (n1 != 0 && (product / n1 != n2))
    return true;
  else
    return false;
}

void pfs_print_error(const char *, ...) {}
