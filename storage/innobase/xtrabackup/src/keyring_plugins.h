/******************************************************
Copyright (c) 2016 Percona LLC and/or its affiliates.

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

#ifndef XB_KEYRING_PLUGINS_H
#define XB_KEYRING_PLUGINS_H

#include <mysql.h>
#include <os0file.h>
#include "datasink.h"
#include "kdf.h"
#include "log0types.h"

/** Initialize keyring plugin for backup. Config is read from live mysql server.
@param[in]	connection	mysql connection
@return true if success */
bool xb_keyring_init_for_backup(MYSQL *connection);

/** Initialize keyring plugin for prepare mode. Configuration is read from
argc and argv, server uuid and plugin name is read from backup-my.cnf.
@param[in, out]	argc	Command line options (count)
@param[in, out]	argv	Command line options (values)
@return true if success */
bool xb_keyring_init_for_prepare(int argc, char **argv);

/** Initialize keyring plugin for copy-back mode. Configuration is read from
argc and argv, server uuid is read from backup-my.cnf, plugin name is read
from my.cnf.
@param[in, out]	argc	Command line options (count)
@param[in, out]	argv	Command line options (values)
@return true if success */
bool xb_keyring_init_for_copy_back(int argc, char **argv);

/** Check is "xtrabackup_keys" file exists.
@return true if exists */
bool xb_tablespace_keys_exist();

/** Load tablespace keys from encrypted "xtrabackup_keys" file.
@param[in]	dir			load "xtrabackup_keys"
                                        from this directory
@param[in]	transition_key		transition key used to encrypt
                                        tablespace keys
@param[in]	transition_key_len	transition key length
@return true if success */
bool xb_tablespace_keys_load(const char *dir, const char *transition_key,
                             size_t transition_key_len);

/**
  Store binlog password into a backup

  @param[in]  binlog_file_path  binlog file path
  @return     true if success
*/
bool xb_binlog_password_store(const char *binlog_file_path);

/**
  Reencrypt the password in the binlog file header and store the master
  key int a keyring.

  @param[in]  binlog_file_path  binlog file path
  @return     true if success
*/
bool xb_binlog_password_reencrypt(const char *binlog_file_path);

/** Shutdown keyring plugins. */
void xb_keyring_shutdown();

/** Save the encryption metadata of redo log into encryption keys hash.
This hash is later used to dump the saved keys into xtrabackup_keys file
@param[in]   e_m   Encryption metadata of redo log */
void xb_save_redo_encryption_key(const Encryption_metadata &em);

/** Load the encryption metadata of redo log from encryption info hash into
variable
@param[out]   e_m   Encryption metadata of redo log
@return true on success, else false */
bool xb_load_saved_redo_encryption(Encryption_metadata &em);

#define TRANSITION_KEY_PREFIX_STR "XBKey"

const char *const TRANSITION_KEY_PREFIX = TRANSITION_KEY_PREFIX_STR;
const size_t TRANSITION_KEY_PREFIX_LEN = sizeof(TRANSITION_KEY_PREFIX_STR) - 1;
const size_t TRANSITION_KEY_RANDOM_DATA_LEN = 32;
const size_t TRANSITION_KEY_NAME_MAX_LEN_V1 =
    Encryption::SERVER_UUID_LEN + 2 + 45;
const size_t TRANSITION_KEY_NAME_MAX_LEN_V2 =
    TRANSITION_KEY_PREFIX_LEN + Encryption::SERVER_UUID_LEN +
    TRANSITION_KEY_RANDOM_DATA_LEN + 1;

class TablespaceKeyDumper {
 public:
  TablespaceKeyDumper(ds_ctxt_t *ds_ctxt, const char *transition_key,
                      size_t transition_key_len);

  bool initialize();
  bool dump_from_spaces(bool use_ddl_tracker);
  bool dump_from_redo();
  bool dump_from_encryption_infos();
  void finalize();
  bool is_initialized() const;

  ~TablespaceKeyDumper();

 private:
  enum class State {
    Init,
    Initialized,
    SpacesDumpedOnce,
    RecoveryDumped,
    EncryptionInfosDumped
  };

  bool fail_state(const char *func);

  ds_ctxt_t *m_ds_ctxt;
  const char *m_transition_key;
  size_t m_transition_key_len;

  byte m_derived_key[Encryption::KEY_LEN];
  byte m_salt[XB_KDF_SALT_SIZE];
  char m_transition_key_name[TRANSITION_KEY_NAME_MAX_LEN_V2];
  char m_transition_key_buf[Encryption::KEY_LEN];

  ds_file_t *m_stream;
  State m_state;
  bool m_finalized;
};

#endif  // XB_KEYRING_PLUGINS_H
