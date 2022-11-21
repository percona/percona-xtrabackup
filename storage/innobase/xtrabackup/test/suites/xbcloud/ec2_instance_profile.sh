. inc/xbcloud_common.sh
is_xbcloud_credentials_set
is_ec2_with_profile || skip_test "This test requires EC2 Instance Profile set"

if ! [[ $XBCLOUD_CREDENTIALS =~ "s3-bucket=" ]]; then
  skip_test "--s3-bucket is not set"
fi

S3_BUCKET=$(echo $XBCLOUD_CREDENTIALS | awk -F's3-bucket=' '{print $2}' | awk '{print $1}')

start_server --innodb_file_per_table

load_dbase_schema sakila
vlog "take full backup"
xtrabackup --backup --stream=xbstream --extra-lsndir=$full_backup_dir \
	  --target-dir=$full_backup_dir | \
    run_cmd xbcloud put --storage=s3 --s3-bucket=${S3_BUCKET} \
        --parallel=4 ${full_backup_name}

#cleanup
run_cmd xbcloud delete --storage=s3 --s3-bucket=${S3_BUCKET} \
    --parallel=4 ${full_backup_name}
