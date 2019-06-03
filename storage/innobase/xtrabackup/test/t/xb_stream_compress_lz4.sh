############################################################################
# Test streaming + compression
############################################################################

require_qpress

stream_format=xbstream
stream_extract_cmd="xbstream -xv --decompress --decompress-threads=4 <"
stream_uncompress_cmd="true"
xtrabackup_options="--compress=lz4 --compress-threads=4 --compress-chunk-size=8K"

. inc/xb_stream_common.sh
