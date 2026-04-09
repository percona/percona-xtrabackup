#ifndef MY_TEMPORAL_H
#define MY_TEMPORAL_H

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

/**
  @ingroup MY_TEMPORAL
  @{

  @file include/my_temporal.h

  Server classes for temporal handling (DATE, TIME, DATETIME)
*/

#include <stdint.h>  // for uint32_t, uint64_t, int64_t, int32_t, uint8_t
#include <stdlib.h>  // for abs, size_t
#include <cassert>   // for assert
#include <string>    // for string

#include "my_time.h"     // for SECS_PER_MIN, SECS_PER_HOUR, Interval (ptr o...
#include "mysql_time.h"  // for MYSQL_TIME, enum_mysql_timestamp_type

/**
  Time_val is a temporal type that represents only time.
  It has constructors for creating time values from time components
  (hour, minute, second and microseconds), and from seconds and microseconds.
  It also has a constructor to create a value from a MYSQL_TIME value.

  The range of values supported is from -838:59:59 to +838:59:59.
  The negative values, and the values from 24:00:00 and up are dedicated
  for use as a small-range interval type and should not be taken as a time
  within a day.
*/
class Time_val {
 public:
  Time_val() : m_value() {}

  Time_val(bool negative, uint32_t hour, uint32_t minute, uint32_t second,
           uint32_t microsecond) {
    assert(hour <= TIME_MAX_HOUR && minute <= TIME_MAX_MINUTE &&
           second <= TIME_MAX_SECOND && microsecond <= TIME_MAX_MICROSEC);
    m_value = (static_cast<uint64_t>(hour) << TIME_SHIFT_HOUR) |
              (static_cast<uint64_t>(minute) << TIME_SHIFT_MINUTE) |
              (static_cast<uint64_t>(second) << TIME_SHIFT_SECOND) |
              microsecond;
    if (negative) {
      assert(hour != 0 || minute != 0 || second != 0 || microsecond != 0);
      // Stored as one's complement, negate value and subtract one
      m_value = (static_cast<uint64_t>(-static_cast<int64_t>(m_value)) &
                 (BITS_MICROSEC | BITS_SECOND | BITS_MINUTE | BITS_HOUR)) -
                1;
    } else {
      m_value |= BITS_SIGN;
    }
    assert(is_valid());
  }

  Time_val(bool negative, uint32_t second, uint32_t microsecond)
      : Time_val(negative, second / SECS_PER_HOUR,
                 (second / SECS_PER_MIN) % MINS_PER_HOUR, second % SECS_PER_MIN,
                 microsecond) {}

  explicit Time_val(const MYSQL_TIME &mtime)
      : Time_val(mtime.neg, mtime.hour, mtime.minute, mtime.second,
                 mtime.second_part) {
    assert(mtime.time_type == MYSQL_TIMESTAMP_TIME);
  }

  bool is_negative() const { return (m_value & BITS_SIGN) == 0; }
  uint32_t hour() const {
    return is_negative() ? 2047U - ((m_value & BITS_HOUR) >> TIME_SHIFT_HOUR)
                         : (m_value & BITS_HOUR) >> TIME_SHIFT_HOUR;
  }
  uint32_t minute() const {
    return is_negative() ? 63U - ((m_value & BITS_MINUTE) >> TIME_SHIFT_MINUTE)
                         : (m_value & BITS_MINUTE) >> TIME_SHIFT_MINUTE;
  }
  uint32_t second() const {
    return is_negative() ? 63U - ((m_value & BITS_SECOND) >> TIME_SHIFT_SECOND)
                         : (m_value & BITS_SECOND) >> TIME_SHIFT_SECOND;
  }
  uint32_t microsecond() const {
    return is_negative() ? BITS_MICROSEC - (m_value & BITS_MICROSEC)
                         : (m_value & BITS_MICROSEC);
  }

  /**
    @returns whether the value is less than, equal to or greater than
             the argument value.
  */
  int compare(const Time_val arg) const {
    return m_value < arg.m_value ? -1 : m_value > arg.m_value ? 1 : 0;
  }
  /// @returns an integer value for comparison purposes
  int64_t for_comparison() const { return m_value; }

  /// Check against extreme values
  bool is_extreme_value(bool positive) const {
    return m_value == (positive ? MAX_TIME_VALUE : MIN_TIME_VALUE);
  }

  /// Set zero time
  void set_zero() { m_value = BITS_SIGN; }

  /// Set extreme value
  void set_extreme_value(bool negative) {
    m_value = negative ? MIN_TIME_VALUE : MAX_TIME_VALUE;
  }

  bool operator==(const Time_val rhs) const { return m_value == rhs.m_value; }

  bool operator<(const Time_val rhs) const { return m_value < rhs.m_value; }
  /**
    Add a time value to another time value, or subtract it

    @param tv       Time value to add or subtract
    @param subtract If true, subtract the time value, otherwise add it.

    @returns false if result is within valid time range, true otherwise.
  */
  bool add(Time_val tv, bool subtract);

  /**
    Add an interval to a time value, or subtract it

    @param iv       Interval to add or subtract
    @param subtract If true, subtract the time value, otherwise add it.

    @returns false if result is within valid time range, true otherwise.
  */
  bool add(Interval &iv, bool subtract);

  /// Static functions for creation
  /**
    Creates Time_val with range check. Minute, second and microsecond
    values must be within limits.

    @returns false if valid range, true if outside valid range.
  */
  static bool make_time(bool negative, uint32_t hour, uint32_t minute,
                        uint32_t second, uint32_t microsecond, Time_val *time) {
    if (hour > TIME_MAX_HOUR || (hour == TIME_MAX_HOUR && microsecond != 0)) {
      return true;
    }
    assert(minute <= TIME_MAX_MINUTE && second <= TIME_MAX_SECOND &&
           microsecond <= TIME_MAX_MICROSEC);
    *time = Time_val(negative, hour, minute, second, microsecond);
    return false;
  }
  /// Creates a Time_val from a date_time by extracting only the time fields
  static Time_val strip_date(const MYSQL_TIME &mt);

  /// Convert time value to the generalized temporal time format.
  explicit operator MYSQL_TIME() const;

  /// @returns time value as number of seconds. Fraction seconds are ignored.
  int32_t to_seconds() const {
    return static_cast<int32_t>(unsigned_seconds(hour(), minute(), second())) *
           (is_negative() ? -1 : 1);
  }
  /// @returns time value as number of microseconds.
  int64_t to_microseconds() const {
    return static_cast<int64_t>(
               unsigned_microsec(hour(), minute(), second(), microsecond())) *
           (is_negative() ? -1 : 1);
  }
  /**
    base100 representation without microsecond, but rounded
    '-12:34:56.999999' is returned as -123457
  */
  int64_t to_int_rounded() const;
  /**
    base100 representation without microsecond, '-12:34:56.999999' is
    returned as -123456
  */
  int64_t to_int_truncated() const;
  /**
    base100 representation with microseconds, returned as double precision float
  */
  double to_double() const;

  /// @returns true if value is adjusted to number of decimals in fraction
  bool is_adjusted(uint32_t decimals) const;

  /// @returns actual number of decimals in fraction
  uint32_t actual_decimals() const;

  // Mutators
  void adjust_fraction(uint32_t decimals, bool round);

  // Add nanoseconds to a time value, with rounding
  bool add_nanoseconds_round(const int64_t nanoseconds) {
    return nanoseconds < 0 ? add_microseconds((nanoseconds - 500) / 1000)
                           : add_microseconds((nanoseconds + 500) / 1000);
  }

  /**
    Convert server time value to storage engine interface format

    @param [out] ptr  The buffer to put value at.
    @param       dec  Precision.
  */
  void store_time(uint8_t *ptr, uint32_t dec) const;

  /**
    Convert from storage engine interface time format to server time value.

    @param   ptr  The pointer to read the value at.
    @param   dec  Precision.
    @param[out] time Returned time value
  */
  static void load_time(const uint8_t *ptr, uint32_t dec, Time_val *time);

  size_t to_string(char *buffer, uint32_t dec) const;

  std::string to_string() const;

 private:
  explicit Time_val(int64_t val) : m_value(val) {}

  /// Set microsecond part of time value
  void set_microsecond(uint32_t fraction) {
    m_value = (m_value & ~BITS_MICROSEC) |
              (is_negative() ? (0xFFFFFFU - fraction) : fraction);
  }

  bool add_seconds(int32_t seconds) {
    Time_val tv(seconds < 0, 0, 0, static_cast<uint8_t>(abs(seconds)), 0);
    add(tv, false);
    assert(is_valid());
    return false;
  }

  bool add_microseconds(int64_t mu) {
    int64_t signed_micro =
        unsigned_microsec(hour(), minute(), second(), microsecond());
    if (is_negative()) signed_micro = -signed_micro;
    signed_micro += mu;
    bool negative = signed_micro < 0;
    uint64_t micro = negative ? -signed_micro : signed_micro;
    if (micro > MAX_TIME_MICROSEC) {
      return true;
    }
    uint32_t seconds = static_cast<uint32_t>(micro / TIME_MULT_SECOND);
    micro %= TIME_MULT_SECOND;
    *this = Time_val(negative, seconds, micro);
    assert(is_valid());
    return false;
  }

  bool is_valid() const {
    return hour() <= TIME_MAX_HOUR && minute() <= TIME_MAX_MINUTE &&
           second() <= TIME_MAX_SECOND && microsecond() <= TIME_MAX_MICROSEC &&
           m_value <= MAX_TIME_VALUE && m_value >= MIN_TIME_VALUE &&
           m_value != 0x7fffffffffff;
  }

  static uint32_t unsigned_seconds(uint32_t hour, uint32_t minute,
                                   uint32_t second) {
    return (hour * SECS_PER_HOUR) + (minute * SECS_PER_MIN) + second;
  }

  static uint64_t unsigned_microsec(uint32_t hour, uint32_t minute,
                                    uint32_t second, uint32_t microsec) {
    return (hour * TIME_MULT_HOUR) + (minute * TIME_MULT_MINUTE) +
           (second * TIME_MULT_SECOND) + microsec;
  }

  static constexpr const uint32_t TIME_MAX_HOUR = 838;
  static constexpr const uint32_t TIME_MAX_MINUTE = 59;
  static constexpr const uint32_t TIME_MAX_SECOND = 59;
  static constexpr const uint32_t TIME_MAX_MICROSEC = 999999;

  static constexpr uint64_t TIME_MULT_SECOND = 1000000;
  static constexpr uint64_t TIME_MULT_MINUTE = 60000000;
  static constexpr uint64_t TIME_MULT_HOUR = 3600000000;

  static constexpr uint64_t BITS_MICROSEC = 0x0000000000FFFFFF;
  static constexpr uint64_t BITS_SECOND = 0x000000003F000000;
  static constexpr uint64_t BITS_MINUTE = 0x0000000FC0000000;
  static constexpr uint64_t BITS_HOUR = 0x00007FF000000000;
  static constexpr uint64_t BITS_SIGN = 0x0000800000000000;
  static constexpr int TIME_SHIFT_SECOND = 24;
  static constexpr int TIME_SHIFT_MINUTE = 30;
  static constexpr int TIME_SHIFT_HOUR = 36;

  static constexpr uint64_t MAX_TIME_VALUE =
      BITS_SIGN | (static_cast<uint64_t>(TIME_MAX_HOUR) << TIME_SHIFT_HOUR) |
      (static_cast<uint64_t>(TIME_MAX_MINUTE) << TIME_SHIFT_MINUTE) |
      (static_cast<uint64_t>(TIME_MAX_SECOND) << TIME_SHIFT_SECOND);

  static constexpr uint64_t MIN_TIME_VALUE =
      (static_cast<uint64_t>(-static_cast<int64_t>(MAX_TIME_VALUE)) &
       (BITS_MICROSEC | BITS_SECOND | BITS_MINUTE | BITS_HOUR)) -
      1;

  // 838:59:59.000000
  static constexpr uint64_t MAX_TIME_MICROSEC =
      (TIME_MAX_HOUR * TIME_MULT_HOUR) + (TIME_MAX_MINUTE * TIME_MULT_MINUTE) +
      (TIME_MAX_SECOND * TIME_MULT_SECOND);

  /**
    A TIME value is stored in bit coded fields in a 64 bit unsigned value.
    The format is efficient for comparison, storage, retrieval and movement.
    The fields are stored in two's complement, but with a sign bit set for
    non-negative values. This means that values can be compared using regular
    unsigned integer logic.
    Format:
      Bits  0-23: microseconds (0-999999)
      Bits 24-29: seconds (0-59)
      Bits 30-35: minutes (0-59)
      Bits 36-46: hours (0-838) (Theoretical range up to 2047)
      Bits 47-47: Sign (1 for positive value, 0 for negative value)
  */
  uint64_t m_value = 0xffffffffffffffff;
};

class Datetime_val : public MYSQL_TIME {
 public:
  Datetime_val() {
    year = 0;
    month = 0;
    day = 0;
    hour = 0;
    minute = 0;
    second = 0;
    second_part = 0;
    neg = false;
    time_type = MYSQL_TIMESTAMP_DATETIME;
    time_zone_displacement = 0;
  }
  Datetime_val(uint32_t year_arg, uint32_t month_arg, uint32_t day_arg,
               uint32_t hour_arg, uint32_t minute_arg, uint32_t second_arg,
               uint32_t micro_arg) {
    year = year_arg;
    month = month_arg;
    day = day_arg;
    hour = hour_arg;
    minute = minute_arg;
    second = second_arg;
    second_part = micro_arg;
    neg = false;
    time_type = MYSQL_TIMESTAMP_DATETIME;
    time_zone_displacement = 0;
  }
  Datetime_val(uint32_t year_arg, uint32_t month_arg, uint32_t day_arg,
               uint32_t hour_arg, uint32_t minute_arg, uint32_t second_arg,
               uint32_t micro_arg, int32_t time_zone_displacement_arg) {
    year = year_arg;
    month = month_arg;
    day = day_arg;
    hour = hour_arg;
    minute = minute_arg;
    second = second_arg;
    second_part = micro_arg;
    neg = false;
    time_type = MYSQL_TIMESTAMP_DATETIME_TZ;
    time_zone_displacement = time_zone_displacement_arg;
  }
  Datetime_val(uint32_t year_arg, uint32_t month_arg, uint32_t day_arg) {
    year = year_arg;
    month = month_arg;
    day = day_arg;
    hour = 0;
    minute = 0;
    second = 0;
    second_part = 0;
    neg = false;
    time_type = MYSQL_TIMESTAMP_DATE;
    time_zone_displacement = 0;
  }
};

class Date_val : public Datetime_val {
 public:
  Date_val() = default;

  Date_val(uint32_t year_arg, uint32_t month_arg, uint32_t day_arg)
      : Datetime_val(year_arg, month_arg, day_arg) {}
};

/**
  @} (end of ingroup MY_TEMPORAL)
*/
#endif  // MY_TEMPORAL_H
