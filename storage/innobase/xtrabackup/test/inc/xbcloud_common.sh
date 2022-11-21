################################################################################
# Test xbcloud
#
# Set following environment variables to enable this test:
#     XBCLOUD_CREDENTIALS
#
# Example:
#     export XBCLOUD_CREDENTIALS="--storage=swift \
#         --swift-url=http://192.168.8.80:8080/ \
#         --swift-user=test:tester \
#         --swift-key=testing \
#         --swift-container=test_backup"
#
# NOTE: Do not set XBCLOUD_CREDENTIALS with quotes like:
# export XBCLOUD_CREDENTIALS="--storage='swift'" Only use the sorounding double
# quotes.
################################################################################
. inc/common.sh

MYSQLD_EXTRA_MY_CNF_OPTS="
secure-file-priv=$TEST_VAR_ROOT
"
is_galera && skip_test "skipping"

function is_xbcloud_credentials_set() {
  [ "${XBCLOUD_CREDENTIALS:-unset}" == "unset" ] && \
  skip_test "Requires XBCLOUD_CREDENTIALS"
}

now=$(date +%s)
uuid=($(cat /proc/sys/kernel/random/uuid))
full_backup_name=${now}-${uuid}-full_backup
inc_backup_name=${now}-${uuid}-inc_backup

full_backup_dir=$topdir/${full_backup_name}
inc_backup_dir=$topdir/${inc_backup_name}

function write_credentials() {
  # write credentials into xbcloud.cnf
  echo '[xbcloud]' > $topdir/xbcloud.cnf
  echo ${XBCLOUD_CREDENTIALS} | sed 's/ *--/\'$'\n/g' >> $topdir/xbcloud.cnf
}

function is_ec2_with_profile() {
  TOKEN=`curl -sS --connect-timeout 3 -X PUT "http://169.254.169.254/latest/api/token" -H "X-aws-ec2-metadata-token-ttl-seconds: 21600"` || \
  return 1

  PROFILE=`curl -sS --connect-timeout 3 -H "X-aws-ec2-metadata-token: $TOKEN" http://169.254.169.254/latest/meta-data/iam/security-credentials/` || \
  return 1

  curl -sS --connect-timeout 3 -H "X-aws-ec2-metadata-token: $TOKEN" http://169.254.169.254/latest/meta-data/iam/security-credentials/${PROFILE} || \
  return 1
}
