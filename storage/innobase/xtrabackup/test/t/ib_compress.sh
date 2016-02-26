############################################################################
# Test basic local backup with compression
############################################################################

require_qpress

innobackupex_options="--compress --compress-threads=4 --compress-chunk-size=8K"
data_decompress_cmd="innobackupex --decompress ./"

. inc/ib_local.sh
