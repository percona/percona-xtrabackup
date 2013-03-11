############################################################################
# Test basic local backup with encryption
############################################################################

encrypt_algo="AES256"
encrypt_key="percona_xtrabackup_is_awesome___"

echo -n "Percona XtraBackup is awesome!!!" > $encrypt_key_file

innobackupex_options="--encrypt=$encrypt_algo --encrypt-key=$encrypt_key --encrypt-threads=4 --encrypt-chunk-size=8K"
data_decrypt_cmd="for i in *.xbcrypt; do \
xbcrypt -d -a $encrypt_algo -k $encrypt_key < \$i > \${i:0:\${#i}-8}; \
rm -f \$i; done; \
for i in ./sakila/*.xbcrypt; do \
xbcrypt -d -a $encrypt_algo -k $encrypt_key < \$i > \${i:0:\${#i}-8}; \
rm -f \$i; done;"

. inc/xb_local.sh
