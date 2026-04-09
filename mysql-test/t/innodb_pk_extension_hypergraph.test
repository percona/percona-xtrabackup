--source include/have_hypergraph.inc

SET EXPLAIN_FORMAT=tree;

--echo #
--echo # Optimizer switch use_index_extensions=on/off with hypergraph
--echo #

set optimizer_switch= "use_index_extensions=off";
--source include/innodb_pk_extension.inc

set optimizer_switch="use_index_extensions=on";
--source include/innodb_pk_extension.inc

SET EXPLAIN_FORMAT=default;
set optimizer_switch=default;
--source include/disable_hypergraph.inc
