############################################################################
# Test parallel streaming feature of the 'xbstream' format
############################################################################

stream_format=xbstream
stream_extract_cmd="xbstream -xv --parallel=16 <"
innobackupex_options="--parallel=16"

. inc/ib_stream_common.sh
