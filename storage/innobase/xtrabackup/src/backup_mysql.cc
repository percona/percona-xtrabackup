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

#include <ha_prototypes.h>
#include <my_rapidjson_size_t.h>
#include <my_sys.h>
#include <my_systime.h>
#include <mysql.h>
#include <os0thread-create.h>
#include <rapidjson/document.h>
#include <rpl_log_encryption.h>
#include <sql_list.h>
#include <sql_plugin.h>
#include <srv0srv.h>
#include <string.h>
#include <fstream>
#include <limits>
#include "backup_copy.h"
#include "common.h"
#include "keyring_plugins.h"
#include "mysqld.h"
#include "os0event.h"
#include "rpl_log_encryption.h"
#include "space_map.h"
#include "typelib.h"
#include "xb0xb.h"
#include "xtrabackup.h"
#include "xtrabackup_version.h"

#include "backup_mysql.h"
#include "fsp0fsp.h"
#include "xb_regex.h"

extern uint opt_ssl_mode;
extern bool opt_no_server_version_check;
extern char *opt_ssl_ca;
extern char *opt_ssl_capath;
extern char *opt_ssl_cert;
extern char *opt_ssl_cipher;
extern char *opt_ssl_key;
extern char *opt_ssl_crl;
extern char *opt_ssl_crlpath;
extern char *opt_tls_version;
extern ulong opt_ssl_fips_mode;
extern bool ssl_mode_set_explicitly;

/** Possible values for system variable "innodb_checksum_algorithm". */
extern const char *innodb_checksum_algorithm_names[];

/** Used to define an enumerate type of the system variable
innodb_checksum_algorithm. */
extern TYPELIB innodb_checksum_algorithm_typelib;

/** Names of allowed values of innodb_flush_method */
extern const char *innodb_flush_method_names[];

/** Enumeration of innodb_flush_method */
extern TYPELIB innodb_flush_method_typelib;

char *tool_name;
char tool_args[2048];

/* mysql flavor and version */
mysql_flavor_t server_flavor = FLAVOR_UNKNOWN;
unsigned long mysql_server_version = 0;

/* server capabilities */
bool have_changed_page_bitmaps = false;
bool have_backup_locks = false;
bool have_lock_wait_timeout = false;
bool have_galera_enabled = false;
bool have_flush_engine_logs = false;
bool have_multi_threaded_slave = false;
bool have_gtid_slave = false;
bool have_unsafe_ddl_tables = false;
bool have_rocksdb = false;

bool slave_auto_position = false;

/* Kill long selects */
os_thread_id_t kill_query_thread_id;
os_event_t kill_query_thread_started;
os_event_t kill_query_thread_stopped;
os_event_t kill_query_thread_stop;

bool sql_thread_started = false;
std::string mysql_slave_position;
std::string mysql_binlog_position;
char *buffer_pool_filename = NULL;
static char *backup_uuid = NULL;

/* History on server */
time_t history_start_time;
time_t history_end_time;
time_t history_lock_time;

/* Stream type name, to be used with xtrabackup_stream_fmt */
const char *xb_stream_format_name[] = {"file", "xbstream"};

MYSQL *mysql_connection;

/* Whether LOCK TABLES FOR BACKUP / FLUSH TABLES WITH READ LOCK has been issued
during backup */
static bool tables_locked = false;
static bool instance_locked = false;

/* buffer pool dump */
ssize_t innodb_buffer_pool_dump_start_time;
static int original_innodb_buffer_pool_dump_pct;
static bool innodb_buffer_pool_dump;
static bool innodb_buffer_pool_dump_pct;

MYSQL *xb_mysql_connect() {
  MYSQL *connection = mysql_init(NULL);
  char mysql_port_str[std::numeric_limits<int>::digits10 + 3];

  sprintf(mysql_port_str, "%d", opt_port);

  if (connection == NULL) {
    msg("Failed to init MySQL struct: %s.\n", mysql_error(connection));
    return (NULL);
  }

  msg_ts(
      "Connecting to MySQL server host: %s, user: %s, password: %s, "
      "port: %s, socket: %s\n",
      opt_host ? opt_host : "localhost", opt_user ? opt_user : "not set",
      opt_password ? "set" : "not set",
      opt_port != 0 ? mysql_port_str : "not set",
      opt_socket ? opt_socket : "not set");

#ifdef HAVE_OPENSSL
  /*
  Print a warning if explicitly defined combination of --ssl-mode other than
  VERIFY_CA or VERIFY_IDENTITY with explicit --ssl-ca or --ssl-capath values.
  */
  if (ssl_mode_set_explicitly && opt_ssl_mode < SSL_MODE_VERIFY_CA &&
      (opt_ssl_ca || opt_ssl_capath)) {
    msg("WARNING: no verification of server certificate will "
        "be done. Use --ssl-mode=VERIFY_CA or "
        "VERIFY_IDENTITY.\n");
  }

  /* Set SSL parameters: key, cert, ca, capath, cipher, clr, clrpath. */
  if (opt_ssl_mode >= SSL_MODE_VERIFY_CA)
    mysql_ssl_set(connection, opt_ssl_key, opt_ssl_cert, opt_ssl_ca,
                  opt_ssl_capath, opt_ssl_cipher);
  else
    mysql_ssl_set(connection, opt_ssl_key, opt_ssl_cert, NULL, NULL,
                  opt_ssl_cipher);
  mysql_options(connection, MYSQL_OPT_SSL_CRL, opt_ssl_crl);
  mysql_options(connection, MYSQL_OPT_SSL_CRLPATH, opt_ssl_crlpath);
  mysql_options(connection, MYSQL_OPT_TLS_VERSION, opt_tls_version);
  mysql_options(connection, MYSQL_OPT_SSL_MODE, &opt_ssl_mode);
#endif

  if (!mysql_real_connect(connection, opt_host ? opt_host : "localhost",
                          opt_user, opt_password, "" /*database*/, opt_port,
                          opt_socket, 0)) {
    msg("Failed to connect to MySQL server: %s.\n", mysql_error(connection));
    mysql_close(connection);
    return (NULL);
  }

  xb_mysql_query(connection, "SET SESSION wait_timeout=2147483", false, true);

  xb_mysql_query(connection, "SET SESSION autocommit=1", false, true);

  xb_mysql_query(connection, "SET NAMES utf8", false, true);

  return (connection);
}

/*********************************************************************/ /**
 Execute mysql query. */
MYSQL_RES *xb_mysql_query(MYSQL *connection, const char *query, bool use_result,
                          bool die_on_error) {
  MYSQL_RES *mysql_result = NULL;

  if (mysql_query(connection, query)) {
    msg("Error: failed to execute query '%s': %u (%s) %s\n", query,
        mysql_errno(connection),
        mysql_errno_to_sqlstate(mysql_errno(connection)),
        mysql_error(connection));
    if (die_on_error) {
      exit(EXIT_FAILURE);
    }
    return (NULL);
  }

  /* store result set on client if there is a result */
  if (mysql_field_count(connection) > 0) {
    if ((mysql_result = mysql_store_result(connection)) == NULL) {
      msg("Error: failed to fetch query result %s: %s\n", query,
          mysql_error(connection));
      if (die_on_error) {
        exit(EXIT_FAILURE);
      }
    }

    if (!use_result) {
      mysql_free_result(mysql_result);
    }
  }

  return mysql_result;
}

my_ulonglong xb_mysql_numrows(MYSQL *connection, const char *query,
                              bool die_on_error) {
  my_ulonglong rows_count = 0;
  MYSQL_RES *result = xb_mysql_query(connection, query, true, die_on_error);
  if (result) {
    rows_count = mysql_num_rows(result);
    mysql_free_result(result);
  }
  return rows_count;
}

/*********************************************************************/ /**
 Read mysql_variable from MYSQL_RES, return number of rows consumed. */
static int read_mysql_variables_from_result(MYSQL_RES *mysql_result,
                                            mysql_variable *vars,
                                            bool vertical_result) {
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
        if (strcasecmp(var->name, name) == 0 && value != NULL) {
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
      while ((field = mysql_fetch_field(mysql_result)) != NULL) {
        char *name = field->name;
        char *value = row[i];
        for (var = vars; var->name; var++) {
          if (strcasecmp(var->name, name) == 0 && value != NULL) {
            *(var->value) = strdup(value);
          }
        }
        ++i;
      }
    }
  }
  return rows_read;
}

void read_mysql_variables(MYSQL *connection, const char *query,
                          mysql_variable *vars, bool vertical_result) {
  MYSQL_RES *mysql_result = xb_mysql_query(connection, query, true);
  read_mysql_variables_from_result(mysql_result, vars, vertical_result);
  mysql_free_result(mysql_result);
}

void free_mysql_variables(mysql_variable *vars) {
  mysql_variable *var;

  for (var = vars; var->name; var++) {
    free(*(var->value));
    *var->value = NULL;
  }
}

char *read_mysql_one_value(MYSQL *connection, const char *query) {
  MYSQL_RES *mysql_result;
  MYSQL_ROW row;
  char *result = NULL;

  mysql_result = xb_mysql_query(connection, query, true);

  ut_ad(mysql_num_fields(mysql_result) == 1);

  if ((row = mysql_fetch_row(mysql_result))) {
    result = strdup(row[0]);
  }

  mysql_free_result(mysql_result);

  return (result);
}

/* UUID of the backup, gives same value until explicitly reset.
Returned value should NOT be free()-d. */
static const char *get_backup_uuid(MYSQL *connection) {
  if (!backup_uuid) {
    backup_uuid = read_mysql_one_value(connection, "SELECT UUID()");
  }
  return backup_uuid;
}

void parse_show_engine_innodb_status(MYSQL *connection) {
  MYSQL_RES *mysql_result;
  MYSQL_ROW row;

  mysql_result = xb_mysql_query(connection, "SHOW ENGINE INNODB STATUS", true);

  ut_ad(mysql_num_fields(mysql_result) == 3);

  if ((row = mysql_fetch_row(mysql_result))) {
    std::stringstream data(row[2]);
    std::string line;

    while (std::getline(data, line)) {
      lsn_t lsn;
      if (sscanf(line.c_str(), "Log flushed up to " LSN_PF, &lsn) == 1) {
        backup_redo_log_flushed_lsn = lsn;
      }
    }
  }

  mysql_free_result(mysql_result);
}

/* find the pxb base version */
static unsigned long pxb_base_version() {
  std::string pxb_base = MYSQL_SERVER_VERSION;
  unsigned long major = 0, minor = 0, version = 0;
  std::size_t major_p = pxb_base.find(".");
  if (major_p != std::string::npos) major = stoi(pxb_base.substr(0, major_p));

  std::size_t minor_p = pxb_base.find(".", major_p + 1);
  if (minor_p != std::string::npos)
    minor = stoi(pxb_base.substr(major_p + 1, minor_p - major_p));

  std::size_t version_p = pxb_base.find(".", minor_p + 1);
  if (version_p != std::string::npos)
    version = stoi(pxb_base.substr(minor_p + 1, version_p - minor_p));
  else
    version = stoi(pxb_base.substr(minor_p + 1));
  return major * 10000 + minor * 100 + version;
}

static bool check_server_version(unsigned long version_number,
                                 const char *version_string,
                                 const char *version_comment,
                                 const char *innodb_version) {
  bool version_supported = false;
  bool mysql51 = false;
  bool pxb24 = false;

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

  version_supported =
      version_supported || (version_number > 80000 && version_number < 90000);

  mysql51 = version_number > 50100 && version_number < 50500;
  pxb24 = pxb24 || (mysql51 && innodb_version != NULL);
  pxb24 = pxb24 || (version_number > 50500 && version_number < 50800);
  pxb24 = pxb24 || ((version_number > 100000 && version_number < 100300) &&
                    server_flavor == FLAVOR_MARIADB);


  if (!version_supported) {
    msg("Error: Unsupported server version: '%s'.\n"
        "This version of Percona XtraBackup can only perform backups and "
        "restores against MySQL 8.0 and Percona Server 8.0\n",
        version_string);
    if (mysql51 && innodb_version == NULL) {
      msg("You can use Percona XtraBackup 2.0 for MySQL 5.1 with built-in "
          "InnoDB, or upgrade to InnoDB plugin and use Percona XtraBackup "
          "2.4.\n");
    }
    if (pxb24) {
      msg("Please use Percona XtraBackup 2.4 for this database.\n");
    }
  }


  auto pxb_base_ver = pxb_base_version();

  DBUG_EXECUTE_IF("simulate_lower_version", pxb_base_ver = 80014;);

  if (!opt_no_server_version_check && pxb_base_ver < version_number) {
    msg("Error: Unsupported server version %s.\n"
        "Please upgrade PXB, if a new "
        "version is available. To continue with risk, use the option "
        "--no-server-version-check.\n",
        mysql_connection->server_version);
    version_supported = false;
  }

  return (version_supported);
}

/*********************************************************************/ /**
 Receive options important for XtraBackup from MySQL server.
 @return	true on success. */
bool get_mysql_vars(MYSQL *connection) {
  char *gtid_mode_var = nullptr;
  char *version_var = nullptr;
  char *version_comment_var = nullptr;
  char *innodb_version_var = nullptr;
  char *have_backup_locks_var = nullptr;
  char *log_bin_var = nullptr;
  char *lock_wait_timeout_var = nullptr;
  char *wsrep_on_var = nullptr;
  char *slave_parallel_workers_var = nullptr;
  char *gtid_slave_pos_var = nullptr;
  char *innodb_buffer_pool_filename_var = nullptr;
  char *datadir_var = nullptr;
  char *innodb_log_group_home_dir_var = nullptr;
  char *innodb_log_file_size_var = nullptr;
  char *innodb_log_files_in_group_var = nullptr;
  char *innodb_data_file_path_var = nullptr;
  char *innodb_data_home_dir_var = nullptr;
  char *innodb_undo_directory_var = nullptr;
  char *innodb_directories_var = nullptr;
  char *innodb_page_size_var = nullptr;
  char *innodb_log_checksums_var = nullptr;
  char *innodb_checksum_algorithm_var = nullptr;
  char *innodb_redo_log_encrypt_var = nullptr;
  char *innodb_undo_log_encrypt_var = nullptr;
  char *innodb_track_changed_pages_var = nullptr;
  char *server_uuid_var = nullptr;
  char *rocksdb_datadir_var = nullptr;
  char *rocksdb_wal_dir_var = nullptr;
  char *rocksdb_disable_file_deletions_var = nullptr;

  unsigned long server_version = mysql_get_server_version(connection);

  bool ret = true;

  mysql_variable mysql_vars[] = {
      {"have_backup_locks", &have_backup_locks_var},
      {"log_bin", &log_bin_var},
      {"lock_wait_timeout", &lock_wait_timeout_var},
      {"gtid_mode", &gtid_mode_var},
      {"version", &version_var},
      {"version_comment", &version_comment_var},
      {"innodb_version", &innodb_version_var},
      {"wsrep_on", &wsrep_on_var},
      {"slave_parallel_workers", &slave_parallel_workers_var},
      {"gtid_slave_pos", &gtid_slave_pos_var},
      {"innodb_buffer_pool_filename", &innodb_buffer_pool_filename_var},
      {"datadir", &datadir_var},
      {"innodb_log_group_home_dir", &innodb_log_group_home_dir_var},
      {"innodb_log_file_size", &innodb_log_file_size_var},
      {"innodb_log_files_in_group", &innodb_log_files_in_group_var},
      {"innodb_data_file_path", &innodb_data_file_path_var},
      {"innodb_data_home_dir", &innodb_data_home_dir_var},
      {"innodb_undo_directory", &innodb_undo_directory_var},
      {"innodb_directories", &innodb_directories_var},
      {"innodb_page_size", &innodb_page_size_var},
      {"innodb_log_checksums", &innodb_log_checksums_var},
      {"innodb_redo_log_encrypt", &innodb_redo_log_encrypt_var},
      {"innodb_undo_log_encrypt", &innodb_undo_log_encrypt_var},
      {"innodb_track_changed_pages", &innodb_track_changed_pages_var},
      {"server_uuid", &server_uuid_var},
      {"rocksdb_datadir", &rocksdb_datadir_var},
      {"rocksdb_wal_dir", &rocksdb_wal_dir_var},
      {"rocksdb_disable_file_deletions", &rocksdb_disable_file_deletions_var},
      {nullptr, nullptr}};

  read_mysql_variables(connection, "SHOW VARIABLES", mysql_vars, true);

  if (have_backup_locks_var != NULL && !opt_no_backup_locks) {
    have_backup_locks = true;
  }

  if (lock_wait_timeout_var != NULL) {
    have_lock_wait_timeout = true;
  }

  if (wsrep_on_var != NULL) {
    have_galera_enabled = true;
  }

  /* Check server version compatibility and detect server flavor */

  if (!(ret = check_server_version(server_version, version_var,
                                   version_comment_var, innodb_version_var))) {
    goto out;
  }

  if (server_version > 50500) {
    have_flush_engine_logs = true;
  }

  if (slave_parallel_workers_var != NULL &&
      atoi(slave_parallel_workers_var) > 0) {
    have_multi_threaded_slave = true;
  }

  if (innodb_buffer_pool_filename_var != NULL) {
    buffer_pool_filename = strdup(innodb_buffer_pool_filename_var);
  }

  if ((gtid_mode_var && strcmp(gtid_mode_var, "ON") == 0) ||
      (gtid_slave_pos_var && *gtid_slave_pos_var)) {
    have_gtid_slave = true;
  }

  msg("Using server version %s\n", version_var);

  if (!(ret = detect_mysql_capabilities_for_backup())) {
    goto out;
  }

  /* make sure datadir value is the same in configuration file */
  if (check_if_param_set("datadir")) {
    if (!directory_exists(mysql_data_home, false)) {
      msg("Warning: option 'datadir' points to "
          "nonexistent directory '%s'\n",
          mysql_data_home);
    }
    if (!directory_exists(datadir_var, false)) {
      msg("Warning: MySQL variable 'datadir' points to "
          "nonexistent directory '%s'\n",
          datadir_var);
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
    mysql_data_home = mysql_real_data_home;
  }

  if (!check_if_param_set("innodb_data_file_path") &&
      innodb_data_file_path_var && *innodb_data_file_path_var) {
    innobase_data_file_path =
        my_strdup(PSI_NOT_INSTRUMENTED, innodb_data_file_path_var, MYF(MY_FAE));
  }

  if (!check_if_param_set("innodb_data_home_dir") && innodb_data_home_dir_var &&
      *innodb_data_home_dir_var) {
    innobase_data_home_dir =
        my_strdup(PSI_NOT_INSTRUMENTED, innodb_data_home_dir_var, MYF(MY_FAE));
  }

  if (!check_if_param_set("innodb_log_group_home_dir") &&
      innodb_log_group_home_dir_var && *innodb_log_group_home_dir_var) {
    srv_log_group_home_dir = my_strdup(
        PSI_NOT_INSTRUMENTED, innodb_log_group_home_dir_var, MYF(MY_FAE));
  }

  if (!check_if_param_set("innodb_undo_directory") &&
      innodb_undo_directory_var && *innodb_undo_directory_var) {
    srv_undo_dir =
        my_strdup(PSI_NOT_INSTRUMENTED, innodb_undo_directory_var, MYF(MY_FAE));
  }

  if (!check_if_param_set("innodb_directories") && innodb_directories_var &&
      *innodb_directories_var) {
    srv_innodb_directories =
        my_strdup(PSI_NOT_INSTRUMENTED, innodb_directories_var, MYF(MY_FAE));
  }

  if (!check_if_param_set("innodb_log_files_in_group") &&
      innodb_log_files_in_group_var) {
    char *endptr;

    innobase_log_files_in_group =
        strtol(innodb_log_files_in_group_var, &endptr, 10);
    ut_ad(*endptr == 0);
  }

  if (!check_if_param_set("innodb_log_file_size") && innodb_log_file_size_var) {
    char *endptr;

    innobase_log_file_size = strtoll(innodb_log_file_size_var, &endptr, 10);
    ut_ad(*endptr == 0);
  }

  if (!check_if_param_set("innodb_page_size") && innodb_page_size_var) {
    char *endptr;

    innobase_page_size = strtoll(innodb_page_size_var, &endptr, 10);
    ut_ad(*endptr == 0);
  }

  if (!check_if_param_set("innodb_redo_log_encrypt") &&
      innodb_redo_log_encrypt_var) {
    if (strcmp(innodb_redo_log_encrypt_var, "ON") == 0) {
      srv_redo_log_encrypt = true;
    } else {
      srv_redo_log_encrypt = false;
    }
  }

  if (!check_if_param_set("innodb_undo_log_encrypt") &&
      innodb_undo_log_encrypt_var) {
    if (strcmp(innodb_undo_log_encrypt_var, "ON") == 0) {
      srv_undo_log_encrypt = true;
    } else {
      srv_undo_log_encrypt = false;
    }
  }

  if (!innodb_checksum_algorithm_specified && innodb_checksum_algorithm_var) {
    for (uint i = 0; i < innodb_checksum_algorithm_typelib.count; i++) {
      if (strcasecmp(innodb_checksum_algorithm_var,
                     innodb_checksum_algorithm_typelib.type_names[i]) == 0) {
        srv_checksum_algorithm = i;
      }
    }
  }

  if (!innodb_log_checksums_specified && innodb_log_checksums_var) {
    if (strcasecmp(innodb_log_checksums_var, "ON") == 0) {
      srv_log_checksums = true;
    } else {
      srv_log_checksums = false;
    }
  }

  memset(server_uuid, 0, Encryption::SERVER_UUID_LEN + 1);
  if (server_uuid_var != NULL) {
    strncpy(server_uuid, server_uuid_var, Encryption::SERVER_UUID_LEN);
  }

  if (innodb_track_changed_pages_var != nullptr &&
      strcasecmp(innodb_track_changed_pages_var, "ON") == 0) {
    have_changed_page_bitmaps = true;
  }

  if (!check_if_param_set("rocksdb_datadir") && rocksdb_datadir_var &&
      *rocksdb_datadir_var) {
    opt_rocksdb_datadir =
        my_strdup(PSI_NOT_INSTRUMENTED, rocksdb_datadir_var, MYF(MY_FAE));
  }

  if (!check_if_param_set("rocksdb_wal_dir") && rocksdb_wal_dir_var &&
      *rocksdb_wal_dir_var) {
    opt_rocksdb_wal_dir =
        my_strdup(PSI_NOT_INSTRUMENTED, rocksdb_wal_dir_var, MYF(MY_FAE));
  }

  if (rocksdb_disable_file_deletions_var != nullptr) {
    /* rocksdb backup extensions are supported */
    have_rocksdb = true;
  } else {
    char *engine = nullptr;

    mysql_variable vars[] = {{"Engine", &engine}, {nullptr, nullptr}};

    MYSQL_RES *res =
        xb_mysql_query(mysql_connection, "SHOW ENGINES", true, true);

    while (read_mysql_variables_from_result(res, vars, false)) {
      if (strcasecmp(engine, "ROCKSDB") == 0) {
        msg_ts(
            "WARNING: ROCKSB storage engine is enabled, but ROCKSB backup "
            "extensions are not supported by server. Please upgrade "
            "Percona Server to enable ROCKSDB backups.\n");
      }
      free_mysql_variables(vars);
    }
    mysql_free_result(res);
  }

out:
  free_mysql_variables(mysql_vars);

  return (ret);
}

/*********************************************************************/ /**
 Query the server to find out what backup capabilities it supports.
 @return	true on success. */
bool detect_mysql_capabilities_for_backup() {
  if (xtrabackup_incremental) {
    /* INNODB_CHANGED_PAGES are listed in
    INFORMATION_SCHEMA.PLUGINS in MariaDB, but
    FLUSH NO_WRITE_TO_BINLOG CHANGED_PAGE_BITMAPS
    is not supported for versions below 10.1.6
    (see MDEV-7472) */
    if (server_flavor == FLAVOR_MARIADB && mysql_server_version < 100106) {
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

  if (opt_slave_info && have_multi_threaded_slave && !have_gtid_slave &&
      !opt_safe_slave_backup) {
    msg("The --slave-info option requires GTID enabled or --safe-slave-backup "
        "option used for a multi-threaded slave.\n");
    return (false);
  }

  char *count_str =
      read_mysql_one_value(mysql_connection,
                           "SELECT COUNT(*) FROM information_schema.tables "
                           "WHERE engine = 'MyISAM' OR engine = 'RocksDB'");
  unsigned long long count = strtoull(count_str, nullptr, 10);
  have_unsafe_ddl_tables = (count > 0);
  free(count_str);

  if (opt_slave_info) {
    char *auto_position = NULL;

    mysql_variable status[] = {{"Auto_Position", &auto_position}, {NULL, NULL}};

    MYSQL_RES *res =
        xb_mysql_query(mysql_connection, "SHOW SLAVE STATUS", true, true);

    slave_auto_position = true;

    while (read_mysql_variables_from_result(res, status, false)) {
      slave_auto_position =
          slave_auto_position && (strcmp(auto_position, "1") == 0);
      free_mysql_variables(status);
    }
    mysql_free_result(res);
  }

  return (true);
}

static bool select_incremental_lsn_from_history(lsn_t *incremental_lsn) {
  MYSQL_RES *mysql_result;
  MYSQL_ROW row;
  char query[1000];
  char buf[100];

  if (opt_incremental_history_name) {
    mysql_real_escape_string(mysql_connection, buf,
                             opt_incremental_history_name,
                             strlen(opt_incremental_history_name));
    snprintf(query, sizeof(query),
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
    snprintf(query, sizeof(query),
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
        opt_incremental_history_uuid ? opt_incremental_history_uuid
                                     : opt_incremental_history_name);
    return (false);
  }

  *incremental_lsn = strtoull(row[0], NULL, 10);

  mysql_free_result(mysql_result);

  msg("Found and using lsn: " LSN_PF " for %s %s\n", *incremental_lsn,
      opt_incremental_history_uuid ? "uuid" : "name",
      opt_incremental_history_uuid ? opt_incremental_history_uuid
                                   : opt_incremental_history_name);

  return (true);
}

static const char *eat_sql_whitespace(const char *query) {
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

  return (query);
}

static bool is_query_from_list(const char *query, const char **list) {
  const char **item;

  query = eat_sql_whitespace(query);

  item = list;
  while (*item) {
    if (strncasecmp(query, *item, strlen(*item)) == 0) {
      return (true);
    }
    ++item;
  }

  return (false);
}

static bool is_query(const char *query) {
  const char *query_list[] = {"insert",  "update", "delete", "replace", "alter",
                              "load",    "select", "do",     "handler", "call",
                              "execute", "begin",  NULL};

  return is_query_from_list(query, query_list);
}

static bool is_select_query(const char *query) {
  const char *query_list[] = {"select", NULL};

  return is_query_from_list(query, query_list);
}

static bool is_update_query(const char *query) {
  const char *query_list[] = {"insert", "update", "delete", "replace",
                              "alter",  "load",   NULL};

  return is_query_from_list(query, query_list);
}

static bool have_queries_to_wait_for(MYSQL *connection, uint threshold) {
  MYSQL_RES *result;
  MYSQL_ROW row;
  bool all_queries;

  result = xb_mysql_query(connection, "SHOW FULL PROCESSLIST", true);

  all_queries = (opt_lock_wait_query_type == QUERY_TYPE_ALL);
  while ((row = mysql_fetch_row(result)) != NULL) {
    const char *info = row[7];
    char *id = row[0];
    int duration;

    duration = (row[5] != NULL) ? atoi(row[5]) : 0;

    if (info != NULL && duration >= (int)threshold &&
        ((all_queries && is_query(info)) || is_update_query(info))) {
      msg_ts("Waiting for query %s (duration %d sec): %s", id, duration, info);
      mysql_free_result(result);
      return (true);
    }
  }

  mysql_free_result(result);

  return (false);
}

static void kill_long_queries(MYSQL *connection, uint timeout) {
  MYSQL_RES *result;
  MYSQL_ROW row;
  bool all_queries;
  char kill_stmt[100];

  result = xb_mysql_query(connection, "SHOW FULL PROCESSLIST", true);

  all_queries = (opt_kill_long_query_type == QUERY_TYPE_ALL);
  while ((row = mysql_fetch_row(result)) != NULL) {
    const char *info = row[7];
    int duration = atoi(row[5]);
    char *id = row[0];

    if (info != NULL && duration >= (int)timeout &&
        ((all_queries && is_query(info)) || is_select_query(info))) {
      msg_ts("Killing query %s (duration %d sec): %s\n", id, duration, info);
      snprintf(kill_stmt, sizeof(kill_stmt), "KILL %s", id);
      xb_mysql_query(connection, kill_stmt, false, false);
    }
  }

  mysql_free_result(result);
}

static bool wait_for_no_updates(MYSQL *connection, uint timeout,
                                uint threshold) {
  time_t start_time;

  my_thread_init();

  start_time = time(NULL);

  msg_ts(
      "Waiting %u seconds for queries running longer than %u seconds "
      "to finish\n",
      timeout, threshold);

  while (time(NULL) <= (time_t)(start_time + timeout)) {
    if (!have_queries_to_wait_for(connection, threshold)) {
      return (true);
    }
    os_thread_sleep(1000000);
  }

  msg_ts("Unable to obtain lock. Please try again later.");

  return (false);
}

static void kill_query_thread() {
  MYSQL *mysql;
  time_t start_time;

  start_time = time(NULL);

  os_event_set(kill_query_thread_started);

  msg_ts("Kill query timeout %d seconds.\n", opt_kill_long_queries_timeout);

  while (time(NULL) - start_time < (time_t)opt_kill_long_queries_timeout) {
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
}

static void start_query_killer() {
  kill_query_thread_stop = os_event_create();
  kill_query_thread_started = os_event_create();
  kill_query_thread_stopped = os_event_create();

  os_thread_create(PSI_NOT_INSTRUMENTED, kill_query_thread).start();

  os_event_wait(kill_query_thread_started);
}

static void stop_query_killer() {
  os_event_set(kill_query_thread_stop);
  os_event_wait_time(kill_query_thread_stopped, 60000);

  os_event_destroy(kill_query_thread_stop);
  os_event_destroy(kill_query_thread_started);
  os_event_destroy(kill_query_thread_stopped);
}

static bool execute_query_with_timeout(MYSQL *mysql, const char *query,
                                       int timeout, int retry_count) {
  bool success = false;
  if (have_lock_wait_timeout) {
    char query[200];
    snprintf(query, sizeof(query), "SET SESSION lock_wait_timeout=%d", timeout);
    xb_mysql_query(mysql, query, false, true);
  }

  for (int i = 0; i <= retry_count; ++i) {
    msg_ts("Executing %s...\n", query);
    xb_mysql_query(mysql, query, true);
    uint err = mysql_errno(mysql);
    if (err == ER_LOCK_WAIT_TIMEOUT) {
      os_thread_sleep(1000000);
      continue;
    }
    if (err == 0) {
      success = true;
    }
    break;
  }

  return (success);
}

/*********************************************************************/ /**
 Function acquires a backup tables lock if supported by the server.
 Allows to specify timeout in seconds for attempts to acquire the lock.
 @returns true if lock acquired */
bool lock_tables_for_backup(MYSQL *connection, int timeout, int retry_count) {
  if (have_backup_locks) {
    execute_query_with_timeout(connection, "LOCK TABLES FOR BACKUP", timeout,
                               retry_count);
    tables_locked = true;

    return (true);
  }

  execute_query_with_timeout(connection, "LOCK INSTANCE FOR BACKUP", timeout,
                             retry_count);
  instance_locked = true;

  return (true);
}

/*********************************************************************/ /**
 Function acquires a FLUSH TABLES WITH READ LOCK.
 @returns true if lock acquired */
bool lock_tables_ftwrl(MYSQL *connection) {
  if (have_lock_wait_timeout) {
    /* Set the maximum supported session value for
    lock_wait_timeout to prevent unnecessary timeouts when the
    global value is changed from the default */
    xb_mysql_query(connection, "SET SESSION lock_wait_timeout=31536000", false);
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

    xb_mysql_query(connection, "FLUSH NO_WRITE_TO_BINLOG TABLES", false);
  }

  if (opt_lock_wait_timeout) {
    if (!wait_for_no_updates(connection, opt_lock_wait_timeout,
                             opt_lock_wait_threshold)) {
      return (false);
    }
  }

  msg_ts("Executing FLUSH TABLES WITH READ LOCK...\n");

  if (opt_kill_long_queries_timeout) {
    start_query_killer();
  }

  if (have_galera_enabled) {
    xb_mysql_query(connection, "SET SESSION wsrep_causal_reads=0", false);
  }

  xb_mysql_query(connection, "FLUSH TABLES WITH READ LOCK", false);

  if (opt_kill_long_queries_timeout) {
    stop_query_killer();
  }

  tables_locked = true;

  return (true);
}

/*********************************************************************/ /**
 Function acquires either a backup tables lock, if supported
 by the server, or a global read lock (FLUSH TABLES WITH READ LOCK)
 otherwise. If server does not contain MyISAM tables, no lock will be
 acquired. If slave_info option is specified and slave is not
 using auto_position.
 @returns true if lock acquired */
bool lock_tables_maybe(MYSQL *connection, int timeout, int retry_count) {
  bool force_ftwrl = opt_slave_info && !slave_auto_position &&
                     !(server_flavor == FLAVOR_PERCONA_SERVER);

  if (tables_locked || (opt_lock_ddl_per_table && !force_ftwrl)) {
    return (true);
  }

  if (!have_unsafe_ddl_tables && !force_ftwrl) {
    return (true);
  }

  if (have_backup_locks && !force_ftwrl) {
    return lock_tables_for_backup(connection, timeout, retry_count);
  }

  return lock_tables_ftwrl(connection);
}

/*********************************************************************/ /**
 Releases the lock acquired with FTWRL/LOCK TABLES FOR BACKUP, depending on
 the locking strategy being used */
void unlock_all(MYSQL *connection) {
  if (instance_locked) {
    msg_ts("Executing UNLOCK INSTANCE\n");
    xb_mysql_query(connection, "UNLOCK INSTANCE", false);
    instance_locked = false;
  }

  if (tables_locked) {
    msg_ts("Executing UNLOCK TABLES\n");
    xb_mysql_query(connection, "UNLOCK TABLES", false);
  }

  msg_ts("All tables unlocked\n");
}

static int get_open_temp_tables(MYSQL *connection) {
  char *slave_open_temp_tables = NULL;
  mysql_variable status[] = {
      {"Slave_open_temp_tables", &slave_open_temp_tables}, {NULL, NULL}};
  int result = false;

  read_mysql_variables(connection, "SHOW STATUS LIKE 'slave_open_temp_tables'",
                       status, true);

  result = slave_open_temp_tables ? atoi(slave_open_temp_tables) : 0;

  free_mysql_variables(status);

  return (result);
}

static char *get_slave_coordinates(MYSQL *connection) {
  char *relay_log_file = NULL;
  char *exec_log_pos = NULL;
  char *result = NULL;

  mysql_variable slave_coordinates[] = {
      {"Relay_Master_Log_File", &relay_log_file},
      {"Exec_Master_Log_Pos", &exec_log_pos},
      {NULL, NULL}};

  read_mysql_variables(connection, "SHOW SLAVE STATUS", slave_coordinates,
                       false);
  ut_a(asprintf(&result, "%s\\%s", relay_log_file, exec_log_pos));
  free_mysql_variables(slave_coordinates);
  return result;
}

/*********************************************************************/ /**
 Wait until it's safe to backup a slave.  Returns immediately if
 the host isn't a slave.  Currently there's only one check:
 Slave_open_temp_tables has to be zero.  Dies on timeout. */
bool wait_for_safe_slave(MYSQL *connection) {
  char *read_master_log_pos = NULL;
  char *slave_sql_running = NULL;
  char *curr_slave_coordinates = NULL;
  char *prev_slave_coordinates = NULL;

  const int sleep_time = 3;
  const ssize_t routine_start_time = (ssize_t)my_time(MY_WME);
  const ssize_t timeout = opt_safe_slave_backup_timeout;

  int open_temp_tables = 0;
  bool result = true;

  mysql_variable status[] = {{"Read_Master_Log_Pos", &read_master_log_pos},
                             {"Slave_SQL_Running", &slave_sql_running},
                             {NULL, NULL}};

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
    msg_ts(
        "Starting slave SQL thread, waiting %d seconds, then "
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
    msg_ts("Slave pos:\n\tprev: %s\n\tcurr: %s\n", prev_slave_coordinates,
           curr_slave_coordinates);
    if (prev_slave_coordinates && curr_slave_coordinates &&
        strcmp(prev_slave_coordinates, curr_slave_coordinates) == 0) {
      msg_ts(
          "Slave pos hasn't moved during wait period, "
          "not stopping the SQL thread.\n");
    } else {
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

  msg_ts(
      "Slave_open_temp_tables did not become zero after "
      "%d seconds\n",
      opt_safe_slave_backup_timeout);

  msg_ts("Restoring SQL thread state to %s\n",
         sql_thread_started ? "STARTED" : "STOPPED");
  if (sql_thread_started) {
    xb_mysql_query(connection, "START SLAVE SQL_THREAD", false);
  } else {
    xb_mysql_query(connection, "STOP SLAVE SQL_THREAD", false);
  }

cleanup:
  free(prev_slave_coordinates);
  free(curr_slave_coordinates);
  free_mysql_variables(status);

  return (result);
}

static log_status_t log_status;

/*********************************************************************/ /**
 Retrieves MySQL binlog position of the master server in a replication
 setup and saves it in a file. It also saves it in mysql_slave_position
 variable. */
bool write_slave_info(MYSQL *connection) {
  std::stringstream slave_info;
  std::stringstream mysql_slave_position_s;
  char *master = NULL;
  char *filename = NULL;
  char *position = NULL;
  char *auto_position = NULL;
  char *channel_name = NULL;
  char *slave_sql_running = NULL;
  bool result = true;

  typedef struct {
    std::string master;
    std::string filename;
    uint64_t position;
    bool auto_position;
  } channel_info_t;

  std::map<std::string, channel_info_t> channels;

  mysql_variable status[] = {{"Master_Host", &master},
                             {"Relay_Master_Log_File", &filename},
                             {"Exec_Master_Log_Pos", &position},
                             {"Channel_Name", &channel_name},
                             {"Slave_SQL_Running", &slave_sql_running},
                             {"Auto_Position", &auto_position},
                             {NULL, NULL}};

  MYSQL_RES *slave_status_res =
      xb_mysql_query(connection, "SHOW SLAVE STATUS", true, true);

  while (read_mysql_variables_from_result(slave_status_res, status, false)) {
    channel_info_t info;
    info.master = master;
    info.auto_position = (strcmp(auto_position, "1") == 0);
    info.filename = filename;
    info.position = strtoull(position, NULL, 10);
    channels[channel_name ? channel_name : ""] = info;

    ut_ad(!have_multi_threaded_slave || have_gtid_slave ||
          strcasecmp(slave_sql_running, "No") == 0);

    free_mysql_variables(status);
  }

  int channel_idx = 0;
  for (auto &channel : log_status.channels) {
    auto ch = channels.find(channel.channel_name);
    std::string for_channel;

    if (!channel.channel_name.empty()) {
      for_channel = " FOR CHANNEL '" + channel.channel_name + "'";
    }

    if (ch == channels.end()) {
      msg("xtrabackup: Error: Failed to find information for "
          "channel '%s' in SHOW SLAVE STATUS output.\n",
          channel.channel_name.c_str());
      result = false;
      goto cleanup;
    }

    ++channel_idx;

    if (ch->second.auto_position) {
      if (channel_idx == 1) {
        slave_info << "SET GLOBAL gtid_purged='" << log_status.gtid_executed
                   << "';\n";
      }
      slave_info << "CHANGE MASTER TO MASTER_AUTO_POSITION=1" << for_channel
                 << ";\n";

      mysql_slave_position_s << "master host '" << ch->second.master
                             << "', purge list '" << log_status.gtid_executed
                             << "', channel name: '" << channel.channel_name
                             << "'\n";
    } else {
      const auto filename = channel.relay_master_log_file.empty()
                                ? ch->second.filename
                                : channel.relay_master_log_file;
      const auto position = channel.relay_master_log_file.empty()
                                ? ch->second.position
                                : channel.exec_master_log_position;
      slave_info << "CHANGE MASTER TO MASTER_LOG_FILE='" << filename
                 << "', MASTER_LOG_POS=" << position << for_channel << ";\n";

      mysql_slave_position_s << "master host '" << ch->second.master
                             << "', filename '" << filename << "', position '"
                             << position << "', channel name: '"
                             << channel.channel_name << "'\n";
    }
  }

  mysql_slave_position = mysql_slave_position_s.str();

  result = backup_file_print(XTRABACKUP_SLAVE_INFO, slave_info.str().c_str(),
                             slave_info.str().size());

cleanup:
  mysql_free_result(slave_status_res);
  free_mysql_variables(status);

  return (result);
}

/*********************************************************************/ /**
 Retrieves MySQL Galera and
 saves it in a file. It also prints it to stdout. */
bool write_galera_info(MYSQL *connection) {
  char *state_uuid = NULL, *state_uuid55 = NULL;
  char *last_committed = NULL, *last_committed55 = NULL;
  bool result;

  mysql_variable status[] = {{"Wsrep_local_state_uuid", &state_uuid},
                             {"wsrep_local_state_uuid", &state_uuid55},
                             {"Wsrep_last_committed", &last_committed},
                             {"wsrep_last_committed", &last_committed55},
                             {NULL, NULL}};

  /* When backup locks are supported by the server, we should skip
  creating xtrabackup_galera_info file on the backup stage, because
  wsrep_local_state_uuid and wsrep_last_committed will be inconsistent
  without blocking commits. The state file will be created on the prepare
  stage using the WSREP recovery procedure. */
  if (have_backup_locks) {
    return (true);
  }

  read_mysql_variables(connection, "SHOW STATUS", status, true);

  if ((state_uuid == NULL && state_uuid55 == NULL) ||
      (last_committed == NULL && last_committed55 == NULL)) {
    msg("Failed to get master wsrep state from SHOW STATUS.\n");
    result = false;
    goto cleanup;
  }

  result = backup_file_printf(
      XTRABACKUP_GALERA_INFO, "%s:%s\n", state_uuid ? state_uuid : state_uuid55,
      last_committed ? last_committed : last_committed55);

cleanup:
  free_mysql_variables(status);

  return (result);
}

/**
 Get encryption header size for given file by reading its magic header

 @param filepath[in] binlog file size
 @return encryption header size
*/
size_t binlog_encryption_header_size(const char *filepath) {
  std::ifstream fstr(filepath);
  char magic[Rpl_encryption_header::ENCRYPTION_MAGIC_SIZE];
  fstr.read(magic, Rpl_encryption_header::ENCRYPTION_MAGIC_SIZE);
  if (memcmp(magic, Rpl_encryption_header::ENCRYPTION_MAGIC,
             Rpl_encryption_header::ENCRYPTION_MAGIC_SIZE) == 0) {
    return (Rpl_encryption_header_v1::HEADER_SIZE);
  }
  return (0);
}

/**
 Copy the current binary log file into the backup

 @param      connection  mysql connection
 @return     true if success
*/
bool write_current_binlog_file(MYSQL *connection) {
  char *log_bin_dir = nullptr;
  char *log_bin_index = nullptr;
  char *log_bin_index_filename = nullptr;
  FILE *f_index = nullptr;
  bool result = true;
  char filepath[FN_REFLEN];
  size_t log_bin_dir_length;

  mysql_variable vars[] = {{"log_bin_index", &log_bin_index},
                           {"log_bin_basename", &log_bin_dir},
                           {nullptr, nullptr}};

  if (log_status.filename.empty()) {
    goto cleanup;
  }

  read_mysql_variables(connection, "SHOW VARIABLES", vars, true);

  if (opt_log_bin != nullptr && strchr(opt_log_bin, FN_LIBCHAR)) {
    /* If log_bin is set, it has priority */
    free(log_bin_dir);
    log_bin_dir = strdup(opt_log_bin);
  } else if (log_bin_dir == nullptr) {
    /* Default location is MySQL datadir */
    log_bin_dir = strdup("./");
  }

  if (opt_binlog_index_name != nullptr) {
    free(log_bin_index);
    std::string index = opt_binlog_index_name;
    if (index.length() < 6 ||
        index.compare(index.length() - 6, index.length(), ".index") != 0) {
      /* doesn't end with .index */
      index.append(".index");
    }
    log_bin_index = strdup(index.c_str());
  } else if (log_bin_index == nullptr) {
    /* if index file name is not set, compose it from the current log file name
    by replacing its number with ".index" */
    std::string index = log_status.filename;
    size_t dot_pos = index.find_last_of(".");
    if (dot_pos != std::string::npos) {
      index.replace(dot_pos, std::string::npos, ".index");
    } else {
      index.append(".index");
    }
    log_bin_index = strdup(index.c_str());
  }

  dirname_part(log_bin_dir, log_bin_dir, &log_bin_dir_length);

  /* strip final slash if it is not the only path component */
  if (log_bin_dir_length > 1 &&
      log_bin_dir[log_bin_dir_length - 1] == FN_LIBCHAR) {
    log_bin_dir[log_bin_dir_length - 1] = 0;
  }

  snprintf(filepath, sizeof(filepath), "%s%c%s", log_bin_dir, FN_LIBCHAR,
           log_status.filename.c_str());
  result = copy_file(
      ds_data, filepath, log_status.filename.c_str(), 0, FILE_PURPOSE_BINLOG,
      log_status.position + binlog_encryption_header_size(filepath));
  if (!result) {
    goto cleanup;
  }

  if (opt_transition_key != NULL || opt_generate_transition_key) {
    result = xb_binlog_password_store(log_status.filename.c_str());
    if (!result) {
      msg("xtrabackup: Error: failed to dump binary log password.\n");
      goto cleanup;
    }
  }

  log_bin_index_filename = strrchr(log_bin_index, FN_LIBCHAR);
  if (log_bin_index_filename == nullptr) {
    log_bin_index_filename = log_bin_index;
  } else {
    ++log_bin_index_filename;
  }

  f_index = fopen(log_bin_index, "r");
  if (f_index == nullptr) {
    msg("xtrabackup: Error cannot open binlog index file '%s'\n",
        log_bin_index);
    result = false;
    goto cleanup;
  }

  /* only write current log file into .index in the backup directory */
  result = false;
  while (!feof(f_index)) {
    char line[FN_REFLEN];
    if (fgets(line, sizeof(line), f_index) != nullptr) {
      if (strstr(line, log_status.filename.c_str()) != nullptr) {
        backup_file_print(log_bin_index_filename, line, strlen(line));
        result = true;
        break;
      }
    }
  }
  fclose(f_index);

  if (!result) {
    msg("xtrabackup: Error cannot find current log file in the '%s'\n",
        log_bin_index);
  }

cleanup:
  free_mysql_variables(vars);

  return (result);
}

/** Parse replication channels information from JSON.
@param[in]   s            JSON string
@param[out]  log_status   replication info */
static void log_status_replication_parse(const char *s,
                                         log_status_t &log_status) {
  using rapidjson::Document;
  Document doc;
  doc.Parse(s);

  ut_a(!doc.HasParseError());

  auto root = doc.GetObject();

  for (auto &ch : root["channels"].GetArray()) {
    replication_channel_status_t cs;
    cs.channel_name = ch["channel_name"].GetString();
    cs.relay_log_file = ch["relay_log_file"].GetString();
    cs.relay_log_position = ch["relay_log_position"].GetUint64();
    if (server_flavor == FLAVOR_PERCONA_SERVER) {
      cs.relay_master_log_file = ch["relay_master_log_file"].GetString();
      cs.exec_master_log_position = ch["exec_master_log_position"].GetUint64();
    }
    log_status.channels.push_back(cs);
  }
}

/** Parse LSN information from JSON.
@param[in]   s            JSON string
@param[out]  log_status   LSN info */
static void log_status_storage_engines_parse(const char *s,
                                             log_status_t &log_status) {
  using rapidjson::Document;
  Document doc;
  doc.Parse(s);

  ut_a(!doc.HasParseError());

  auto root = doc.GetObject();

  auto innodb = root["InnoDB"].GetObject();
  log_status.lsn = innodb["LSN"].GetUint64();
  log_status.lsn_checkpoint = innodb["LSN_checkpoint"].GetUint64();

  if (root.HasMember("RocksDB")) {
    auto rocksdb = root["RocksDB"].GetObject();
    for (auto &wal : rocksdb["wal_files"].GetArray()) {
      rocksdb_wal_t rdb_wal;
      rdb_wal.file_size_bytes = wal["size_file_bytes"].GetUint64();
      rdb_wal.log_number = wal["log_number"].GetUint();
      rdb_wal.path_name = wal["path_name"].GetString();
      log_status.rocksdb_wal_files.push_back(rdb_wal);
    }
  }
}

/** Parse binaty log position from JSON.
@param[in]   s            JSON string
@param[out]  log_status   binary log info */
static void log_status_local_parse(const char *s, log_status_t &log_status) {
  using rapidjson::Document;
  Document doc;
  doc.Parse(s);

  ut_a(!doc.HasParseError());

  auto root = doc.GetObject();

  if (root.HasMember("gtid_executed")) {
    log_status.gtid_executed = root["gtid_executed"].GetString();
    /* replace newlines in gtid */
    std::string::size_type pos = 0;
    while ((pos = log_status.gtid_executed.find("\n", pos)) !=
           std::string::npos) {
      log_status.gtid_executed.erase(pos, 1);
    }
  }
  if (root.HasMember("binary_log_file")) {
    log_status.filename = root["binary_log_file"].GetString();
  }
  if (root.HasMember("binary_log_position")) {
    log_status.position = root["binary_log_position"].GetUint64();
  }
}

/** Read binaty log position and InnoDB LSN from p_s.log_status.
@param[in]   conn         mysql connection handle */
const log_status_t &log_status_get(MYSQL *conn) {
  msg_ts("Selecting LSN and binary log position from p_s.log_status\n");

  debug_sync_point("log_status_get");

  ut_ad(!have_unsafe_ddl_tables || tables_locked || instance_locked ||
        opt_no_lock || opt_lock_ddl_per_table);

  const char *query =
      "SELECT server_uuid, local, replication, "
      "storage_engines FROM performance_schema.log_status";
  MYSQL_RES *result = xb_mysql_query(conn, query, true, true);
  MYSQL_ROW row;
  if ((row = mysql_fetch_row(result))) {
    const char *local = row[1];
    const char *replication = row[2];
    const char *storage_engines = row[3];

    log_status_local_parse(local, log_status);
    log_status_storage_engines_parse(storage_engines, log_status);
    log_status_replication_parse(replication, log_status);
  }
  mysql_free_result(result);

  return log_status;
}

/*********************************************************************/ /**
 Retrieves MySQL binlog position and
 saves it in a file. It also prints it to stdout.
 @param[in]   connection  MySQL connection handler
 @return true if success. */
bool write_binlog_info(MYSQL *connection) {
  std::ostringstream s;
  char *gtid_mode = NULL;
  bool result, gtid;

  mysql_variable vars[] = {{"gtid_mode", &gtid_mode}, {NULL, NULL}};
  read_mysql_variables(connection, "SHOW VARIABLES", vars, true);

  if (log_status.filename.empty() && log_status.gtid_executed.empty()) {
    /* Do not create xtrabackup_binlog_info if binary
    log is disabled */
    result = true;
    goto cleanup;
  }

  s << "filename '" << log_status.filename << "', position '"
    << log_status.position << "'";

  gtid = ((gtid_mode != NULL) && (strcmp(gtid_mode, "ON") == 0));

  if (!log_status.gtid_executed.empty() && gtid) {
    s << ", GTID of the last change '" << log_status.gtid_executed << "'";
  }

  mysql_binlog_position = s.str();

  if (!log_status.gtid_executed.empty() && gtid) {
    result =
        backup_file_printf(XTRABACKUP_BINLOG_INFO, "%s\t" UINT64PF "\t%s\n",
                           log_status.filename.c_str(), log_status.position,
                           log_status.gtid_executed.c_str());
  } else {
    result =
        backup_file_printf(XTRABACKUP_BINLOG_INFO, "%s\t" UINT64PF "\n",
                           log_status.filename.c_str(), log_status.position);
  }

cleanup:
  free_mysql_variables(vars);

  return (result);
}

inline static bool format_time(time_t time, char *dest, size_t max_size) {
  tm tm;
  localtime_r(&time, &tm);
  return strftime(dest, max_size, "%Y-%m-%d %H:%M:%S", &tm) != 0;
}

/*********************************************************************/ /**
 Allocates and writes contents of xtrabackup_info into buffer;
 Invoke free() on return value once you don't need it.
 */
char *get_xtrabackup_info(MYSQL *connection) {
  const char *uuid = get_backup_uuid(connection);
  char *server_version = read_mysql_one_value(connection, "SELECT VERSION()");

  static const size_t time_buf_size = 100;
  char buf_start_time[time_buf_size];
  char buf_end_time[time_buf_size];

  format_time(history_start_time, buf_start_time, time_buf_size);
  format_time(history_end_time, buf_end_time, time_buf_size);

  ut_a(uuid);
  ut_a(server_version);
  char *result = NULL;
  int ret = asprintf(&result,
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
                     "innodb_from_lsn = " LSN_PF
                     "\n"
                     "innodb_to_lsn = " LSN_PF
                     "\n"
                     "partial = %s\n"
                     "incremental = %s\n"
                     "format = %s\n"
                     "compressed = %s\n"
                     "encrypted = %s\n",
                     uuid,                           /* uuid */
                     opt_history ? opt_history : "", /* name */
                     tool_name,                      /* tool_name */
                     tool_args,                      /* tool_command */
                     XTRABACKUP_VERSION,             /* tool_version */
                     XTRABACKUP_VERSION,             /* ibbackup_version */
                     server_version,                 /* server_version */
                     buf_start_time,                 /* start_time */
                     buf_end_time,                   /* end_time */
                     (long int)history_lock_time,    /* lock_time */
                     mysql_binlog_position.c_str(),  /* binlog_pos */
                     incremental_lsn,                /* innodb_from_lsn */
                     metadata_to_lsn,                /* innodb_to_lsn */
                     (xtrabackup_tables              /* partial */
                      || xtrabackup_tables_exclude || xtrabackup_tables_file ||
                      xtrabackup_databases || xtrabackup_databases_exclude ||
                      xtrabackup_databases_file)
                         ? "Y"
                         : "N",
                     xtrabackup_incremental ? "Y" : "N", /* incremental */
                     xb_stream_format_name[xtrabackup_stream_fmt], /* format */
                     xtrabackup_compress ? "compressed" : "N", /* compressed */
                     xtrabackup_encrypt ? "Y" : "N");          /* encrypted */

  ut_a(ret != 0);

  free(server_version);
  return result;
}

/*********************************************************************/ /**
 Writes xtrabackup_info file and if backup_history is enable creates
 PERCONA_SCHEMA.xtrabackup_history and writes a new history record to the
 table containing all the history info particular to the just completed
 backup. */
bool write_xtrabackup_info(MYSQL *connection) {
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[19];
  const char *uuid = NULL;
  char *server_version = NULL;
  char *xtrabackup_info_data = NULL;
  int idx;
  bool null = TRUE;

  const char *ins_query =
      "insert into PERCONA_SCHEMA.xtrabackup_history("
      "uuid, name, tool_name, tool_command, tool_version, "
      "ibbackup_version, server_version, start_time, end_time, "
      "lock_time, binlog_pos, innodb_from_lsn, innodb_to_lsn, "
      "partial, incremental, format, compact, compressed, "
      "encrypted) "
      "values(?,?,?,?,?,?,?,from_unixtime(?),from_unixtime(?),"
      "?,?,?,?,?,?,?,?,?,?)";

  ut_ad((uint)xtrabackup_stream_fmt < array_elements(xb_stream_format_name));
  const char *stream_format_name = xb_stream_format_name[xtrabackup_stream_fmt];
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

  xb_mysql_query(connection, "CREATE DATABASE IF NOT EXISTS PERCONA_SCHEMA",
                 false);
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
                 ") CHARACTER SET utf8 ENGINE=innodb",
                 false);

  stmt = mysql_stmt_init(connection);

  mysql_stmt_prepare(stmt, ins_query, strlen(ins_query));

  memset(bind, 0, sizeof(bind));
  idx = 0;

  /* uuid */
  bind[idx].buffer_type = MYSQL_TYPE_STRING;
  bind[idx].buffer = (char *)uuid;
  bind[idx].buffer_length = strlen(uuid);
  ++idx;

  /* name */
  bind[idx].buffer_type = MYSQL_TYPE_STRING;
  bind[idx].buffer = (char *)(opt_history);
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
  bind[idx].buffer = (char *)(XTRABACKUP_VERSION);
  bind[idx].buffer_length = strlen(XTRABACKUP_VERSION);
  ++idx;

  /* ibbackup_version */
  bind[idx].buffer_type = MYSQL_TYPE_STRING;
  bind[idx].buffer = (char *)(XTRABACKUP_VERSION);
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
  bind[idx].buffer = (void *)mysql_binlog_position.c_str();
  if (!mysql_binlog_position.empty()) {
    bind[idx].buffer_length = mysql_binlog_position.length();
  } else {
    bind[idx].is_null = &null;
  }
  ++idx;

  /* innodb_from_lsn */
  bind[idx].buffer_type = MYSQL_TYPE_LONGLONG;
  bind[idx].buffer = (char *)(&incremental_lsn);
  ++idx;

  /* innodb_to_lsn */
  bind[idx].buffer_type = MYSQL_TYPE_LONGLONG;
  bind[idx].buffer = (char *)(&metadata_to_lsn);
  ++idx;

  /* partial (Y | N) */
  bind[idx].buffer_type = MYSQL_TYPE_STRING;
  bind[idx].buffer =
      (char *)((xtrabackup_tables || xtrabackup_tables_exclude ||
                xtrabackup_tables_file || xtrabackup_databases ||
                xtrabackup_databases_exclude || xtrabackup_databases_file)
                   ? "Y"
                   : "N");
  bind[idx].buffer_length = 1;
  ++idx;

  /* incremental (Y | N) */
  bind[idx].buffer_type = MYSQL_TYPE_STRING;
  bind[idx].buffer =
      (char *)((xtrabackup_incremental || xtrabackup_incremental_basedir ||
                opt_incremental_history_name || opt_incremental_history_uuid)
                   ? "Y"
                   : "N");
  bind[idx].buffer_length = 1;
  ++idx;

  /* format (file | tar | xbstream) */
  bind[idx].buffer_type = MYSQL_TYPE_STRING;
  bind[idx].buffer = (char *)(stream_format_name);
  bind[idx].buffer_length = strlen(stream_format_name);
  ++idx;

  /* compact (Y | N) */
  bind[idx].buffer_type = MYSQL_TYPE_STRING;
  bind[idx].buffer = (char *)"N";
  bind[idx].buffer_length = 1;
  ++idx;

  /* compressed (Y | N) */
  bind[idx].buffer_type = MYSQL_TYPE_STRING;
  bind[idx].buffer = (char *)(xtrabackup_compress ? "Y" : "N");
  bind[idx].buffer_length = 1;
  ++idx;

  /* encrypted (Y | N) */
  bind[idx].buffer_type = MYSQL_TYPE_STRING;
  bind[idx].buffer = (char *)(xtrabackup_encrypt ? "Y" : "N");
  bind[idx].buffer_length = 1;
  ++idx;

  ut_ad(idx == 19);

  mysql_stmt_bind_param(stmt, bind);

  mysql_stmt_execute(stmt);
  mysql_stmt_close(stmt);

cleanup:

  free(xtrabackup_info_data);
  free(server_version);

  return (true);
}

bool write_backup_config_file() {
  std::ostringstream s;

  s << "# This MySQL options file was generated by innobackupex.\n\n"
    << "# The MySQL server\n"
    << "[mysqld]\n"
    << "innodb_checksum_algorithm="
    << innodb_checksum_algorithm_names[srv_checksum_algorithm] << "\n"
    << "innodb_log_checksums=" << srv_log_checksums << "\n"
    << "innodb_data_file_path=" << innobase_data_file_path << "\n"
    << "innodb_log_files_in_group=" << srv_n_log_files << "\n"
    << "innodb_log_file_size=" << innobase_log_file_size << "\n"
    << "innodb_page_size=" << srv_page_size << "\n"
    << "innodb_undo_directory=" << srv_undo_dir << "\n"
    << "innodb_undo_tablespaces=" << srv_undo_tablespaces << "\n"
    << "server_id=" << server_id << "\n"
    << "innodb_log_checksums=" << (srv_log_checksums ? "ON" : "OFF") << "\n"
    << "innodb_redo_log_encrypt=" << (srv_redo_log_encrypt ? "ON" : "OFF")
    << "\n"
    << "innodb_undo_log_encrypt=" << (srv_undo_log_encrypt ? "ON" : "OFF")
    << "\n";

  if (innobase_buffer_pool_filename) {
    s << "innodb_buffer_pool_filename=" << innobase_buffer_pool_filename
      << "\n";
  }

  I_List_iterator<i_string> iter(*opt_plugin_load_list_ptr);
  i_string *item;
  while ((item = iter++) != NULL) {
    s << "plugin_load=" << item->ptr << "\n";
  }

  if (server_uuid[0] != 0) {
    s << "server_uuid=" << server_uuid << "\n";
  }
  s << "master_key_id=" << Encryption::get_master_key_id() << "\n";

  return backup_file_print("backup-my.cnf", s.str().c_str(), s.tellp());
}

static char *make_argv(char *buf, size_t len, int argc, char **argv) {
  size_t left = len;
  const char *arg;

  buf[0] = 0;
  ++argv;
  --argc;
  while (argc > 0 && left > 0) {
    arg = *argv;
    if (strncmp(*argv, "--password", strlen("--password")) == 0) {
      arg = "--password=...";
    }
    if (strncmp(*argv, "--encrypt-key", strlen("--encrypt-key")) == 0) {
      arg = "--encrypt-key=...";
    }
    if (strncmp(*argv, "--encrypt_key", strlen("--encrypt_key")) == 0) {
      arg = "--encrypt_key=...";
    }
    if (strncmp(*argv, "--transition-key", strlen("--transition-key")) == 0) {
      arg = "--transition-key=...";
    }
    if (strncmp(*argv, "--transition_key", strlen("--transition_key")) == 0) {
      arg = "--transition_key=...";
    }
    left -= snprintf(buf + len - left, left, "%s%c", arg, argc > 1 ? ' ' : 0);
    ++argv;
    --argc;
  }

  return buf;
}

void capture_tool_command(int argc, char **argv) {
  /* capture tool name tool args */
  tool_name = strrchr(argv[0], '/');
  tool_name = tool_name ? tool_name + 1 : argv[0];

  make_argv(tool_args, sizeof(tool_args), argc, argv);
}

bool select_history() {
  if (opt_incremental_history_name || opt_incremental_history_uuid) {
    if (!select_incremental_lsn_from_history(&incremental_lsn)) {
      return (false);
    }
  }
  return (true);
}

bool flush_changed_page_bitmaps() {
  if (xtrabackup_incremental && have_changed_page_bitmaps &&
      !xtrabackup_incremental_force_scan) {
    xb_mysql_query(mysql_connection,
                   "FLUSH NO_WRITE_TO_BINLOG CHANGED_PAGE_BITMAPS", false);
  }
  return (true);
}

/*********************************************************************/ /**
 Deallocate memory, disconnect from MySQL server, etc.
 @return	true on success. */
void backup_cleanup() {
  free(buffer_pool_filename);
  free(backup_uuid);
  backup_uuid = NULL;

  if (mysql_connection) {
    mysql_close(mysql_connection);
  }
}

static MYSQL *mdl_con = NULL;
void mdl_lock_tables() {
  msg_ts("Initializing MDL on all current tables.\n");
  MYSQL_RES *mysql_result = NULL;
  MYSQL_ROW row;
  mdl_con = xb_mysql_connect();
  if (mdl_con != NULL) {
    xb_mysql_query(mdl_con, "BEGIN", false, true);
    mysql_result = xb_mysql_query(mdl_con,
                                  "SELECT NAME, SPACE FROM "
                                  "INFORMATION_SCHEMA.INNODB_TABLES",
                                  true, true);
    while ((row = mysql_fetch_row(mysql_result))) {
      if (fsp_is_ibd_tablespace(atoi(row[1]))) {
        char full_table_name[MAX_FULL_NAME_LEN + 1];
        innobase_format_name(full_table_name, sizeof(full_table_name), row[0]);
        if (is_fts_index(full_table_name)) {
          // We will eventually get to the row to lock the main table
          msg_ts("%s is a Full-Text Index. Skipping\n", full_table_name);
          continue;
        } else if (is_tmp_table(full_table_name)) {
          // We cannot run SELECT ... #sql-; Skipped to avoid invalid query.
          msg_ts("%s is a temporary table. Skipping\n", full_table_name);
          continue;
        }

        msg_ts("Locking MDL for %s\n", full_table_name);
        char *lock_query;
        xb_a(
            asprintf(&lock_query, "SELECT 1 FROM %s LIMIT 0", full_table_name));

        xb_mysql_query(mdl_con, lock_query, false, false);

        free(lock_query);
      }
    }
    mysql_free_result(mysql_result);
  }
}

void mdl_unlock_all() {
  msg_ts("Unlocking MDL for all tables\n");
  if (mdl_con != NULL) {
    xb_mysql_query(mdl_con, "COMMIT", false, true);
    mysql_close(mdl_con);
  }
}
bool is_fts_index(const std::string &tablespace) {
  const char *pattern =
      "^(FTS|fts)_[0-9a-fA-f]{16}_(([0-9a-fA-]{16}_(INDEX|index)_[1-6])|"
      "DELETED_CACHE|deleted_cache|DELETED|deleted|CONFIG|config|BEING_DELETED|"
      "being_deleted|BEING_DELETED_CACHE|beign_deleted_cache)$";
  const char *error_context = "is_fts_index";
  return check_regexp_table_name(tablespace, error_context, pattern);
}

bool is_tmp_table(const std::string &tablespace) {
  const char *pattern = "^#sql";
  const char *error_context = "is_tmp_table";
  return check_regexp_table_name(tablespace, error_context, pattern);
}

bool check_regexp_table_name(std::string tablespace, const char *error_context,
                             const char *pattern) {
  bool result = false;
  get_table_name_from_tablespace(tablespace);
  xb_regex_t preg;
  size_t nmatch = 1;
  xb_regmatch_t pmatch[1];
  compile_regex(pattern, error_context, &preg);

  if (xb_regexec(&preg, tablespace.c_str(), nmatch, pmatch, 0) != REG_NOMATCH) {
    result = true;
  }
  xb_regfree(&preg);
  return result;
}
void get_table_name_from_tablespace(std::string &tablespace) {
  std::size_t pos = tablespace.find("`.`");  //`db`.`table` separator
  if (pos != std::string::npos) {
    tablespace = tablespace.substr(pos + 3);
    tablespace.erase(tablespace.size() - 1);  // remove leading `
  }
}

bool has_innodb_buffer_pool_dump() {
  if ((server_flavor == FLAVOR_PERCONA_SERVER ||
       server_flavor == FLAVOR_MYSQL) &&
      mysql_server_version >= 50603) {
    return (true);
  }

  if (server_flavor == FLAVOR_MARIADB && mysql_server_version >= 10000) {
    return (true);
  }

  msg_ts("Server has no support for innodb_buffer_pool_dump_now");
  return (false);
}

bool has_innodb_buffer_pool_dump_pct() {
  if ((server_flavor == FLAVOR_PERCONA_SERVER ||
       server_flavor == FLAVOR_MYSQL) &&
      mysql_server_version >= 50702) {
    return (true);
  }

  if (server_flavor == FLAVOR_MARIADB && mysql_server_version >= 10110) {
    return (true);
  }

  return (false);
}

void dump_innodb_buffer_pool(MYSQL *connection) {
  innodb_buffer_pool_dump = has_innodb_buffer_pool_dump();
  innodb_buffer_pool_dump_pct = has_innodb_buffer_pool_dump_pct();
  if (!innodb_buffer_pool_dump) {
    return;
  }

  innodb_buffer_pool_dump_start_time = (ssize_t)my_time(MY_WME);

  char *buf_innodb_buffer_pool_dump_pct;
  char change_bp_dump_pct_query[100];

  /* Verify if we need to change innodb_buffer_pool_dump_pct */
  if (opt_dump_innodb_buffer_pool_pct != 0 && innodb_buffer_pool_dump_pct) {
    mysql_variable variables[] = {
        {"innodb_buffer_pool_dump_pct", &buf_innodb_buffer_pool_dump_pct},
        {NULL, NULL}};
    read_mysql_variables(connection,
                         "SHOW GLOBAL VARIABLES "
                         "LIKE 'innodb_buffer_pool_dump_pct'",
                         variables, true);

    original_innodb_buffer_pool_dump_pct =
        atoi(buf_innodb_buffer_pool_dump_pct);

    free_mysql_variables(variables);
    snprintf(change_bp_dump_pct_query, sizeof(change_bp_dump_pct_query),
             "SET GLOBAL innodb_buffer_pool_dump_pct = %u",
             opt_dump_innodb_buffer_pool_pct);
    msg_ts("Executing %s \n", change_bp_dump_pct_query);
    xb_mysql_query(mysql_connection, change_bp_dump_pct_query, false);
  }

  msg_ts("Executing SET GLOBAL innodb_buffer_pool_dump_now=ON...\n");
  xb_mysql_query(mysql_connection, "SET GLOBAL innodb_buffer_pool_dump_now=ON;",
                 false);
}

void check_dump_innodb_buffer_pool(MYSQL *connection) {
  if (!innodb_buffer_pool_dump) {
    return;
  }
  const ssize_t timeout = opt_dump_innodb_buffer_pool_timeout;

  char *innodb_buffer_pool_dump_status;
  char change_bp_dump_pct_query[100];

  mysql_variable status[] = {
      {"Innodb_buffer_pool_dump_status", &innodb_buffer_pool_dump_status},
      {NULL, NULL}};

  read_mysql_variables(connection,
                       "SHOW STATUS LIKE "
                       "'Innodb_buffer_pool_dump_status'",
                       status, true);

  /* check if dump has been completed */
  msg_ts("Checking if InnoDB buffer pool dump has completed\n");
  while (!strstr(innodb_buffer_pool_dump_status, "dump completed at")) {
    if (innodb_buffer_pool_dump_start_time + timeout <
        (ssize_t)my_time(MY_WME)) {
      msg_ts(
          "InnoDB Buffer Pool Dump was not completed "
          "after %d seconds... Adjust "
          "--dump-innodb-buffer-pool-timeout if you "
          "need higher wait time before copying %s.\n",
          opt_dump_innodb_buffer_pool_timeout, buffer_pool_filename);
      break;
    }

    read_mysql_variables(connection,
                         "SHOW STATUS LIKE 'Innodb_buffer_pool_dump_status'",
                         status, true);

    os_thread_sleep(1000000);
  }

  free_mysql_variables(status);

  /* restore original innodb_buffer_pool_dump_pct */
  if (opt_dump_innodb_buffer_pool_pct != 0 && innodb_buffer_pool_dump_pct) {
    snprintf(change_bp_dump_pct_query, sizeof(change_bp_dump_pct_query),
             "SET GLOBAL innodb_buffer_pool_dump_pct = %u",
             original_innodb_buffer_pool_dump_pct);
    xb_mysql_query(mysql_connection, change_bp_dump_pct_query, false);
  }
}
