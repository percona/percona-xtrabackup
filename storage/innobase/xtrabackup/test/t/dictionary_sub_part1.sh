. inc/dictionary_common.sh

start_server

vlog "Creating partition table with SUBPARTITIONS"
create_part_sub_table

vlog "Load data into partition table with sub partitions"
load_data_part_table

vlog "Testing rollback on partition p0sp0"
test_partition 0 199

vlog "Testing rollback on partition p1sp1"
test_partition 2400 2799

vlog "Testing rollback on partition p2*"
test_partition 4000 5999

vlog "Testing rollback on partition p3*"
test_partition 6000 7999

vlog "Testing rollback on partition p4"
test_partition 8000 10000
