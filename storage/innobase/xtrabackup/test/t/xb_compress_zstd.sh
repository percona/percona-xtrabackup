############################################################################
# Test basic local backup with zstd compression
############################################################################

require_zstd

xtrabackup_options="--compress=zstd --compress-threads=4"
data_decompress_cmd="xtrabackup --decompress --target-dir=./"

. inc/xb_local.sh

stop_server
rm -rf $topdir/backup $mysql_datadir
encrypt_algo="AES256"
encrypt_key="percona_xtrabackup_is_awesome___"
stream_format=xbstream
stream_extract_cmd="(xbstream -xv --parallel=16 --decompress --decrypt=$encrypt_algo --encrypt-key=$encrypt_key --encrypt-threads=4) <"
xtrabackup_options="--parallel=16 --compress=zstd --compress-threads=4 --encrypt=$encrypt_algo --encrypt-key=$encrypt_key --encrypt-threads=4 --encrypt-chunk-size=8K"

. inc/xb_stream_common.sh
