############################################################################
# Test basic local backup with encryption
############################################################################

encrypt_algo="AES256"
encrypt_key="percona_xtrabackup_is_awesome___"
encrypt_key_file=${TEST_VAR_ROOT}/xb_encrypt.key

echo -n $encrypt_key > $encrypt_key_file

innobackupex_options="--encrypt=$encrypt_algo --encrypt-key-file=$encrypt_key_file --encrypt-threads=4 --encrypt-chunk-size=8K"
data_decrypt_cmd="for i in *.xbcrypt; do \
xbcrypt -d -a $encrypt_algo -f $encrypt_key_file < \$i > \${i:0:\${#i}-8}; \
rm -f \$i; done; \
for i in ./sakila/*.xbcrypt; do \
xbcrypt -d -a $encrypt_algo -f $encrypt_key_file < \$i > \${i:0:\${#i}-8}; \
rm -f \$i; done;"

. inc/xb_local.sh

rm -f $encrypt_key_file
