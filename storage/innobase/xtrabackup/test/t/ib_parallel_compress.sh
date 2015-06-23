############################################################################
# Test basic local parallel backup with compression
############################################################################

require_qpress

innobackupex_options="--parallel=8 --compress --compress-threads=4 --compress-chunk-size=8K"
data_decompress_cmd="innobackupex --decompress --parallel=8 ./"

. inc/ib_local.sh
