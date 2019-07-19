/******************************************************
Copyright (c) 2016, 2018 Percona LLC and/or its affiliates.

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

#include <my_global.h>
#include <sql_plugin.h>
#include <my_aes.h>
#include <my_rnd.h>
#include <my_default.h>
#include <mysqld.h>
#include <base64.h>
#include "xtrabackup.h"
#include "xb0xb.h"
#include "keyring_plugins.h"
#include "backup_mysql.h"
#include "common.h"
#include "kdf.h"

#include <map>

struct tablespace_encryption_info {
	byte key[ENCRYPTION_KEY_LEN];
	byte iv[ENCRYPTION_KEY_LEN];
};

static std::map<ulint, tablespace_encryption_info> encryption_info;

extern st_mysql_plugin *mysql_optional_plugins[];
extern st_mysql_plugin *mysql_mandatory_plugins[];

const char *XTRABACKUP_KEYS_FILE = "xtrabackup_keys";
const char *XTRABACKUP_KEYS_MAGIC = "KEYSV01";
const size_t XTRABACKUP_KEYS_MAGIC_SIZE = 7;

/** Load plugins and keep argc and argv untouched */
static void
init_plugins(int argc, char **argv)
{
	int t_argc = argc;
	char **t_argv = new char *[t_argc + 1];

	memset(t_argv, 0, sizeof(char *) * (t_argc + 1));
	memcpy(t_argv, argv, sizeof(char *) * t_argc);

	mysql_optional_plugins[0] = 0;
	mysql_mandatory_plugins[0] = 0;

	if (opt_xtra_plugin_dir != NULL) {
		strncpy(opt_plugin_dir, opt_xtra_plugin_dir, FN_REFLEN);
	} else {
		strcpy(opt_plugin_dir, PLUGINDIR);
	}

	plugin_register_early_plugins(&t_argc, t_argv, 0);
	plugin_register_builtin_and_init_core_se(&t_argc, t_argv);
	plugin_register_dynamic_and_init_all(&t_argc, t_argv,
					     PLUGIN_INIT_SKIP_PLUGIN_TABLE);

	delete [] t_argv;
}

/** Fetch tablespace key from "xtrabackup_keys".
@param[in]	space_id	tablespace id
@param[out]	key		fetched tablespace key
@param[out]	key		fetched tablespace iv */
void
xb_fetch_tablespace_key(ulint space_id, byte *key, byte *iv)
{
	std::map<ulint, tablespace_encryption_info>::iterator it;

	it = encryption_info.find(space_id);

	ut_ad(it != encryption_info.end());

	memcpy(key, it->second.key, ENCRYPTION_KEY_LEN);
	memcpy(iv, it->second.iv, ENCRYPTION_KEY_LEN);
}

const char *TRANSITION_KEY_PRIFIX = "XBKey";
const size_t TRANSITION_KEY_NAME_MAX_LEN = ENCRYPTION_SERVER_UUID_LEN + 2 + 45;

/** Fetch the key from keyring.
@param[in]	key_name	key name
@param[out]	key		key
@return false if fetch has failed. */
static bool
xb_fetch_key(char *key_name, char *key) {
	char*	key_type = NULL;
	size_t	key_len;
	char*	tmp_key = NULL;
	int	ret;

	ret = my_key_fetch(key_name, &key_type, NULL,
			   reinterpret_cast<void**>(&tmp_key),
			   &key_len);

	if (ret || tmp_key == NULL) {
		msg("xtrabackup: Error: Can't fetch the key, please "
		    "check the keyring plugin is loaded.\n");
		return(false);
	}

	if (key_len != ENCRYPTION_KEY_LEN) {
		msg("xtrabackup: Error: Can't fetch the key, key length "
		    "mismatch.\n");
		my_free(tmp_key);
		return(false);
	}

	memcpy(key, tmp_key, ENCRYPTION_KEY_LEN);

	my_free(tmp_key);
	my_free(key_type);

	return(true);
}

/** Create transition key and store it into keyring.
@param[out]	key		transition key
@param[out]	key_name	transition key name
@return false if fetch has failed. */
static bool
xb_create_transition_key(char *key_name, char* key)
{
	byte	rand32[32];
	char	rand64[45];
	int	ret;

	msg_ts("Creating transition key.\n");

	ut_ad(base64_needed_encoded_length(32) <= 45);

	if (my_rand_buffer(rand32, sizeof(rand32)) != 0) {
		return(false);
	}

	base64_encode(rand32, 32, rand64);

	/* Trasnsition key name is composed of server uuid and random suffix. */
	ut_snprintf(key_name, TRANSITION_KEY_NAME_MAX_LEN,
		    "%s-%s-%s", TRANSITION_KEY_PRIFIX, server_uuid, rand64);

	/* Let keyring generate key for us. */
	ret = my_key_generate(key_name, "AES", NULL, ENCRYPTION_KEY_LEN);
	if (ret) {
		msg("xtrabackup: Error: Can't generate the key, please "
		    "check the keyring plugin is loaded.\n");
		return(false);
	}

	/* Fetch the key and return. */
	return xb_fetch_key(key_name, key);
}

/** Initialize keyring plugin for backup. Config is read from live mysql server.
@param[in]	connection	mysql connection
@return true if success */
bool
xb_keyring_init_for_backup(MYSQL *connection)
{
	std::vector<std::string> keyring_plugin_args;
	std::string keyring_plugin_name;
	std::string keyring_plugin_lib;

	const char *query = "SELECT PLUGIN_NAME, PLUGIN_LIBRARY "
		"FROM information_schema.plugins "
		"WHERE PLUGIN_STATUS = 'ACTIVE' AND PLUGIN_TYPE = 'KEYRING'";

	MYSQL_RES *mysql_result;
	MYSQL_ROW row;

	mysql_result = xb_mysql_query(connection, query, true);

	if ((row = mysql_fetch_row(mysql_result)) != NULL) {
		keyring_plugin_name = row[0];
		keyring_plugin_lib = row[1];
		opt_plugin_load_list_ptr->push_back(
			new i_string(my_strdup(
				PSI_NOT_INSTRUMENTED,
				keyring_plugin_lib.c_str(),
				MYF(MY_FAE))));
		msg_ts("Added plugin '%s' to load list.\n",
			keyring_plugin_lib.c_str());
	}

	mysql_free_result(mysql_result);

	if (keyring_plugin_lib.empty()) {
		return(true);
	}

	std::ostringstream vars_query;
	vars_query << "SHOW VARIABLES LIKE '"
		   << keyring_plugin_name << "_%'";

	mysql_result = xb_mysql_query(connection,
				      vars_query.str().c_str(), true);

	while ((row = mysql_fetch_row(mysql_result)) != NULL) {
		std::ostringstream var;

		var << "--" << row[0] << "=" << row[1];

		keyring_plugin_args.push_back(var.str());
	}

	mysql_free_result(mysql_result);

	int t_argc = keyring_plugin_args.size() + 1;
	char **t_argv = new char *[t_argc + 1];

	memset(t_argv, 0, sizeof(char *) * (t_argc + 1));

	t_argv[0] = (char *) "";
	for (int i = 1; i < t_argc; i++) {
		t_argv[i] = (char *) keyring_plugin_args[i - 1].c_str();
	}

	init_plugins(t_argc, t_argv);

	delete [] t_argv;

	return(true);
}

static
my_bool
get_one_option(int optid MY_ATTRIBUTE((unused)),
	       const struct my_option *opt MY_ATTRIBUTE((unused)),
	       char *argument MY_ATTRIBUTE((unused)))
{
	return(FALSE);
}

/** Initialize keyring plugin for stats mode. Configuration is read from
argc and argv.
@param[in, out]	argc	Command line options (count)
@param[in, out]	argv	Command line options (values)
@return true if success */
bool
xb_keyring_init_for_stats(int argc, char **argv)
{
	char *plugin_load = NULL;

	my_option keyring_options[] = {
		{"plugin-load", 0, "", &plugin_load, &plugin_load, 0, GET_STR,
		 REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
	};

	if (handle_options(&argc, &argv, keyring_options, get_one_option)) {
		return(false);
	}

	if (plugin_load != NULL) {
		opt_plugin_load_list_ptr->push_back(
			new i_string(my_strdup(
				PSI_NOT_INSTRUMENTED, plugin_load,
				MYF(MY_FAE))));
	}

	init_plugins(argc, argv);

	return(true);
}

/** Initialize keyring plugin for stats mode. Configuration is read from
argc and argv, server uuid and plugin name is read from backup-my.cnf.
@param[in, out]	argc	Command line options (count)
@param[in, out]	argv	Command line options (values)
@return true if success */
bool
xb_keyring_init_for_prepare(int argc, char **argv)
{
	char *uuid = NULL;
	char *plugin_load = NULL;
	const char *groups[] = {"mysqld", NULL};

	my_option keyring_options[] = {
		{"server-uuid", 0, "", &uuid, &uuid, 0, GET_STR,
		 REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
		{"plugin-load", 0, "", &plugin_load, &plugin_load, 0, GET_STR,
		 REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
	};

	char *exename = (char *) "xtrabackup";
	char **backup_my_argv = &exename;
	int backup_my_argc = 1;
	char fname[FN_REFLEN];

	/* we need full name so that only backup-my.cnf will be read */
	if (fn_format(fname, "backup-my.cnf", xtrabackup_target_dir, "",
		      MY_UNPACK_FILENAME | MY_SAFE_PATH) == NULL) {
		return(false);
	}

	if (my_load_defaults(fname, groups, &backup_my_argc,
			     &backup_my_argv, NULL)) {
		return(false);
	}

	char **old_argv = backup_my_argv;
	if (handle_options(&backup_my_argc, &backup_my_argv, keyring_options,
			   get_one_option)) {
		return(false);
	}

	if (plugin_load != NULL) {
		opt_plugin_load_list_ptr->push_back(
			new i_string(my_strdup(
				PSI_NOT_INSTRUMENTED, plugin_load,
				MYF(MY_FAE))));
	}

	memset(server_uuid, 0, ENCRYPTION_SERVER_UUID_LEN + 1);
	if (uuid != NULL) {
		strncpy(server_uuid, uuid, ENCRYPTION_SERVER_UUID_LEN);
	}

	init_plugins(argc, argv);

	free_defaults(old_argv);

	return(true);
}

static std::vector<std::string> plugin_load_list;

static
my_bool
get_plugin_load_option(int optid MY_ATTRIBUTE((unused)),
	       const struct my_option *opt MY_ATTRIBUTE((unused)),
	       char *argument)
{
	std::string token;
	std::istringstream token_stream(argument);
	while (std::getline(token_stream, token, ';'))
	{
		plugin_load_list.push_back(token);
	}
	return(FALSE);
}

/** Initialize keyring plugin for stats mode. Configuration is read from
argc and argv, server uuid is read from backup-my.cnf, plugin name is read
from my.cnf.
@param[in, out]	argc	Command line options (count)
@param[in, out]	argv	Command line options (values)
@return true if success */
bool
xb_keyring_init_for_copy_back(int argc, char **argv)
{
	mysql_optional_plugins[0] = 0;
	mysql_mandatory_plugins[0] = 0;

	if (opt_xtra_plugin_dir != NULL) {
		strncpy(opt_plugin_dir, opt_xtra_plugin_dir, FN_REFLEN);
	} else {
		strcpy(opt_plugin_dir, PLUGINDIR);
	}

	char *uuid = NULL;
	const char *groups[] = {"mysqld", NULL};

	my_option keyring_options[] = {
		{"server-uuid", 0, "", &uuid, &uuid, 0, GET_STR,
		 REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
	};

	char *exename = (char *) "xtrabackup";
	char **backup_my_argv = &exename;
	int backup_my_argc = 1;
	char fname[FN_REFLEN];

	/* we need full name so that only backup-my.cnf will be read */
	if (fn_format(fname, "backup-my.cnf", xtrabackup_target_dir, "",
		      MY_UNPACK_FILENAME | MY_SAFE_PATH) == NULL) {
		return(false);
	}

	if (my_load_defaults(fname, groups, &backup_my_argc,
			     &backup_my_argv, NULL)) {
		return(false);
	}

	char **old_argv = backup_my_argv;
	if (handle_options(&backup_my_argc, &backup_my_argv, keyring_options,
			   get_one_option)) {
		return(false);
	}

	memset(server_uuid, 0, ENCRYPTION_SERVER_UUID_LEN + 1);
	if (uuid != NULL) {
		strncpy(server_uuid, uuid, ENCRYPTION_SERVER_UUID_LEN);
	}

	/* copy argc, argv because handle_options will destroy them */

	int t_argc = argc + 1;
	char **t_argv = new char *[t_argc + 1];

	memset(t_argv, 0, sizeof(char *) * (t_argc + 1));
	t_argv[0] = exename;
	memcpy(t_argv + 1, argv, sizeof(char *) * argc);

	my_option plugin_load_options[] = {
		{"early-plugin-load", 1, "", 0, 0, 0, GET_STR, REQUIRED_ARG,
		 0, 0, 0, 0, 0, 0},
		{"plugin-load", 2, "", 0, 0, 0, GET_STR, REQUIRED_ARG,
		 0, 0, 0, 0, 0, 0},
		{"plugin-load-add", 3, "", 0, 0, 0, GET_STR, REQUIRED_ARG,
		 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
	};

	char **old_t_argv = t_argv;
	if (handle_options(&t_argc, &t_argv, plugin_load_options,
			   get_plugin_load_option)) {
		delete [] old_t_argv;
		return(false);
	}

	/* pick only plugins starting with keyring_ */
	for (std::vector<std::string>::iterator i = plugin_load_list.begin();
		i != plugin_load_list.end(); i++) {
		const size_t prefix_len = sizeof("keyring_") - 1;
		const char *plugin_name = i->c_str();
		if (strlen(plugin_name) > prefix_len &&
		    strncmp(plugin_name, "keyring_", prefix_len) == 0) {
			opt_plugin_load_list_ptr->push_back(
				new i_string(my_strdup(PSI_NOT_INSTRUMENTED,
					plugin_name, MYF(MY_FAE))));
		}
	}

	init_plugins(argc, argv);

	free_defaults(old_argv);
	delete [] old_t_argv;

	return(true);
}

/** Check is "xtrabackup_keys" file exists.
@return true if exists */
bool
xb_tablespace_keys_exist()
{
	MY_STAT stat_arg;

	if (!my_stat(XTRABACKUP_KEYS_FILE, &stat_arg, MYF(0))) {

		return(false);
	}

	return(true);
}

bool
xb_tablespace_keys_load_one(const char *dir, const char *transition_key,
			    size_t transition_key_len)
{
	byte derived_key[ENCRYPTION_KEY_LEN];
	byte salt[XB_KDF_SALT_SIZE];
	byte read_buf[ENCRYPTION_KEY_LEN * 2 + 8];
	byte tmp[ENCRYPTION_KEY_LEN * 2];
	char magic[XTRABACKUP_KEYS_MAGIC_SIZE];
	char transition_key_name[TRANSITION_KEY_NAME_MAX_LEN];
	char transition_key_buf[ENCRYPTION_KEY_LEN];
	bool ret;

	const size_t record_len = ENCRYPTION_KEY_LEN * 2 + 8;

	char fname[FN_REFLEN];

	/* expand full name */
	if (fn_format(fname, XTRABACKUP_KEYS_FILE, dir, "",
		      MY_UNPACK_FILENAME | MY_SAFE_PATH) == NULL) {
		return(false);
	}

	FILE *f = fopen(fname, "rb");

	if (f == NULL) {
		return(true);
	}

	msg_ts("Loading %s.\n", XTRABACKUP_KEYS_FILE);

	if (fread(magic, 1, XTRABACKUP_KEYS_MAGIC_SIZE, f)
			!= XTRABACKUP_KEYS_MAGIC_SIZE) {
		msg_ts("Error reading %s: failed to read magic.\n",
			XTRABACKUP_KEYS_FILE);
		goto error;
	}

	if (memcmp(magic, XTRABACKUP_KEYS_MAGIC,
			XTRABACKUP_KEYS_MAGIC_SIZE) != 0) {
		msg_ts("Error reading %s: wrong magic.\n",
			XTRABACKUP_KEYS_FILE);
		goto error;
	}

	if (fread(salt, 1, sizeof(salt), f) != sizeof(salt)) {
		msg_ts("Error reading %s: failed to read salt.\n",
			XTRABACKUP_KEYS_FILE);
		goto error;
	}

	if (fread(transition_key_name, 1, sizeof(transition_key_name), f)
		!= sizeof(transition_key_name)) {
		msg_ts("Error reading %s: failed to read "
			"transition key name.\n",
			XTRABACKUP_KEYS_FILE);
		goto error;
	}

	if (transition_key == NULL && transition_key_name[0] != 0) {
		if (!xb_fetch_key(transition_key_name, transition_key_buf)) {
			goto error;
		}
		transition_key = transition_key_buf;
		transition_key_len = ENCRYPTION_KEY_LEN;
	}

	ret = xb_derive_key(transition_key, transition_key_len,
			salt, sizeof(salt), sizeof(derived_key), derived_key);

	if (!ret) {
		msg_ts("Error reading %s: failed to derive encryption key.\n",
			XTRABACKUP_KEYS_FILE);
		goto error;
	}

	while (fread(read_buf, 1, record_len, f) == record_len) {
		ulint space_id = uint4korr(read_buf);
		tablespace_encryption_info info;
		int olen;

		olen = my_aes_decrypt(
			read_buf + 4,
			ENCRYPTION_KEY_LEN,
			info.key,
			derived_key,
			sizeof(derived_key),
			my_aes_256_ecb,
			NULL, false);

		if (olen == MY_AES_BAD_DATA) {
			msg_ts("Error reading %s: failed to decrypt key for "
				"tablespace %lu.\n",
				XTRABACKUP_KEYS_FILE, space_id);
			goto error;
		}

		olen = my_aes_decrypt(
			read_buf + ENCRYPTION_KEY_LEN + 4,
			ENCRYPTION_KEY_LEN,
			info.iv,
			derived_key,
			sizeof(derived_key),
			my_aes_256_ecb,
			NULL, false);

		if (olen == MY_AES_BAD_DATA) {
			msg_ts("Error reading %s: failed to decrypt iv for "
				"tablespace %lu.\n",
				XTRABACKUP_KEYS_FILE, space_id);
			goto error;
		}

		memcpy(tmp, info.key, ENCRYPTION_KEY_LEN);
		memcpy(tmp + ENCRYPTION_KEY_LEN, info.iv, ENCRYPTION_KEY_LEN);
		ulint crc1 = ut_crc32(tmp, ENCRYPTION_KEY_LEN * 2);
		ulint crc2 = uint4korr(read_buf + ENCRYPTION_KEY_LEN * 2 + 4);

		if (crc1 != crc2) {
			msg_ts("Error reading %s: failed to decrypt key and iv "
				"for tablespace %lu. Wrong transition key?\n",
				XTRABACKUP_KEYS_FILE, space_id);
			goto error;
		}

		encryption_info[space_id] = info;
	}

	fclose(f);
	return(true);

error:
	fclose(f);
	return(false);
}

/** Load tablespace keys from encrypted "xtrabackup_keys" file.
@param[in]	dir			load "xtrabackup_keys"
					from this directory
@param[in]	transition_key		transition key used to encrypt
					tablespace keys
@param[in]	transition_key_len	transition key length
@return true if success */
bool
xb_tablespace_keys_load(const char *dir,
			const char *transition_key, size_t transition_key_len)
{
	if (!xb_tablespace_keys_load_one(
		"./", transition_key, transition_key_len)) {
		return(false);
	}
	if (dir != NULL) {
		return xb_tablespace_keys_load_one(
			dir, transition_key, transition_key_len);
	}
	return(true);
}

static bool
xb_tablespace_keys_write_single(ds_file_t *stream, const byte *derived_key,
				ulint space_id, const byte *key, const byte *iv)
{
	byte write_buf[ENCRYPTION_KEY_LEN * 2 + 8];
	byte tmp[ENCRYPTION_KEY_LEN * 2];
	int elen;

	/* Store space id. */
	int4store(write_buf, space_id);

	/* Store encrypted tablespace key. */
	elen = my_aes_encrypt(
		key,
		ENCRYPTION_KEY_LEN,
		write_buf + 4,
		derived_key,
		ENCRYPTION_KEY_LEN,
		my_aes_256_ecb,
		NULL, false);

	if (elen == MY_AES_BAD_DATA) {
		msg_ts("Failed to encrypt key for tablespace %lu.\n", space_id);
		return(false);
	}

	/* Store encrypted tablespace iv. */
	elen = my_aes_encrypt(
		iv,
		ENCRYPTION_KEY_LEN,
		write_buf + ENCRYPTION_KEY_LEN + 4,
		derived_key,
		ENCRYPTION_KEY_LEN,
		my_aes_256_ecb,
		NULL, false);

	if (elen == MY_AES_BAD_DATA) {
		msg_ts("Failed to encrypt iv for tablespace %lu.\n", space_id);
		return(false);
	}

	memcpy(tmp, key, ENCRYPTION_KEY_LEN);
	memcpy(tmp + ENCRYPTION_KEY_LEN, iv, ENCRYPTION_KEY_LEN);
	ulint crc = ut_crc32(tmp, ENCRYPTION_KEY_LEN * 2);

	/* Store crc. */
	int4store(write_buf + ENCRYPTION_KEY_LEN * 2 + 4, crc);

	if (ds_write(stream, write_buf, ENCRYPTION_KEY_LEN * 2 + 8)) {
		msg_ts("Failed to write key for tablespace %lu.\n", space_id);
		return(false);
	}

	return(true);
}

/** Dump tablespace keys into encrypted "xtrabackup_keys" file.
@param[in]	ds_ctxt			datasink context to output file into
@param[in]	transition_key		transition key used to encrypt
					tablespace keys
@param[in]	transition_key_len	transition key length
@return true if success */
bool
xb_tablespace_keys_dump(ds_ctxt_t *ds_ctxt, const char *transition_key,
			size_t transition_key_len)
{
	byte derived_key[ENCRYPTION_KEY_LEN];
	byte salt[XB_KDF_SALT_SIZE];
	char transition_key_name[TRANSITION_KEY_NAME_MAX_LEN];
	char transition_key_buf[ENCRYPTION_KEY_LEN];

	msg_ts("Saving %s.\n", XTRABACKUP_KEYS_FILE);

	if (my_rand_buffer(salt, sizeof(salt)) != 0) {
		return(false);
	}

	memset(transition_key_name, 0, sizeof(transition_key_name));

	if (transition_key == NULL) {
		if (!xb_create_transition_key(
			transition_key_name, transition_key_buf)) {
			return(false);
		}
		transition_key = transition_key_buf;
		transition_key_len = ENCRYPTION_KEY_LEN;
	}

	bool ret = xb_derive_key(transition_key, transition_key_len,
			salt, sizeof(salt), sizeof(derived_key), derived_key);

	if (!ret) {
		msg_ts("Error writing %s: failed to derive encryption key.\n",
			XTRABACKUP_KEYS_FILE);
		return(false);
	}

	fil_space_t *space;
	MY_STAT stat_info;
	memset(&stat_info, 0, sizeof(MY_STAT));

	ds_file_t *stream = ds_open(ds_ctxt, XTRABACKUP_KEYS_FILE, &stat_info);
	if (stream == NULL) {
		msg_ts("Error writing %s: failed to create file.\n",
			XTRABACKUP_KEYS_FILE);
		return(false);
	}

	if (ds_write(stream, XTRABACKUP_KEYS_MAGIC,
			XTRABACKUP_KEYS_MAGIC_SIZE)) {
		msg_ts("Error writing %s: failed to write magic.\n",
			XTRABACKUP_KEYS_FILE);
		goto error;
	}

	if (ds_write(stream, salt, sizeof(salt))) {
		msg_ts("Error writing %s: failed to write salt.\n",
			XTRABACKUP_KEYS_FILE);
		goto error;
	}

	if (ds_write(stream, transition_key_name,
			sizeof(transition_key_name))) {
		msg_ts("Error writing %s: failed to write "
			"transition key name.\n",
			XTRABACKUP_KEYS_FILE);
		goto error;
	}

	space = UT_LIST_GET_FIRST(fil_system->space_list);

	while (space != NULL) {

		if (space->encryption_type == Encryption::NONE) {
			space = UT_LIST_GET_NEXT(space_list, space);
			continue;
		}

		if (!xb_tablespace_keys_write_single(stream, derived_key,
			space->id, space->encryption_key,
			space->encryption_iv)) {
			msg_ts("Error writing %s: failed to save "
				"tablespace key.\n",
				XTRABACKUP_KEYS_FILE);
			goto error;
		}

		space = UT_LIST_GET_NEXT(space_list, space);
	}

	if (recv_sys->encryption_list != NULL) {
		for (encryption_list_t::iterator i =
			recv_sys->encryption_list->begin();
		     i != recv_sys->encryption_list->end(); i++) {

			if (!xb_tablespace_keys_write_single(
				stream, derived_key, i->space_id,
				i->key, i->iv)) {
				msg_ts("Error writing %s: failed to save "
					"tablespace key.\n",
					XTRABACKUP_KEYS_FILE);
				goto error;
			}
		}
	}

	ds_close(stream);
	return(true);

error:
	ds_close(stream);
	return(false);
}


/** Shutdown keyring plugins. */
void
xb_keyring_shutdown()
{
	plugin_shutdown();

	I_List_iterator<i_string> iter(*opt_plugin_load_list_ptr);
	i_string *item;
	while ((item = iter++) != NULL) {
		my_free((void*)item->ptr);
	}

	free_list(opt_plugin_load_list_ptr);
}
