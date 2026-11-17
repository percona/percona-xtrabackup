########################################################################
# The graph of an HNSW vector index is in the backup
#
# index.sh, and on a debug server the graph of the vector index too: it
# is dumped on the server before the backup and on the restored server,
# and the two dumps must be the same, line for line.
########################################################################

. inc/common.sh

require_debug_server

# Write the graph of the vector index: the aux table, one record per
# line, as vec_aux_dump prints it. For example:
#
#   count=14 max_id=13
#   id=0 level=0 base_pk=1 nb=
#   id=1 level=0 base_pk=1 nb=2,3,4,5,6,7,8,9,10,11,12,13
#   ...
#   id=13 level=0 base_pk=13 nb=9,11,5,8,1,7,2,6,12,3,10,4
#
#   id       the label of the node
#   level    the highest HNSW layer the node is on
#   base_pk  the id of the row the node is for
#   nb       the labels of its neighbours, 0 for an empty slot
#   id=0     not a node: the index metadata, base_pk is the entry point
#
# @param $1 file
function dump_graph()
{
  $MYSQL $MYSQL_ARGS -Ns -e "SET SESSION innodb_interpreter = 'vec_aux_dump test/t';
      SELECT @@session.innodb_interpreter_output" | sed 's/\\n/\n/g' > $1
  # An empty dump on both servers would compare equal.
  grep -q '^id=1 ' $1 || die "vec_aux_dump printed no nodes: $(cat $1)"
}

. inc/vector_index_common.sh
