############################################################################
# Test basic local backup with encryption
############################################################################

encrypt_algo="AES256"
encrypt_key="percona_xtrabackup_is_awesome___"
wrong_encrypt_key="Percona_XtraBackup_Is_Awesome___"

xtrabackup_options="--encrypt=$encrypt_algo --encrypt-key=$encrypt_key --encrypt-threads=4 --encrypt-chunk-size=8K"
data_decrypt_cmd="xtrabackup --decrypt=${encrypt_algo} --encrypt-key=${encrypt_key} --target-dir=./"
data_decrypt_cmd_wrong_passphrase="xtrabackup --decrypt=${encrypt_algo} --encrypt-key=${wrong_encrypt_key} --target-dir=./"

. inc/xb_local.sh
