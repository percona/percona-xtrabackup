/* Copyright (c) 2018 Percona LLC and/or its affiliates. All rights reserved.

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

#include "vault_parser.h"
#include <algorithm>
#include <sstream>
#include <vector>
#include "vault_base64.h"
#include "vault_key.h"

namespace keyring {

bool Vault_parser::retrieve_tag_value(const Secure_string &payload,
                                      const Secure_string &tag,
                                      const char opening_bracket,
                                      const char closing_bracket,
                                      Secure_string *value) {
  size_t opening_bracket_pos, closing_bracket_pos, tag_pos = payload.find(tag);
  if (tag_pos == Secure_string::npos) {
    value->clear();
    return false;
  }

  if ((opening_bracket_pos = (payload.find(opening_bracket, tag_pos))) ==
          Secure_string::npos ||
      (closing_bracket_pos = (payload.find(
           closing_bracket, opening_bracket_pos))) == Secure_string::npos) {
    std::ostringstream err_ss("Could not parse tag ");
    err_ss << tag << " from Vault's response.";
    logger->log(MY_ERROR_LEVEL, err_ss.str().c_str());
    return true;
  }

  *value = payload.substr(opening_bracket_pos,
                          closing_bracket_pos - opening_bracket_pos + 1);
  value->erase(std::remove(value->begin(), value->end(), '\n'), value->end());
  return false;
}

bool Vault_parser::retrieve_list(const Secure_string &payload,
                                 const Secure_string &list_name,
                                 Secure_string *list) {
  return retrieve_tag_value(payload, list_name, '[', ']', list);
}

bool Vault_parser::retrieve_map(const Secure_string &payload,
                                const Secure_string &map_name,
                                Secure_string *map) {
  return retrieve_tag_value(payload, map_name, '{', '}', map);
}

bool Vault_parser::retrieve_tokens_from_list(const Secure_string &list,
                                             Tokens *tokens) {
  std::size_t token_start = 0, token_end = 0;
  while ((token_start = list.find('\"', token_end)) != Secure_string::npos &&
         token_start < list.size()) {
    if ((token_end = list.find('\"', token_start + 1)) == Secure_string::npos) {
      tokens->clear();
      return true;
    }
    tokens->push_back(
        list.substr(token_start + 1, token_end - token_start - 1));
    ++token_end;
  }
  return false;
}

const size_t Vault_parser::start_tag_length = strlen(":\"");

bool Vault_parser::retrieve_value_from_map(const Secure_string &map,
                                           const Secure_string &key,
                                           Secure_string *value) {
  size_t key_tag_pos = Secure_string::npos,
         value_start_pos = Secure_string::npos,
         value_end_pos = Secure_string::npos;
  bool was_error = false;
  const size_t start_tag_length = strlen(":\"");

  if ((key_tag_pos = map.find(key)) != Secure_string::npos &&
      (value_start_pos = map.find(":\"", key_tag_pos)) != Secure_string::npos &&
      (value_end_pos = map.find("\"", value_start_pos + start_tag_length)) !=
          Secure_string::npos) {
    value_start_pos += start_tag_length;
    DBUG_ASSERT(value_end_pos > 0);
    value_end_pos--;  // due to closing "
    *value = map.substr(value_start_pos, (value_end_pos - value_start_pos + 1));
  } else
    was_error = true;

  if (was_error || value->empty()) {
    std::ostringstream err_ss;
    err_ss << "Could not parse " << key << " tag for a key.";
    logger->log(MY_ERROR_LEVEL, err_ss.str().c_str());
    return true;
  }
  return false;
}

bool Vault_parser::parse_errors(const Secure_string &payload,
                                Secure_string *errors) {
  return retrieve_list(payload, "errors", errors);
}

bool Vault_parser::parse_keys(const Secure_string &payload,
                              Vault_keys_list *keys) {
  /* payload is built as follows:
   * (...)"data":{"keys":["keysignature","keysignature"]}(...)
   * We need to retrieve keys signatures from it
   */
  Tokens key_tokens;
  Secure_string keys_list;
  if (retrieve_list(payload, "keys", &keys_list) || keys_list.empty() ||
      retrieve_tokens_from_list(keys_list, &key_tokens)) {
    logger->log(MY_ERROR_LEVEL,
                "Could not parse keys tag with keys list from Vault.");
    return true;
  }
  KeyParameters key_parameters;
  for (Tokens::const_iterator iter = key_tokens.begin();
       iter != key_tokens.end(); ++iter) {
    if (parse_key_signature(*iter, &key_parameters)) {
      logger->log(MY_WARNING_LEVEL,
                  "Could not parse key's signature, skipping the key.");
      continue;  // found incorrect key, skipping it
    }
    keys->push_back(new Vault_key(key_parameters.key_id.c_str(), NULL,
                                  key_parameters.user_id.c_str(), NULL, 0));
  }
  return false;
}

bool Vault_parser::parse_key_signature(
    const Secure_string &base64_key_signature, KeyParameters *key_parameters) {
  // key_signature= lengthof(key_id)||_||key_id||lengthof(user_id)||_||user_id
  static const Secure_string digits("0123456789");
  Secure_string key_signature;
  if (Vault_base64::decode(base64_key_signature, &key_signature)) {
    logger->log(MY_WARNING_LEVEL, "Could not decode base64 key's signature");
    return true;
  }

  size_t next_pos_to_start_from = 0;
  for (int i = 0; i < 2; ++i) {
    size_t key_id_pos =
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

bool Vault_parser::parse_key_data(const Secure_string &payload, IKey *key) {
  Secure_string map, type, value;
  if (retrieve_map(payload, "data", &map) ||
      retrieve_value_from_map(map, "type", &type) ||
      retrieve_value_from_map(map, "value", &value))
    return true;

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

}  // namespace keyring
