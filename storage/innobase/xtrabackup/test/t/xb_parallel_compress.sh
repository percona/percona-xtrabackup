############################################################################
# Test basic local parallel backup with compression
############################################################################

require_qpress

xtrabackup_options="--parallel=8 --compress --compress-threads=4 --compress-chunk-size=8K"
data_decompress_cmd="xtrabackup --decompress --parallel=8 --target-dir=./"

. inc/xb_local.sh
