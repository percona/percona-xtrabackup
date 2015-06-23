############################################################################
# Test basic local parallel backup with compression and encryption
############################################################################

require_qpress

encrypt_algo="AES256"
encrypt_key="percona_xtrabackup_is_awesome___"

innobackupex_options="--parallel=8 --compress --compress-threads=4 --compress-chunk-size=8K --encrypt=$encrypt_algo --encrypt-key=$encrypt_key --encrypt-threads=4 --encrypt-chunk-size=8K"

data_decrypt_cmd="innobackupex --decrypt=${encrypt_algo} --encrypt-key=${encrypt_key} --decompress --parallel=8 ./"

. inc/ib_local.sh
