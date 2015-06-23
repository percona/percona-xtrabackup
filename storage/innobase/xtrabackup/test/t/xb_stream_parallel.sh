############################################################################
# Test parallel streaming feature of the 'xbstream' format
############################################################################

stream_format=xbstream
stream_extract_cmd="xbstream -xv <"
xtrabackup_options="--parallel=16"

. inc/xb_stream_common.sh
