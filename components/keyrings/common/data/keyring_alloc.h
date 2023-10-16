/* Copyright (c) 2023, Percona and/or its affiliates.

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

#ifndef KEYRING_ALLOC_INCLUDED
#define KEYRING_ALLOC_INCLUDED

#include "pfs_string.h"

namespace keyring_common::data {

template <class T>
T comp_keyring_malloc(size_t size) {
  void *allocated_memory = my_malloc(KEY_mem_keyring, size, MYF(MY_WME));
  return allocated_memory ? reinterpret_cast<T>(allocated_memory) : NULL;
}

class Comp_keyring_alloc {
 public:
  static void *operator new(size_t size) noexcept {
    return comp_keyring_malloc<void *>(size);
  }
  static void *operator new[](size_t size) noexcept {
    return comp_keyring_malloc<void *>(size);
  }
  static void operator delete(void *ptr, std::size_t) { my_free(ptr); }
  static void operator delete[](void *ptr, std::size_t) { my_free(ptr); }
};

}  // namespace keyring_common::data

#endif  // KEYRING_ALLOC_INCLUDED
