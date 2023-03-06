. inc/dictionary_common.sh

start_server

vlog "test rollback on a table (fil_per_table_tablespace)"
create_tables t1

vlog "test rollback on a table in general tablespaces"
create_tablespace
create_tables t2 ts1

vlog "test rollback on a table in system tablespaces"
create_tables t3 innodb_system
