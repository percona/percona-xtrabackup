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

#ifndef MOCK_SERIALIZED_OBJECT_H
#define MOCK_SERIALIZED_OBJECT_H

#include <gmock/gmock.h>

#include "plugin/keyring/common/i_serialized_object.h"

namespace keyring {
class Mock_serialized_object : public ISerialized_object {
 public:
  MOCK_METHOD1(get_next_key, bool(IKey **key));
  MOCK_METHOD0(has_next_key, bool());
  MOCK_METHOD0(get_key_operation, Key_operation());
  MOCK_METHOD1(set_key_operation, void(Key_operation));
};
}  // namespace keyring

#endif  // MOCK_SERIALIZED_OBJECT_H
