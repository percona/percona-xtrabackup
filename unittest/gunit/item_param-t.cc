/* Copyright (c) 2014, 2025, Oracle and/or its affiliates.

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

#include <gtest/gtest.h>
#include <sql/item.h>
#include "my_config.h"

#include "test_utils.h"

namespace item_param_unittest {
using my_testing::Server_initializer;

class ItemParamTest : public ::testing::Test {
 protected:
  void SetUp() override {
    m_initializer.SetUp();
    // An Item expects to be owned by current_thd->free_list, so allocate with
    // new, and do not delete it.
  }

  void TearDown() override { m_initializer.TearDown(); }

  Server_initializer m_initializer;
};

TEST_F(ItemParamTest, ItemParamHashTest) {
  auto *m_item_param2 = new Item_param(POS(), 1.0);
  auto *m_item_param3 = new Item_param(POS(), 1.0);
  auto *m_item_param4 = new Item_param(POS(), 2.0);
  EXPECT_EQ(m_item_param2->hash(), m_item_param3->hash());
  EXPECT_EQ(m_item_param3->hash(), m_item_param2->hash());
  EXPECT_NE(m_item_param4->hash(), m_item_param3->hash());

  auto *m_item_param5 = new Item_param(POS(), 2.0);
  m_item_param5->set_null();
  auto *m_item_param6 = new Item_param(POS(), 2.0);
  m_item_param6->set_null();

  EXPECT_EQ(m_item_param5->hash(), m_item_param6->hash());
  EXPECT_NE(m_item_param5->hash(), m_item_param4->hash());

  auto *m_item_param_time = new Item_param(POS(), 1.0);
  auto *m_item_param_time2 = new Item_param(POS(), 1.0);

  auto *m_item_param_time3 = new Item_param(POS(), 1.0);

  MYSQL_TIME time;
  time.year = 2006;
  time.month = 3;
  time.day = 24;
  time.hour = 22;
  time.minute = 10;
  time.second = 24;
  time.second_part = 10;
  time.time_zone_displacement = 1;

  MYSQL_TIME time2;
  time2.year = 2007;
  time2.month = 2;
  time2.day = 21;
  time.hour = 22;
  time.minute = 9;
  time.second = 12;
  time.second_part = 5;
  time.time_zone_displacement = 2;

  m_item_param_time->set_time(&time, MYSQL_TIMESTAMP_DATE);
  m_item_param_time2->set_time(&time2, MYSQL_TIMESTAMP_DATE);
  m_item_param_time3->set_time(&time2, MYSQL_TIMESTAMP_DATE);

  EXPECT_EQ(m_item_param_time3->hash(), m_item_param_time2->hash());
  EXPECT_EQ(m_item_param_time3->hash(), m_item_param_time2->hash());

  auto *m_item_param_real = new Item_param(POS(), 1.0);
  auto *m_item_param_real2 = new Item_param(POS(), 1.0);

  auto *m_item_param_real3 = new Item_param(POS(), 1.0);

  m_item_param_real->set_double(1.0);
  m_item_param_real2->set_double(2.0);
  m_item_param_real3->set_double(2.0);

  EXPECT_NE(m_item_param_real->hash(), m_item_param_real2->hash());
  EXPECT_NE(m_item_param5->hash(), m_item_param_real->hash());
  EXPECT_EQ(m_item_param_real3->hash(), m_item_param_real2->hash());
}
}  // namespace item_param_unittest
