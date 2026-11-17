########################################################################
# The rows of an HNSW vector index are in the backup
#
# Percona Server redo-logs the label counter of a vector-indexed table
# (the hidden column percona_vec_aux_id) as dynamic metadata of type
# PM_TABLE_VEC_IDX_ID = 200. xtrabackup must parse it during --backup and
# carry it through --prepare. See inc/vector_index_common.sh.
#
# index_debug.sh runs the same and checks the graph on a debug server.
########################################################################

. inc/common.sh

. inc/vector_index_common.sh
