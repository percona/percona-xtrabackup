############################################################################
# A few tests for encryption:
#  1 - Test that a file that goes through the encrypt/decrypt cycle is
#      exactly the same after decryption as the original.
#  2 - Test that files encrypted with prior versions of xbcrypt can be 
#      correctly decrypted. Introduced when fixing bug 1185343 - Fixed IV 
#      used in Xtrabackup encryption
############################################################################

encrypt_algo="AES256"
encrypt_key="percona_xtrabackup_is_awesome___"

# test that decrypted file is identical to original
run_cmd xbcrypt -i inc/decrypt_v1_test_file.txt -o ${topdir}/decrypt_v1_test_file.xbcrypt -a ${encrypt_algo} -k ${encrypt_key}
run_cmd xbcrypt -d -i ${topdir}/decrypt_v1_test_file.xbcrypt -o ${topdir}/decrypt_v1_test_file.txt -a ${encrypt_algo} -k ${encrypt_key}
run_cmd cmp inc/decrypt_v1_test_file.txt ${topdir}/decrypt_v1_test_file.txt
rm ${topdir}/decrypt_v1_test_file.xbcrypt
rm ${topdir}/decrypt_v1_test_file.txt


# test that file encrypted w/ v1 can be decrypted perfectly w/ v2
run_cmd xbcrypt -d -i inc/decrypt_v1_test_file.xbcrypt -o ${topdir}/decrypt_v1_test_file.txt -a ${encrypt_algo} -k ${encrypt_key}
run_cmd cmp inc/decrypt_v1_test_file.txt ${topdir}/decrypt_v1_test_file.txt
rm ${topdir}/decrypt_v1_test_file.txt
