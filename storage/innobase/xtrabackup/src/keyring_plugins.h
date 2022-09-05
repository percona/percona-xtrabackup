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
#include "log0types.h"

/** Initialize keyring plugin for backup. Config is read from live mysql server.
@param[in]	connection	mysql connection
@return true if success */
bool xb_keyring_init_for_backup(MYSQL *connection);

/** Initialize keyring plugin for stats mode. Configuration is read from
argc and argv.
@param[in, out]	argc	Command line options (count)
@param[in, out]	argv	Command line options (values)
@return true if success */
bool xb_keyring_init_for_stats(int argc, char **argv);

/** Initialize keyring plugin for stats mode. Configuration is read from
argc and argv, server uuid and plugin name is read from backup-my.cnf.
@param[in, out]	argc	Command line options (count)
@param[in, out]	argv	Command line options (values)
@return true if success */
bool xb_keyring_init_for_prepare(int argc, char **argv);

/** Initialize keyring plugin for stats mode. Configuration is read from
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

/** Dump tablespace keys into encrypted "xtrabackup_keys" file.
@param[in]	ds_ctxt			datasink context to output file into
@param[in]	transition_key		transition key used to encrypt
                                        tablespace keys
@param[in]	transition_key_len	transition key length
@return true if success */
bool xb_tablespace_keys_dump(ds_ctxt_t *ds_ctxt, const char *transition_key,
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
#endif  // XB_KEYRING_PLUGINS_H
