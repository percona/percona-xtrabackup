/* Copyright (c) 2018 Percona LLC and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; version 2 of
   the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef MYSQL_KEYRING_SECURE_STRING
#define MYSQL_KEYRING_SECURE_STRING

#include <sstream>
#include "keyring_memory.h"

namespace keyring {
typedef std::basic_string<char, std::char_traits<char>, Secure_allocator<char>>
    Secure_string;
typedef std::basic_ostringstream<char, std::char_traits<char>,
                                 Secure_allocator<char>>
    Secure_ostringstream;
typedef std::basic_istringstream<char, std::char_traits<char>,
                                 Secure_allocator<char>>
    Secure_istringstream;
}  // namespace keyring

#endif  // MYSQL_KEYRING_SECURE_STRING
