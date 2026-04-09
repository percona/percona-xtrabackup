
/* Copyright (c) 2025, Oracle and/or its affiliates.

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
#include <memory>

#include "my_temporal.h"
#include "my_time.h"

namespace Time_val_unittest {

//////////////////////////////////////////////////////////////////////////////

TEST(Time_val, MYSQL_TIME) {
  Time_val time1(false, 24, 0, 0, 0);
  MYSQL_TIME mt = static_cast<MYSQL_TIME>(time1);
  Time_val time2 = Time_val(mt);
  EXPECT_EQ(0, time1.compare(time2));
  MYSQL_TIME mytime(2023, 1, 30, 12, 0, 0, 0, false, MYSQL_TIMESTAMP_DATETIME,
                    0);
  Time_val a(false, 12, 0, 0, 0);
  EXPECT_EQ(Time_val::strip_date(mytime), a);
  Time_val b{mt};
  EXPECT_EQ(b, time1);
}
TEST(Time_val, fields) {
  Time_val a(true, 1, 2, 3, 4);
  EXPECT_EQ(a.is_negative(), true);
  EXPECT_EQ(a.hour(), 1);
  EXPECT_EQ(a.minute(), 2);
  EXPECT_EQ(a.second(), 3);
  EXPECT_EQ(a.microsecond(), 4);
}
TEST(Time_val, compare) {
  Time_val time00(true, 838, 59, 59, 0);
  Time_val time01(true, 838, 0, 0, 0);
  Time_val time02(true, 1, 0, 0, 0);
  Time_val time10(true, 0, 59, 0, 0);
  Time_val time11(true, 0, 1, 0, 0);
  Time_val time20(true, 0, 0, 59, 0);
  Time_val time21(true, 0, 0, 1, 0);
  Time_val time30(true, 0, 0, 0, 999999);
  Time_val time31(true, 0, 0, 0, 1);
  Time_val time40(false, 0, 0, 0, 0);
  Time_val time41(false, 0, 0, 0, 1);
  Time_val time42(false, 0, 0, 0, 999999);
  Time_val time50(false, 0, 0, 1, 0);
  Time_val time51(false, 0, 0, 59, 0);
  Time_val time60(false, 0, 1, 0, 0);
  Time_val time61(false, 0, 59, 0, 0);
  Time_val time70(false, 1, 0, 0, 0);
  Time_val time71(false, 838, 0, 0, 0);
  Time_val time72(false, 838, 59, 59, 0);

  EXPECT_GT(0, time00.compare(time01));
  EXPECT_GT(0, time01.compare(time02));
  EXPECT_GT(0, time02.compare(time10));
  EXPECT_GT(0, time10.compare(time11));
  EXPECT_GT(0, time11.compare(time20));
  EXPECT_GT(0, time20.compare(time21));
  EXPECT_GT(0, time21.compare(time30));
  EXPECT_GT(0, time30.compare(time31));
  EXPECT_GT(0, time31.compare(time40));
  EXPECT_GT(0, time40.compare(time41));
  EXPECT_GT(0, time41.compare(time42));
  EXPECT_GT(0, time42.compare(time50));
  EXPECT_GT(0, time50.compare(time51));
  EXPECT_GT(0, time51.compare(time60));
  EXPECT_GT(0, time60.compare(time61));
  EXPECT_GT(0, time61.compare(time70));
  EXPECT_GT(0, time70.compare(time71));
  EXPECT_GT(0, time71.compare(time72));
}
TEST(Time_val, to_seconds) {
  int32_t seconds1 = Time_val(false, 2, 10, 10, 123456).to_seconds();
  EXPECT_EQ(seconds1, 7810);
  int32_t seconds2 = Time_val(true, 2, 10, 10, 123456).to_seconds();
  EXPECT_EQ(seconds2, -7810);
}
TEST(Time_val, to_microseconds) {
  int64_t micro1 = Time_val(false, 2, 10, 10, 123456).to_microseconds();
  EXPECT_EQ(micro1, 7810123456);
  int64_t micro2 = Time_val(true, 2, 10, 10, 123456).to_microseconds();
  EXPECT_EQ(micro2, -7810123456);
}
TEST(Time_val, to_int_rounded) {
  int64_t hhmmss1 = Time_val(false, 2, 10, 10, 500000).to_int_rounded();
  EXPECT_EQ(hhmmss1, 21011);
  int64_t hhmmss2 = Time_val(false, 2, 10, 10, 499999).to_int_rounded();
  EXPECT_EQ(hhmmss2, 21010);
  int64_t hhmmss3 = Time_val(true, 2, 10, 10, 500000).to_int_rounded();
  EXPECT_EQ(hhmmss3, -21011);
  int64_t hhmmss4 = Time_val(true, 2, 10, 10, 499999).to_int_rounded();
  EXPECT_EQ(hhmmss4, -21010);
}
TEST(Time_val, to_int_truncated) {
  int64_t hhmmss1 = Time_val(false, 2, 10, 10, 500000).to_int_truncated();
  EXPECT_EQ(hhmmss1, 21010);
  int64_t hhmmss2 = Time_val(false, 2, 10, 10, 499999).to_int_truncated();
  EXPECT_EQ(hhmmss2, 21010);
  int64_t hhmmss3 = Time_val(true, 2, 10, 10, 500000).to_int_truncated();
  EXPECT_EQ(hhmmss3, -21010);
  int64_t hhmmss4 = Time_val(true, 2, 10, 10, 499999).to_int_truncated();
  EXPECT_EQ(hhmmss4, -21010);
}
TEST(Time_val, to_double) {
  Time_val tv1(false, 23, 3, 23, 456789);
  EXPECT_EQ(230323.456789, tv1.to_double());
  Time_val tv2(true, 23, 3, 23, 456789);
  EXPECT_EQ(-230323.456789, tv2.to_double());
}
TEST(Time_val, add_nanoseconds_round) {
  Time_val a;
  a.set_zero();
  a.add_nanoseconds_round(999999999);
  Time_val b(false, 0, 0, 1, 0);
  EXPECT_EQ(a, b);
  Time_val c;
  c.set_zero();
  c.add_nanoseconds_round(-999999999);
  Time_val d(true, 0, 0, 1, 0);
  EXPECT_EQ(c, d);
}
TEST(Time_val, round) {
  Time_val time0 = Time_val(false, 0, 0, 0, 940000);
  Time_val time1 = Time_val(false, 0, 0, 0, 950000);
  Time_val time2 = Time_val(false, 0, 0, 0, 990000);
  time0.adjust_fraction(1, true);
  time1.adjust_fraction(1, true);
  time2.adjust_fraction(1, true);
  Time_val rounded0(false, 0, 0, 0, 900000);
  Time_val rounded1(false, 0, 0, 1, 0);
  Time_val rounded2(false, 0, 0, 1, 0);
  EXPECT_EQ(time0, rounded0);
  EXPECT_EQ(time1, rounded1);
  EXPECT_EQ(time2, rounded2);

  Time_val time3 = Time_val(true, 0, 0, 0, 940000);
  Time_val time4 = Time_val(true, 0, 0, 0, 950000);
  Time_val time5 = Time_val(true, 0, 0, 0, 990000);
  time3.adjust_fraction(1, true);
  time4.adjust_fraction(1, true);
  time5.adjust_fraction(1, true);
  Time_val rounded3(true, 0, 0, 0, 900000);
  Time_val rounded4(true, 0, 0, 1, 0);
  Time_val rounded5(true, 0, 0, 1, 0);
  EXPECT_EQ(time3, rounded3);
  EXPECT_EQ(time4, rounded4);
  EXPECT_EQ(time5, rounded5);

  Time_val time6(false, 10, 20, 30, 0);
  time6.adjust_fraction(2, true);
  Time_val time6_dup(false, 10, 20, 30, 0);
  EXPECT_EQ(time6, time6_dup);
}

TEST(Time_val, truncate) {
  Time_val time0 = Time_val(false, 0, 0, 0, 940000);
  Time_val time1 = Time_val(false, 0, 0, 0, 950000);
  Time_val time2 = Time_val(false, 0, 0, 0, 990000);
  time0.adjust_fraction(1, false);
  time1.adjust_fraction(1, false);
  time2.adjust_fraction(1, false);
  Time_val truncated0(false, 0, 0, 0, 900000);
  Time_val truncated1(false, 0, 0, 0, 900000);
  Time_val truncated2(false, 0, 0, 0, 900000);
  EXPECT_EQ(time0, truncated0);
  EXPECT_EQ(time1, truncated1);
  EXPECT_EQ(time2, truncated2);
  Time_val time3 = Time_val(true, 0, 0, 0, 940000);
  Time_val time4 = Time_val(true, 0, 0, 0, 950000);
  Time_val time5 = Time_val(true, 0, 0, 0, 990000);
  time3.adjust_fraction(1, false);
  time4.adjust_fraction(1, false);
  time5.adjust_fraction(1, false);
  Time_val truncated3(true, 0, 0, 0, 900000);
  Time_val truncated4(true, 0, 0, 0, 900000);
  Time_val truncated5(true, 0, 0, 0, 900000);
  EXPECT_EQ(time3, truncated3);
  EXPECT_EQ(time4, truncated4);
  EXPECT_EQ(time5, truncated5);
}
TEST(Time_val, to_string) {
  char buffer[20];
  Time_val time = Time_val(true, 1, 2, 3, 4);
  size_t buf_len = time.to_string(buffer, 6);
  buffer[buf_len] = 0;
  EXPECT_EQ(0, strcmp(buffer, "-  1:02:03.000004"));
}
TEST(Time_val, add) {
  Time_val time0(false, 10, 10, 10, 10);
  time0.add(Time_val(false, 10, 10, 10, 10), false);
  EXPECT_EQ(time0, Time_val(false, 20, 20, 20, 20));
  time0.add(Time_val(false, 10, 10, 10, 10), true);
  EXPECT_EQ(time0, Time_val(false, 10, 10, 10, 10));

  Interval iv;
  memset(&iv, 0, sizeof(iv));

  Time_val time1(false, 11, 12, 13, 456789);
  iv.second_part = 900000;
  time1.add(iv, false);
  EXPECT_EQ(time1, Time_val(false, 11, 12, 14, 356789));
  time1.add(iv, true);
  EXPECT_EQ(time1, Time_val(false, 11, 12, 13, 456789));

  Time_val time2(false, 11, 12, 13, 456789);
  iv.second_part = 0;
  iv.second = 60 * 60 + 59;
  time2.add(iv, false);
  EXPECT_EQ(time2, Time_val(false, 12, 13, 12, 456789));
  time2.add(iv, true);
  EXPECT_EQ(time2, Time_val(false, 11, 12, 13, 456789));

  Time_val time3(false, 11, 12, 13, 456789);
  iv.second = 0;
  iv.minute = 24 * 60 + 59;
  time3.add(iv, false);
  EXPECT_EQ(time3, Time_val(false, 36, 11, 13, 456789));
  time3.add(iv, true);
  EXPECT_EQ(time3, Time_val(false, 11, 12, 13, 456789));

  Time_val time4(false, 11, 12, 13, 456789);
  iv.minute = 0;
  iv.hour = 800;
  time4.add(iv, false);
  EXPECT_EQ(time4, Time_val(false, 811, 12, 13, 456789));
  time4.add(iv, true);
  EXPECT_EQ(time4, Time_val(false, 11, 12, 13, 456789));

  Time_val time5(false, 0, 0, 0, 0);
  time5.add_nanoseconds_round(500);
  EXPECT_EQ(time5, Time_val(false, 0, 0, 0, 1));

  Time_val time6(false, 0, 0, 0, 0);
  time6.add_nanoseconds_round(-500);
  EXPECT_EQ(time6, Time_val(true, 0, 0, 0, 1));
}
TEST(Time_val, extreme_values) {
  Time_val time;
  time.set_zero();
  EXPECT_EQ(time, Time_val(false, 0, 0, 0, 0));
  time.set_extreme_value(false);
  EXPECT_EQ(time, Time_val(false, 838, 59, 59, 0));
  time.set_extreme_value(true);
  EXPECT_EQ(time, Time_val(true, 838, 59, 59, 0));
}
TEST(Time_val, is_adjusted) {
  EXPECT_TRUE(Time_val(false, 838, 59, 58, 999999).is_adjusted(6));
  EXPECT_FALSE(Time_val(false, 838, 59, 58, 999999).is_adjusted(5));
  EXPECT_TRUE(Time_val(false, 838, 58, 59, 999990).is_adjusted(5));
  EXPECT_FALSE(Time_val(false, 838, 58, 59, 999990).is_adjusted(4));
  EXPECT_TRUE(Time_val(false, 23, 59, 59, 999900).is_adjusted(4));
  EXPECT_FALSE(Time_val(false, 23, 59, 59, 999900).is_adjusted(3));
  EXPECT_TRUE(Time_val(true, 23, 59, 59, 999000).is_adjusted(3));
  EXPECT_FALSE(Time_val(true, 23, 59, 59, 999000).is_adjusted(2));
  EXPECT_TRUE(Time_val(false, 23, 59, 59, 990000).is_adjusted(2));
  EXPECT_FALSE(Time_val(false, 23, 59, 59, 990000).is_adjusted(1));
  EXPECT_TRUE(Time_val(true, 23, 59, 59, 900000).is_adjusted(1));
  EXPECT_FALSE(Time_val(true, 23, 59, 59, 900000).is_adjusted(0));
  EXPECT_TRUE(Time_val(false, 23, 59, 59, 0).is_adjusted(0));
  EXPECT_TRUE(Time_val(true, 23, 59, 59, 0).is_adjusted(0));
}
TEST(Time_val, actual_decimals) {
  EXPECT_EQ(6, Time_val(false, 838, 59, 58, 999999).actual_decimals());
  EXPECT_EQ(5, Time_val(true, 838, 59, 58, 999990).actual_decimals());
  EXPECT_EQ(4, Time_val(false, 23, 59, 59, 999900).actual_decimals());
  EXPECT_EQ(3, Time_val(true, 23, 59, 59, 999000).actual_decimals());
  EXPECT_EQ(2, Time_val(false, 23, 59, 59, 990000).actual_decimals());
  EXPECT_EQ(1, Time_val(true, 23, 59, 59, 900000).actual_decimals());
  EXPECT_EQ(0, Time_val(false, 23, 59, 59, 0).actual_decimals());
}
}  // namespace Time_val_unittest
