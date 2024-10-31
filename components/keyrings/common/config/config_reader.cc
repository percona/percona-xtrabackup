/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#include <fstream> /* std::ifstream */
#include <memory>

#include <components/keyrings/common/component_helpers/include/service_requirements.h>
#include <mysql/components/services/log_builtins.h> /* LogComponentErr */

#include <mysqld_error.h>
#include <rapidjson/error/en.h>
#include <rapidjson/istreamwrapper.h> /* IStreamWrapper */
#include <iostream>

namespace keyring_common::config {

<<<<<<< HEAD
inline Config_reader::Config_reader(const std::string config_file_path)
    : config_file_path_(config_file_path), data_(), valid_(false) {
||||||| dc86e412f18
Config_reader::Config_reader(const std::string config_file_path)
    : config_file_path_(config_file_path), data_(), valid_(false) {
=======
Config_reader::Config_reader(std::string config_file_path)
    : config_file_path_(std::move(config_file_path)), valid_(false) {
>>>>>>> mysql-9.1.0
  std::ifstream file_stream(config_file_path_);
<<<<<<< HEAD
  if (!file_stream.is_open()) {
    LogComponentErr(ERROR_LEVEL, ER_KEYRING_COMPONENT_NO_CONFIG,
                    config_file_path_.c_str());
    return;
  }
||||||| dc86e412f18
  if (!file_stream.is_open()) return;
=======
  if (!file_stream.is_open()) {
    err_ = "cannot read config file " + config_file_path_;
    return;
  }
>>>>>>> mysql-9.1.0
  rapidjson::IStreamWrapper json_fstream_reader(file_stream);
<<<<<<< HEAD
  data_.ParseStream(json_fstream_reader);
  valid_ = !data_.HasParseError();
  if (!valid_) {
    LogComponentErr(ERROR_LEVEL, ER_KEYRING_COMPONENT_CONFIG_PARSE_FAILED,
                    rapidjson::GetParseError_En(data_.GetParseError()),
                    data_.GetErrorOffset());
  }
  file_stream.close();
||||||| dc86e412f18
  valid_ = !data_.ParseStream(json_fstream_reader).HasParseError();
  file_stream.close();
=======
  if (data_.ParseStream(json_fstream_reader).HasParseError()) {
    err_ = "config file " + config_file_path_ + " has not valid format";
    return;
  }
  valid_ = true;
>>>>>>> mysql-9.1.0
}

<<<<<<< HEAD
bool Config_reader::has_element(const std::string &element_name) {
  return !valid_ || !data_.HasMember(element_name);
}

bool Config_reader::is_number(const std::string &element_name) {
  return !valid_ || !data_.HasMember(element_name) ||
         !data_[element_name].IsNumber();
}

bool Config_reader::is_string(const std::string &element_name) {
  return !valid_ || !data_.HasMember(element_name) ||
         !data_[element_name].IsString();
}

}  // namespace config
}  // namespace keyring_common
||||||| dc86e412f18
}  // namespace config
}  // namespace keyring_common
=======
}  // namespace keyring_common::config
>>>>>>> mysql-9.1.0
