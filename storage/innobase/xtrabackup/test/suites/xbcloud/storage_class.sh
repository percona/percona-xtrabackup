. inc/xbcloud_common.sh
is_xbcloud_credentials_set
is_minio_server && skip_test "Storage Class is not supported on MinIO servers"
write_credentials

storage_class_folder=${now}-${uuid}-storage_class

# PXB-2112 - Support for storage class
test_storage_class=false

if grep -q "storage=s3" $topdir/xbcloud.cnf ; then
  vlog "Testing S3 storage class"
  test_storage_class=true
  storage_class_parameter="--s3-storage-class=GLACIER"
elif grep -q "storage=google" $topdir/xbcloud.cnf ; then
  vlog "Testing GC storage class"
  test_storage_class=true
  storage_class_parameter="--google-storage-class=COLDLINE"
fi

if grep -q "minioadmin" $topdir/xbcloud.cnf ; then
  test_storage_class=false
fi

if [ "$test_storage_class" = true ]; then
  run_cmd touch $topdir/xtrabackup_tablespaces
  cd $topdir/
  xbstream -c xtrabackup_tablespaces | \
  xbcloud --defaults-file=$topdir/xbcloud.cnf ${storage_class_parameter} \
   put ${storage_class_folder} 2> $topdir/storage_class.log

 if  grep -q "expected objects using storage class" $topdir/storage_class.log ; then
   sleep 120
    xbcloud --defaults-file=$topdir/xbcloud.cnf delete \
     ${storage_class_folder}
     die 'xbcloud did not upload using correct storage class'
 fi
 if  grep -q "Upload failed" $topdir/storage_class.log ; then
   xbcloud --defaults-file=$topdir/xbcloud.cnf delete \
     ${storage_class_folder}
     die 'xbcloud exit with error on upload'
 fi

 #cleanup
 run_cmd xbcloud --defaults-file=$topdir/xbcloud.cnf delete \
  ${storage_class_folder}
fi
