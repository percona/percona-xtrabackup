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

#include <algorithm>
#include <sstream>
#include <vector>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/lexical_cast/try_lexical_convert.hpp>
#include <boost/optional.hpp>

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
// if we build within the server, it will set RAPIDJSON_NO_SIZETYPEDEFINE
// globally and require to include my_rapidjson_size_t.h
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "plugin/keyring/common/logger.h"
#include "vault_base64.h"
#include "vault_parser_composer.h"

namespace {
const char errors_key[] = "errors";
const char data_key[] = "data";
const char keys_key[] = "keys";
const char type_key[] = "type";
const char value_key[] = "value";
const char max_versions_key[] = "max_versions";
const char cas_required_key[] = "cas_required";
const char delete_version_after_key[] = "delete_version_after";

}  // anonymous namespace

namespace keyring {

bool Vault_parser_composer::parse_mount_point_config(
    const Secure_string &config_payload, std::size_t &max_versions,
    bool &cas_required, Optional_secure_string &delete_version_after) {
  /*
  {
    "request_id": "a2c9306a-7f82-6a59-ebfa-bc6142d66c39",
    "lease_id": "",
    "renewable": false,
    "lease_duration": 0,
    "data": {
      "max_versions": 0,
      "cas_required": false,
      "delete_version_after": "0s"
    },
    "wrap_info": null,
    "warnings": null,
    "auth": null
  }
  */
  rapidjson::Document doc;
  doc.Parse(config_payload.c_str());
  const rapidjson::Document &cdoc = doc;
  if (cdoc.HasParseError()) {
    logger->log(MY_ERROR_LEVEL,
                "Could not parse Vault Server mount config response.");
    return true;
  }
  if (!cdoc.IsObject()) {
    logger->log(MY_ERROR_LEVEL,
                "Vault Server mount config response is not an Object");
    return true;
  }

  rapidjson::Document::ConstMemberIterator it = cdoc.FindMember(data_key);
  if (it == cdoc.MemberEnd()) {
    logger->log(
        MY_ERROR_LEVEL,
        "Vault Server mount config response does not have \"data\" member");
    return true;
  }

  const rapidjson::Value &data_node = it->value;
  if (!data_node.IsObject()) {
    logger->log(
        MY_ERROR_LEVEL,
        "Vault Server mount config response[\"data\"] is not an Object");
    return true;
  }

  it = data_node.FindMember(max_versions_key);
  if (it == data_node.MemberEnd()) {
    logger->log(MY_ERROR_LEVEL,
                "Vault Server mount config response[\"data\"] does not have "
                "\"max_versions\" member");
    return true;
  }

  const rapidjson::Value &max_versions_node = it->value;
  if (!max_versions_node.IsUint()) {
    logger->log(MY_ERROR_LEVEL,
                "Vault Server mount config "
                "response[\"data\"][\"max_versions\"] is not an Unsigned "
                "Integer");
    return true;
  }
  std::size_t local_max_versions = max_versions_node.GetUint();

  it = data_node.FindMember(cas_required_key);
  if (it == data_node.MemberEnd()) {
    logger->log(MY_ERROR_LEVEL,
                "Vault Server mount config response[\"data\"] does not have "
                "\"cas_required\" member");
    return true;
  }

  const rapidjson::Value &cas_required_node = it->value;
  if (!cas_required_node.IsBool()) {
    logger->log(MY_ERROR_LEVEL,
                "Vault Server mount config "
                "response[\"data\"][\"cas_required\"] is not a Boolean");
    return true;
  }
  bool local_cas_required = cas_required_node.GetBool();

  Optional_secure_string local_delete_version_after;
  it = data_node.FindMember(delete_version_after_key);
  if (it != data_node.MemberEnd()) {
    const rapidjson::Value &delete_version_after_node = it->value;
    if (!delete_version_after_node.IsString()) {
      logger->log(MY_ERROR_LEVEL,
                  "Vault Server mount config "
                  "response[\"data\"][\"delete_version_after\"] is not a "
                  "String");
      return true;
    }
    local_delete_version_after = delete_version_after_node.GetString();
  }

  max_versions = local_max_versions;
  cas_required = local_cas_required;
  delete_version_after.swap(local_delete_version_after);
  return false;
}

bool Vault_parser_composer::parse_errors(const Secure_string &payload,
                                         Secure_string *errors) {
  rapidjson::Document doc;
  doc.Parse(payload.c_str());
  const rapidjson::Document &cdoc = doc;
  if (cdoc.HasParseError()) return true;
  if (!cdoc.IsObject()) return true;
  rapidjson::Document::ConstMemberIterator it = cdoc.FindMember(errors_key);
  if (it == cdoc.MemberEnd()) return false;

  const rapidjson::Value &errors_node = it->value;
  if (!errors_node.IsArray()) return true;

  Secure_ostringstream oss;
  for (std::size_t u = 0; u < errors_node.Size(); ++u) {
    if (u != 0) oss << '\n';
    const rapidjson::Value &first_error_node = errors_node[u];
    if (first_error_node.IsString()) {
      oss << first_error_node.GetString();
    } else {
      rapidjson::StringBuffer buffer;
      rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
      first_error_node.Accept(writer);
      oss << buffer.GetString();
    }
  }

  Secure_string res = oss.str();
  errors->swap(res);
  return false;
}

bool Vault_parser_composer::parse_keys(const Secure_string &payload,
                                       Vault_keys_list *keys) {
  /* payload is built as follows:
   * (...)"data":{"keys":["keysignature","keysignature"]}(...)
   * We need to retrieve keys signatures from it
   */
  rapidjson::Document doc;
  doc.Parse(payload.c_str());
  const rapidjson::Document &cdoc = doc;
  if (cdoc.HasParseError()) {
    logger->log(MY_ERROR_LEVEL, "Could not parse Vault Server response.");
    return true;
  }
  if (!cdoc.IsObject()) {
    logger->log(MY_ERROR_LEVEL, "Vault Server response is not an Object");
    return true;
  }

  rapidjson::Document::ConstMemberIterator it = cdoc.FindMember(data_key);
  if (it == cdoc.MemberEnd()) {
    logger->log(MY_ERROR_LEVEL,
                "Vault Server response does not have \"data\" member");
    return true;
  }

  const rapidjson::Value &data_node = it->value;
  if (!data_node.IsObject()) {
    logger->log(MY_ERROR_LEVEL,
                "Vault Server response[\"data\"] is not an Object");
    return true;
  }

  it = data_node.FindMember(keys_key);
  if (it == data_node.MemberEnd()) {
    logger->log(
        MY_ERROR_LEVEL,
        "Vault Server response[\"data\"] does not have \"keys\" member");
    return true;
  }

  const rapidjson::Value &keys_node = it->value;
  if (!keys_node.IsArray()) {
    logger->log(MY_ERROR_LEVEL,
                "Vault Server response[\"data\"][\"keys\"] is not an Array");
    return true;
  }

  // empty key arrays are OK here
  KeyParameters key_parameters;
  for (rapidjson::Value::ConstValueIterator array_it = keys_node.Begin(),
                                            array_en = keys_node.End();
       array_it != array_en; ++array_it) {
    const rapidjson::Value &array_element_node = *array_it;
    if (!array_element_node.IsString()) {
      logger->log(MY_WARNING_LEVEL,
                  "Vault Server response[\"data\"][\"keys\"][<index>] is not "
                  "a String");
    } else if (parse_key_signature(array_element_node.GetString(),
                                   &key_parameters)) {
      logger->log(MY_WARNING_LEVEL,
                  "Could not parse key's signature, skipping the key.");
    } else {
      keys->push_back(new Vault_key(key_parameters.key_id.c_str(), nullptr,
                                    key_parameters.user_id.c_str(), nullptr,
                                    0));
    }
  }
  return false;
}

static const char *const digits = "0123456789";
bool Vault_parser_composer::parse_key_signature(
    const Secure_string &base64_key_signature, KeyParameters *key_parameters) {
  // key_signature = lengthof(key_id)||_||key_id||lengthof(user_id)||_||user_id
  Secure_string key_signature;
  if (Vault_base64::decode(base64_key_signature, &key_signature)) {
    logger->log(MY_WARNING_LEVEL, "Could not decode base64 key's signature");
    return true;
  }

  std::size_t next_pos_to_start_from = 0;
  for (std::size_t i = 0; i < 2; ++i) {
    std::size_t key_id_pos =
        key_signature.find_first_not_of(digits, next_pos_to_start_from);
    if (key_id_pos == Secure_string::npos || key_signature[key_id_pos] != '_')
      return true;
    ++key_id_pos;
    Secure_string key_id_length =
        key_signature.substr(next_pos_to_start_from, key_id_pos);
    int key_l = atoi(key_id_length.c_str());
    if (key_l < 0 || key_l + key_id_pos > key_signature.length()) return true;
    (*key_parameters)[i] = key_signature.substr(key_id_pos, key_l);
    next_pos_to_start_from = key_id_pos + key_l;
  }
  return false;
}

bool Vault_parser_composer::parse_key_data(const Secure_string &payload,
                                           IKey *key,
                                           Vault_version_type vault_version) {
  rapidjson::Document doc;
  doc.Parse(payload.c_str());
  const rapidjson::Document &cdoc = doc;
  if (cdoc.HasParseError()) {
    logger->log(MY_ERROR_LEVEL, "Could not parse Vault Server response.");
    return true;
  }
  if (!cdoc.IsObject()) {
    logger->log(MY_ERROR_LEVEL, "Vault Server response is not an Object");
    return true;
  }

  rapidjson::Document::ConstMemberIterator it = cdoc.FindMember(data_key);
  if (it == cdoc.MemberEnd()) {
    logger->log(MY_ERROR_LEVEL,
                "Vault Server response does not have \"data\" member");
    return true;
  }

  const rapidjson::Value *data_node = &it->value;
  if (!data_node->IsObject()) {
    logger->log(MY_ERROR_LEVEL,
                "Vault Server response[\"data\"] is not an Object");
    return true;
  }

  if (vault_version == Vault_version_v2) {
    it = data_node->FindMember(data_key);
    if (it == data_node->MemberEnd()) {
      logger->log(
          MY_ERROR_LEVEL,
          "Vault Server response[\"data\"] does not have \"data\" member");
      return true;
    }
    data_node = &it->value;
    if (!data_node->IsObject()) {
      logger->log(MY_ERROR_LEVEL,
                  "Vault Server response data is not an Object");
      return true;
    }
  }

  it = data_node->FindMember(type_key);
  if (it == data_node->MemberEnd()) {
    logger->log(MY_ERROR_LEVEL,
                "Vault Server response data does not have \"type\" member");
    return true;
  }

  const rapidjson::Value &type_node = it->value;
  if (!type_node.IsString()) {
    logger->log(MY_ERROR_LEVEL,
                "Vault Server response data[\"type\"] is not a String");
    return true;
  }

  Secure_string type(type_node.GetString());

  it = data_node->FindMember(value_key);
  if (it == data_node->MemberEnd()) {
    logger->log(MY_ERROR_LEVEL,
                "Vault Server response data does not have \"value\" member");
    return true;
  }

  const rapidjson::Value &value_node = it->value;
  if (!value_node.IsString()) {
    logger->log(MY_ERROR_LEVEL,
                "Vault Server response data[\"value\"] is not a String");
    return true;
  }

  Secure_string value(value_node.GetString());

  char *decoded_key_data;
  uint64 decoded_key_data_length;
  if (Vault_base64::decode(value, &decoded_key_data,
                           &decoded_key_data_length)) {
    logger->log(MY_ERROR_LEVEL, "Could not decode base64 key's value");
    return true;
  }

  key->set_key_data(
      const_cast<uchar *>(reinterpret_cast<const uchar *>(decoded_key_data)),
      decoded_key_data_length);
  std::string key_type(type.c_str(), type.length());
  key->set_key_type(&key_type);

  return false;
}

bool Vault_parser_composer::compose_write_key_postdata(
    const Vault_key &key, const Secure_string &encoded_key_data,
    Vault_version_type vault_version, Secure_string &postdata) {
  rapidjson::Document doc;
  rapidjson::Document::AllocatorType &alloc = doc.GetAllocator();
  rapidjson::Value &root = doc.SetObject();

  rapidjson::Value kv_node(rapidjson::kObjectType);
  kv_node.MemberReserve(2, alloc);
  kv_node.AddMember(type_key,
                    rapidjson::StringRef(key.get_key_type_as_string()->c_str(),
                                         key.get_key_type_as_string()->size()),
                    alloc);
  kv_node.AddMember(
      value_key,
      rapidjson::StringRef(encoded_key_data.c_str(), encoded_key_data.size()),
      alloc);

  if (vault_version == Vault_version_v2)
    root.AddMember(rapidjson::Value::StringRefType(data_key), kv_node, alloc);
  else
    root.Swap(kv_node);

  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  doc.Accept(writer);

  postdata.assign(buffer.GetString());
  return false;
}

}  // namespace keyring
