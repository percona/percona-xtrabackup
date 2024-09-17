/******************************************************
Copyright (c) 2019, 2021 Percona LLC and/or its affiliates.

Aux functions used by xbcloud.

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

#ifndef XBCLOUD_UTIL_H
#define XBCLOUD_UTIL_H

#include <algorithm>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>

#include <base64.h>
#include <math.h>

namespace xbcloud {

#define TRACE(...)                           \
  {                                          \
    if (false) fprintf(stderr, __VA_ARGS__); \
  }

template <typename T, typename... Args>
std::unique_ptr<T> make_unique(Args &&... args) {
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

static inline void ltrim(std::string &s) {
  s.erase(s.begin(), std::find_if(s.begin(), s.end(),
                                  [](int ch) { return !std::isspace(ch); }));
}

static inline void rtrim(std::string &s) {
  s.erase(std::find_if(s.rbegin(), s.rend(),
                       [](int ch) { return !std::isspace(ch); })
              .base(),
          s.end());
}

static inline void trim(std::string &s) {
  ltrim(s);
  rtrim(s);
}

static inline void rtrim_slashes(std::string &s) {
  s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) { return ch != '/'; })
              .base(),
          s.end());
}

static inline void to_lower(std::string &s) {
  s.resize(s.length());
  std::transform(s.begin(), s.end(), s.begin(), ::tolower);
}

static inline bool starts_with(std::string const &value,
                               std::string const &prefix) {
  if (prefix.size() > value.size()) return false;
  return std::equal(prefix.begin(), prefix.end(), value.begin());
}

static inline bool ends_with(std::string const &value,
                             std::string const &suffix) {
  if (suffix.size() > value.size()) return false;
  return std::equal(suffix.rbegin(), suffix.rend(), value.rbegin());
}

static inline std::pair<std::string, std::string> parse_http_header(
    const std::string &header) {
  auto colon_index = header.find(':');
  if (colon_index != std::string::npos) {
    auto k = header.substr(0, colon_index);
    auto v = header.substr(colon_index + 1);
    trim(k);
    trim(v);
    return std::make_pair(k, v);
  }
  auto r = header;
  trim(r);
  return std::make_pair(r, std::string());
}

inline ulong get_exponential_backoff(int count, uint64_t max_backoff) {
  uint64_t delay = pow(2, count) * 1000;
  int random = (rand() % 1000) + 1;
  return std::min(delay + random, max_backoff);
}

inline std::string canonicalize_http_header_value(const std::string &s) {
  std::string r = s;

  /* replace multiple spaces with single space */
  auto new_end = std::unique(r.begin(), r.end(), [](char lhs, char rhs) {
    return rhs == ' ' && lhs == ' ';
  });
  r.erase(new_end, r.end());

  /* trim trailing and leading spaces */
  trim(r);

  return r;
}

template <typename T>
std::string base64_encode(const T &s) {
  uint64 encoded_size = ::base64_needed_encoded_length(s.size());
  std::unique_ptr<char[]> buf(new char[encoded_size]);

  if (::base64_encode(&s[0], s.size(), buf.get()) != 0) {
    return std::string();
  }

  return std::string(buf.get());
}

template <typename T>
std::string base64_decode(const T &s) {
  uint64 max_decoded_size = ::base64_needed_decoded_length(s.size());
  std::unique_ptr<char[]> buf(new char[max_decoded_size]);

  auto decoded_size = ::base64_decode(&s[0], s.size(), buf.get(), NULL, 0);
  if (decoded_size <= 0) {
    return std::string();
  }

  return std::string(buf.get(), decoded_size);
}

template <typename T>
std::string hex_encode(const T &s) {
  std::stringstream ss;
  for (size_t i = 0; i < s.size(); i++) {
    ss << std::hex << std::setw(2) << std::setfill('0')
       << (int)(unsigned char)s[i];
  }
  return ss.str();
}

}  // namespace xbcloud

#endif  // XBCLOUD_UTIL_H
