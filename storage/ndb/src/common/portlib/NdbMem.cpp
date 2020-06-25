/*
   Copyright (c) 2003, 2019, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/


#ifdef _WIN32
#include <malloc.h> // _aligned_alloc
#include <Windows.h>
#else
#include <stdlib.h> // aligned_alloc or posix_memalign
#include <sys/mman.h>
#endif

#include <NdbMem.h>


int NdbMem_MemLockAll(int i){
  if (i == 1)
  {
#if defined(HAVE_MLOCKALL) && defined(MCL_CURRENT) && defined (MCL_FUTURE)
    return mlockall(MCL_CURRENT | MCL_FUTURE);
#else
    return -1;
#endif
  }
#if defined(HAVE_MLOCKALL) && defined(MCL_CURRENT)
  return mlockall(MCL_CURRENT);
#else
  return -1;
#endif
}

int NdbMem_MemUnlockAll(){
#if defined(HAVE_MLOCKALL) && defined(MCL_CURRENT)
  return munlockall();
#else
  return -1;
#endif
}

int NdbMem_MemLock(const void * ptr, size_t len)
{
#if defined(HAVE_MLOCK)
  return mlock(ptr, len);
#else
  return -1;
#endif
}

#if defined(VM_TRACE) && !defined(__APPLE__)

/**
 * Experimental functions to manage virtual memory without backing, nor
 * in memory nor on disk.
 *
 * These functions can be used in debug builds to test some extreme memory
 * configuration not normally possible since not enough physical memory
 * pages to map to whole address space.
 *
 * Likely these functions will not work as expected on all platforms, nor
 * does them consider large pages or NUMA in any way.
 *
 */

/**
 * NdbMem_ReserveSpace
 *
 * Should reserve only an address space in the process virtual memory,
 * without any physical pages reserved neither in memory nor on disk.
 *
 * Especially the address space should not be dumped if process crash.
 *
 * Also if lock all memory is configured, there should be no memory
 * locked to the reserved address space.
 */
int NdbMem_ReserveSpace(void** ptr, size_t len)
{
  void * p;
  if (ptr == NULL)
  {
    return -1;
  }
#ifdef _WIN32
  p = VirtualAlloc(*ptr, len, MEM_RESERVE, PAGE_NOACCESS);
  *ptr = p;
  return (p == NULL) ? 0 : -1;
#elif defined(MAP_NORESERVE)
  /*
   * MAP_NORESERVE is essential to not reserve swap space on Solaris.
   * If this code is activated on operating systems not having the non standard
   * MAP_NORESERVE one need to find out how to do it instead or make sure these
   * functions (NdbMem_ReserveSpace(), NdbMem_PopulateSpace(),
   * NdbMem_FreeSpace()) are not used on that platform.
   *
   * This code is currently only used in debug build there can be called with a
   * huge length as 128TB, not all intended to be backed my storage.
   *
   * Even when we start using these function in production code and all mapped
   * memory should be backed it makes sense not to enforce swap space
   * reservation since swapping the memory of data node is not recommended, on
   * the contrary it should be locked to RAM if possible.
   *
   * If only memory that should be backed is mapped one could consider skipping
   * MAP_NORESERVE to have an early error (for badly configured systems) rather
   * than have some undefined behaviour in later calls to NdbMem_PopulateSpace.
   */
  p = mmap(*ptr,
           len,
           PROT_NONE,
           MAP_ANONYMOUS | MAP_PRIVATE | MAP_NORESERVE,
           -1,
           0);
  if (p == MAP_FAILED)
  {
    *ptr = NULL;
    return -1;
  }
#if defined(MADV_DONTDUMP)
  if (-1 == madvise(p, len, MADV_DONTDUMP))
  {
    require(0 == munmap(p, len));
    *ptr = NULL;
    return -1;
  }
#endif
  *ptr = p;
  return 0;
#elif defined(MAP_GUARD) /* FreeBSD */
  p = mmap(*ptr,
           len,
           PROT_NONE,
           MAP_ANONYMOUS | MAP_PRIVATE | MAP_GUARD,
           -1,
           0);
  *ptr = p;
  return 0;
#else
#error Need mmap() to not reserve swap for mapping.
#endif
}

/**
 * NdbMem_PopulateSpace
 *
 * Make sure there will be memory reserved and initialized for requested
 * address range.
 *
 * The range should be dumped if process crash, and locked if that is
 * configured.
 *
 * The range must be a subrange of what is returned by a preceding call
 * to NdbMem_ReserveSpace.
 *
 * Range must also be aligned to page boundaries.
 */
int NdbMem_PopulateSpace(void* ptr, size_t len)
{
#ifdef _WIN32
  void* p = VirtualAlloc(ptr, len, MEM_COMMIT, PAGE_READWRITE);
  return (p == NULL) ? 0 : -1;
#elif defined(MAP_GUARD) /* FreeBSD */
  void* p = mmap(ptr,
                 len,
                 PROT_READ | PROT_WRITE,
                 MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED,
                 -1,
                 0);
  if (p == MAP_FAILED)
  {
    return -1;
  }
  require(p == ptr);
  return 0;
#else /* Linux, Solaris */
  int ret = mprotect(ptr, len, PROT_READ | PROT_WRITE);
  if (ret == 0)
  {
    char* p = (char*)ptr;
    size_t i;
    // Assume page size is a multiple of 4096 bytes
    for(i=0; i < len; i += 4096)
    {
      p[i]=0;
    }
#ifdef MADV_DODUMP
    ret = madvise(ptr, len, MADV_DODUMP);
    if (ret == -1)
    {
      (void) mprotect(ptr, len, PROT_NONE);
    }
#endif
  }
  return ret;
#endif
}

/**
 * NdbMem_FreeSpace
 *
 * Release address space previously reserved by NdbMem_ReserveSpace.
 *
 * The input arguments must be exactly what was returned and used for
 * NdbMem_ReserveSpace call.
 *
 * Any memory reserved with NdbMem_PopulateMemory will be released without
 * further action.
 */
int NdbMem_FreeSpace(void* ptr, size_t len)
{
#ifdef _WIN32
  const BOOL ok = VirtualFree(ptr, 0, MEM_RELEASE);
  (void)len;
  return ok ? 0 : -1;
#else
  return munmap(ptr, len);
#endif
}

#endif

void* NdbMem_AlignedAlloc(size_t alignment, size_t size)
{
  void* p = NULL;
#if defined(_ISOC11_SOURCE)
  p = aligned_alloc(alignment, size);
#elif defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200112L
  (void) posix_memalign(&p, alignment, size);
#elif defined(_WIN32)
  p = _aligned_malloc(size, alignment);
#else
  if (alignment < sizeof(void*))
  {
    alignment = sizeof(void*);
  }
  char*charp = (char*) malloc(size + alignment);
  if (charp != NULL)
  {
    void* q = (void*)(charp + (alignment - ((uintptr_t)charp % alignment)));
    void** qp = (void**)q;
    qp[-1] = p;
    p = q;
  }
#endif
  return p;
}

void NdbMem_AlignedFree(void* p)
{
#if defined(_ISOC11_SOURCE) || \
    (defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200112L)
  free(p);
#elif defined(_WIN32)
  _aligned_free(p);
#else
  void** qp = (void**)p;
  p = qp[-1];
  free(p);
#endif
}
