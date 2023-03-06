. inc/dictionary_common.sh

start_server

#   PARTITION p0 VALUES LESS THAN (2000) ENGINE = InnoDB,
#   PARTITION P1 VALUES LESS THAN (4000) ENGINE = InnoDB,
#   PARTITION p2 VALUES LESS THAN (6000) ENGINE = InnoDB,
#   PARTITION p3 VALUES LESS THAN (8000) ENGINE = InnoDB,
#   PARTITION p4 VALUES LESS THAN MAXVALUE ENGINE = InnoDB

vlog "Truncate Parition p0 and rollback on p1"
create_part_table
load_data_part_table

mysql test <<EOF
ALTER TABLE t1 TRUNCATE PARTITION p0;
EOF
test_partition 2000 3999

vlog "Drop partition p0 and rollback on p3"
mysql test <<EOF
ALTER TABLE t1 DROP PARTITION p0;
EOF
test_partition 6000 7999
