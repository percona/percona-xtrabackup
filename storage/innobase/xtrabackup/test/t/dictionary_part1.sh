. inc/dictionary_common.sh

start_server

#   PARTITION p0 VALUES LESS THAN (2000) ENGINE = InnoDB,
#   PARTITION P1 VALUES LESS THAN (4000) ENGINE = InnoDB,
#   PARTITION p2 VALUES LESS THAN (6000) ENGINE = InnoDB,
#   PARTITION p3 VALUES LESS THAN (8000) ENGINE = InnoDB,
#   PARTITION p4 VALUES LESS THAN MAXVALUE ENGINE = InnoDB

vlog "Creating partition table and loading data"
create_part_table

vlog "Load data into partition table"
load_data_part_table

vlog "Testing rollback on partition p0"
test_partition 0 1999

vlog "Testing rollback on partition p1"
test_partition 2000 3999

vlog "Testing rollback on partition p2"
test_partition 4000 5999

vlog "Testing rollback on partition p3"
test_partition 6000 7999

vlog "Testing rollback on partition p4"
test_partition 8000 10000
