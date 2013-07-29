############################################################################
# Test basic local parallel backup with compression
############################################################################

require_qpress

innobackupex_options="--parallel=8 --compress --compress-threads=4 --compress-chunk-size=8K"
data_decompress_cmd="for i in *.qp;  do qpress -d \$i ./; done; \
for i in sakila/*.qp; do qpress -d \$i sakila/; done"

. inc/xb_local.sh
