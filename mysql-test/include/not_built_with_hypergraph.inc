# include/not_built_with_hypergraph.inc
#
# Skip the test if the server is built with the Hypergraph Optimizer.
# This is different from not_hypergraph, as we here check if the Hypergraph
# Optimizer is available at all, not just that it is off.
#

--disable_query_log
--error 0, ER_HYPERGRAPH_NOT_SUPPORTED_YET
SET @@session.optimizer_switch = 'hypergraph_optimizer=on',
    @@global.optimizer_switch = 'hypergraph_optimizer=on';
if (`SELECT FIND_IN_SET('hypergraph_optimizer=on', @@optimizer_switch)`) {
    SET SESSION optimizer_switch=DEFAULT;
    SET GLOBAL optimizer_switch=DEFAULT;
   --skip Test requires binaries built with WITH_HYPERGRAPH_OPTIMIZER=OFF
}
--enable_query_log