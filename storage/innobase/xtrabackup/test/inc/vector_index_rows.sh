########################################################################
# Rows of test.t (id BIGINT UNSIGNED PRIMARY KEY AUTO_INCREMENT,
# v VECTOR(4) NOT NULL) with the HNSW vector index vk, for the tests of
# suites/vector. Sourced after inc/common.sh.
########################################################################

# INSERT n rows of random vectors, one transaction each.
# @param $1 n
function insert_rows()
{
  for i in $(seq $1)
  do
    echo "INSERT INTO t (v) VALUES (STRING_TO_VECTOR(CONCAT('[', RAND(), ',',
            RAND(), ',', RAND(), ',', RAND(), ']')));"
  done | mysql test
}

# Save what the server has into $topdir/<name>.*:
#   rows     id and vector of each row, read through the primary key
#   index    the same, read through vk; the read goes on past LIMIT until
#            the graph is exhausted, so it has every row with a node
#   autoinc  AUTO_INCREMENT
#   graph    if the test defines dump_graph
# @param $1 name
function snapshot()
{
  # Without statistics a table looks empty and is not searched through vk.
  $MYSQL $MYSQL_ARGS -Ns test -e "ANALYZE TABLE t" >/dev/null
  $MYSQL $MYSQL_ARGS -Ns test -e "EXPLAIN SELECT id FROM t FORCE INDEX (vk)
      ORDER BY DISTANCE(v, STRING_TO_VECTOR('[0,0,0,0]'), 'EUCLIDEAN')
      LIMIT 1000" |
    grep -q "Vector index search on t using vk" ||
    die "$1: the read through vk does not use the vector index"

  $MYSQL $MYSQL_ARGS -Ns test -e "SELECT id, VECTOR_TO_STRING(v)
      FROM t FORCE INDEX (PRIMARY) ORDER BY id" > $topdir/$1.rows
  $MYSQL $MYSQL_ARGS -Ns test -e "SELECT id, VECTOR_TO_STRING(v)
      FROM t FORCE INDEX (vk)
      ORDER BY DISTANCE(v, STRING_TO_VECTOR('[0,0,0,0]'), 'EUCLIDEAN')
      LIMIT 1000" | sort -n > $topdir/$1.index
  $MYSQL $MYSQL_ARGS -Ns test -e "SET SESSION information_schema_stats_expiry = 0;
      SELECT AUTO_INCREMENT FROM information_schema.tables
      WHERE table_schema = 'test' AND table_name = 't'" > $topdir/$1.autoinc
  if declare -F dump_graph >/dev/null
  then
    dump_graph $topdir/$1.graph
  fi

  # Without this, an index that returns nothing on both servers would pass.
  diff $topdir/$1.rows $topdir/$1.index ||
    die "$1: the rows read through vk are not the rows of the table"
}
