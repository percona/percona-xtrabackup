############################################################################
# Test basic local backup with encryption
############################################################################

encrypt_algo="AES256"
encrypt_key="percona_xtrabackup_is_awesome___"
encrypt_key_file=${TEST_VAR_ROOT}/xb_encrypt.key

echo -n $encrypt_key > $encrypt_key_file

innobackupex_options="--encrypt=$encrypt_algo --encrypt-key-file=$encrypt_key_file --encrypt-threads=4 --encrypt-chunk-size=8K"
data_decrypt_cmd="if [ -n \"\`find \\\`pwd\\\` -type f \! -name \*.xbcrypt\`\" ]; then \
echo \"*********************** UNENCRYPTED FILES FOUND ***********************\"; \
for i in \`find \\\`pwd\\\` -type f \! -name \*.xbcrypt\`; do \
echo \"UNENCRYPTED: \$i\"; done; \
else \
for i in \`find \\\`pwd\\\` -name \*.xbcrypt\`; do \
xbcrypt -d -a $encrypt_algo -f $encrypt_key_file -i \$i -o \${i:0:\${#i}-8}; \
rm -f \$i; done; fi"

. inc/xb_local.sh

rm -f $encrypt_key_file
