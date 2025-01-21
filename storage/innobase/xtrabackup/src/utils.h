/******************************************************
Copyright (c) 2021 Percona LLC and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA

*******************************************************/

#ifndef XTRABACKUP_UTILS_H
#define XTRABACKUP_UTILS_H
#include <my_getopt.h>
#include <chrono>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace xtrabackup {
namespace utils {

/**
  Reads list of options from backup-my.cnf at a given path

  @param [in]  options       list of options to read
  @param [in]  path       path where backup-my.cnf is located

  @return false in case of error, true otherwise
*/
extern bool load_backup_my_cnf(my_option *options, char *path);

/**
  Read server_uuid from backup-my.cnf

  @return false in case of error, true otherwise
*/
bool read_server_uuid();

/* convert the version_str to version
@param[in] version_str version string like 8.0.22.debug
@return version_number like 80022 */
unsigned long get_version_number(std::string version_str);

/**
  Convert version_str to major.minor version

  @param[in] version_str version string like 8.2.0.debug
  @param[in/out] version version string like 8.2
  @return true in case of success, false otherwise
*/
bool get_major_minor_version(const std::string &version_str,
                             std::string &version);

unsigned long host_total_memory();
unsigned long host_free_memory();

/** Generates uuid
@return uuid string */
std::string generate_uuid();

using time = std::chrono::time_point<std::chrono::high_resolution_clock>;
using HighResTimePoint =
    std::chrono::time_point<std::chrono::high_resolution_clock>;
constexpr HighResTimePoint INVALID_TIME = HighResTimePoint::min();

std::string formatElapsedTime(std::chrono::nanoseconds elapsed);

// Helper to convert single values to strings
template <typename T>
std::string to_string(const T &value) {
  std::ostringstream oss;
  oss << value;
  return oss.str();
}

// Specialization for std::pair
template <typename T1, typename T2>
std::string to_string(const std::pair<T1, T2> &p) {
  std::ostringstream oss;
  oss << "{" << to_string(p.first) << ", " << to_string(p.second) << "}";
  return oss.str();
}

// Specialization for std::unordered_map
template <typename K, typename V>
std::string to_string(const std::unordered_map<K, V> &umap) {
  std::ostringstream oss;
  oss << "{\n";
  for (auto it = umap.begin(); it != umap.end(); ++it) {
    oss << to_string(it->first) << ": " << to_string(it->second) << "\n";
  }
  oss << "}";
  return oss.str();
}

// Specialization for std::vector
template <typename T>
std::string to_string(const std::vector<T> &vec) {
  std::ostringstream oss;
  oss << "[";
  for (size_t i = 0; i < vec.size(); ++i) {
    if (i > 0) {
      oss << ", ";
    }
    oss << to_string(vec[i]);
  }
  oss << "]";
  return oss.str();
}

// Specialization for std::unordered_set
template <typename T>
std::string to_string(const std::unordered_set<T> &uset) {
  std::ostringstream oss;
  oss << "{\n";
  for (auto it = uset.begin(); it != uset.end(); ++it) {
    oss << to_string(*it) << " ";
  }
  oss << "\n}";
  return oss.str();
}
}  // namespace utils
}  // namespace xtrabackup
#endif  // XTRABACKUP_UTILS_H
