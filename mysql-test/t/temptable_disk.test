#
# Test temptable_max_mmap
#

# Restart the server to clean up any prior allocations
let $restart_parameters=;
--source include/restart_mysqld.inc

SELECT @@global.temptable_max_mmap;

--error ER_GLOBAL_VARIABLE
SET @@session.temptable_max_mmap=1073741824;

--error ER_WRONG_TYPE_FOR_VAR
SET @@global.temptable_max_mmap=NONE;

--echo # Test with temptable_max_mmap set to 0
--echo # to stop overflowing temptable to disk

SET @@global.temptable_max_mmap=0;

SELECT @@global.temptable_max_mmap;

# Ensure there are no existing allocations
SELECT count_alloc > 0
FROM performance_schema.memory_summary_global_by_event_name
WHERE event_name = 'memory/temptable/physical_disk';

CREATE TABLE t (c VARCHAR(128));

INSERT INTO t VALUES
(REPEAT('a', 128)),
(REPEAT('b', 128)),
(REPEAT('c', 128)),
(REPEAT('d', 128));

ANALYZE TABLE t;

SET GLOBAL temptable_max_ram = 2097152;

-- disable_result_log
SELECT * FROM
t AS t1,
t AS t2,
t AS t3,
t AS t4,
t AS t5,
t AS t6
ORDER BY 1
LIMIT 2;
-- enable_result_log

SET GLOBAL temptable_max_ram = 1073741824;

# There should be no allocations on the physical disk
SELECT count_alloc > 0
FROM performance_schema.memory_summary_global_by_event_name
WHERE event_name = 'memory/temptable/physical_disk';

DROP TABLE t;

--echo # Test with temptable_max_mmap set to nonzero
--echo # to enable overflowing temptable to disk

SET @@global.temptable_max_mmap = 1073741824;
SELECT @@global.temptable_max_mmap;

#
# Test TempTable overflow to disk
#

CREATE TABLE t (c LONGBLOB);  # Forces use of temporary table in filesort.

INSERT INTO t VALUES
(REPEAT('a', 128)),
(REPEAT('b', 128)),
(REPEAT('c', 128)),
(REPEAT('d', 128));

ANALYZE TABLE t;

SET GLOBAL temptable_max_ram = 2097152;
-- disable_result_log
SELECT * FROM
t AS t1,
t AS t2,
t AS t3,
t AS t4,
t AS t5,
t AS t6
ORDER BY 1
LIMIT 2;
-- enable_result_log

SET GLOBAL temptable_max_ram = 1073741824;
SET GLOBAL temptable_max_mmap = 0;

SELECT @@global.temptable_max_mmap;

# Just make sure some disk pages were allocated, the exact number of bytes
# and pages is irrelevant for this test.
--skip_if_hypergraph  # Different plan. Avoids temporary table.
SELECT count_alloc > 0
FROM performance_schema.memory_summary_global_by_event_name
WHERE event_name = 'memory/temptable/physical_disk';

DROP TABLE t;

#
# Test TempTable by using a query which will exhaust both RAM and MMAP limits.
#

CREATE TABLE t (c LONGBLOB);  # Forces use of temporary table in filesort.

INSERT INTO t VALUES
(REPEAT('a', 128)),
(REPEAT('b', 128)),
(REPEAT('c', 128)),
(REPEAT('d', 128));

ANALYZE TABLE t;

SET GLOBAL temptable_max_ram = 2*1024*1024;
SET GLOBAL temptable_max_mmap = 4*1024*1024;

-- disable_result_log
SELECT * FROM
t AS t1,
t AS t2,
t AS t3,
t AS t4,
t AS t5,
t AS t6
ORDER BY 1
LIMIT 2;
-- enable_result_log

# We need to make sure we have exhausted the RAM limit
# Greater-or-equal must be used as memory for shared-block is not accounted for. After fixing
# Bug #29890126 TEMPTABLE ALLOCATOR DOESN'T TRACK RAM-CONSUMPTION FOR SHARED-BLOCK we can
# switch to using equal-operator (as we do for MMAP-ed allocations down below)
--skip_if_hypergraph  # Different plan. Avoids temporary table.
SELECT sum_number_of_bytes_alloc >= 2*1024*1024
FROM performance_schema.memory_summary_global_by_event_name
WHERE event_name = 'memory/temptable/physical_ram';

# We need to make sure we have exhausted the MMAP limit
--skip_if_hypergraph  # Different plan. Avoids temporary table.
SELECT sum_number_of_bytes_alloc = 4*1024*1024 + 64
FROM performance_schema.memory_summary_global_by_event_name
WHERE event_name = 'memory/temptable/physical_disk';

DROP TABLE t;

SET GLOBAL temptable_max_ram = 1073741824;
SET GLOBAL temptable_max_mmap = 1073741824;
