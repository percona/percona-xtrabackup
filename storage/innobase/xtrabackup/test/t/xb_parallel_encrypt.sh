############################################################################
# Test basic local parallel backup with encryption
############################################################################

encrypt_algo="AES256"
encrypt_key="percona_xtrabackup_is_awesome___"

xtrabackup_options="--parallel=4 --encrypt=$encrypt_algo --encrypt-key=$encrypt_key --encrypt-threads=4 --encrypt-chunk-size=8K"
data_decrypt_cmd="xtrabackup --decrypt=${encrypt_algo} --encrypt-key=${encrypt_key} --parallel=4 --target-dir=./"

. inc/xb_local.sh
