############################################################################
# Test streaming + compression
############################################################################

require_qpress

stream_format=xbstream
stream_extract_cmd="xbstream -xv <"
stream_uncompress_cmd="xtrabackup --decompress --target-dir=./"
xtrabackup_options="--compress --compress-threads=4 --compress-chunk-size=8K"

. inc/xb_stream_common.sh
