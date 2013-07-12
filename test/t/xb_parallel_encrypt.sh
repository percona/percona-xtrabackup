############################################################################
# Test basic local parallel backup with encryption
############################################################################

encrypt_algo="AES256"
encrypt_key="percona_xtrabackup_is_awesome___"

innobackupex_options="--parallel=4 --encrypt=$encrypt_algo --encrypt-key=$encrypt_key --encrypt-threads=4 --encrypt-chunk-size=8K"
data_decrypt_cmd="for i in \`find \\\`pwd\\\` -name \*.xbcrypt\`; do \
xbcrypt -d -a $encrypt_algo -k $encrypt_key -i \$i -o \${i:0:\${#i}-8}; \
rm -f \$i; done;"

. inc/xb_local.sh
