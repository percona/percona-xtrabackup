/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef MYSQL_KEYRING_MEMORY_H
#define MYSQL_KEYRING_MEMORY_H

#include <my_global.h>
#include <mysql/plugin_keyring.h>
#include <limits>
#include <memory>

namespace keyring {
#ifdef HAVE_PSI_INTERFACE
  extern PSI_memory_key key_memory_KEYRING;
#endif /* HAVE_PSI_INTERFACE */
  template <class T>
  T keyring_malloc(size_t size)
  {
#ifdef HAVE_PSI_INTERFACE
    void *allocated_memory= my_malloc(key_memory_KEYRING, size, MYF(MY_WME));
#else
    void *allocated_memory= my_malloc(PSI_NOT_INSTRUMENTED, size, MYF(MY_WME));
#endif /* HAVE_PSI_INTERFACE */
    return allocated_memory ? reinterpret_cast<T>(allocated_memory) : NULL;
  }

  class Keyring_alloc
  {
    public:
      static void *operator new(size_t size) throw ()
      {
        return keyring_malloc<void*>(size);
      }
      static void *operator new[](size_t size) throw ()
      {
        return keyring_malloc<void*>(size);
      }
      static void operator delete(void* ptr, std::size_t sz)
      {
          my_free(ptr);
      }
      static void operator delete[](void* ptr, std::size_t sz)
      {
          my_free(ptr);
      }
  };

  template <class T> class Secure_allocator : public std::allocator<T>
  {
  public:

    template<class U> struct rebind { typedef Secure_allocator<U> other; };
    Secure_allocator() throw() {}
    Secure_allocator(const Secure_allocator& secure_allocator) : std::allocator<T>(secure_allocator)
    {}
    template <class U> Secure_allocator(const Secure_allocator<U>&) throw() {}

    T* allocate(size_t n)
    {
      if (n == 0)
        return NULL;
      else if (n > INT_MAX)
        throw std::bad_alloc();
      return keyring_malloc<T*>(n*sizeof(T));
    }

    void deallocate(T *p, size_t n)
    {
      memset_s(p, n, 0, n);
      my_free(p);
    }
  };
} //namespace keyring
 
#endif //MYSQL_KEYRING_MEMORY_H
