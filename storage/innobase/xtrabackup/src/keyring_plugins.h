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

/** Initialize keyring plugin for backup. Config is read from live mysql server.
@param[in]	connection	mysql connection
@return true if success */
bool
xb_keyring_init_for_backup(MYSQL *connection);


/** Initialize keyring plugin for stats mode. Configuration is read from
argc and argv.
@param[in, out]	argc	Command line options (count)
@param[in, out]	argv	Command line options (values)
@return true if success */
bool
xb_keyring_init_for_stats(int argc, char **argv);


/** Initialize keyring plugin for stats mode. Configuration is read from
argc and argv, server uuid and plugin name is read from backup-my.cnf.
@param[in, out]	argc	Command line options (count)
@param[in, out]	argv	Command line options (values)
@return true if success */
bool
xb_keyring_init_for_prepare(int argc, char **argv);


/** Initialize keyring plugin for stats mode. Configuration is read from
argc and argv, server uuid is read from backup-my.cnf, plugin name is read
from my.cnf.
@param[in, out]	argc	Command line options (count)
@param[in, out]	argv	Command line options (values)
@return true if success */
bool
xb_keyring_init_for_copy_back(int argc, char **argv);


/** Check is "xtrabackup_keys" file exists.
@return true if exists */
bool
xb_tablespace_keys_exist();


/** Load tablespace keys from encrypted "xtrabackup_keys" file.
@param[in]	dir			load "xtrabackup_keys"
					from this directory
@param[in]	transition_key		transition key used to encrypt
					tablespace keys
@param[in]	transition_key_len	transition key length
@return true if success */
bool
xb_tablespace_keys_load(const char *dir,
			const char *transition_key, size_t transition_key_len);


/** Dump tablespace keys into encrypted "xtrabackup_keys" file.
@param[in]	ds_ctxt			datasink context to output file into
@param[in]	transition_key		transition key used to encrypt
					tablespace keys
@param[in]	transition_key_len	transition key length
@return true if success */
bool
xb_tablespace_keys_dump(ds_ctxt_t *ds_ctxt, const char *transition_key,
			size_t transition_key_len);

/** Shutdown keyring plugins. */
void
xb_keyring_shutdown();

#endif // XB_KEYRING_PLUGINS_H
