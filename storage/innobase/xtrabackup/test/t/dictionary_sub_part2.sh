. inc/dictionary_common.sh

start_server

create_part_sub_table
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
