# include/hypergraph_is_active.inc
#
# Ask the server whether hypergraph_optimizer is ON
#
--let $hypergraph_is_active= 0
if (`select locate('hypergraph_optimizer=on', @@optimizer_switch) > 0`)
{
   --let $hypergraph_is_active= 1
}
