--source include/have_hypergraph.inc
--source include/elide_costs.inc

SET EXPLAIN_FORMAT=tree;

--echo "Optimizer switch subquery_to_derived for subquery scalar to derived transformations."
--source include/subquery_scalar_to_derived.inc

--echo "Optimizer switch subquery_to_derived for subquery table to derived transformations."
--source include/subquery_table_to_derived.inc

SET EXPLAIN_FORMAT=default;
--source include/disable_hypergraph.inc
