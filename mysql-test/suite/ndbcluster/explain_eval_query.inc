--disable_query_log
echo ## Query ##;
echo $query;
echo ## Plan ##;
SET explain_json_format_version = 1;
eval explain format=json into @plan $query;
SET explain_json_format_version = DEFAULT;
--vertical_results

if(`SELECT JSON_CONTAINS_PATH(@plan, 'one', '$.query_block.insert_from.nested_loop')`){
  SELECT "insert_from.nested_loop" plan,
   JSON_EXTRACT(@plan, '$.query_block.insert_from.nested_loop[*].table.access_type') AS access_type,
   JSON_EXTRACT(@plan, "$.query_block.insert_from.nested_loop[*].table.pushed_condition") AS pushed_condition,
   JSON_EXTRACT(@plan, "$.query_block.insert_from.nested_loop[*].table.pushed_join") AS pushed_join,
   JSON_EXTRACT(@plan, '$.query_block.insert_from.nested_loop[*].table.using_MRR') AS using_MRR;
}
if(`SELECT JSON_CONTAINS_PATH(@plan, 'one', '$.query_block.insert_from.ordering_operation')`){
  SELECT "insert_from.ordering_operation" plan,
   JSON_EXTRACT(@plan, '$.query_block.insert_from.ordering_operation.table.access_type') AS access_type,
   JSON_EXTRACT(@plan, "$.query_block.insert_from.ordering_operation.table.pushed_condition") AS pushed_condition,
   JSON_EXTRACT(@plan, '$.query_block.insert_from.ordering_operation.table.using_MRR') AS using_MRR;
}
if(`SELECT JSON_CONTAINS_PATH(@plan, 'one', '$.query_block.nested_loop')`){
  SELECT "nested_loop" plan,
   JSON_EXTRACT(@plan, '$.query_block.nested_loop[*].table.access_type') AS access_type,
   JSON_EXTRACT(@plan, "$.query_block.nested_loop[*].table.pushed_condition") AS pushed_condition,
   JSON_EXTRACT(@plan, "$.query_block.nested_loop[*].table.pushed_join") AS pushed_join,
   JSON_EXTRACT(@plan, '$.query_block.nested_loop[*].table.using_MRR') AS using_MRR;
}
if(`SELECT JSON_CONTAINS_PATH(@plan, 'one', '$.query_block.ordering_operation')`){
  SELECT "ordering_operation" plan,
   JSON_EXTRACT(@plan, '$.query_block.ordering_operation.table.access_type') AS access_type,
   JSON_EXTRACT(@plan, "$.query_block.ordering_operation.table.pushed_condition") AS pushed_condition,
   JSON_EXTRACT(@plan, '$.query_block.ordering_operation.table.using_MRR') AS using_MRR;
}

echo ## Result ##;
--horizontal_results
--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
--sorted_result
eval $query;
--enable_query_log
