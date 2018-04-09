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

#include "system_keys_container.h"
#include <sstream>
#include <climits>

namespace keyring {

// System keys cannot have ':' in their name. We use ':' as a separator between
// system key's name and system key's version
const std::string System_keys_container::system_key_prefix = "percona_";

System_keys_container::~System_keys_container()
{
  for (System_key_id_to_system_key::iterator iter = system_key_id_to_system_key.begin();
       iter != system_key_id_to_system_key.end();
       ++iter)
    delete iter->second;
}

bool System_keys_container::is_system_key(IKey *key)
{
  std::string system_key_id;
  uint key_version;

  return is_system_key_with_version(key, system_key_id, key_version) ||
         is_system_key_without_version(key);
}

IKey* System_keys_container::get_latest_key_if_system_key_without_version(IKey *key)
{
  return (!key->get_user_id()->empty() || system_key_id_to_system_key.count(*key->get_key_id()) == 0)
         ? NULL
         : system_key_id_to_system_key[*key->get_key_id()];
}

bool System_keys_container::parse_system_key_id_with_version(std::string &key_id, std::string &system_key_id, uint &key_version)
{
  const std::size_t colon_position = key_id.find_last_of(':');

  if (colon_position == std::string::npos || colon_position == key_id.length() - 1)
    return true;

  system_key_id = key_id.substr(0, colon_position);
  const std::string version = key_id.substr(colon_position + 1,
                                            key_id.length() - colon_position);

  if (version.empty())
    return true;

  char *endptr = NULL;
  ulong ulong_key_version = strtoul(version.c_str(), &endptr, 10);
  if (ulong_key_version > UINT_MAX || endptr == NULL || *endptr != '\0')
   return true;

  key_version = static_cast<uint>(ulong_key_version);
  return false;
}

bool System_keys_container::is_system_key_without_version(IKey *key)
{
  return key->get_user_id()->empty() &&
         key->get_key_id()->compare(0, system_key_prefix.length(),
                                    system_key_prefix) == 0 &&
         key->get_key_id()->find_first_of(':') == std::string::npos; // system keys cannot have ':' in their name
}

bool System_keys_container::is_system_key_with_version(IKey *key, std::string &system_key_id, uint &key_version)
{
  return key->get_user_id()->empty() &&
         !parse_system_key_id_with_version(*key->get_key_id(), system_key_id, key_version) &&
         key->get_key_id()->compare(0, system_key_prefix.length(),
                                    system_key_prefix) == 0;
}

void System_keys_container::update_system_key(IKey* key, const std::string &system_key_id, uint key_version)
{
  if (system_key_id_to_system_key[system_key_id]->get_key_version() < key_version)
    system_key_id_to_system_key[system_key_id]->set_keyring_key(key, key_version);
}

bool System_keys_container::rotate_key_id_if_system_key_without_version(IKey *key)
{
  if (!is_system_key_without_version(key))
    return false;

  uint key_version = 0; // if we rotate from plain system key, we assign version 0 to it
  if (system_key_id_to_system_key.count(*key->get_key_id()) != 0)
  {
    key_version = system_key_id_to_system_key[*key->get_key_id()]->get_key_version();
    if (key_version == UINT_MAX)
    {
      logger->log(MY_ERROR_LEVEL, "System key cannot be rotated anymore, "
                                  "the maximum key version has been reached.");
      return true;
    }
    ++key_version;
  }
  std::ostringstream system_key_id_with_inc_version_ss;
  system_key_id_with_inc_version_ss << *key->get_key_id() << ':' << key_version;
  *(key->get_key_id()) = system_key_id_with_inc_version_ss.str();

  return false;
}

void System_keys_container::store_or_update_if_system_key_with_version(IKey *key)
{
  std::string system_key_id;
  uint key_version;

  if (is_system_key_with_version(key, system_key_id, key_version))
  {
    if (system_key_id_to_system_key.count(system_key_id) == 0) // add a new system key
      system_key_id_to_system_key.insert(std::make_pair<std::string, System_key_adapter*>(system_key_id, new System_key_adapter(key_version, key)));
    else
      update_system_key(key, system_key_id, key_version);
  }
}

} //namespace keyring
