/* Copyright (c) 2025 Oracle and/or its affiliates.

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

#include <unistd.h>
#include <cassert>
#include <climits>
#include <cstdint>
#include <fstream>
#include <optional>
#include <string_view>

#include "my_config.h"  // HAVE_UNISTD_H
#include "my_system_api.h"

/**
  @file components/library_mysys/my_system_api/my_system_api_cgroup.cc
  Functions to retrieve total physical memory and total number of logical CPUs
  available to the server by reading the limits set by cgroups
*/

namespace {
/** cgroup v1 path to file containing CPU quota */
constexpr std::string_view quota_path{"/sys/fs/cgroup/cpu/cpu.cfs_quota_us"};
/** cgroup v1 path to file containing CPU period */
constexpr std::string_view period_path{"/sys/fs/cgroup/cpu/cpu.cfs_period_us"};
/** cgroup v1 path to file containing Memory limits */
constexpr std::string_view mem_path_v1{
    "/sys/fs/cgroup/memory/memory.limit_in_bytes"};

/** cgroup v2 path to file containing CPU limts */
constexpr std::string_view cpu_path_v2{"/sys/fs/cgroup/cpu.max"};
/** cgroup v2 path to file containing Memory limits */
constexpr std::string_view mem_path_v2{"/sys/fs/cgroup/memory.max"};

/**
  Utility: Read the first line from the file specified in path and copy its
  contents into the arguments passed
  @param[in]   path  Path to file
  @param[out]  args  Pass the arguments that you expect to read from the file
  in the order of their appearance in the file
  @return true if able to read and parse the file, false otherwise
*/
template <typename... Args>
bool read_line_from_file(const std::string_view &path, Args &...args) {
  std::ifstream file(path.data());
  if (!file.is_open()) {
    return false;
  }

  (file >> ... >> args);

  /* Unable to parse contents or hit error */
  if (file.fail() || file.bad()) {
    return false;
  }
  return true;
}

/**
  Read CPU limits as if it were set by cgroup v1
  @return CPU limits set by cgroup v1 or std::nullopt on failure
  @note Return value of 0 implies no limits are set
*/
std::optional<uint32_t> cgroup_v1_cpu() {
  int32_t quota;
  uint32_t period;

  /* When no limits are set, -1 is written to the file quota_path */
  if (!read_line_from_file(quota_path, quota) || quota < 0) {
    return std::nullopt;
  }

  if (!read_line_from_file(period_path, period) || period == 0) {
    return std::nullopt;
  }
  return static_cast<uint32_t>(quota) / period;
}

/**
  Read memory limits as if it were set by cgroup v1
  @return Memory limits set by cgroup v1 or std::nullopt on failure
  @note Return value of 0 implies no limits are set
*/
std::optional<uint64_t> cgroup_v1_memory() {
  uint64_t memory;

  if (!read_line_from_file(mem_path_v1, memory)) {
    return std::nullopt;
  }

#ifdef HAVE_UNISTD_H
  const long page_size = sysconf(_SC_PAGESIZE);
  assert(page_size > 0);

  /* Default value of memory limit in cgroup v1 */
  const uint64_t default_limit = LONG_MAX - (LONG_MAX % page_size);

  /* Treat default value as no limits and return 0 */
  return (memory == default_limit) ? 0 : memory;
#endif
  return memory;
}

/**
  Read CPU limits as if it were set by cgroup v2
  @return CPU limits set by cgroup v2 or std::nullopt on failure
  @note Return value of 0 implies no limits are set
*/
std::optional<uint32_t> cgroup_v2_cpu() {
  uint32_t quota, period;

  if (!read_line_from_file(cpu_path_v2.data(), quota, period)) {
    return std::nullopt;
  }

  if (period == 0) {
    return std::nullopt;
  }
  return quota / period;
}

/**
  Read Memory limits as if it were set by cgroup v2
  @return Memory limits set by cgroup v2 or std::nullopt on failure
  @note Return value of 0 implies no limits are set
*/
std::optional<uint64_t> cgroup_v2_memory() {
  uint64_t memory;

  if (!read_line_from_file(mem_path_v2, memory)) {
    return std::nullopt;
  }
  return memory;
}
} /* namespace */

bool is_running_in_cgroup() {
  return (cgroup_v1_memory().has_value() && cgroup_v1_cpu().has_value()) ||
         (cgroup_v2_memory().has_value() && cgroup_v2_cpu().has_value());
}

bool does_cgroup_limit_resources() {
  bool memory_restricted = false;
  bool cpu_restricted = false;

  /* Value less than 1 indicates no limits are set */
  if (const auto v2_mem = cgroup_v2_memory(); v2_mem.has_value()) {
    memory_restricted = (v2_mem.value() >= 1);
  } else if (const auto v1_mem = cgroup_v1_memory(); v1_mem.has_value()) {
    memory_restricted = (v1_mem.value() >= 1);
  }

  /* Value less than 1 indicates no limits are set */
  if (const auto v2_cpu = cgroup_v2_cpu(); v2_cpu.has_value()) {
    cpu_restricted = (v2_cpu.value() >= 1);
  } else if (const auto v1_cpu = cgroup_v1_cpu(); v1_cpu.has_value()) {
    cpu_restricted = (v1_cpu.value() >= 1);
  }
  return cpu_restricted || memory_restricted;
}

uint64_t my_cgroup_mem_limit() {
  if (const auto v2_mem = cgroup_v2_memory(); v2_mem.has_value()) {
    return v2_mem.value();
  }

  if (const auto v1_mem = cgroup_v1_memory(); v1_mem.has_value()) {
    return v1_mem.value();
  }
  return 0;
}

uint32_t my_cgroup_vcpu_limit() {
  if (const auto v2_cpu = cgroup_v2_cpu(); v2_cpu.has_value()) {
    return v2_cpu.value();
  }

  if (const auto v1_cpu = cgroup_v1_cpu(); v1_cpu.has_value()) {
    return v1_cpu.value();
  }
  return 0;
}
