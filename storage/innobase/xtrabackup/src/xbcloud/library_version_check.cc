/******************************************************
Copyright (c) 2025 Percona LLC and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

*******************************************************/

#include "library_version_check.h"
#include <curl/curl.h>
#include <optional>
#include <string>
#include <tuple>
#include "msg.h"

namespace xbcloud {

// Semantic version ([major].[minor].[patch]).
using version = std::tuple<unsigned int, unsigned int, unsigned int>;

static std::string version_to_string(const version &ver) {
  return std::to_string(std::get<0>(ver)) + "." +
         std::to_string(std::get<1>(ver)) + "." +
         std::to_string(std::get<2>(ver));
}

static std::optional<std::string> check_curl_version() {
  auto *curl_ver = curl_version_info(CURLVERSION_NOW);
  if (!curl_ver) {
    return "Failed to detect libcurl version.";
  }

  auto ver = version((curl_ver->version_num >> 16) & 0xff,
                     (curl_ver->version_num >> 8) & 0xff,
                     curl_ver->version_num & 0xff);

  if ((ver == version(7, 64, 0)) ||
      (ver >= version(8, 11, 1) && ver < version(8, 12, 0))) {
    return "Unsupported libcurl version: " + version_to_string(ver);
  }

  return {};
}

void check_library_versions() {
  bool warning_found = false;

  auto warn = [&warning_found](const std::optional<std::string> &info) {
    if (info) {
      warning_found = true;
      msg_ts("Dependency warning: %s\n", info->c_str());
    }
  };

  warn(check_curl_version());

  if (warning_found) {
    msg_ts(
        "Please consult Percona XtraBackup documentation about incompatible "
        "dependencies.\n");
  }
}

}  // namespace xbcloud
