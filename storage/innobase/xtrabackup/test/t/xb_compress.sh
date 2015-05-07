############################################################################
# Test basic local backup with compression
############################################################################

require_qpress

xtrabackup_options="--compress --compress-threads=4 --compress-chunk-size=8K"
data_decompress_cmd="xtrabackup --decompress --target-dir=./"

. inc/xb_local.sh
