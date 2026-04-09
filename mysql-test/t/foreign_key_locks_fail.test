# Hypergraph does not show InnoDB locks on secondary indexes
# when performance_schema.data_locks is queried.
--source include/not_hypergraph.inc
--source include/have_debug.inc

--connect(con1, localhost, root,,)
SET SESSION innodb_lock_wait_timeout = 1;

connection default;
let $trx_isolation_level = READ UNCOMMITTED;
--source include/foreign_key_locks_fail.inc

let $trx_isolation_level = READ COMMITTED;
--source include/foreign_key_locks_fail.inc

let $trx_isolation_level = REPEATABLE READ;
--source include/foreign_key_locks_fail.inc

let $trx_isolation_level = SERIALIZABLE;
--source include/foreign_key_locks_fail.inc

connection default;
disconnect con1;
