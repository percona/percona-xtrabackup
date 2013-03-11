############################################################################
# Test basic local backup with compression
############################################################################

if ! which qpress > /dev/null 2>&1 ; then
  echo "Requires qpress to be installed" > $SKIPPED_REASON
  exit $SKIPPED_EXIT_CODE
fi

innobackupex_options="--compress --compress-threads=4 --compress-chunk-size=8K"
data_decompress_cmd="for i in *.qp;  do qpress -d \$i ./; done; \
for i in sakila/*.qp; do qpress -d \$i sakila/; done"

. inc/xb_local.sh
