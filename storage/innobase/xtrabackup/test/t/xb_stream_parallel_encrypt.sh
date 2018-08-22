############################################################################
# Test streaming + parallel + encryption
############################################################################


encrypt_algo="AES256"
encrypt_key="percona_xtrabackup_is_awesome___"
stream_format=xbstream
stream_extract_cmd="(xbstream -xv --parallel=16 && xtrabackup --decrypt=$encrypt_algo --encrypt-key=$encrypt_key --target-dir=./) <"
xtrabackup_options="--parallel=16 --encrypt=$encrypt_algo --encrypt-key=$encrypt_key --encrypt-threads=4 --encrypt-chunk-size=8K"

. inc/xb_stream_common.sh
