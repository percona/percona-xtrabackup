############################################################################
# Test streaming + compression
############################################################################

require_qpress

stream_format=xbstream
stream_extract_cmd="xbstream -xv <"
stream_uncompress_cmd="for i in *.qp;  do qpress -d \$i ./; done; \
for i in sakila/*.qp; do qpress -d \$i sakila/; done"
innobackupex_options="--compress --compress-threads=4 --compress-chunk-size=8K"

. inc/ib_stream_common.sh
