# 
# Run join_outer.test with BKA enabled 
#
set optimizer_switch='batched_key_access=on,mrr_cost_based=off';

# Hypergraph ignores the batched_key_access switch, so this test
# doesn't add test coverage for hypergraph.
--source include/not_hypergraph.inc

--source t/join_outer.test

set optimizer_switch=default;
