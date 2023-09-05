/******************************************************
Copyright (c) 2016, 2021 Percona LLC and/or its affiliates.

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

#include <base64.h>
#include <dict0dict.h>
#include <my_aes.h>
#include <my_rnd.h>
#include <mysql/components/services/log_builtins.h>
#include <mysql/service_mysql_keyring.h>
#include <mysqld.h>
#include <sql/basic_istream.h>
#include <sql/basic_ostream.h>
#include <sql/log_event.h>
#include <sql/rpl_log_encryption.h>
#include <sql/server_component/mysql_server_keyring_lockable_imp.h>
#include <sql/sql_list.h>
#include <sql/sql_plugin.h>
#include <sql_plugin.h>
#include <ut0crc32.h>
#include "common.h"
#include "kdf.h"
#include "keyring_components.h"
#include "keyring_operations_helper.h"

#include "backup_mysql.h"
#include "keyring_plugins.h"
#include "rpl_log_encryption.h"
#include "utils.h"
#include "xb0xb.h"
#include "xbcrypt_common.h"
#include "xtrabackup.h"

#include <map>

struct tablespace_encryption_info {
  byte key[Encryption::KEY_LEN];
  byte iv[Encryption::KEY_LEN];
};

static std::map<ulint, tablespace_encryption_info> encryption_info;

extern st_mysql_plugin *mysql_optional_plugins[];
extern st_mysql_plugin *mysql_mandatory_plugins[];

const char *XTRABACKUP_KEYS_FILE = "xtrabackup_keys";
const char *XTRABACKUP_KEYS_MAGIC_V1 = "KEYSV01";
const char *XTRABACKUP_KEYS_MAGIC_V2 = "KEYSV02";
constexpr size_t XTRABACKUP_KEYS_MAGIC_SIZE = 7;

const char *BINLOG_KEY_MAGIC = "MYSQL-BINARY-LOG";
constexpr size_t BINLOG_KEY_MAGIC_SIZE = sizeof("MYSQL-BINARY-LOG");

static MEM_ROOT argv_alloc{PSI_NOT_INSTRUMENTED, 512};

/* adjust mysql_mandatory_plugins to load keyring proxy only */
static bool set_keyring_proxy_plugin() {
  I_List_iterator<i_string> iter(*opt_plugin_load_list_ptr);
  i_string *item;
  while (nullptr != (item = iter++)) {
    const size_t prefix_len = strlen("keyring_");
    const char *plugin_name = item->ptr;
    if (strlen(plugin_name) > prefix_len &&
        strncmp(plugin_name, "keyring_", prefix_len) == 0) {
      /* initialize only daemon_keyring_proxy_plugin */
      set_srv_keyring_implementation_as_default();
      for (st_mysql_plugin **plugin = mysql_mandatory_plugins; *plugin;
           plugin++) {
        if (strcmp((*plugin)->name, "daemon_keyring_proxy_plugin") == 0) {
          mysql_mandatory_plugins[0] = *plugin;
          mysql_mandatory_plugins[1] = 0;
          return true;
        }
      }
    }
  }

  if (strcmp(mysql_mandatory_plugins[0]->name, "daemon_keyring_proxy_plugin") !=
      0)
    mysql_mandatory_plugins[0] = 0;
  return false;
}

/** Load plugins and keep argc and argv untouched */
static void init_plugins(int argc, char **argv) {
  int t_argc = argc;
  char **t_argv = new char *[t_argc + 1];

  memset(t_argv, 0, sizeof(char *) * (t_argc + 1));
  memcpy(t_argv, argv, sizeof(char *) * t_argc);

  mysql_optional_plugins[0] = 0;
  set_keyring_proxy_plugin();

  plugin_register_early_plugins(&t_argc, t_argv, 0);
  plugin_register_builtin_and_init_core_se(&t_argc, t_argv);
  plugin_register_dynamic_and_init_all(&t_argc, t_argv,
                                       PLUGIN_INIT_SKIP_PLUGIN_TABLE);

  delete[] t_argv;
}

/** Load the encryption metadata of redo log from encryption info hash into
variable
@param[out]   e_m   Encryption metadata of redo log
@return true on success, else false */
bool xb_load_saved_redo_encryption(Encryption_metadata &e_m) {
  bool success =
      xb_fetch_tablespace_key(dict_sys_t::s_log_space_id, e_m.m_key, e_m.m_iv);
  if (success) {
    e_m.m_key_len = Encryption::KEY_LEN;
    e_m.m_type = Encryption::AES;
    return (true);
  }
  return (false);
}

/** Fetch tablespace key from "xtrabackup_keys".
@param[in]	space_id	tablespace id
@param[out]	key		fetched tablespace key
@param[out]	key		fetched tablespace iv */
bool xb_fetch_tablespace_key(ulint space_id, byte *key, byte *iv) {
  std::map<ulint, tablespace_encryption_info>::iterator it;

  it = encryption_info.find(space_id);

  if (it == encryption_info.end()) {
    return (false);
  }

  memcpy(key, it->second.key, Encryption::KEY_LEN);
  memcpy(iv, it->second.iv, Encryption::KEY_LEN);

  return (true);
}

/** Save tablespace key for later use.
@param[in]	space_id	tablespace id
@param[in]	key		tablespace key
@param[in]	key		tablespace iv */
void xb_insert_tablespace_key(ulint space_id, const byte *key, const byte *iv) {
  tablespace_encryption_info info;

  memcpy(info.key, key, Encryption::KEY_LEN);
  memcpy(info.iv, iv, Encryption::KEY_LEN);
  encryption_info[space_id] = info;
}

/** Save the encryption metadata of redo log into encryption keys hash.
This hash is later used to dump the saved keys into xtrabackup_keys file
@param[in]   e_m   Encryption metadata of redo log */
void xb_save_redo_encryption_key(const Encryption_metadata& em) {
  tablespace_encryption_info info;

  memcpy(info.key, em.m_key, Encryption::KEY_LEN);
  memcpy(info.iv, em.m_iv, Encryption::KEY_LEN);
  encryption_info[dict_sys_t::s_log_space_id] = info;
}

/** Fetch tablespace key from "xtrabackup_keys" and set the encryption
type for the tablespace.
@param[in]	space		tablespace
@return DB_SUCCESS or error code */
dberr_t xb_set_encryption(fil_space_t *space) {
  byte key[Encryption::KEY_LEN];
  byte iv[Encryption::KEY_LEN];

  bool found = xb_fetch_tablespace_key(space->id, key, iv);
  ut_a(found);

  space->flags |= FSP_FLAGS_MASK_ENCRYPTION;
  return (fil_set_encryption(space->id, Encryption::AES, key, iv));
}

const char *TRANSITION_KEY_PREFIX = "XBKey";
const size_t TRANSITION_KEY_PREFIX_LEN =
    sizeof(TRANSITION_KEY_PREFIX) / sizeof(char);
const size_t TRANSITION_KEY_RANDOM_DATA_LEN = 32;
const size_t TRANSITION_KEY_NAME_MAX_LEN_V1 =
    Encryption::SERVER_UUID_LEN + 2 + 45;
const size_t TRANSITION_KEY_NAME_MAX_LEN_V2 =
    TRANSITION_KEY_PREFIX_LEN + Encryption::SERVER_UUID_LEN +
    TRANSITION_KEY_RANDOM_DATA_LEN + 1;

/** Fetch the key from keyring.
@param[in]	key_name	key name
@param[out]	key		key
@return false if fetch has failed. */
static bool xb_fetch_key(char *key_name, char *key) {
  char *key_type = NULL;
  size_t key_len;
  unsigned char *tmp_key = NULL;
  int ret;

  ret = keyring_operations_helper::read_secret(
      srv_keyring_reader, key_name, nullptr,
      &tmp_key, &key_len, &key_type, PSI_INSTRUMENT_ME);
  if (ret == -1 || tmp_key == NULL) {
    xb::error() << "Can't fetch the key, please "
                   "check the keyring plugin is loaded.";
    return (false);
  }

  if (key_len != Encryption::KEY_LEN) {
    xb::error() << "Can't fetch the key, key length "
                   "mismatch.";
    my_free(tmp_key);
    return (false);
  }

  memcpy(key, tmp_key, Encryption::KEY_LEN);

  my_free(tmp_key);
  my_free(key_type);

  return (true);
}

/** Create transition key and store it into keyring.
@param[out]	key		transition key
@param[out]	key_name	transition key name
@return false if fetch has failed. */
static bool xb_create_transition_key(char *key_name, char *key) {
  byte rand32[20];
  char rand64[TRANSITION_KEY_RANDOM_DATA_LEN];
  int ret;

  xb::info() << "Creating transition key.";

  ut_ad(base64_needed_encoded_length(20) <= TRANSITION_KEY_RANDOM_DATA_LEN);

  if (my_rand_buffer(rand32, sizeof(rand32)) != 0) {
    return (false);
  }

  base64_encode(rand32, 20, rand64);

  /* Trasnsition key name is composed of server uuid and random suffix. */
  snprintf(key_name, TRANSITION_KEY_NAME_MAX_LEN_V2, "%s-%s-%s",
           TRANSITION_KEY_PREFIX, server_uuid, rand64);

  /* Let keyring generate key for us. */
  ret = srv_keyring_generator->generate(key_name, nullptr, "AES",
                                          Encryption::KEY_LEN);
  if (ret) {
    xb::error() << "Can't generate the key, please "
                   "check the keyring plugin is loaded.";
    return (false);
  }

  /* Fetch the key and return. */
  return xb_fetch_key(key_name, key);
}

/** Initialize keyring plugin for backup. Config is read from live mysql server.
@param[in]	connection	mysql connection
@return true if success */
bool xb_keyring_init_for_backup(MYSQL *connection) {
  std::vector<std::string> keyring_plugin_args;
  std::string keyring_plugin_name;
  std::string keyring_plugin_lib;

  const char *query =
      "SELECT PLUGIN_NAME, PLUGIN_LIBRARY "
      "FROM information_schema.plugins "
      "WHERE PLUGIN_STATUS = 'ACTIVE' AND PLUGIN_TYPE = 'KEYRING'";

  MYSQL_RES *mysql_result;
  MYSQL_ROW row;

  mysql_result = xb_mysql_query(connection, query, true);

  if ((row = mysql_fetch_row(mysql_result)) != NULL) {
    keyring_plugin_name = row[0];
    keyring_plugin_lib = row[1];
    opt_plugin_load_list_ptr->push_back(new i_string(my_strdup(
        PSI_NOT_INSTRUMENTED, keyring_plugin_lib.c_str(), MYF(MY_FAE))));
    xb::info() << "Added plugin " << SQUOTE(keyring_plugin_lib.c_str())
               << " to load list.";
  }

  mysql_free_result(mysql_result);

  if (keyring_plugin_lib.empty()) {
    return (true);
  }

  std::ostringstream vars_query;
  vars_query << "SHOW VARIABLES LIKE '" << keyring_plugin_name << "_%'";

  mysql_result = xb_mysql_query(connection, vars_query.str().c_str(), true);

  while ((row = mysql_fetch_row(mysql_result)) != NULL) {
    std::ostringstream var;

    var << "--" << row[0] << "=" << row[1];

    keyring_plugin_args.push_back(var.str());
  }

  mysql_free_result(mysql_result);

  int t_argc = keyring_plugin_args.size() + 1;
  char **t_argv = new char *[t_argc + 1];

  memset(t_argv, 0, sizeof(char *) * (t_argc + 1));

  t_argv[0] = (char *)"";
  for (int i = 1; i < t_argc; i++) {
    t_argv[i] = (char *)keyring_plugin_args[i - 1].c_str();
  }

  init_plugins(t_argc, t_argv);

  delete[] t_argv;

  return (true);
}

/** Initialize keyring plugin for prepare mode. Configuration is read from
argc and argv, server uuid and plugin name is read from backup-my.cnf.
@param[in, out]	argc	Command line options (count)
@param[in, out]	argv	Command line options (values)
@return true if success */
bool xb_keyring_init_for_prepare(int argc, char **argv) {
  char *uuid = NULL;
  char *plugin_load = NULL;

  my_option keyring_options[] = {
      {"server-uuid", 0, "", &uuid, &uuid, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0,
       0, 0},
      {"plugin-load", 0, "", &plugin_load, &plugin_load, 0, GET_STR,
       REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
      {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}};

  if (!xtrabackup::utils::load_backup_my_cnf(keyring_options,
                                             xtrabackup_target_dir)) {
    return (false);
  }

  if (plugin_load != NULL) {
    opt_plugin_load_list_ptr->push_back(new i_string(
        my_strdup(PSI_NOT_INSTRUMENTED, plugin_load, MYF(MY_FAE))));
  }

  memset(server_uuid, 0, Encryption::SERVER_UUID_LEN + 1);
  if (uuid != NULL) {
    strncpy(server_uuid, uuid, Encryption::SERVER_UUID_LEN);
  }

  init_plugins(argc, argv);

  return (true);
}

static std::vector<std::string> plugin_load_list;

static bool get_plugin_load_option(
    int optid MY_ATTRIBUTE((unused)),
    const struct my_option *opt MY_ATTRIBUTE((unused)), char *argument) {
  std::string token;
  std::istringstream token_stream(argument);
  while (std::getline(token_stream, token, ';')) {
    plugin_load_list.push_back(token);
  }
  return (false);
}

/** Initialize keyring plugin for copy-back mode. Configuration is read from
argc and argv, server uuid is read from backup-my.cnf, plugin name is read
from my.cnf.
@param[in, out]	argc	Command line options (count)
@param[in, out]	argv	Command line options (values)
@return true if success */
bool xb_keyring_init_for_copy_back(int argc, char **argv) {
  if(!xtrabackup::components::keyring_init_offline())
  {
    xb::error() << "failed to init keyring component";
    return (false);
  }
  if (!xtrabackup::components::keyring_component_initialized) {
    mysql_optional_plugins[0] = 0;

    char *uuid = NULL;

    my_option keyring_options[] = {
        {"server-uuid", 0, "", &uuid, &uuid, 0, GET_STR, REQUIRED_ARG, 0, 0, 0,
         0, 0, 0},
        {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}};

    if (!xtrabackup::utils::load_backup_my_cnf(keyring_options,
                                               xtrabackup_target_dir)) {
      return (false);
    }

    memset(server_uuid, 0, Encryption::SERVER_UUID_LEN + 1);
    if (uuid != NULL) {
      strncpy(server_uuid, uuid, Encryption::SERVER_UUID_LEN);
    }

    /* copy argc, argv because handle_options will destroy them */

    int t_argc = argc + 1;
    char **t_argv = new char *[t_argc + 1];

    memset(t_argv, 0, sizeof(char *) * (t_argc + 1));
    t_argv[0] = (char *)"xtrabackup";
    memcpy(t_argv + 1, argv, sizeof(char *) * argc);

    my_option plugin_load_options[] = {
        {"early-plugin-load", 1, "", 0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0,
         0, 0},
        {"plugin-load", 2, "", 0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0,
         0},
        {"plugin-load-add", 3, "", 0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0,
         0, 0},
        {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}};

    char **old_t_argv = t_argv;
    if (handle_options(&t_argc, &t_argv, plugin_load_options,
                       get_plugin_load_option)) {
      delete[] old_t_argv;
      return (false);
    }

    /* pick only plugins starting with keyring_ */
    for (std::vector<std::string>::iterator i = plugin_load_list.begin();
         i != plugin_load_list.end(); i++) {
      const size_t prefix_len = sizeof("keyring_") - 1;
      const char *plugin_name = i->c_str();
      if (strlen(plugin_name) > prefix_len &&
          strncmp(plugin_name, "keyring_", prefix_len) == 0) {
        opt_plugin_load_list_ptr->push_back(new i_string(
            my_strdup(PSI_NOT_INSTRUMENTED, plugin_name, MYF(MY_FAE))));
      }
    }
    init_plugins(argc, argv);

    delete[] old_t_argv;
  }
  xtrabackup::components::inititialize_service_handles();
  return (true);
}

/** Check is "xtrabackup_keys" file exists.
@return true if exists */
bool xb_tablespace_keys_exist() {
  MY_STAT stat_arg;

  if (!my_stat(XTRABACKUP_KEYS_FILE, &stat_arg, MYF(0))) {
    return (false);
  }

  return (true);
}

bool xb_tablespace_keys_load_one(const char *dir, const char *transition_key,
                                 size_t transition_key_len) {
  byte derived_key[Encryption::KEY_LEN];
  byte salt[XB_KDF_SALT_SIZE];
  byte read_buf[Encryption::KEY_LEN * 2 + 8];
  byte tmp[Encryption::KEY_LEN * 2];
  char magic[XTRABACKUP_KEYS_MAGIC_SIZE];
  uint8_t transition_key_name_size = 0;
  std::unique_ptr<char[]> transition_key_name;
  char transition_key_buf[Encryption::KEY_LEN];
  bool ret;

  const size_t record_len = Encryption::KEY_LEN * 2 + 8;

  char fname[FN_REFLEN];

  /* expand full name */
  if (fn_format(fname, XTRABACKUP_KEYS_FILE, dir, "",
                MY_UNPACK_FILENAME | MY_SAFE_PATH) == NULL) {
    return (false);
  }

  FILE *f = fopen(fname, "rb");

  if (f == NULL) {
    return (true);
  }

  xb::info() << "Loading " << XTRABACKUP_KEYS_FILE;

  if (fread(magic, 1, XTRABACKUP_KEYS_MAGIC_SIZE, f) !=
      XTRABACKUP_KEYS_MAGIC_SIZE) {
    xb::error() << "Error reading " << XTRABACKUP_KEYS_FILE
                << ": failed to read magic.";
    goto error;
  }

  if (memcmp(magic, XTRABACKUP_KEYS_MAGIC_V1, XTRABACKUP_KEYS_MAGIC_SIZE) ==
      0) {
    transition_key_name_size = TRANSITION_KEY_NAME_MAX_LEN_V1;

  } else if (memcmp(magic, XTRABACKUP_KEYS_MAGIC_V2,
                    XTRABACKUP_KEYS_MAGIC_SIZE) == 0) {
    transition_key_name_size = TRANSITION_KEY_NAME_MAX_LEN_V2;
  } else {
    xb::error() << "Error reading " << XTRABACKUP_KEYS_FILE << ": wrong magic.";
    goto error;
  }

  if (fread(salt, 1, sizeof(salt), f) != sizeof(salt)) {
    xb::error() << "Error reading " << XTRABACKUP_KEYS_FILE
                << ": failed to read salt.";
    goto error;
  }

  transition_key_name = std::make_unique<char[]>(transition_key_name_size);
  if (fread(transition_key_name.get(), 1, transition_key_name_size, f) !=
      transition_key_name_size) {
    xb::error() << "Error reading " << XTRABACKUP_KEYS_FILE
                << ": failed to read transition key name.";
    goto error;
  }

  if (transition_key == NULL && transition_key_name.get()[0] != 0) {
    if (!xb_fetch_key(transition_key_name.get(), transition_key_buf)) {
      goto error;
    }
    transition_key = transition_key_buf;
    transition_key_len = Encryption::KEY_LEN;
  }

  xb_libgcrypt_init();
  ret = xb_derive_key(transition_key, transition_key_len, salt, sizeof(salt),
                      sizeof(derived_key), derived_key);

  if (!ret) {
    xb::error() << "Error reading " << XTRABACKUP_KEYS_FILE
                << ": failed to derive encryption key.";
    goto error;
  }

  while (fread(read_buf, 1, record_len, f) == record_len) {
    ulint space_id = uint4korr(read_buf);
    tablespace_encryption_info info;
    int olen;

    olen =
        my_aes_decrypt(read_buf + 4, Encryption::KEY_LEN, info.key, derived_key,
                       sizeof(derived_key), my_aes_256_ecb, NULL, false);

    if (olen == MY_AES_BAD_DATA) {
      xb::error() << "Error reading " << XTRABACKUP_KEYS_FILE
                  << " : failed to decrypt key for tablespace " << space_id;
      goto error;
    }

    olen = my_aes_decrypt(read_buf + Encryption::KEY_LEN + 4,
                          Encryption::KEY_LEN, info.iv, derived_key,
                          sizeof(derived_key), my_aes_256_ecb, NULL, false);

    if (olen == MY_AES_BAD_DATA) {
      xb::error() << "Error reading " << XTRABACKUP_KEYS_FILE
                  << ": failed to decrypt iv for tablespace " << space_id;
      goto error;
    }

    memcpy(tmp, info.key, Encryption::KEY_LEN);
    memcpy(tmp + Encryption::KEY_LEN, info.iv, Encryption::KEY_LEN);
    ulint crc1 = ut_crc32(tmp, Encryption::KEY_LEN * 2);
    ulint crc2 = uint4korr(read_buf + Encryption::KEY_LEN * 2 + 4);

    if (crc1 != crc2) {
      xb::error() << "Error reading " << XTRABACKUP_KEYS_FILE
                  << ": failed to decrypt key and iv for tablespace "
                  << space_id << ". Wrong transition key?";
      goto error;
    }

    encryption_info[space_id] = info;
  }

  fclose(f);
  return (true);

error:
  fclose(f);
  return (false);
}

/** Load tablespace keys from encrypted "xtrabackup_keys" file.
@param[in]	dir			load "xtrabackup_keys"
                                        from this directory
@param[in]	transition_key		transition key used to encrypt
                                        tablespace keys
@param[in]	transition_key_len	transition key length
@return true if success */
bool xb_tablespace_keys_load(const char *dir, const char *transition_key,
                             size_t transition_key_len) {
  if (!xb_tablespace_keys_load_one("./", transition_key, transition_key_len)) {
    return (false);
  }
  if (dir != NULL) {
    return xb_tablespace_keys_load_one(dir, transition_key, transition_key_len);
  }
  return (true);
}

static bool xb_tablespace_keys_write_single(ds_file_t *stream,
                                            const byte *derived_key,
                                            ulint space_id, const byte *key,
                                            const byte *iv) {
  byte write_buf[Encryption::KEY_LEN * 2 + 8];
  byte tmp[Encryption::KEY_LEN * 2];
  int elen;

  /* Store space id. */
  int4store(write_buf, space_id);

  /* Store encrypted tablespace key. */
  elen = my_aes_encrypt(key, Encryption::KEY_LEN, write_buf + 4, derived_key,
                        Encryption::KEY_LEN, my_aes_256_ecb, NULL, false);

  if (elen == MY_AES_BAD_DATA) {
    xb::error() << "Failed to encrypt key for tablespace " << space_id;
    return (false);
  }

  /* Store encrypted tablespace iv. */
  elen = my_aes_encrypt(iv, Encryption::KEY_LEN,
                        write_buf + Encryption::KEY_LEN + 4, derived_key,
                        Encryption::KEY_LEN, my_aes_256_ecb, NULL, false);

  if (elen == MY_AES_BAD_DATA) {
    xb::error() << "Failed to encrypt iv for tablespace " << space_id;
    return (false);
  }

  memcpy(tmp, key, Encryption::KEY_LEN);
  memcpy(tmp + Encryption::KEY_LEN, iv, Encryption::KEY_LEN);
  ulint crc = ut_crc32(tmp, Encryption::KEY_LEN * 2);

  /* Store crc. */
  int4store(write_buf + Encryption::KEY_LEN * 2 + 4, crc);

  if (ds_write(stream, write_buf, Encryption::KEY_LEN * 2 + 8)) {
    xb::error() << "Failed to write key for tablespace " << space_id;
    return (false);
  }

  return (true);
}

/** Dump tablespace keys into encrypted "xtrabackup_keys" file.
@param[in]	ds_ctxt			datasink context to output file into
@param[in]	transition_key		transition key used to encrypt
                                        tablespace keys
@param[in]	transition_key_len	transition key length
@return true if success */
bool xb_tablespace_keys_dump(ds_ctxt_t *ds_ctxt, const char *transition_key,
                             size_t transition_key_len) {
  byte derived_key[Encryption::KEY_LEN];
  byte salt[XB_KDF_SALT_SIZE];
  char transition_key_name[TRANSITION_KEY_NAME_MAX_LEN_V2];
  char transition_key_buf[Encryption::KEY_LEN];

  xb::info() << "Saving " << XTRABACKUP_KEYS_FILE;

  if (my_rand_buffer(salt, sizeof(salt)) != 0) {
    return (false);
  }

  memset(transition_key_name, 0, sizeof(transition_key_name));

  if (transition_key == NULL) {
    if (!xb_create_transition_key(transition_key_name, transition_key_buf)) {
      return (false);
    }
    transition_key = transition_key_buf;
    transition_key_len = Encryption::KEY_LEN;
  }

  xb_libgcrypt_init();
  bool ret = xb_derive_key(transition_key, transition_key_len, salt,
                           sizeof(salt), sizeof(derived_key), derived_key);

  if (!ret) {
    xb::error() << "Error writing " << XTRABACKUP_KEYS_FILE
                << ": failed to derive encryption key.";
    return (false);
  }

  dberr_t err;
  MY_STAT stat_info;
  memset(&stat_info, 0, sizeof(MY_STAT));

  ds_file_t *stream = ds_open(ds_ctxt, XTRABACKUP_KEYS_FILE, &stat_info);
  if (stream == NULL) {
    xb::error() << "Error writing " << XTRABACKUP_KEYS_FILE
                << ": failed to create file.";
    return (false);
  }

  if (ds_write(stream, XTRABACKUP_KEYS_MAGIC_V2, XTRABACKUP_KEYS_MAGIC_SIZE)) {
    xb::error() << "Error writing " << XTRABACKUP_KEYS_FILE
                << ": failed to write magic.";
    goto error;
  }

  if (ds_write(stream, salt, sizeof(salt))) {
    xb::error() << "Error writing " << XTRABACKUP_KEYS_FILE
                << ": failed to write salt.";
    goto error;
  }

  if (ds_write(stream, transition_key_name, sizeof(transition_key_name))) {
    xb::error() << "Error writing " << XTRABACKUP_KEYS_FILE
                << ": failed to write transition key name.";
    goto error;
  }

  err = Fil_space_iterator::for_each_space([&](fil_space_t *space) {
    if (space->m_encryption_metadata.m_type == Encryption::NONE) {
      return (DB_SUCCESS);
    }
    if (!xb_tablespace_keys_write_single(stream, derived_key, space->id,
                                         space->m_encryption_metadata.m_key,
                                         space->m_encryption_metadata.m_iv)) {
      xb::error() << "Error writing " << XTRABACKUP_KEYS_FILE
                  << ": failed to save tablespace key.";
      return (DB_ERROR);
    }
    return (DB_SUCCESS);
  });

  if (err != DB_SUCCESS) {
    goto error;
  }

  if (recv_sys->keys != nullptr) {
    for (auto &key : *recv_sys->keys) {
      if (!xb_tablespace_keys_write_single(stream, derived_key, key.space_id,
                                           key.ptr, key.iv)) {
        xb::error() << "Error writing " << XTRABACKUP_KEYS_FILE
                    << ": failed to save tablespace key.";
        goto error;
      }
    }
  }

  for (const auto &entry : encryption_info) {
    if (!xb_tablespace_keys_write_single(stream, derived_key, entry.first,
                                         entry.second.key, entry.second.iv)) {
      xb::error() << "Error writing " << XTRABACKUP_KEYS_FILE
                  << ": failed to save tablespace key.";
      goto error;
    }
  }

  ds_close(stream);
  return (true);

error:
  ds_close(stream);
  return (false);
}

/**
  Read encrypted binlog file header

  @param[in]  istream  input stream
  @return     encrypted binlog file header or nullptr
*/
static std::unique_ptr<Rpl_encryption_header> binlog_header_read(
    Basic_istream *istream) {
  std::unique_ptr<Rpl_encryption_header> header;
  unsigned char magic[BINLOG_MAGIC_SIZE];

  /* Open a simple istream to read the magic from the file */
  if (istream->read(magic, BINLOG_MAGIC_SIZE) != BINLOG_MAGIC_SIZE)
    return (nullptr);

  ut_ad(Rpl_encryption_header::ENCRYPTION_MAGIC_SIZE == BINLOG_MAGIC_SIZE);
  /* Identify the file type by the magic to get the encryption header */
  if (memcmp(magic, Rpl_encryption_header::ENCRYPTION_MAGIC,
             BINLOG_MAGIC_SIZE) == 0) {
    header = Rpl_encryption_header::get_header(istream);
    if (header == nullptr) return (nullptr);
  } else if (memcmp(magic, BINLOG_MAGIC, BINLOG_MAGIC_SIZE) != 0) {
    return (nullptr);
  }

  return (header);
}

/**
  Read encrypted binlog file header

  @param[in]  binlog_file_path  binlog file path
  @return     encrypted binlog file header or nullptr
*/
static std::unique_ptr<Rpl_encryption_header> binlog_header_read(
    const char *binlog_file_path) {
  /* Open a simple istream to read the magic from the file */
  IO_CACHE_istream istream;
  if (istream.open(key_file_binlog, key_file_binlog_cache, binlog_file_path,
                   MYF(MY_WME | MY_DONT_CHECK_FILESIZE), IO_SIZE * 2))
    return (nullptr);

  return binlog_header_read(&istream);
}

bool xb_binlog_password_store(const char *binlog_file_path) {
  auto header = binlog_header_read(binlog_file_path);
  if (header == nullptr) {
    return (true);
  }

  Key_string file_password = header->decrypt_file_password();

  unsigned char iv[Encryption::KEY_LEN]{0};
  memcpy(iv, BINLOG_KEY_MAGIC, BINLOG_KEY_MAGIC_SIZE);

  xb_insert_tablespace_key(dict_sys_t::s_invalid_space_id, file_password.data(),
                           iv);

  return (true);
}

bool xb_binlog_password_reencrypt(const char *binlog_file_path) {
  unsigned char key[Encryption::KEY_LEN];
  unsigned char iv[Encryption::KEY_LEN];

  bool found = xb_fetch_tablespace_key(dict_sys_t::s_invalid_space_id, key, iv);
  if (!found) {
    return (true);
  }

  rpl_encryption.initialize();
  if (rpl_encryption.enable_for_xtrabackup()) {
    xb::error() << "cannot generate master key for binlog encryption.";
    return (false);
  }

  if (memcmp(iv, BINLOG_KEY_MAGIC, BINLOG_KEY_MAGIC_SIZE) != 0) {
    xb::error() << "key entry for mysql binary log is corrupt.";
    return (false);
  }

  auto header = binlog_header_read(binlog_file_path);

  if (header == nullptr) {
    return (false);
  }

  Key_string file_password(key, Encryption::KEY_LEN);
  header->encrypt_file_password(file_password);

  IO_CACHE_ostream ostream;
  if (ostream.open(key_file_binlog, binlog_file_path,
                   MYF(MY_WME | MY_DONT_CHECK_FILESIZE)))
    return (false);

  ostream.seek(0);

  if (header->serialize(&ostream)) {
    xb::error() << "Error writing to " << binlog_file_path
                << ". Cannot update binlog file encryption header.";
    return (false);
  }

  return (true);
}

/** Shutdown keyring plugins. */
void xb_keyring_shutdown() {
  release_keyring_handles();
  plugin_shutdown();

  I_List_iterator<i_string> iter(*opt_plugin_load_list_ptr);
  i_string *item;
  while ((item = iter++) != NULL) {
    my_free((void *)item->ptr);
  }

  free_list(opt_plugin_load_list_ptr);
  xtrabackup::components::deinitialize_service_handles();
}
