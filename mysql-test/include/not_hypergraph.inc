# include/not_hypergraph.inc
#
# Skip the test if we are using the hypergraph optimizer. This is typically
# because the test expects some given query plan, or more generally expects
# certain debug/non-critical input that will differ. Tests that give the wrong
# answer or crash under the hypergraph optimizer should not be using this file;
# instead, the optimizer should error out with ER_HYPERGRAPH_NOT_SUPPORTED_YET.
#

if (`select locate('hypergraph_optimizer=on', @@optimizer_switch) > 0`)
{
  --skip server has hypergraph_optimizer ON
}
