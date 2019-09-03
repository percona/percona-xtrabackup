#ifndef XTRABACKUP_BACKUP_MYSQL_H
#define XTRABACKUP_BACKUP_MYSQL_H

#include <mysql.h>
#include <string>

/* mysql flavor and version */
enum mysql_flavor_t { FLAVOR_UNKNOWN, FLAVOR_MYSQL,
		      FLAVOR_PERCONA_SERVER, FLAVOR_MARIADB };
extern mysql_flavor_t server_flavor;
extern unsigned long mysql_server_version;

/* server capabilities */
extern bool have_changed_page_bitmaps;
extern bool have_backup_locks;
extern bool have_lock_wait_timeout;
extern bool have_galera_enabled;
extern bool have_flush_engine_logs;
extern bool have_multi_threaded_slave;
extern bool have_gtid_slave;


/* History on server */
extern time_t history_start_time;
extern time_t history_end_time;
extern time_t history_lock_time;


extern bool sql_thread_started;
extern std::string mysql_slave_position;
extern char *mysql_binlog_position;
extern char *buffer_pool_filename;

/** connection to mysql server */
extern MYSQL *mysql_connection;

void
capture_tool_command(int argc, char **argv);

bool
select_history();

bool
flush_changed_page_bitmaps();

void
backup_cleanup();

bool
get_mysql_vars(MYSQL *connection);

bool
detect_mysql_capabilities_for_backup();

MYSQL *
xb_mysql_connect();

MYSQL_RES *
xb_mysql_query(MYSQL *connection, const char *query, bool use_result,
		bool die_on_error = true);

my_ulonglong
xb_mysql_numrows(MYSQL *connection, const char *query, bool die_on_error);

char *
read_mysql_one_value(MYSQL *connection, const char *query);

void
unlock_all(MYSQL *connection);

bool
write_current_binlog_file(MYSQL *connection);

bool
write_binlog_info(MYSQL *connection);

char*
get_xtrabackup_info(MYSQL *connection);

bool
write_xtrabackup_info(MYSQL *connection);

bool
write_backup_config_file();

bool
lock_binlog_maybe(MYSQL *connection, int timeout, int retry_count);

bool
lock_tables_for_backup(MYSQL *connection, int timeout, int retry_count);

bool
lock_tables_maybe(MYSQL *connection, int timeout, int retry_count);

bool
wait_for_safe_slave(MYSQL *connection);

bool
write_galera_info(MYSQL *connection);

bool
write_slave_info(MYSQL *connection);

void
parse_show_engine_innodb_status(MYSQL *connection);

void
mdl_lock_init();

void
mdl_lock_table(ulint space_id);

void
mdl_unlock_all();

bool
has_innodb_buffer_pool_dump();

bool
has_innodb_buffer_pool_dump_pct();

void
dump_innodb_buffer_pool(MYSQL *connection);

void
check_dump_innodb_buffer_pool(MYSQL *connection);

#endif
