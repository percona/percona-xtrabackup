/* Copyright (c) 2018, 2021 Percona LLC and/or its affiliates. All rights
   reserved.

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

#include "vault_credentials_parser.h"

#include <fstream>
#include <limits>
#include <map>
#include <sstream>
#include "plugin/keyring/common/logger.h"
#include "plugin/keyring/common/secure_string.h"
#include "vault_credentials.h"

#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/lexical_cast.hpp>

namespace {
const char option_key_value_delimiter = '=';
const char mount_point_version_auto[] = "AUTO";
const char http_protocol_prefix[] = "http://";
const char https_protocol_prefix[] = "https://";
const char mount_point_path_delimiter = '/';

using option_value_container = std::map<std::string, keyring::Secure_string>;

#define X_MACRO_SEQ()                                                 \
  X_MACRO(vault_url), X_MACRO(secret_mount_point), X_MACRO(vault_ca), \
      X_MACRO(token), X_MACRO(secret_mount_point_version)

#define X_MACRO(VALUE) #VALUE
const std::string option_labels[] = {X_MACRO_SEQ()};
#undef X_MACRO

#define X_MACRO(VALUE) option_##VALUE
enum option_type { X_MACRO_SEQ() };
#undef X_MACRO
#undef X_MACRO_SEQ

bool is_valid_option(const std::string &option) {
  const std::string *bg = option_labels;
  const std::string *en = bg + sizeof(option_labels) / sizeof(option_labels[0]);
  return std::find(bg, en, option) != en;
}

bool parse_line(keyring::ILogger *logger, std::size_t line_number,
                const keyring::Secure_string &line,
                option_value_container &option_values) {
  keyring::Secure_string trimmed_line = boost::trim_copy(line);
  if (trimmed_line.empty()) return false;

  std::size_t delimiter_pos = trimmed_line.find(option_key_value_delimiter);
  std::ostringstream err_ss;

  if (delimiter_pos == std::string::npos) {
    err_ss << "Could not parse credential file. Cannot find delimiter ("
           << option_key_value_delimiter << " in line: ";
    err_ss << line_number << '.';
    logger->log(MY_ERROR_LEVEL, err_ss.str().c_str());
    return true;
  }
  std::string option(trimmed_line.c_str(), delimiter_pos);
  boost::trim_right(option);

  if (!is_valid_option(option)) {
    err_ss << "Could not parse credential file. Unknown option \"" << option
           << "\" in line: ";
    err_ss << line_number << '.';
    return true;
  }
  std::pair<option_value_container::iterator, bool> res =
      option_values.insert(std::make_pair(option, ""));
  if (!res.second) {  // repeated option in file
    err_ss << "Could not parse credential file. Seems that value for option "
           << option;
    err_ss << " has been specified more than once in line: " << line_number
           << '.';
    logger->log(MY_ERROR_LEVEL, err_ss.str().c_str());
    return true;
  }
  keyring::Secure_string value = trimmed_line.substr(delimiter_pos + 1);
  boost::trim_left(value);

  if (value.empty()) {
    err_ss << "Could not parse credential file. Seems there is no value "
              "specified ";
    err_ss << "for option " << option << " in line: " << line_number << '.';

    logger->log(MY_ERROR_LEVEL, err_ss.str().c_str());
    return true;
  }
  res.first->second.swap(value);
  return false;
}

}  // anonymous namespace

namespace keyring {

bool Vault_credentials_parser::parse(
    const std::string &conf_file_path,
    Vault_credentials &vault_credentials) const {
  std::ifstream ifs(conf_file_path.c_str());
  if (!ifs.is_open()) {
    std::string err_msg = "Could not open credentials file '";
    err_msg += conf_file_path;
    err_msg += "\'.";
    logger_->log(MY_ERROR_LEVEL, err_msg.c_str());
    return true;
  }
  const char *const cannot_identify_msg =
      "Could not identify credentials file size.";
  if (!ifs.seekg(0, std::ios_base::end)) {
    logger_->log(MY_ERROR_LEVEL, cannot_identify_msg);
    return true;
  }
  std::ifstream::pos_type file_size = ifs.tellg();
  if (!ifs) {
    logger_->log(MY_ERROR_LEVEL, cannot_identify_msg);
    return true;
  }
  // on some platforms when a directory (say, "/") is opened
  // tellg() may return max positive signed integer value
  if (file_size == static_cast<std::streamoff>(-1) ||
      file_size == static_cast<std::streamoff>(
                       std::numeric_limits<std::streamoff>::max())) {
    logger_->log(MY_ERROR_LEVEL, cannot_identify_msg);
    return true;
  }
  if (file_size == 0) {
    logger_->log(MY_ERROR_LEVEL, "Credentials file is empty.");
    return true;
  }
  if (file_size > static_cast<std::streamoff>(max_file_size)) {
    logger_->log(MY_ERROR_LEVEL, "Credentials file is too large.");
    return true;
  }
  if (!ifs.seekg(0, std::ios_base::beg)) {
    logger_->log(MY_ERROR_LEVEL, "Could not reset credentials file position.");
    return true;
  }

  Secure_stringstream buffer;
  buffer << ifs.rdbuf();
  if (!ifs || !buffer) {
    logger_->log(MY_ERROR_LEVEL, "Could not read credentials file content.");
    return true;
  }
  ifs.close();

  option_value_container option_values;
  std::size_t line_number = 1;
  Secure_string line;
  while (!buffer.eof()) {
    std::getline(buffer, line);
    if (parse_line(logger_, line_number, line, option_values)) return true;
    ++line_number;
  }

  option_value_container::const_iterator fnd;
  option_value_container::const_iterator en = option_values.end();
  const std::string *option_name;
  Secure_string empty_string;
  std::ostringstream err_ss;

  // "vault_url": string
  option_name = &option_labels[option_vault_url];
  fnd = option_values.find(*option_name);
  if (fnd == en) {
    err_ss << "Could not read " << *option_name
           << " from the configuration file.";
    logger_->log(MY_ERROR_LEVEL, err_ss.str().c_str());
    return true;
  }
  const Secure_string &vault_url = fnd->second;
  bool vault_url_is_https = false;
  bool vault_url_is_http = boost::starts_with(vault_url, http_protocol_prefix);
  if (!vault_url_is_http)
    vault_url_is_https = boost::starts_with(vault_url, https_protocol_prefix);
  if (!vault_url_is_http && !vault_url_is_https) {
    err_ss << *option_name << " must be either " << http_protocol_prefix
           << " or " << https_protocol_prefix << " URL.";
    logger_->log(MY_ERROR_LEVEL, err_ss.str().c_str());
    return true;
  }

  // "secret_mount_point": string
  option_name = &option_labels[option_secret_mount_point];
  fnd = option_values.find(*option_name);
  if (fnd == en) {
    err_ss << "Could not read " << *option_name
           << " from the configuration file.";
    logger_->log(MY_ERROR_LEVEL, err_ss.str().c_str());
    return true;
  }
  const Secure_string &secret_mount_point = fnd->second;
  if (secret_mount_point[0] == mount_point_path_delimiter) {
    err_ss << *option_name << " must not start with "
           << mount_point_path_delimiter << ".";
    logger_->log(MY_ERROR_LEVEL, err_ss.str().c_str());
    return true;
  }
  if (secret_mount_point[secret_mount_point.size() - 1] ==
      mount_point_path_delimiter) {
    err_ss << *option_name << " must not end with "
           << mount_point_path_delimiter << ".";
    logger_->log(MY_ERROR_LEVEL, err_ss.str().c_str());
    return true;
  }

  // "vault_ca": string, optional
  option_name = &option_labels[option_vault_ca];
  fnd = option_values.find(*option_name);
  const Secure_string &vault_ca = (fnd != en ? fnd->second : empty_string);

  // "token": string
  option_name = &option_labels[option_token];
  fnd = option_values.find(*option_name);
  if (fnd == en) {
    err_ss << "Could not read " << *option_name
           << " from the configuration file.";
    logger_->log(MY_ERROR_LEVEL, err_ss.str().c_str());
    return true;
  }
  const Secure_string &token = fnd->second;

  // "secret_mount_point_version": enum[1|2|AUTO], optional
  option_name = &option_labels[option_secret_mount_point_version];
  fnd = option_values.find(*option_name);
  // by default, when no "secret_mount_point_version" is specified explicitly,
  // it is considered to be AUTO
  Vault_version_type secret_mount_point_version = Vault_version_auto;
  if (fnd != en) {
    boost::uint32_t extracted_version = 0;
    const Secure_string &secret_mount_point_version_raw = fnd->second;
    if (secret_mount_point_version_raw == mount_point_version_auto) {
      secret_mount_point_version = Vault_version_auto;
    } else if (boost::conversion::try_lexical_convert(
                   secret_mount_point_version_raw, extracted_version)) {
      switch (extracted_version) {
        case 1:
          secret_mount_point_version = Vault_version_v1;
          break;
        case 2:
          secret_mount_point_version = Vault_version_v2;
          break;
        default: {
          err_ss << *option_name
                 << " in the configuration file must be either 1 or 2.";
          logger_->log(MY_ERROR_LEVEL, err_ss.str().c_str());
          return true;
        }
      }
    } else {
      err_ss << *option_name
             << " in the configuration file is neither AUTO nor a numeric "
                "value.";
      logger_->log(MY_ERROR_LEVEL, err_ss.str().c_str());
      return true;
    }
  }

  // checks for combination op options
  if (!vault_ca.empty() && vault_url_is_http) {
    err_ss << option_labels[option_vault_ca] << " is specified but "
           << option_labels[option_vault_url] << " is " << http_protocol_prefix
           << ".";
    logger_->log(MY_ERROR_LEVEL, err_ss.str().c_str());
    return true;
  }
  if (vault_ca.empty() && vault_url_is_https) {
    err_ss << option_labels[option_vault_ca] << " is not specified but "
           << option_labels[option_vault_url] << " is " << https_protocol_prefix
           << ". "
           << "Please make sure that Vault's CA certificate is trusted by "
              "the machine from "
           << "which you intend to connect to Vault.";

    logger_->log(MY_WARNING_LEVEL, err_ss.str().c_str());
  }

  Vault_credentials local_vault_credentials(vault_url, secret_mount_point,
                                            vault_ca, token,
                                            secret_mount_point_version);
  vault_credentials.swap(local_vault_credentials);
  return false;
}

}  // namespace keyring
