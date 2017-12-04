############################################################################
# Test basic local backup with compression and encryption
############################################################################

require_qpress

encrypt_algo="AES256"
encrypt_key="percona_xtrabackup_is_awesome___"

xtrabackup_options="--compress --compress-threads=4 --compress-chunk-size=8K --encrypt=$encrypt_algo --encrypt-key=$encrypt_key --encrypt-threads=4 --encrypt-chunk-size=8K"

data_decrypt_cmd="xtrabackup --decrypt=${encrypt_algo} --encrypt-key=$encrypt_key --target-dir=./"
data_decompress_cmd="xtrabackup --decompress --target-dir=./"

. inc/xb_local.sh

stop_server
rm -rf $mysql_datadir
rm -rf ${topdir}/backup

KEY_FILE_NAME=${topdir}/key-file
echo -n $encrypt_key > $KEY_FILE_NAME

xtrabackup_options="--compress --compress-threads=4 --compress-chunk-size=8K --encrypt=$encrypt_algo --encrypt-key=$encrypt_key --encrypt-threads=4 --encrypt-chunk-size=8K"

data_decrypt_cmd="xtrabackup --decrypt=${encrypt_algo} --encrypt-key-file=$KEY_FILE_NAME --target-dir=./"
data_decompress_cmd="xtrabackup --decompress --target-dir=./"

. inc/xb_local.sh
