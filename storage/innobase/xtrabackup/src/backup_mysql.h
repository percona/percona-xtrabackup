/******************************************************
Copyright (c) 2011-2022 Percona LLC and/or its affiliates.

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

*******************************************************/

#ifndef XTRABACKUP_BACKUP_MYSQL_H
#define XTRABACKUP_BACKUP_MYSQL_H

#include <mysql.h>
#include <string>
#include <unordered_set>
#include <vector>

#include "redo_log.h"
#include "xtrabackup.h"

// Forward declaration
class Redo_Log_Data_Manager;

/* mysql flavor and version */
enum mysql_flavor_t {
  FLAVOR_UNKNOWN,
  FLAVOR_MYSQL,
  FLAVOR_PERCONA_SERVER,
  FLAVOR_MARIADB
};
extern mysql_flavor_t server_flavor;
extern unsigned long mysql_server_version;
extern std::string mysql_server_version_comment;

struct replication_channel_status_t {
  std::string channel_name;
  std::string relay_log_file;
  uint64_t relay_log_position;
  std::string relay_master_log_file;
  uint64_t exec_master_log_position;
};

struct rocksdb_wal_t {
  size_t log_number;
  std::string path_name;
  size_t file_size_bytes;
};

struct log_status_t {
  std::string filename;
  uint64_t position;
  std::string gtid_executed;
  lsn_t lsn;
  lsn_t lsn_checkpoint;
  std::vector<replication_channel_status_t> channels;
  std::vector<rocksdb_wal_t> rocksdb_wal_files;
  void clear() {
    filename = "";
    position = 0;
    gtid_executed = "";
    lsn = 0;
    lsn_checkpoint = 0;
    channels.clear();
    rocksdb_wal_files.clear();
  }
};

struct mysql_variable {
  const char *name;
  char **value;
};

#define ROCKSDB_SUBDIR ".rocksdb"

class Myrocks_datadir {
 public:
  using file_list = std::vector<datadir_entry_t>;

  using const_iterator = file_list::const_iterator;

  Myrocks_datadir(const std::string &datadir,
                  const std::string &wal_dir = std::string()) {
    rocksdb_datadir = datadir;
    rocksdb_wal_dir = wal_dir;
  }

  file_list files(const char *dest_data_dir = ROCKSDB_SUBDIR,
                  const char *dest_wal_dir = ROCKSDB_SUBDIR) const;

  file_list data_files(const char *dest_datadir = ROCKSDB_SUBDIR) const;

  file_list wal_files(const char *dest_wal_dir = ROCKSDB_SUBDIR) const;

  file_list meta_files(const char *dest_wal_dir = ROCKSDB_SUBDIR) const;

 private:
  std::string rocksdb_datadir;
  std::string rocksdb_wal_dir;

  enum scan_type_t { SCAN_ALL, SCAN_WAL, SCAN_DATA, SCAN_META };

  void scan_dir(const std::string &dir, const char *dest_data_dir,
                const char *dest_wal_dir, scan_type_t scan_type,
                file_list &result) const;
};

class Myrocks_checkpoint {
 private:
  std::string checkpoint_dir;
  std::string rocksdb_datadir;
  std::string rocksdb_wal_dir;

  MYSQL *con;

 public:
  using file_list = Myrocks_datadir::file_list;

  Myrocks_checkpoint() {}

  /* create checkpoint and optionally disable file deletions */
  void create(MYSQL *con, bool disable_file_deletions);

  /* remove checkpoint */
  void remove() const;

  /* enable file deletions */
  void enable_file_deletions() const;

  /* get the list of live WAL files */
  file_list wal_files(const log_status_t &log_status) const;

  /* get the list of checkpoint files */
  file_list checkpoint_files(const log_status_t &log_status) const;

  /* get the list of sst files */
  file_list data_files() const;
};

struct Backup_context {
  Myrocks_checkpoint myrocks_checkpoint;
  std::unordered_set<std::string> rocksdb_files;
  Redo_Log_Data_Manager *redo_mgr;
};

/* server capabilities */
extern bool have_backup_locks;
extern bool have_lock_wait_timeout;
extern bool have_galera_enabled;
extern bool have_flush_engine_logs;
extern bool have_multi_threaded_slave;
extern bool have_gtid_slave;
extern bool have_rocksdb;
extern bool have_keyring_component;

/* History on server */
extern time_t history_start_time;
extern time_t history_end_time;
extern time_t history_lock_time;

extern bool sql_thread_started;
extern std::string mysql_slave_position;
extern std::string mysql_binlog_position;
extern char *buffer_pool_filename;

/** connection to mysql server */
extern MYSQL *mysql_connection;

void capture_tool_command(int argc, char **argv);

bool select_history();

void backup_cleanup();

bool get_mysql_vars(MYSQL *connection);

bool detect_mysql_capabilities_for_backup();

MYSQL *xb_mysql_connect();

MYSQL_RES *xb_mysql_query(MYSQL *connection, const char *query, bool use_result,
                          bool die_on_error = true);

my_ulonglong xb_mysql_numrows(MYSQL *connection, const char *query,
                              bool die_on_error);

char *read_mysql_one_value(MYSQL *connection, const char *query);

void read_mysql_variables(MYSQL *connection, const char *query,
                          mysql_variable *vars, bool vertical_result);

void free_mysql_variables(mysql_variable *vars);

void unlock_all(MYSQL *connection);

bool write_current_binlog_file(MYSQL *connection);

/** Read binary log position, InnoDB LSN and other storage engine information
from p_s.log_status and update global log_status variable.
@param[in]   conn                     mysql connection handle
@param[in]   consumer_can_advance     bool to define if consumer can advance */
void log_status_get(MYSQL *conn, bool consumer_can_advance);

/*********************************************************************/ /**
 Retrieves MySQL binlog position and
 saves it in a file. It also prints it to stdout.
 @param[in]   connection  MySQL connection handler
 @return true if success. */
bool write_binlog_info(MYSQL *connection);

char *get_xtrabackup_info(MYSQL *connection);

bool write_xtrabackup_info(MYSQL *connection);

bool write_backup_config_file();

bool lock_tables_for_backup(MYSQL *connection, int timeout, int retry_count);

bool lock_tables_maybe(MYSQL *connection, int timeout, int retry_count);

bool wait_for_safe_slave(MYSQL *connection);

bool write_galera_info(MYSQL *connection);

bool write_slave_info(MYSQL *connection);

void parse_show_engine_innodb_status(MYSQL *connection);

/** Acquires MDL lock on all tables */
void mdl_lock_tables();

/** Identifies if table_name is a Full Text Index.
@param[in]	table_name		table_name
@return true if table_name is FTS. */
bool is_fts_index(const std::string &table_name);

/** Identifies if table_name is a temporary table (#sql-)
@param[in]	table_name		table_name
@return true if table_name is temporary table. */
bool is_tmp_table(const std::string &table_name);

/** Identifies if table is blocked by select
SELECT from mysql.compression_* tables is not allowed by Server.
@param[in]	table_name		table_name
@return true if table_name is blocked. */
bool is_access_blocked(const std::string &table_name);

/** Runs a regexp against a table name
@param[in]	table_name		table_name
@param[in]	error_context		error to be return in case of errors
@param[in]	pattern		regexp pattern to try a match
@return true if there is a match, or false otherwise */
bool check_regexp_table_name(std::string table_name, const char *error_context,
                             const char *pattern);

/** Extract the table name from a full qualified `db`.`table` string
removing escape string. Replace the name inplace
@param[in/out]	fq_table_name		full qualified table name
 */
void get_table_name_from_fq(std::string &fq_table_name);

void mdl_unlock_all();

bool has_innodb_buffer_pool_dump();

bool has_innodb_buffer_pool_dump_pct();

void dump_innodb_buffer_pool(MYSQL *connection);

void check_dump_innodb_buffer_pool(MYSQL *connection);

/* Check if a specific server/backup dir version is supported.
 * @param[in]   version_number  version numbers ( 80100)
 * @param[in]   version_string  version string (8.1.0-1)
 * @param[in]   version_comment version comment (Percona Server (GPL),
 * Release 8.1.0-1, Revision xxx)
 * @param[in]   innodb_version  innodb version (8.1.0-1)
 * @return true if tables with row versions > 0 */
bool check_server_version(unsigned long version_number,
                          const char *version_string,
                          const char *version_comment,
                          const char *innodb_version);
extern log_status_t log_status;
#endif
