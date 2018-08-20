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
parent_dir=$TEST_VAR_ROOT
test_file=${parent_dir}/xbcrypt_v3_test_file

# test that decrypted file is identical to original
vlog "Encrypting..."
run_cmd xbcrypt -i inc/decrypt_v1_test_file.txt \
    -o ${test_file}.xbcrypt \
    -a ${encrypt_algo} -k ${encrypt_key}

vlog "Verifying output file..."
ls -l ${test_file}.xbcrypt

vlog "Decrypting..."
run_cmd xbcrypt -d -i ${test_file}.xbcrypt \
    -o ${test_file}.txt \
    -a ${encrypt_algo} -k ${encrypt_key}

vlog "Comparing..."
run_cmd cmp inc/decrypt_v1_test_file.txt ${test_file}.txt

vlog "Piping..."
run_cmd xbcrypt -i inc/decrypt_v1_test_file.txt \
    -a ${encrypt_algo} -k ${encrypt_key} \
    | xbcrypt -d -a ${encrypt_algo} -k ${encrypt_key} \
    > ${test_file}_pipes.txt
run_cmd cmp inc/decrypt_v1_test_file.txt ${test_file}_pipes.txt

vlog "Parallel encryption-decryption"
run_cmd xbcrypt -i inc/decrypt_v1_test_file.txt \
    -o ${test_file}_parallel.xbcrypt \
    -a ${encrypt_algo} -k ${encrypt_key} \
    --encrypt-threads 4

run_cmd xbcrypt -d -i ${test_file}.xbcrypt \
    -o ${test_file}_parallel.txt \
    -a ${encrypt_algo} -k ${encrypt_key} \
    --encrypt-threads 4

run_cmd xbcrypt -d -i ${test_file}_parallel.xbcrypt \
    -o ${test_file}_parallel2.txt \
    -a ${encrypt_algo} -k ${encrypt_key} \
    --encrypt-threads 4

vlog "Comparing..."
run_cmd cmp inc/decrypt_v1_test_file.txt ${test_file}_parallel.txt
run_cmd cmp inc/decrypt_v1_test_file.txt ${test_file}_parallel2.txt

rm -rf ${parent_dir}/*

# test that file encrypted w/ v1 and v2 can be decrypted perfectly w/ v3
vlog "Verify that we can still decrypt v1"
run_cmd xbcrypt -d -i inc/decrypt_v1_test_file.xbcrypt \
     -o ${parent_dir}/decrypt_v1_test_file.txt \
     -a ${encrypt_algo} -k ${encrypt_key}

run_cmd cmp inc/decrypt_v1_test_file.txt ${parent_dir}/decrypt_v1_test_file.txt

vlog "Verify that we can still decrypt v2"
run_cmd xbcrypt -d -i inc/decrypt_v2_test_file.xbcrypt \
     -o ${parent_dir}/decrypt_v2_test_file.txt \
     -a ${encrypt_algo} -k ${encrypt_key}

run_cmd cmp inc/decrypt_v1_test_file.txt ${parent_dir}/decrypt_v2_test_file.txt
