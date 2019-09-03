/******************************************************
hot backup tool for InnoDB
(c) 2009-2015 Percona LLC and/or its affiliates
Originally Created 3/3/2009 Yasufumi Kinoshita
Written by Alexey Kopytov, Aleksandr Kuzminsky, Stewart Smith, Vadim Tkachenko,
Yasufumi Kinoshita, Ignacio Nin and Baron Schwartz.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA

*******************************************************

This file incorporates work covered by the following copyright and
permission notice:

Copyright (c) 2000, 2011, MySQL AB & Innobase Oy. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

*******************************************************/

#include <my_global.h>
#include <mysql.h>
#include <my_sys.h>
#include <sql_plugin.h>
#include <ha_prototypes.h>
#include <srv0srv.h>
#include <string.h>
#include <limits>
#include "common.h"
#include "xtrabackup.h"
#include "xtrabackup_version.h"
#include "xb0xb.h"
#include "backup_copy.h"
#include "backup_mysql.h"
#include "mysqld.h"


char *tool_name;
char tool_args[2048];

/* mysql flavor and version */
mysql_flavor_t server_flavor = FLAVOR_UNKNOWN;
unsigned long mysql_server_version = 0;

/* server capabilities */
bool have_changed_page_bitmaps = false;
bool have_backup_locks = false;
bool have_backup_safe_binlog_info = false;
bool have_lock_wait_timeout = false;
bool have_galera_enabled = false;
bool have_flush_engine_logs = false;
bool have_multi_threaded_slave = false;
bool have_gtid_slave = false;

/* Kill long selects */
os_thread_id_t	kill_query_thread_id;
os_event_t	kill_query_thread_started;
os_event_t	kill_query_thread_stopped;
os_event_t	kill_query_thread_stop;

bool sql_thread_started = false;
std::string mysql_slave_position;
char *mysql_binlog_position = NULL;
char *buffer_pool_filename = NULL;
static char *backup_uuid = NULL;

/* History on server */
time_t history_start_time;
time_t history_end_time;
time_t history_lock_time;

/* Stream type name, to be used with xtrabackup_stream_fmt */
const char *xb_stream_format_name[] = {"file", "tar", "xbstream"};

MYSQL *mysql_connection;

/* Whether LOCK BINLOG FOR BACKUP has been issued during backup */
static bool binlog_locked;

/* Whether LOCK TABLES FOR BACKUP / FLUSH TABLES WITH READ LOCK has been issued
during backup */
static bool tables_locked;

/* buffer pool dump */
ssize_t innodb_buffer_pool_dump_start_time;
static int original_innodb_buffer_pool_dump_pct;
static bool innodb_buffer_pool_dump;
static bool innodb_buffer_pool_dump_pct;


extern "C" {
MYSQL * STDCALL
cli_mysql_real_connect(MYSQL *mysql,const char *host, const char *user,
		       const char *passwd, const char *db,
		       uint port, const char *unix_socket,ulong client_flag);
}

#define mysql_real_connect cli_mysql_real_connect


MYSQL *
xb_mysql_connect()
{
	MYSQL *connection = mysql_init(NULL);
	char mysql_port_str[std::numeric_limits<int>::digits10 + 3];

	sprintf(mysql_port_str, "%d", opt_port);

	if (connection == NULL) {
		msg("Failed to init MySQL struct: %s.\n",
			mysql_error(connection));
		return(NULL);
	}

	msg_ts("Connecting to MySQL server host: %s, user: %s, password: %s, "
	       "port: %s, socket: %s\n", opt_host ? opt_host : "localhost",
	       opt_user ? opt_user : "not set",
	       opt_password ? "set" : "not set",
	       opt_port != 0 ? mysql_port_str : "not set",
	       opt_socket ? opt_socket : "not set");

#ifdef HAVE_OPENSSL
	/*
	Print a warning if explicitly defined combination of --ssl-mode other than
	VERIFY_CA or VERIFY_IDENTITY with explicit --ssl-ca or --ssl-capath values.
	*/
	if (ssl_mode_set_explicitly &&
	    opt_ssl_mode < SSL_MODE_VERIFY_CA &&
	    (opt_ssl_ca || opt_ssl_capath))
	{
		msg("WARNING: no verification of server certificate will "
		       "be done. Use --ssl-mode=VERIFY_CA or "
		       "VERIFY_IDENTITY.\n");
	}

	/* Set SSL parameters: key, cert, ca, capath, cipher, clr, clrpath. */
	if (opt_ssl_mode >= SSL_MODE_VERIFY_CA)
		mysql_ssl_set(connection, opt_ssl_key, opt_ssl_cert, opt_ssl_ca,
			      opt_ssl_capath, opt_ssl_cipher);
	else
		mysql_ssl_set(connection, opt_ssl_key, opt_ssl_cert, NULL,
			      NULL, opt_ssl_cipher);
	mysql_options(connection, MYSQL_OPT_SSL_CRL, opt_ssl_crl);
	mysql_options(connection, MYSQL_OPT_SSL_CRLPATH, opt_ssl_crlpath);
	mysql_options(connection, MYSQL_OPT_TLS_VERSION, opt_tls_version);
	mysql_options(connection, MYSQL_OPT_SSL_MODE, &opt_ssl_mode);
#endif

	if (!mysql_real_connect(connection,
				opt_host ? opt_host : "localhost",
				opt_user,
				opt_password,
				"" /*database*/, opt_port,
				opt_socket, 0)) {
		msg("Failed to connect to MySQL server: %s.\n",
			mysql_error(connection));
		mysql_close(connection);
		return(NULL);
	}

	xb_mysql_query(connection, "SET SESSION wait_timeout=2147483",
		       false, true);

	xb_mysql_query(connection, "SET SESSION autocommit=1",
		       false, true);

	xb_mysql_query(connection, "SET NAMES utf8",
		       false, true);

	return(connection);
}

/*********************************************************************//**
Execute mysql query. */
MYSQL_RES *
xb_mysql_query(MYSQL *connection, const char *query, bool use_result,
		bool die_on_error)
{
	MYSQL_RES *mysql_result = NULL;

	if (mysql_query(connection, query)) {
		msg("Error: failed to execute query '%s': %u (%s) %s\n", query,
		    mysql_errno(connection),
		    mysql_errno_to_sqlstate(mysql_errno(connection)),
		    mysql_error(connection));
		if (die_on_error) {
			exit(EXIT_FAILURE);
		}
		return(NULL);
	}

	/* store result set on client if there is a result */
	if (mysql_field_count(connection) > 0) {
		if ((mysql_result = mysql_store_result(connection)) == NULL) {
			msg("Error: failed to fetch query result %s: %s\n",
			    query, mysql_error(connection));
			exit(EXIT_FAILURE);
		}

		if (!use_result) {
			mysql_free_result(mysql_result);
		}
	}

	return mysql_result;
}

my_ulonglong
xb_mysql_numrows(MYSQL *connection, const char *query, bool die_on_error)
{
	my_ulonglong rows_count = 0;
	MYSQL_RES *result = xb_mysql_query(connection, query, true,
		die_on_error);
	if (result) {
		rows_count = mysql_num_rows(result);
		mysql_free_result(result);
	}
	return rows_count;
}


struct mysql_variable {
	const char *name;
	char **value;
};

/*********************************************************************//**
Read mysql_variable from MYSQL_RES, return number of rows consumed. */
static
int
read_mysql_variables_from_result(MYSQL_RES *mysql_result, mysql_variable *vars,
	bool vertical_result)
{
	MYSQL_ROW row;
	mysql_variable *var;
	ut_ad(!vertical_result || mysql_num_fields(mysql_result) == 2);
	int rows_read = 0;

	if (vertical_result) {
		while ((row = mysql_fetch_row(mysql_result))) {
			++rows_read;
			char *name = row[0];
			char *value = row[1];
			for (var = vars; var->name; var++) {
				if (strcmp(var->name, name) == 0
				    && value != NULL) {
					*(var->value) = strdup(value);
				}
			}
		}
	} else {
		MYSQL_FIELD *field;

		if ((row = mysql_fetch_row(mysql_result)) != NULL) {
			mysql_field_seek(mysql_result, 0);
			++rows_read;
			int i = 0;
			while ((field = mysql_fetch_field(mysql_result))
				!= NULL) {
				char *name = field->name;
				char *value = row[i];
				for (var = vars; var->name; var++) {
					if (strcmp(var->name, name) == 0
					    && value != NULL) {
						*(var->value) = strdup(value);
					}
				}
				++i;
			}
		}
	}
	return rows_read;
}


static
void
read_mysql_variables(MYSQL *connection, const char *query, mysql_variable *vars,
	bool vertical_result)
{
	MYSQL_RES *mysql_result = xb_mysql_query(connection, query, true);
	read_mysql_variables_from_result(mysql_result, vars, vertical_result);
	mysql_free_result(mysql_result);
}


static
void
free_mysql_variables(mysql_variable *vars)
{
	mysql_variable *var;

	for (var = vars; var->name; var++) {
		free(*(var->value));
		*var->value = NULL;
	}
}

char *
read_mysql_one_value(MYSQL *connection, const char *query)
{
	MYSQL_RES *mysql_result;
	MYSQL_ROW row;
	char *result = NULL;

	mysql_result = xb_mysql_query(connection, query, true);

	ut_ad(mysql_num_fields(mysql_result) == 1);

	if ((row = mysql_fetch_row(mysql_result))) {
		result = strdup(row[0]);
	}

	mysql_free_result(mysql_result);

	return(result);
}

/* UUID of the backup, gives same value until explicitly reset.
Returned value should NOT be free()-d. */
static
const char* get_backup_uuid(MYSQL *connection)
{
	if (!backup_uuid) {
		backup_uuid = read_mysql_one_value(connection, "SELECT UUID()");
	}
	return backup_uuid;
}

void
parse_show_engine_innodb_status(MYSQL *connection)
{
	MYSQL_RES *mysql_result;
	MYSQL_ROW row;

	mysql_result = xb_mysql_query(connection, "SHOW ENGINE INNODB STATUS",
				      true);

	ut_ad(mysql_num_fields(mysql_result) == 3);

	if ((row = mysql_fetch_row(mysql_result))) {
		std::stringstream data(row[2]);
		std::string line;

		while (std::getline(data, line)) {
			lsn_t lsn;
			if (sscanf(line.c_str(), "Log flushed up to " LSN_PF,
				   &lsn) == 1) {
				backup_redo_log_flushed_lsn = lsn;
			}
		}
	}

	mysql_free_result(mysql_result);
}

static
bool
check_server_version(unsigned long version_number,
		     const char *version_string,
		     const char *version_comment,
		     const char *innodb_version)
{
	bool version_supported = false;
	bool mysql51 = false;

	mysql_server_version = version_number;

	server_flavor = FLAVOR_UNKNOWN;
	if (strstr(version_comment, "Percona") != NULL) {
		server_flavor = FLAVOR_PERCONA_SERVER;
	} else if (strstr(version_comment, "MariaDB") != NULL ||
		   strstr(version_string, "MariaDB") != NULL) {
		server_flavor = FLAVOR_MARIADB;
	} else if (strstr(version_comment, "MySQL") != NULL) {
		server_flavor = FLAVOR_MYSQL;
	}

	mysql51 = version_number > 50100 && version_number < 50500;
	version_supported = version_supported
		|| (mysql51 && innodb_version != NULL);
	version_supported = version_supported
		|| (version_number > 50500 && version_number < 50800);
	version_supported = version_supported
		|| ((version_number > 100000 && version_number < 100300)
		    && server_flavor == FLAVOR_MARIADB);

	if (mysql51 && innodb_version == NULL) {
		msg("Error: Built-in InnoDB in MySQL 5.1 is not "
		    "supported in this release. You can either use "
		    "Percona XtraBackup 2.0, or upgrade to InnoDB "
		    "plugin.\n");
	} else if (version_number > 80000 && version_number < 90000) {
		msg("Error: MySQL 8.0 and Percona Server 8.0 are not "
		    "supported by Percona Xtrabackup 2.4.x series. "
		    "Please use Percona Xtrabackup 8.0.x for backups "
		    "and restores.\n");
	} else if (!version_supported) {
		msg("Error: Unsupported server version: '%s'. Please "
		    "report a bug at "
		    "https://jira.percona.com/projects/PXB\n",
		    version_string);
	}

	return(version_supported);
}

/*********************************************************************//**
Receive options important for XtraBackup from MySQL server.
@return	true on success. */
bool
get_mysql_vars(MYSQL *connection)
{
	char *gtid_mode_var = NULL;
	char *version_var = NULL;
	char *version_comment_var = NULL;
	char *innodb_version_var = NULL;
	char *have_backup_locks_var = NULL;
	char *have_backup_safe_binlog_info_var = NULL;
	char *log_bin_var = NULL;
	char *lock_wait_timeout_var= NULL;
	char *wsrep_on_var = NULL;
	char *slave_parallel_workers_var = NULL;
	char *gtid_slave_pos_var = NULL;
	char *innodb_buffer_pool_filename_var = NULL;
	char *datadir_var = NULL;
	char *innodb_log_group_home_dir_var = NULL;
	char *innodb_log_file_size_var = NULL;
	char *innodb_log_files_in_group_var = NULL;
	char *innodb_data_file_path_var = NULL;
	char *innodb_data_home_dir_var = NULL;
	char *innodb_undo_directory_var = NULL;
	char *innodb_page_size_var = NULL;
	char *innodb_log_checksums_var = NULL;
	char *innodb_log_checksum_algorithm_var = NULL;
	char *innodb_checksum_algorithm_var = NULL;
	char *innodb_track_changed_pages_var = NULL;
	char *server_uuid_var = NULL;

	unsigned long server_version = mysql_get_server_version(connection);

	bool ret = true;

	mysql_variable mysql_vars[] = {
		{"have_backup_locks", &have_backup_locks_var},
		{"have_backup_safe_binlog_info",
		 &have_backup_safe_binlog_info_var},
		{"log_bin", &log_bin_var},
		{"lock_wait_timeout", &lock_wait_timeout_var},
		{"gtid_mode", &gtid_mode_var},
		{"version", &version_var},
		{"version_comment", &version_comment_var},
		{"innodb_version", &innodb_version_var},
		{"wsrep_on", &wsrep_on_var},
		{"slave_parallel_workers", &slave_parallel_workers_var},
		{"gtid_slave_pos", &gtid_slave_pos_var},
		{"innodb_buffer_pool_filename",
			&innodb_buffer_pool_filename_var},
		{"datadir", &datadir_var},
		{"innodb_log_group_home_dir", &innodb_log_group_home_dir_var},
		{"innodb_log_file_size", &innodb_log_file_size_var},
		{"innodb_log_files_in_group", &innodb_log_files_in_group_var},
		{"innodb_data_file_path", &innodb_data_file_path_var},
		{"innodb_data_home_dir", &innodb_data_home_dir_var},
		{"innodb_undo_directory", &innodb_undo_directory_var},
		{"innodb_page_size", &innodb_page_size_var},
		{"innodb_log_checksums", &innodb_log_checksums_var},
		{"innodb_log_checksum_algorithm",
			&innodb_log_checksum_algorithm_var},
		{"innodb_checksum_algorithm", &innodb_checksum_algorithm_var},
		{"innodb_track_changed_pages", &innodb_track_changed_pages_var},
		{"server_uuid", &server_uuid_var},
		{NULL, NULL}
	};

	read_mysql_variables(connection, "SHOW VARIABLES",
				mysql_vars, true);

	if (have_backup_locks_var != NULL && !opt_no_backup_locks) {
		have_backup_locks = true;
	}

	if (have_backup_safe_binlog_info_var == NULL &&
	    opt_binlog_info == BINLOG_INFO_LOCKLESS) {

		msg("Error: --binlog-info=LOCKLESS is not supported by the "
		    "server\n");
		return(false);
	}

	if (lock_wait_timeout_var != NULL) {
		have_lock_wait_timeout = true;
	}

	if (wsrep_on_var != NULL) {
		have_galera_enabled = true;
	}

	/* Check server version compatibility and detect server flavor */

	if (!(ret = check_server_version(server_version, version_var,
					 version_comment_var,
					 innodb_version_var))) {
		goto out;
	}

	if (server_version > 50500) {
		have_flush_engine_logs = true;
	}

	if (slave_parallel_workers_var != NULL
		&& atoi(slave_parallel_workers_var) > 0) {
		have_multi_threaded_slave = true;
	}

	if (innodb_buffer_pool_filename_var != NULL) {
		buffer_pool_filename = strdup(innodb_buffer_pool_filename_var);
	}

	if ((gtid_mode_var && strcmp(gtid_mode_var, "ON") == 0) ||
	    (gtid_slave_pos_var && *gtid_slave_pos_var)) {
		have_gtid_slave = true;
	}

	if (opt_binlog_info == BINLOG_INFO_AUTO) {

		if (have_backup_safe_binlog_info_var != NULL &&
		    !have_gtid_slave)
			opt_binlog_info = BINLOG_INFO_LOCKLESS;
		else if (log_bin_var != NULL && !strcmp(log_bin_var, "ON"))
			opt_binlog_info = BINLOG_INFO_ON;
		else
			opt_binlog_info = BINLOG_INFO_OFF;
	}

	msg("Using server version %s\n", version_var);

	if (!(ret = detect_mysql_capabilities_for_backup())) {
		goto out;
	}

	/* make sure datadir value is the same in configuration file */
	if (check_if_param_set("datadir")) {
		if (!directory_exists(mysql_data_home, false)) {
			msg("Warning: option 'datadir' points to "
			    "nonexistent directory '%s'\n", mysql_data_home);
		}
		if (!directory_exists(datadir_var, false)) {
			msg("Warning: MySQL variable 'datadir' points to "
			    "nonexistent directory '%s'\n", datadir_var);
		}
		if (!equal_paths(mysql_data_home, datadir_var)) {
			msg("Warning: option 'datadir' has different "
				"values:\n"
				"  '%s' in defaults file\n"
				"  '%s' in SHOW VARIABLES\n",
				mysql_data_home, datadir_var);
		}
	}

	/* get some default values is they are missing from my.cnf */
	if (!check_if_param_set("datadir") && datadir_var && *datadir_var) {
		strmake(mysql_real_data_home, datadir_var, FN_REFLEN - 1);
		mysql_data_home= mysql_real_data_home;
	}

	if (!check_if_param_set("innodb_data_file_path")
	    && innodb_data_file_path_var && *innodb_data_file_path_var) {
		innobase_data_file_path = my_strdup(PSI_NOT_INSTRUMENTED,
			innodb_data_file_path_var, MYF(MY_FAE));
	}

	if (!check_if_param_set("innodb_data_home_dir")
	    && innodb_data_home_dir_var && *innodb_data_home_dir_var) {
		innobase_data_home_dir = my_strdup(PSI_NOT_INSTRUMENTED,
			innodb_data_home_dir_var, MYF(MY_FAE));
	}

	if (!check_if_param_set("innodb_log_group_home_dir")
	    && innodb_log_group_home_dir_var
	    && *innodb_log_group_home_dir_var) {
		srv_log_group_home_dir = my_strdup(PSI_NOT_INSTRUMENTED,
			innodb_log_group_home_dir_var, MYF(MY_FAE));
	}

	if (!check_if_param_set("innodb_undo_directory")
	    && innodb_undo_directory_var && *innodb_undo_directory_var) {
		srv_undo_dir = my_strdup(PSI_NOT_INSTRUMENTED,
			innodb_undo_directory_var, MYF(MY_FAE));
	}

	if (!check_if_param_set("innodb_log_files_in_group")
	    && innodb_log_files_in_group_var) {
		char *endptr;

		innobase_log_files_in_group = strtol(
			innodb_log_files_in_group_var, &endptr, 10);
		ut_ad(*endptr == 0);
	}

	if (!check_if_param_set("innodb_log_file_size")
	    && innodb_log_file_size_var) {
		char *endptr;

		innobase_log_file_size = strtoll(
			innodb_log_file_size_var, &endptr, 10);
		ut_ad(*endptr == 0);
	}

	if (!check_if_param_set("innodb_page_size") && innodb_page_size_var) {
		char *endptr;

		innobase_page_size = strtoll(
			innodb_page_size_var, &endptr, 10);
		ut_ad(*endptr == 0);
	}

	if (!innodb_log_checksum_algorithm_specified &&
		innodb_log_checksum_algorithm_var) {
		for (uint i = 0;
		     i < innodb_checksum_algorithm_typelib.count;
		     i++) {
			if (strcasecmp(innodb_log_checksum_algorithm_var,
			    innodb_checksum_algorithm_typelib.type_names[i])
			    == 0) {
				srv_log_checksum_algorithm = i;
			}
		}
	}

	if (!innodb_checksum_algorithm_specified &&
		innodb_checksum_algorithm_var) {
		for (uint i = 0;
		     i < innodb_checksum_algorithm_typelib.count;
		     i++) {
			if (strcasecmp(innodb_checksum_algorithm_var,
			    innodb_checksum_algorithm_typelib.type_names[i])
			    == 0) {
				srv_checksum_algorithm = i;
			}
		}
	}

	if (!innodb_log_checksum_algorithm_specified &&
		innodb_log_checksums_var) {
		if (strcasecmp(innodb_log_checksums_var, "ON") == 0) {
			srv_log_checksum_algorithm =
				SRV_CHECKSUM_ALGORITHM_STRICT_CRC32;
		} else {
			srv_log_checksum_algorithm =
				SRV_CHECKSUM_ALGORITHM_NONE;
		}
	}

	memset(server_uuid, 0, ENCRYPTION_SERVER_UUID_LEN + 1);
	if (server_uuid_var != NULL) {
		strncpy(server_uuid, server_uuid_var,
			ENCRYPTION_SERVER_UUID_LEN);
	}

	if (innodb_track_changed_pages_var != NULL &&
	    strcasecmp(innodb_track_changed_pages_var, "ON") == 0) {
	  have_changed_page_bitmaps = true;
	}

out:
	free_mysql_variables(mysql_vars);

	return(ret);
}

/*********************************************************************//**
Query the server to find out what backup capabilities it supports.
@return	true on success. */
bool
detect_mysql_capabilities_for_backup()
{
	if (xtrabackup_incremental) {
		/* INNODB_CHANGED_PAGES are listed in
		INFORMATION_SCHEMA.PLUGINS in MariaDB, but
		FLUSH NO_WRITE_TO_BINLOG CHANGED_PAGE_BITMAPS
		is not supported for versions below 10.1.6
		(see MDEV-7472) */
		if (server_flavor == FLAVOR_MARIADB &&
		    mysql_server_version < 100106) {
			have_changed_page_bitmaps = false;
		}
	}

	/* do some sanity checks */
	if (opt_galera_info && !have_galera_enabled) {
		msg("--galera-info is specified on the command "
		 	"line, but the server does not support Galera "
		 	"replication. Ignoring the option.\n");
		opt_galera_info = false;
	}

	if (opt_slave_info && have_multi_threaded_slave &&
	    !have_gtid_slave && !opt_safe_slave_backup) {
		msg("The --slave-info option requires GTID enabled or "
			"--safe-slave-backup option used for a multi-threaded "
			"slave.\n");
		return(false);
	}

	return(true);
}

static
bool
select_incremental_lsn_from_history(lsn_t *incremental_lsn)
{
	MYSQL_RES *mysql_result;
	MYSQL_ROW row;
	char query[1000];
	char buf[100];

	if (opt_incremental_history_name) {
		mysql_real_escape_string(mysql_connection, buf,
				opt_incremental_history_name,
				strlen(opt_incremental_history_name));
		ut_snprintf(query, sizeof(query),
			"SELECT innodb_to_lsn "
			"FROM PERCONA_SCHEMA.xtrabackup_history "
			"WHERE name = '%s' "
			"AND innodb_to_lsn IS NOT NULL "
			"ORDER BY innodb_to_lsn DESC LIMIT 1",
			buf);
	}

	if (opt_incremental_history_uuid) {
		mysql_real_escape_string(mysql_connection, buf,
				opt_incremental_history_uuid,
				strlen(opt_incremental_history_uuid));
		ut_snprintf(query, sizeof(query),
			"SELECT innodb_to_lsn "
			"FROM PERCONA_SCHEMA.xtrabackup_history "
			"WHERE uuid = '%s' "
			"AND innodb_to_lsn IS NOT NULL "
			"ORDER BY innodb_to_lsn DESC LIMIT 1",
			buf);
	}

	mysql_result = xb_mysql_query(mysql_connection, query, true);

	ut_ad(mysql_num_fields(mysql_result) == 1);
	if (!(row = mysql_fetch_row(mysql_result))) {
		msg("Error while attempting to find history record "
			"for %s %s\n",
			opt_incremental_history_uuid ? "uuid" : "name",
			opt_incremental_history_uuid ?
		    		opt_incremental_history_uuid :
		    		opt_incremental_history_name);
		return(false);
	}

	*incremental_lsn = strtoull(row[0], NULL, 10);

	mysql_free_result(mysql_result);

	msg("Found and using lsn: " LSN_PF " for %s %s\n", *incremental_lsn,
		opt_incremental_history_uuid ? "uuid" : "name",
		opt_incremental_history_uuid ?
	    		opt_incremental_history_uuid :
	    		opt_incremental_history_name);

	return(true);
}

static
const char *
eat_sql_whitespace(const char *query)
{
	bool comment = false;

	while (*query) {
		if (comment) {
			if (query[0] == '*' && query[1] == '/') {
				query += 2;
				comment = false;
				continue;
			}
			++query;
			continue;
		}
		if (query[0] == '/' && query[1] == '*') {
			query += 2;
			comment = true;
			continue;
		}
		if (strchr("\t\n\r (", query[0])) {
			++query;
			continue;
		}
		break;
	}

	return(query);
}

static
bool
is_query_from_list(const char *query, const char **list)
{
	const char **item;

	query = eat_sql_whitespace(query);

	item = list;
	while (*item) {
		if (strncasecmp(query, *item, strlen(*item)) == 0) {
			return(true);
		}
		++item;
	}

	return(false);
}

static
bool
is_query(const char *query)
{
	const char *query_list[] = {"insert", "update", "delete", "replace",
		"alter", "load", "select", "do", "handler", "call", "execute",
		"begin", NULL};

	return is_query_from_list(query, query_list);
}

static
bool
is_select_query(const char *query)
{
	const char *query_list[] = {"select", NULL};

	return is_query_from_list(query, query_list);
}

static
bool
is_update_query(const char *query)
{
	const char *query_list[] = {"insert", "update", "delete", "replace",
					"alter", "load", NULL};

	return is_query_from_list(query, query_list);
}

static
bool
have_queries_to_wait_for(MYSQL *connection, uint threshold)
{
	MYSQL_RES *result;
	MYSQL_ROW row;
	bool all_queries;

	result = xb_mysql_query(connection, "SHOW FULL PROCESSLIST", true);

	all_queries = (opt_lock_wait_query_type == QUERY_TYPE_ALL);
	while ((row = mysql_fetch_row(result)) != NULL) {
		const char	*info		= row[7];
		char		*id		= row[0];
		int		duration;

		duration = (row[5] != NULL) ? atoi(row[5]) : 0;

		if (info != NULL
		    && duration >= (int)threshold
		    && ((all_queries && is_query(info))
		    	|| is_update_query(info))) {
			msg_ts("Waiting for query %s (duration %d sec): %s",
			       id, duration, info);
			mysql_free_result(result);
			return(true);
		}
	}

	mysql_free_result(result);

	return(false);
}

static
void
kill_long_queries(MYSQL *connection, uint timeout)
{
	MYSQL_RES *result;
	MYSQL_ROW row;
	bool all_queries;
	char kill_stmt[100];

	result = xb_mysql_query(connection, "SHOW FULL PROCESSLIST", true);

	all_queries = (opt_kill_long_query_type == QUERY_TYPE_ALL);
	while ((row = mysql_fetch_row(result)) != NULL) {
		const char	*info		= row[7];
		int		duration	= atoi(row[5]);
		char		*id		= row[0];

		if (info != NULL &&
		    duration >= (int)timeout &&
		    ((all_queries && is_query(info)) ||
		    	is_select_query(info))) {
			msg_ts("Killing query %s (duration %d sec): %s\n",
			       id, duration, info);
			ut_snprintf(kill_stmt, sizeof(kill_stmt),
				    "KILL %s", id);
			xb_mysql_query(connection, kill_stmt, false, false);
		}
	}

	mysql_free_result(result);
}

static
bool
wait_for_no_updates(MYSQL *connection, uint timeout, uint threshold)
{
	time_t	start_time;

	start_time = time(NULL);

	msg_ts("Waiting %u seconds for queries running longer than %u seconds "
	       "to finish\n", timeout, threshold);

	while (time(NULL) <= (time_t)(start_time + timeout)) {
		if (!have_queries_to_wait_for(connection, threshold)) {
			return(true);
		}
		os_thread_sleep(1000000);
	}

	msg_ts("Unable to obtain lock. Please try again later.");

	return(false);
}

static
os_thread_ret_t
kill_query_thread(
/*===============*/
	void *arg __attribute__((unused)))
{
	MYSQL	*mysql;
	time_t	start_time;

	my_thread_init();

	start_time = time(NULL);

	os_event_set(kill_query_thread_started);

	msg_ts("Kill query timeout %d seconds.\n",
	       opt_kill_long_queries_timeout);

	while (time(NULL) - start_time <
				(time_t)opt_kill_long_queries_timeout) {
		if (os_event_wait_time(kill_query_thread_stop, 1000) !=
		    OS_SYNC_TIME_EXCEEDED) {
			goto stop_thread;
		}
	}

	if ((mysql = xb_mysql_connect()) == NULL) {
		msg("Error: kill query thread failed\n");
		goto stop_thread;
	}

	while (true) {
		kill_long_queries(mysql, time(NULL) - start_time);
		if (os_event_wait_time(kill_query_thread_stop, 1000) !=
		    OS_SYNC_TIME_EXCEEDED) {
			break;
		}
	}

	mysql_close(mysql);

stop_thread:
	msg_ts("Kill query thread stopped\n");

	my_thread_end();

	os_event_set(kill_query_thread_stopped);

	os_thread_exit();
	OS_THREAD_DUMMY_RETURN;
}


static
void
start_query_killer()
{
	kill_query_thread_stop    = os_event_create("kill_query_thread_stop");
	kill_query_thread_started = os_event_create("kill_query_thread_started");
	kill_query_thread_stopped = os_event_create("kill_query_thread_stopped");

	os_thread_create(kill_query_thread, NULL, &kill_query_thread_id);

	os_event_wait(kill_query_thread_started);
}

static
void
stop_query_killer()
{
	os_event_set(kill_query_thread_stop);
	os_event_wait_time(kill_query_thread_stopped, 60000);
}


/*********************************************************************//**
Function acquires a backup tables lock if supported by the server.
Allows to specify timeout in seconds for attempts to acquire the lock.
@returns true if lock acquired */
bool
lock_tables_for_backup(MYSQL *connection, int timeout, int retry_count)
{
	if (have_lock_wait_timeout) {
		char query[200];

		ut_snprintf(query, sizeof(query),
			    "SET SESSION lock_wait_timeout=%d", timeout);

		xb_mysql_query(connection, query, false);
	}

	if (have_backup_locks) {
		for (int i = 0; i <= retry_count; ++i) {
			msg_ts("Executing LOCK TABLES FOR BACKUP...\n");
			xb_mysql_query(connection, "LOCK TABLES FOR BACKUP",
				       false, false);
			uint err = mysql_errno(connection);
			if (err == ER_LOCK_WAIT_TIMEOUT) {
				os_thread_sleep(1000000);
				continue;
			}
			if (err == 0) {
				tables_locked = true;
			}
			break;
		}
		if (!tables_locked) {
			exit(EXIT_FAILURE);
		}
		return(true);
	}

	msg_ts("Error: LOCK TABLES FOR BACKUP is not supported.\n");

	return(false);
}

/*********************************************************************//**
Function acquires either a backup tables lock, if supported
by the server, or a global read lock (FLUSH TABLES WITH READ LOCK)
otherwise.
@returns true if lock acquired */
bool
lock_tables_maybe(MYSQL *connection, int timeout, int retry_count)
{
	if (tables_locked || opt_lock_ddl_per_table) {
		return(true);
	}

	if (have_backup_locks) {
		return lock_tables_for_backup(connection, timeout, retry_count);
	}

	if (have_lock_wait_timeout) {
		char query[200];

		ut_snprintf(query, sizeof(query),
			    "SET SESSION lock_wait_timeout=%d", timeout);

		xb_mysql_query(connection, query, false);
	}

	if (!opt_lock_wait_timeout && !opt_kill_long_queries_timeout) {

		/* We do first a FLUSH TABLES. If a long update is running, the
		FLUSH TABLES will wait but will not stall the whole mysqld, and
		when the long update is done the FLUSH TABLES WITH READ LOCK
		will start and succeed quickly. So, FLUSH TABLES is to lower
		the probability of a stage where both mysqldump and most client
		connections are stalled. Of course, if a second long update
		starts between the two FLUSHes, we have that bad stall.

		Option lock_wait_timeout serve the same purpose and is not
		compatible with this trick.
		*/

		msg_ts("Executing FLUSH NO_WRITE_TO_BINLOG TABLES...\n");

		xb_mysql_query(connection,
			       "FLUSH NO_WRITE_TO_BINLOG TABLES", false);
	}

	if (opt_lock_wait_timeout) {
		if (!wait_for_no_updates(connection, opt_lock_wait_timeout,
					 opt_lock_wait_threshold)) {
			return(false);
		}
	}

	msg_ts("Executing FLUSH TABLES WITH READ LOCK...\n");

	if (opt_kill_long_queries_timeout) {
		start_query_killer();
	}

	if (have_galera_enabled) {
		xb_mysql_query(connection,
				"SET SESSION wsrep_causal_reads=0", false);
	}

	xb_mysql_query(connection, "FLUSH TABLES WITH READ LOCK", false);

	if (opt_kill_long_queries_timeout) {
		stop_query_killer();
	}

	tables_locked = true;

	return(true);
}


/*********************************************************************//**
If backup locks are used, execute LOCK BINLOG FOR BACKUP provided that we are
not in the --no-lock mode and the lock has not been acquired already.
@returns true if lock acquired */
bool
lock_binlog_maybe(MYSQL *connection, int timeout, int retry_count)
{
	if (have_backup_locks && !opt_no_lock && !binlog_locked) {
		if (have_lock_wait_timeout) {
			char query[200];

			ut_snprintf(query, sizeof(query),
				    "SET SESSION lock_wait_timeout=%d",
				    timeout);
		}

		for (int i = 0; i <= retry_count; ++i) {
			msg_ts("Executing LOCK BINLOG FOR BACKUP...\n");
			xb_mysql_query(connection, "LOCK BINLOG FOR BACKUP",
				       false, false);
			uint err = mysql_errno(connection);
			if (err == ER_LOCK_WAIT_TIMEOUT) {
				os_thread_sleep(1000000);
				continue;
			}
			if (err == 0) {
				binlog_locked = true;
			}
			break;
		}
		if (!binlog_locked) {
			exit(EXIT_FAILURE);
		}

		return(true);
	}

	return(false);
}


/*********************************************************************//**
Releases either global read lock acquired with FTWRL and the binlog
lock acquired with LOCK BINLOG FOR BACKUP, depending on
the locking strategy being used */
void
unlock_all(MYSQL *connection)
{
	if (opt_debug_sleep_before_unlock) {
		msg_ts("Debug sleep for %u seconds\n",
		       opt_debug_sleep_before_unlock);
		os_thread_sleep(opt_debug_sleep_before_unlock * 1000);
	}

	if (binlog_locked) {
		msg_ts("Executing UNLOCK BINLOG\n");
		xb_mysql_query(connection, "UNLOCK BINLOG", false);
	}

	msg_ts("Executing UNLOCK TABLES\n");
	xb_mysql_query(connection, "UNLOCK TABLES", false);

	msg_ts("All tables unlocked\n");
}


static
int
get_open_temp_tables(MYSQL *connection)
{
	char *slave_open_temp_tables = NULL;
	mysql_variable status[] = {
		{"Slave_open_temp_tables", &slave_open_temp_tables},
		{NULL, NULL}
	};
	int result = false;

	read_mysql_variables(connection,
		"SHOW STATUS LIKE 'slave_open_temp_tables'", status, true);

	result = slave_open_temp_tables ? atoi(slave_open_temp_tables) : 0;

	free_mysql_variables(status);

	return(result);
}

static
char*
get_slave_coordinates(MYSQL *connection)
{
	char *relay_log_file = NULL;
	char *exec_log_pos = NULL;
	char *result = NULL;

	mysql_variable slave_coordinates[] = {
		{"Relay_Master_Log_File", &relay_log_file},
		{"Exec_Master_Log_Pos", &exec_log_pos},
		{NULL, NULL}
	};

	read_mysql_variables(connection, "SHOW SLAVE STATUS",
		slave_coordinates, false);
	ut_a(asprintf(&result, "%s\\%s", relay_log_file, exec_log_pos));
	free_mysql_variables(slave_coordinates);
	return result;
}

/*********************************************************************//**
Wait until it's safe to backup a slave.  Returns immediately if
the host isn't a slave.  Currently there's only one check:
Slave_open_temp_tables has to be zero.  Dies on timeout. */
bool
wait_for_safe_slave(MYSQL *connection)
{
	char *read_master_log_pos = NULL;
	char *slave_sql_running = NULL;
	char *curr_slave_coordinates = NULL;
	char *prev_slave_coordinates = NULL;

	const int sleep_time = 3;
	const ssize_t routine_start_time = (ssize_t)my_time(MY_WME);
	const ssize_t timeout = opt_safe_slave_backup_timeout;

	int open_temp_tables = 0;
	bool result = true;

	mysql_variable status[] = {
		{"Read_Master_Log_Pos", &read_master_log_pos},
		{"Slave_SQL_Running", &slave_sql_running},
		{NULL, NULL}
	};

	sql_thread_started = false;

	read_mysql_variables(connection, "SHOW SLAVE STATUS", status, false);

	if (!(read_master_log_pos && slave_sql_running)) {
		msg("Not checking slave open temp tables for "
			"--safe-slave-backup because host is not a slave\n");
		goto cleanup;
	}

	if (strcmp(slave_sql_running, "Yes") == 0) {
		/* Stopping slave may take significant amount of time,
		take that into account as part of total timeout.
		*/
		sql_thread_started = true;
		xb_mysql_query(connection, "STOP SLAVE SQL_THREAD", false);
	}

retry:
	open_temp_tables = get_open_temp_tables(connection);
	msg_ts("Slave open temp tables: %d\n", open_temp_tables);
	curr_slave_coordinates = get_slave_coordinates(connection);

	while (open_temp_tables &&
	       routine_start_time + timeout > (ssize_t)my_time(MY_WME)) {
		msg_ts("Starting slave SQL thread, waiting %d seconds, then "
		       "checking Slave_open_temp_tables again (%d seconds of "
		       "sleep time remaining)...\n",
		       sleep_time,
		       (int)(routine_start_time + timeout - (ssize_t)my_time(MY_WME)));
		free(prev_slave_coordinates);
		prev_slave_coordinates = curr_slave_coordinates;
		curr_slave_coordinates = NULL;

		xb_mysql_query(connection, "START SLAVE SQL_THREAD", false);
		os_thread_sleep(sleep_time * 1000000);

		curr_slave_coordinates = get_slave_coordinates(connection);
		msg_ts("Slave pos:\n\tprev: %s\n\tcurr: %s\n",
		       prev_slave_coordinates, curr_slave_coordinates);
		if (prev_slave_coordinates && curr_slave_coordinates &&
		    strcmp(prev_slave_coordinates, curr_slave_coordinates) == 0) {
			msg_ts("Slave pos hasn't moved during wait period, "
			       "not stopping the SQL thread.\n");
		}
		else {
			msg_ts("Stopping SQL thread.\n");
			xb_mysql_query(connection, "STOP SLAVE SQL_THREAD", false);
		}

		open_temp_tables = get_open_temp_tables(connection);
		msg_ts("Slave open temp tables: %d\n", open_temp_tables);
	}

	if (open_temp_tables == 0) {
		/* We are in a race here, slave might open other temp tables
		inbetween last check and stop. So we have to re-check
		and potentially retry after stopping SQL thread. */
		xb_mysql_query(connection, "STOP SLAVE SQL_THREAD", false);
		open_temp_tables = get_open_temp_tables(connection);
		if (open_temp_tables != 0) {
			goto retry;
		}

		msg_ts("Slave is safe to backup.\n");
		goto cleanup;
	}

	result = false;

	msg_ts("Slave_open_temp_tables did not become zero after "
	       "%d seconds\n", opt_safe_slave_backup_timeout);

	msg_ts("Restoring SQL thread state to %s\n",
	       sql_thread_started ? "STARTED" : "STOPPED");
	if (sql_thread_started) {
		xb_mysql_query(connection, "START SLAVE SQL_THREAD", false);
	}
	else {
		xb_mysql_query(connection, "STOP SLAVE SQL_THREAD", false);
	}

cleanup:
	free(prev_slave_coordinates);
	free(curr_slave_coordinates);
	free_mysql_variables(status);

	return(result);
}

static
size_t format_append(std::string& dest, const char *format, ...) {
	char* buffer;
	int len = 0;
	va_list ap;

	va_start(ap, format);
	ut_a((len = vasprintf(&buffer, format, ap)) != -1 );
	va_end(ap);

	dest += buffer;
	free(buffer);
	return (size_t)len;
}

/*********************************************************************//**
Retrieves MySQL binlog position of the master server in a replication
setup and saves it in a file. It also saves it in mysql_slave_position
variable. */
bool
write_slave_info(MYSQL *connection)
{
	char *master = NULL;
	char *filename = NULL;
	char *gtid_executed = NULL;
	char *position = NULL;
	char *gtid_slave_pos = NULL;
	char *auto_position = NULL;
	char *using_gtid = NULL;
	char *slave_sql_running = NULL;

	char *ptr = NULL;
	char *writable_channel_name = NULL;
	const char* channel_info = NULL;
	const size_t channel_info_maxlen = 128;
	bool result = true;
	char channel_info_buf[channel_info_maxlen];
	std::string slave_info;

	mysql_variable status[] = {
		{"Master_Host", &master},
		{"Relay_Master_Log_File", &filename},
		{"Exec_Master_Log_Pos", &position},
		{"Executed_Gtid_Set", &gtid_executed},
		{"Channel_Name", &writable_channel_name},
		{"Auto_Position", &auto_position},
		{"Using_Gtid", &using_gtid},
		{"Slave_SQL_Running", &slave_sql_running},
		{NULL, NULL}
	};

	mysql_variable variables[] = {
		{"gtid_slave_pos", &gtid_slave_pos},
		{NULL, NULL}
	};
	read_mysql_variables(connection, "SHOW VARIABLES", variables, true);

	MYSQL_RES* slave_status_res = xb_mysql_query(connection,
		"SHOW SLAVE STATUS", true, true);
	int master_index = 0;
	while (read_mysql_variables_from_result(slave_status_res, status,
		false)) {
		if (master == NULL || filename == NULL || position == NULL) {
			msg("Failed to get master binlog coordinates "
				"from SHOW SLAVE STATUS\n");
			msg("This means that the server is not a "
				"replication slave. Ignoring the --slave-info "
				"option\n");
			/* we still want to continue the backup */
			goto cleanup;
		}

		const char* channel_name = writable_channel_name;
		if (channel_name && channel_name[0] != '\0') {
			const char* clause_format = " FOR CHANNEL '%s'";
			xb_a(channel_info_maxlen >
				strlen(channel_name) + strlen(clause_format));
			snprintf(channel_info_buf, channel_info_maxlen,
				clause_format, channel_name);
			channel_info = channel_info_buf;
		} else {
			channel_name = channel_info = "";
		}

		ut_ad(!have_multi_threaded_slave || have_gtid_slave ||
			strcasecmp(slave_sql_running, "No") == 0);

		if (slave_info.capacity() == 0) {
			slave_info.reserve(4096);
		}

		++master_index;

		/* Print slave status to a file.
		If GTID mode is used, construct a CHANGE MASTER statement with
		MASTER_AUTO_POSITION and correct a gtid_purged value. */
		if (auto_position != NULL && !strcmp(auto_position, "1")) {
			/* MySQL >= 5.6 with GTID enabled */
			for (ptr = strchr(gtid_executed, '\n');
			     ptr;
			     ptr = strchr(ptr, '\n')) {
				*ptr = ' ';
			}

			if (master_index == 1) {
				format_append(slave_info,
					"SET GLOBAL gtid_purged='%s';\n",
					gtid_executed);
			}
			format_append(slave_info,
				"CHANGE MASTER TO MASTER_AUTO_POSITION=1%s;\n",
				channel_info);

			format_append(mysql_slave_position,
				"master host '%s', purge list '%s', "
				"channel name: '%s'\n",
				master, gtid_executed, channel_name);
		} else if (using_gtid && !strcasecmp(using_gtid, "yes")) {
			/* MariaDB >= 10.0 with GTID enabled */
			if (master_index == 1) {
				format_append(slave_info,
					"SET GLOBAL gtid_slave_pos = '%s';\n",
					gtid_slave_pos);
			}
			format_append(slave_info,
				"CHANGE MASTER TO master_use_gtid = slave_pos"
				"%s;\n",
				channel_info);
			format_append(mysql_slave_position,
				"master host '%s', gtid_slave_pos %s, "
				"channel name: '%s'\n",
				master, gtid_slave_pos, channel_name);
		} else {
			format_append(slave_info,
				"CHANGE MASTER TO MASTER_LOG_FILE='%s', "
				"MASTER_LOG_POS=%s%s;\n",
				filename, position, channel_info);
			format_append(mysql_slave_position,
				"master host '%s', filename '%s', "
				"position '%s', channel name: '%s'\n",
				master, filename, position,
				channel_name);
		}
		free_mysql_variables(status);
	}
	result = backup_file_print(XTRABACKUP_SLAVE_INFO, slave_info.c_str(),
			  slave_info.size());

cleanup:
	mysql_free_result(slave_status_res);
	free_mysql_variables(status);
	free_mysql_variables(variables);

	return(result);
}


/*********************************************************************//**
Retrieves MySQL Galera and
saves it in a file. It also prints it to stdout. */
bool
write_galera_info(MYSQL *connection)
{
	char *state_uuid = NULL, *state_uuid55 = NULL;
	char *last_committed = NULL, *last_committed55 = NULL;
	bool result;

	mysql_variable status[] = {
		{"Wsrep_local_state_uuid", &state_uuid},
		{"wsrep_local_state_uuid", &state_uuid55},
		{"Wsrep_last_committed", &last_committed},
		{"wsrep_last_committed", &last_committed55},
		{NULL, NULL}
	};

	/* When backup locks are supported by the server, we should skip
	creating xtrabackup_galera_info file on the backup stage, because
	wsrep_local_state_uuid and wsrep_last_committed will be inconsistent
	without blocking commits. The state file will be created on the prepare
	stage using the WSREP recovery procedure. */
	if (have_backup_locks) {
		return(true);
	}

	read_mysql_variables(connection, "SHOW STATUS", status, true);

	if ((state_uuid == NULL && state_uuid55 == NULL)
		|| (last_committed == NULL && last_committed55 == NULL)) {
		msg("Failed to get master wsrep state from SHOW STATUS.\n");
		result = false;
		goto cleanup;
	}

	result = backup_file_printf(XTRABACKUP_GALERA_INFO,
		"%s:%s\n", state_uuid ? state_uuid : state_uuid55,
			last_committed ? last_committed : last_committed55);

cleanup:
	free_mysql_variables(status);

	return(result);
}


/*********************************************************************//**
Flush and copy the current binary log file into the backup,
if GTID is enabled */
bool
write_current_binlog_file(MYSQL *connection)
{
	char *executed_gtid_set = NULL;
	char *gtid_binlog_state = NULL;
	char *log_bin_file = NULL;
	char *log_bin_dir = NULL;
	bool gtid_exists;
	bool result = true;
	char filepath[FN_REFLEN];

	mysql_variable status[] = {
		{"Executed_Gtid_Set", &executed_gtid_set},
		{NULL, NULL}
	};

	mysql_variable status_after_flush[] = {
		{"File", &log_bin_file},
		{NULL, NULL}
	};

	mysql_variable vars[] = {
		{"gtid_binlog_state", &gtid_binlog_state},
		{"log_bin_basename", &log_bin_dir},
		{NULL, NULL}
	};

	read_mysql_variables(connection, "SHOW MASTER STATUS", status, false);
	read_mysql_variables(connection, "SHOW VARIABLES", vars, true);

	gtid_exists = (executed_gtid_set && *executed_gtid_set)
			|| (gtid_binlog_state && *gtid_binlog_state);

	if (gtid_exists) {
		size_t log_bin_dir_length;

		lock_binlog_maybe(connection, opt_backup_lock_timeout,
				  opt_backup_lock_retry_count);

		xb_mysql_query(connection, "FLUSH BINARY LOGS", false);

		read_mysql_variables(connection, "SHOW MASTER STATUS",
			status_after_flush, false);

		if (opt_log_bin != NULL && strchr(opt_log_bin, FN_LIBCHAR)) {
			/* If log_bin is set, it has priority */
			if (log_bin_dir) {
				free(log_bin_dir);
			}
			log_bin_dir = strdup(opt_log_bin);
		} else if (log_bin_dir == NULL) {
			/* Default location is MySQL datadir */
			log_bin_dir = strdup("./");
		}

		dirname_part(log_bin_dir, log_bin_dir, &log_bin_dir_length);

		/* strip final slash if it is not the only path component */
		if (log_bin_dir_length > 1 &&
		    log_bin_dir[log_bin_dir_length - 1] == FN_LIBCHAR) {
			log_bin_dir[log_bin_dir_length - 1] = 0;
		}

		if (log_bin_dir == NULL || log_bin_file == NULL) {
			msg("Failed to get master binlog coordinates from "
				"SHOW MASTER STATUS");
			result = false;
			goto cleanup;
		}

		ut_snprintf(filepath, sizeof(filepath), "%s%c%s",
				log_bin_dir, FN_LIBCHAR, log_bin_file);
		result = copy_file(ds_data, filepath, log_bin_file, 0);
	}

cleanup:
	free_mysql_variables(status_after_flush);
	free_mysql_variables(status);
	free_mysql_variables(vars);

	return(result);
}


/*********************************************************************//**
Retrieves MySQL binlog position and
saves it in a file. It also prints it to stdout. */
bool
write_binlog_info(MYSQL *connection)
{
	char *filename = NULL;
	char *position = NULL;
	char *gtid_mode = NULL;
	char *gtid_current_pos = NULL;
	char *gtid_executed = NULL;
	char *gtid = NULL;
	bool result;
	bool mysql_gtid;
	bool mariadb_gtid;

	mysql_variable status[] = {
		{"File", &filename},
		{"Position", &position},
		{"Executed_Gtid_Set", &gtid_executed},
		{NULL, NULL}
	};

	mysql_variable vars[] = {
		{"gtid_mode", &gtid_mode},
		{"gtid_current_pos", &gtid_current_pos},
		{NULL, NULL}
	};

	read_mysql_variables(connection, "SHOW MASTER STATUS", status, false);
	read_mysql_variables(connection, "SHOW VARIABLES", vars, true);

	if (filename == NULL || position == NULL) {
		/* Do not create xtrabackup_binlog_info if binary
		log is disabled */
		result = true;
		goto cleanup;
	}

	mysql_gtid = ((gtid_mode != NULL) && (strcmp(gtid_mode, "ON") == 0));
	mariadb_gtid = (gtid_current_pos != NULL);

	gtid = (gtid_executed != NULL ? gtid_executed : gtid_current_pos);

	if (mariadb_gtid || mysql_gtid) {
		ut_a(asprintf(&mysql_binlog_position,
			"filename '%s', position '%s', "
			"GTID of the last change '%s'",
			filename, position, gtid) != -1);
		result = backup_file_printf(XTRABACKUP_BINLOG_INFO,
					    "%s\t%s\t%s\n", filename, position,
					    gtid);
	} else {
		ut_a(asprintf(&mysql_binlog_position,
			"filename '%s', position '%s'",
			filename, position) != -1);
		result = backup_file_printf(XTRABACKUP_BINLOG_INFO,
					    "%s\t%s\n", filename, position);
	}

cleanup:
	free_mysql_variables(status);
	free_mysql_variables(vars);

	return(result);
}

inline static
bool format_time(time_t time, char *dest, size_t max_size)
{
	tm tm;
	localtime_r(&time, &tm);
	return strftime(dest, max_size,
		 "%Y-%m-%d %H:%M:%S", &tm) != 0;
}

/*********************************************************************//**
Allocates and writes contents of xtrabackup_info into buffer;
Invoke free() on return value once you don't need it.
*/
char* get_xtrabackup_info(MYSQL *connection)
{
	const char *uuid = get_backup_uuid(connection);
	char *server_version = read_mysql_one_value(connection, "SELECT VERSION()");

	static const size_t time_buf_size = 100;
	char buf_start_time[time_buf_size];
	char buf_end_time[time_buf_size];

	format_time(history_start_time, buf_start_time, time_buf_size);
	format_time(history_end_time, buf_end_time, time_buf_size);

	ut_a(uuid);
	ut_a(server_version);
	char* result = NULL;
	asprintf(&result,
		"uuid = %s\n"
		"name = %s\n"
		"tool_name = %s\n"
		"tool_command = %s\n"
		"tool_version = %s\n"
		"ibbackup_version = %s\n"
		"server_version = %s\n"
		"start_time = %s\n"
		"end_time = %s\n"
		"lock_time = %ld\n"
		"binlog_pos = %s\n"
		"innodb_from_lsn = " LSN_PF "\n"
		"innodb_to_lsn = " LSN_PF "\n"
		"partial = %s\n"
		"incremental = %s\n"
		"format = %s\n"
		"compact = %s\n"
		"compressed = %s\n"
		"encrypted = %s\n",
		uuid, /* uuid */
		opt_history ? opt_history : "",  /* name */
		tool_name,  /* tool_name */
		tool_args,  /* tool_command */
		XTRABACKUP_VERSION,  /* tool_version */
		XTRABACKUP_VERSION,  /* ibbackup_version */
		server_version,  /* server_version */
		buf_start_time,  /* start_time */
		buf_end_time,  /* end_time */
		(long int)history_lock_time, /* lock_time */
		mysql_binlog_position ?
			mysql_binlog_position : "", /* binlog_pos */
		incremental_lsn, /* innodb_from_lsn */
		metadata_to_lsn, /* innodb_to_lsn */
		(xtrabackup_tables /* partial */
		 || xtrabackup_tables_exclude
		 || xtrabackup_tables_file
		 || xtrabackup_databases
		 || xtrabackup_databases_exclude
		 || xtrabackup_databases_file) ? "Y" : "N",
		xtrabackup_incremental ? "Y" : "N", /* incremental */
		xb_stream_format_name[xtrabackup_stream_fmt], /* format */
		xtrabackup_compact ? "Y" : "N", /* compact */
		xtrabackup_compress ? "compressed" : "N", /* compressed */
		xtrabackup_encrypt ? "Y" : "N"); /* encrypted */

	free(server_version);
	return result;
}



/*********************************************************************//**
Writes xtrabackup_info file and if backup_history is enable creates
PERCONA_SCHEMA.xtrabackup_history and writes a new history record to the
table containing all the history info particular to the just completed
backup. */
bool
write_xtrabackup_info(MYSQL *connection)
{
	MYSQL_STMT *stmt;
	MYSQL_BIND bind[19];
	const char *uuid = NULL;
	char *server_version = NULL;
	char* xtrabackup_info_data = NULL;
	int idx;
	my_bool null = TRUE;

	const char *ins_query = "insert into PERCONA_SCHEMA.xtrabackup_history("
		"uuid, name, tool_name, tool_command, tool_version, "
		"ibbackup_version, server_version, start_time, end_time, "
		"lock_time, binlog_pos, innodb_from_lsn, innodb_to_lsn, "
		"partial, incremental, format, compact, compressed, "
		"encrypted) "
		"values(?,?,?,?,?,?,?,from_unixtime(?),from_unixtime(?),"
		"?,?,?,?,?,?,?,?,?,?)";

	ut_ad((uint)xtrabackup_stream_fmt <
		array_elements(xb_stream_format_name));
	const char *stream_format_name =
		xb_stream_format_name[xtrabackup_stream_fmt];
	history_end_time = time(NULL);

	xtrabackup_info_data = get_xtrabackup_info(connection);
	if (!backup_file_printf(XTRABACKUP_INFO, "%s", xtrabackup_info_data)) {
		goto cleanup;
	}

	if (!opt_history) {
		goto cleanup;
	}

	uuid = get_backup_uuid(connection);
	server_version = read_mysql_one_value(connection, "SELECT VERSION()");

	xb_mysql_query(connection,
		"CREATE DATABASE IF NOT EXISTS PERCONA_SCHEMA", false);
	xb_mysql_query(connection,
		"CREATE TABLE IF NOT EXISTS PERCONA_SCHEMA.xtrabackup_history("
		"uuid VARCHAR(40) NOT NULL PRIMARY KEY,"
		"name VARCHAR(255) DEFAULT NULL,"
		"tool_name VARCHAR(255) DEFAULT NULL,"
		"tool_command TEXT DEFAULT NULL,"
		"tool_version VARCHAR(255) DEFAULT NULL,"
		"ibbackup_version VARCHAR(255) DEFAULT NULL,"
		"server_version VARCHAR(255) DEFAULT NULL,"
		"start_time TIMESTAMP NULL DEFAULT NULL,"
		"end_time TIMESTAMP NULL DEFAULT NULL,"
		"lock_time BIGINT UNSIGNED DEFAULT NULL,"
		"binlog_pos VARCHAR(128) DEFAULT NULL,"
		"innodb_from_lsn BIGINT UNSIGNED DEFAULT NULL,"
		"innodb_to_lsn BIGINT UNSIGNED DEFAULT NULL,"
		"partial ENUM('Y', 'N') DEFAULT NULL,"
		"incremental ENUM('Y', 'N') DEFAULT NULL,"
		"format ENUM('file', 'tar', 'xbstream') DEFAULT NULL,"
		"compact ENUM('Y', 'N') DEFAULT NULL,"
		"compressed ENUM('Y', 'N') DEFAULT NULL,"
		"encrypted ENUM('Y', 'N') DEFAULT NULL"
		") CHARACTER SET utf8 ENGINE=innodb", false);

	stmt = mysql_stmt_init(connection);

	mysql_stmt_prepare(stmt, ins_query, strlen(ins_query));

	memset(bind, 0, sizeof(bind));
	idx = 0;

	/* uuid */
	bind[idx].buffer_type = MYSQL_TYPE_STRING;
	bind[idx].buffer = (char*)uuid;
	bind[idx].buffer_length = strlen(uuid);
	++idx;

	/* name */
	bind[idx].buffer_type = MYSQL_TYPE_STRING;
	bind[idx].buffer = (char*)(opt_history);
	bind[idx].buffer_length = strlen(opt_history);
	if (!(opt_history && *opt_history)) {
		bind[idx].is_null = &null;
	}
	++idx;

	/* tool_name */
	bind[idx].buffer_type = MYSQL_TYPE_STRING;
	bind[idx].buffer = tool_name;
	bind[idx].buffer_length = strlen(tool_name);
	++idx;

	/* tool_command */
	bind[idx].buffer_type = MYSQL_TYPE_STRING;
	bind[idx].buffer = tool_args;
	bind[idx].buffer_length = strlen(tool_args);
	++idx;

	/* tool_version */
	bind[idx].buffer_type = MYSQL_TYPE_STRING;
	bind[idx].buffer = (char*)(XTRABACKUP_VERSION);
	bind[idx].buffer_length = strlen(XTRABACKUP_VERSION);
	++idx;

	/* ibbackup_version */
	bind[idx].buffer_type = MYSQL_TYPE_STRING;
	bind[idx].buffer = (char*)(XTRABACKUP_VERSION);
	bind[idx].buffer_length = strlen(XTRABACKUP_VERSION);
	++idx;

	/* server_version */
	bind[idx].buffer_type = MYSQL_TYPE_STRING;
	bind[idx].buffer = server_version;
	bind[idx].buffer_length = strlen(server_version);
	++idx;

	/* start_time */
	bind[idx].buffer_type = MYSQL_TYPE_LONG;
	bind[idx].buffer = &history_start_time;
	++idx;

	/* end_time */
	bind[idx].buffer_type = MYSQL_TYPE_LONG;
	bind[idx].buffer = &history_end_time;
	++idx;

	/* lock_time */
	bind[idx].buffer_type = MYSQL_TYPE_LONG;
	bind[idx].buffer = &history_lock_time;
	++idx;

	/* binlog_pos */
	bind[idx].buffer_type = MYSQL_TYPE_STRING;
	bind[idx].buffer = mysql_binlog_position;
	if (mysql_binlog_position != NULL) {
		bind[idx].buffer_length = strlen(mysql_binlog_position);
	} else {
		bind[idx].is_null = &null;
	}
	++idx;

	/* innodb_from_lsn */
	bind[idx].buffer_type = MYSQL_TYPE_LONGLONG;
	bind[idx].buffer = (char*)(&incremental_lsn);
	++idx;

	/* innodb_to_lsn */
	bind[idx].buffer_type = MYSQL_TYPE_LONGLONG;
	bind[idx].buffer = (char*)(&metadata_to_lsn);
	++idx;

	/* partial (Y | N) */
	bind[idx].buffer_type = MYSQL_TYPE_STRING;
	bind[idx].buffer = (char*)((xtrabackup_tables
				    || xtrabackup_tables_exclude
				    || xtrabackup_tables_file
				    || xtrabackup_databases
				    || xtrabackup_databases_exclude
				    || xtrabackup_databases_file) ? "Y" : "N");
	bind[idx].buffer_length = 1;
	++idx;

	/* incremental (Y | N) */
	bind[idx].buffer_type = MYSQL_TYPE_STRING;
	bind[idx].buffer = (char*)(
		(xtrabackup_incremental
		 || xtrabackup_incremental_basedir
		 || opt_incremental_history_name
		 || opt_incremental_history_uuid) ? "Y" : "N");
	bind[idx].buffer_length = 1;
	++idx;

	/* format (file | tar | xbstream) */
	bind[idx].buffer_type = MYSQL_TYPE_STRING;
	bind[idx].buffer = (char*)(stream_format_name);
	bind[idx].buffer_length = strlen(stream_format_name);
	++idx;

	/* compact (Y | N) */
	bind[idx].buffer_type = MYSQL_TYPE_STRING;
	bind[idx].buffer = (char*)(xtrabackup_compact ? "Y" : "N");
	bind[idx].buffer_length = 1;
	++idx;

	/* compressed (Y | N) */
	bind[idx].buffer_type = MYSQL_TYPE_STRING;
	bind[idx].buffer = (char*)(xtrabackup_compress ? "Y" : "N");
	bind[idx].buffer_length = 1;
	++idx;

	/* encrypted (Y | N) */
	bind[idx].buffer_type = MYSQL_TYPE_STRING;
	bind[idx].buffer = (char*)(xtrabackup_encrypt ? "Y" : "N");
	bind[idx].buffer_length = 1;
	++idx;

	ut_ad(idx == 19);

	mysql_stmt_bind_param(stmt, bind);

	mysql_stmt_execute(stmt);
	mysql_stmt_close(stmt);

cleanup:

	free(xtrabackup_info_data);
	free(server_version);

	return(true);
}

bool
write_backup_config_file()
{
	std::ostringstream s;

	s << "# This MySQL options file was generated by innobackupex.\n\n"
	  << "# The MySQL server\n"
	  << "[mysqld]\n"
	  << "innodb_checksum_algorithm="
	  << innodb_checksum_algorithm_names[srv_checksum_algorithm] << "\n"
	  << "innodb_log_checksum_algorithm="
	  << innodb_checksum_algorithm_names[srv_log_checksum_algorithm] << "\n"
	  << "innodb_data_file_path=" << innobase_data_file_path << "\n"
	  << "innodb_log_files_in_group=" << srv_n_log_files << "\n"
	  << "innodb_log_file_size=" << innobase_log_file_size << "\n"
	  << "innodb_fast_checksum="
	  << (srv_fast_checksum ? "true" : "false") << "\n"
	  << "innodb_page_size=" << srv_page_size << "\n"
	  << "innodb_log_block_size=" << srv_log_block_size << "\n"
	  << "innodb_undo_directory=" << srv_undo_dir << "\n"
	  << "innodb_undo_tablespaces=" << srv_undo_tablespaces << "\n"
	  << "server_id=" << server_id << "\n"
	  << "redo_log_version=" << redo_log_version << "\n";

	if (innobase_doublewrite_file) {
		s << "innodb_doublewrite_file="
		  << innobase_doublewrite_file << "\n";
	}

	if (innobase_buffer_pool_filename) {
		s << "innodb_buffer_pool_filename="
		  << innobase_buffer_pool_filename << "\n";
	}

	I_List_iterator<i_string> iter(*opt_plugin_load_list_ptr);
	i_string *item;
	while ((item = iter++) != NULL) {
		s << "plugin_load=" << item->ptr << "\n";
	}

	if (server_uuid[0] != 0) {
		s << "server_uuid=" << server_uuid << "\n";
	}
	s << "master_key_id=" << Encryption::master_key_id << "\n";

	return backup_file_print("backup-my.cnf", s.str().c_str(), s.tellp());
}


static
char *make_argv(char *buf, size_t len, int argc, char **argv)
{
	size_t left= len;
	const char *arg;

	buf[0]= 0;
	++argv; --argc;
	while (argc > 0 && left > 0)
	{
		arg = *argv;
		if (strncmp(*argv, "--password", strlen("--password")) == 0) {
			arg = "--password=...";
		}
		if (strncmp(*argv, "--encrypt-key",
				strlen("--encrypt-key")) == 0) {
			arg = "--encrypt-key=...";
		}
		if (strncmp(*argv, "--encrypt_key",
				strlen("--encrypt_key")) == 0) {
			arg = "--encrypt_key=...";
		}
		left-= ut_snprintf(buf + len - left, left,
			"%s%c", arg, argc > 1 ? ' ' : 0);
		++argv; --argc;
	}

	return buf;
}

void
capture_tool_command(int argc, char **argv)
{
	/* capture tool name tool args */
	tool_name = strrchr(argv[0], '/');
	tool_name = tool_name ? tool_name + 1 : argv[0];

	make_argv(tool_args, sizeof(tool_args), argc, argv);
}


bool
select_history()
{
	if (opt_incremental_history_name || opt_incremental_history_uuid) {
		if (!select_incremental_lsn_from_history(
			&incremental_lsn)) {
			return(false);
		}
	}
	return(true);
}

bool
flush_changed_page_bitmaps()
{
	if (xtrabackup_incremental && have_changed_page_bitmaps &&
	    !xtrabackup_incremental_force_scan) {
		xb_mysql_query(mysql_connection,
			"FLUSH NO_WRITE_TO_BINLOG CHANGED_PAGE_BITMAPS", false);
	}
	return(true);
}


/*********************************************************************//**
Deallocate memory, disconnect from MySQL server, etc.
@return	true on success. */
void
backup_cleanup()
{
	free(mysql_binlog_position);
	free(buffer_pool_filename);
	free(backup_uuid);
	backup_uuid = NULL;

	if (mysql_connection) {
		mysql_close(mysql_connection);
	}
}

static ib_mutex_t mdl_lock_con_mutex;
static MYSQL *mdl_con = NULL;

void
mdl_lock_init()
{
	mutex_create(LATCH_ID_XTRA_DATAFILES_ITER_MUTEX, &mdl_lock_con_mutex);

	mdl_con = xb_mysql_connect();

	if (mdl_con != NULL) {
		xb_mysql_query(mdl_con, "BEGIN", false, true);
	}
}


void
mdl_lock_table(ulint space_id)
{
	MYSQL_RES *mysql_result = NULL;
	MYSQL_ROW row;
	char *query;

	mutex_enter(&mdl_lock_con_mutex);

	xb_a(asprintf(&query,
		"SELECT NAME FROM INFORMATION_SCHEMA.INNODB_SYS_TABLES "
		"WHERE SPACE = %lu", space_id));

	mysql_result = xb_mysql_query(mdl_con, query, true);

	while ((row = mysql_fetch_row(mysql_result))) {
		char *lock_query;
		char table_name[MAX_FULL_NAME_LEN + 1];

		innobase_format_name(table_name, sizeof(table_name), row[0]);

		msg_ts("Locking MDL for %s\n", table_name);

		xb_a(asprintf(&lock_query,
			"SELECT * FROM %s LIMIT 1",
			table_name));

		xb_mysql_query(mdl_con, lock_query, false, false);

		free(lock_query);
	}

	mysql_free_result(mysql_result);
	free(query);

	mutex_exit(&mdl_lock_con_mutex);
}


void
mdl_unlock_all()
{
	msg_ts("Unlocking MDL for all tables");

	xb_mysql_query(mdl_con, "COMMIT", false, true);

	mutex_free(&mdl_lock_con_mutex);
}

bool
has_innodb_buffer_pool_dump()
{
	if ((server_flavor == FLAVOR_PERCONA_SERVER ||
		server_flavor == FLAVOR_MYSQL) &&
		mysql_server_version >= 50603) {
		return(true);
	}

	if (server_flavor == FLAVOR_MARIADB && mysql_server_version >= 10000) {
		return(true);
	}

	msg_ts("Server has no support for innodb_buffer_pool_dump_now");
	return(false);
}

bool
has_innodb_buffer_pool_dump_pct()
{
	if ((server_flavor == FLAVOR_PERCONA_SERVER ||
		server_flavor == FLAVOR_MYSQL) &&
		mysql_server_version >= 50702) {
		return(true);
	}

	if (server_flavor == FLAVOR_MARIADB && mysql_server_version >= 10110) {
		return(true);
	}

	return(false);
}

void
dump_innodb_buffer_pool(MYSQL *connection)
{
	innodb_buffer_pool_dump = has_innodb_buffer_pool_dump();
	innodb_buffer_pool_dump_pct = has_innodb_buffer_pool_dump_pct();
	if (!innodb_buffer_pool_dump) {
		return;
	}

	innodb_buffer_pool_dump_start_time = (ssize_t)my_time(MY_WME);

	char *buf_innodb_buffer_pool_dump_pct;
	char change_bp_dump_pct_query[100];

	/* Verify if we need to change innodb_buffer_pool_dump_pct */
	if (opt_dump_innodb_buffer_pool_pct != 0 &&
		innodb_buffer_pool_dump_pct) {
			mysql_variable variables[] = {
				{"innodb_buffer_pool_dump_pct",
				&buf_innodb_buffer_pool_dump_pct},
				{NULL, NULL}
			};
			read_mysql_variables(connection,
				"SHOW GLOBAL VARIABLES "
				"LIKE 'innodb_buffer_pool_dump_pct'",
				variables, true);

			original_innodb_buffer_pool_dump_pct =
					atoi(buf_innodb_buffer_pool_dump_pct);

			free_mysql_variables(variables);
			ut_snprintf(change_bp_dump_pct_query,
				sizeof(change_bp_dump_pct_query),
				"SET GLOBAL innodb_buffer_pool_dump_pct = %u",
				opt_dump_innodb_buffer_pool_pct);
			msg_ts("Executing %s \n", change_bp_dump_pct_query);
			xb_mysql_query(mysql_connection,
				change_bp_dump_pct_query, false);
	}

	msg_ts("Executing SET GLOBAL innodb_buffer_pool_dump_now=ON...\n");
	xb_mysql_query(mysql_connection,
		"SET GLOBAL innodb_buffer_pool_dump_now=ON;", false);
}

void
check_dump_innodb_buffer_pool(MYSQL *connection)
{
	if (!innodb_buffer_pool_dump) {
		return;
	}
	const ssize_t timeout = opt_dump_innodb_buffer_pool_timeout;

	char *innodb_buffer_pool_dump_status;
	char change_bp_dump_pct_query[100];


	mysql_variable status[] = {
			{"Innodb_buffer_pool_dump_status",
			&innodb_buffer_pool_dump_status},
			{NULL, NULL}
	};

	read_mysql_variables(connection,
		"SHOW STATUS LIKE "
		"'Innodb_buffer_pool_dump_status'",
		status, true);

	/* check if dump has been completed */
	msg_ts("Checking if InnoDB buffer pool dump has completed\n");
	while (!strstr(innodb_buffer_pool_dump_status,
					"dump completed at")) {
		if (innodb_buffer_pool_dump_start_time +
				timeout < (ssize_t)my_time(MY_WME)){
			msg_ts("InnoDB Buffer Pool Dump was not completed "
				"after %d seconds... Adjust "
				"--dump-innodb-buffer-pool-timeout if you "
				"need higher wait time before copying %s.\n",
				opt_dump_innodb_buffer_pool_timeout,
				buffer_pool_filename);
			break;
		}

		read_mysql_variables(connection,
			"SHOW STATUS LIKE 'Innodb_buffer_pool_dump_status'",
			status, true);

		os_thread_sleep(1000000);
	}

	free_mysql_variables(status);

	/* restore original innodb_buffer_pool_dump_pct */
	if (opt_dump_innodb_buffer_pool_pct != 0 &&
			innodb_buffer_pool_dump_pct) {
		ut_snprintf(change_bp_dump_pct_query,
			sizeof(change_bp_dump_pct_query),
			"SET GLOBAL innodb_buffer_pool_dump_pct = %u",
			original_innodb_buffer_pool_dump_pct);
		xb_mysql_query(mysql_connection,
			change_bp_dump_pct_query, false);
	}
}
