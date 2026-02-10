#
# BIT type checks
#
--source include/elide_costs.inc

SET internal_tmp_mem_storage_engine='memory';
--source include/group_by_type_bit.inc
SET internal_tmp_mem_storage_engine='TempTable';
--source include/group_by_type_bit.inc
SET internal_tmp_mem_storage_engine=default;
SET SESSION big_tables=true;
--source include/group_by_type_bit.inc
SET SESSION big_tables=default;
