################################################################################
# PXB-3609: deleting the .md5 file must not require new privileges.
#
# The .md5 file is deleted unconditionally rather than checked for first,
# precisely so that a least-privilege retention user can still clean it up.
# Checking would need s3:GetObject, which such a user is not granted -- the
# check would be denied, the delete would be skipped, and the .md5 file would
# be left behind for exactly the users who follow least-privilege guidance.
#
# This test pins that: a MinIO user holding only
#   s3:ListBucket   (on the bucket)
#   s3:DeleteObject (on the objects)
# must be able to delete a backup taken with --md5, .md5 file included.
#
# Needs the MinIO client (mc) to create the restricted user and to look at
# object names.  mc talks to MinIO over the network, so no docker is involved.
# In CI the pipeline copies mc out of the MinIO image and points XBCLOUD_MC at
# it; developers can also just have mc in PATH.
################################################################################
. inc/xbcloud_common.sh
is_xbcloud_credentials_set
is_minio_server || skip_test "requires MinIO for IAM users and bucket listing"

MC_BIN=${XBCLOUD_MC:-$(command -v mc 2>/dev/null || true)}
[ -n "$MC_BIN" ] && [ -x "$MC_BIN" ] \
    || skip_test "requires the MinIO client: set XBCLOUD_MC or put mc in PATH"

ENDPOINT=$(echo "$XBCLOUD_CREDENTIALS" \
    | awk -F's3-endpoint=' '{print $2}' | awk '{print $1}' | tr -d "'")
ROOT_KEY=$(echo "$XBCLOUD_CREDENTIALS" \
    | awk -F's3-access-key=' '{print $2}' | awk '{print $1}' | tr -d "'")
ROOT_SECRET=$(echo "$XBCLOUD_CREDENTIALS" \
    | awk -F's3-secret-key=' '{print $2}' | awk '{print $1}' | tr -d "'")
[ -n "$ENDPOINT" ] && [ -n "$ROOT_KEY" ] && [ -n "$ROOT_SECRET" ] \
    || skip_test "XBCLOUD_CREDENTIALS lacks s3-endpoint/access-key/secret-key"

# mc keeps its configuration under $HOME, which is not writable in the test
# image, so give it one of our own.  Credentials go in MC_HOST_<alias> rather
# than on the command line, where they would end up in the test log.
# $topdir only exists once the server has been set up, and mc needs a config
# directory it can write to before that, so fall back to the worker's own
# scratch space rather than ending up at /mcconf.
MC="$MC_BIN --config-dir ${topdir:-${MYSQLD_VARDIR:-$PWD/var}}/mcconf.$$"
export MC_HOST_pxb="${ENDPOINT%%://*}://${ROOT_KEY}:${ROOT_SECRET}@${ENDPOINT#*://}"

# Workers run in parallel against one MinIO, so the bucket, the user and the
# policy all carry this test's own uuid.  Bucket names must be lowercase and
# at most 63 characters; the uuid is 36.
BUCKET="pxbmd5perm-${uuid}"
DEL_USER="md5perm-${uuid}"
DEL_POLICY="${DEL_USER}-pol"
BACKUP_NAME="md5perm_backup_${uuid}"

ROOT_FLAGS="--storage=s3 --s3-endpoint=${ENDPOINT} --s3-bucket=${BUCKET} \
--s3-access-key=${ROOT_KEY} --s3-secret-key=${ROOT_SECRET} --s3-bucket-lookup=path"
DEL_PASS="md5permsecret"
DEL_FLAGS="--storage=s3 --s3-endpoint=${ENDPOINT} --s3-bucket=${BUCKET} \
--s3-access-key=${DEL_USER} --s3-secret-key=${DEL_PASS} --s3-bucket-lookup=path"

$MC mb -p "pxb/${BUCKET}" >/dev/null 2>&1 || die "could not create bucket ${BUCKET}"

# An alias mc cannot resolve is treated as a local path rather than an error,
# which would make every listing come back empty instead of failing.  Check it
# once here so the tests below cannot pass for that reason.
$MC ls "pxb/${BUCKET}" >/dev/null 2>&1 \
    || die "cannot reach bucket ${BUCKET} through mc -- check XBCLOUD_CREDENTIALS"

# List the bucket, delete objects, and deliberately no s3:GetObject.
cat > $topdir/${DEL_POLICY}.json <<EOF
{
  "Version": "2012-10-17",
  "Statement": [
    { "Effect": "Allow",
      "Action": "s3:ListBucket",
      "Resource": "arn:aws:s3:::${BUCKET}" },
    { "Effect": "Allow",
      "Action": "s3:DeleteObject",
      "Resource": "arn:aws:s3:::${BUCKET}/*" }
  ]
}
EOF

$MC admin policy create pxb "$DEL_POLICY" $topdir/${DEL_POLICY}.json >/dev/null \
    || die "could not create policy ${DEL_POLICY}"
$MC admin user add pxb "$DEL_USER" "$DEL_PASS" >/dev/null \
    || die "could not create user ${DEL_USER}"
$MC admin policy attach pxb "$DEL_POLICY" --user "$DEL_USER" >/dev/null \
    || die "could not attach policy ${DEL_POLICY} to ${DEL_USER}"

cleanup_md5_delete_permissions() {
    local rc=$?
    $MC rb --force "pxb/${BUCKET}" >/dev/null 2>&1 || true
    $MC admin policy detach pxb "$DEL_POLICY" --user "$DEL_USER" >/dev/null 2>&1 || true
    $MC admin user remove pxb "$DEL_USER" >/dev/null 2>&1 || true
    $MC admin policy remove pxb "$DEL_POLICY" >/dev/null 2>&1 || true
    return $rc
}
trap cleanup_md5_delete_permissions EXIT

start_server --innodb_file_per_table

mysql -e "CREATE TABLE t (a INT PRIMARY KEY, b VARCHAR(64))" test
mysql -e "INSERT INTO t VALUES (1,'one'),(2,'two'),(3,'three')" test

# Listed with root credentials: the restricted user is what we are testing,
# not what we measure with.
list_files_in_bucket() {
    $MC ls --recursive "pxb/${BUCKET}" 2>/dev/null | awk '{print $NF}'
}

vlog "upload a backup WITH --md5 using root credentials"

LOG_UPLOAD=${OUTFILE}.xbcloud_put.log
full_dir=$topdir/full
mkdir -p "$full_dir"
xtrabackup --backup --stream=xbstream --extra-lsndir="$full_dir" \
    --target-dir="$full_dir" 2>/dev/null \
    | xbcloud put --md5 --parallel=4 $ROOT_FLAGS "$BACKUP_NAME" \
        > $LOG_UPLOAD 2>&1
rc=("${PIPESTATUS[@]}")
[ "${rc[0]}" = "0" ] && [ "${rc[1]}" = "0" ] \
    || { cat $LOG_UPLOAD >&2; die "test setup: xbcloud put --md5 failed"; }

list_files_in_bucket | grep -q "^${BACKUP_NAME}\.md5$" \
    || die "test setup: expected ${BACKUP_NAME}.md5 after put --md5"
vlog "${BACKUP_NAME}.md5 uploaded"

# Prove the policy is in force before drawing any conclusion from a successful
# delete: a policy that failed to attach would leave the user with default
# rights and this test would pass for the wrong reason.  A read of the .md5
# file must be refused -- that is the request a check-before-delete would make.
export MC_HOST_del="${ENDPOINT%%://*}://${DEL_USER}:${DEL_PASS}@${ENDPOINT#*://}"

if $MC cat "del/${BUCKET}/${BACKUP_NAME}.md5" >/dev/null 2>&1; then
    die "delete-only user can read ${BACKUP_NAME}.md5 -- the policy is not in \
force, so this test would not prove anything"
fi
vlog "confirmed: the delete-only user is denied read access to ${BACKUP_NAME}.md5"

# ... and it must still be able to list, or a failed delete would prove nothing
# either.
$MC ls "del/${BUCKET}" >/dev/null 2>&1 \
    || die "delete-only user cannot list the bucket -- policy setup is broken"

vlog "delete the backup as the delete-only user (ListBucket + DeleteObject, no GetObject)"

LOG_DELETE=${OUTFILE}.xbcloud_delete.log
if ! xbcloud delete --parallel=4 $DEL_FLAGS "$BACKUP_NAME" > $LOG_DELETE 2>&1; then
    cat $LOG_DELETE >&2
    die "PXB-3609: delete-only user could not delete the backup -- xbcloud is \
demanding privileges beyond ListBucket + DeleteObject"
fi

# An AccessDenied that xbcloud swallowed would show up here even with a clean
# exit status.
if grep -qiE 'access denied|forbidden|403' $LOG_DELETE; then
    cat $LOG_DELETE >&2
    die "PXB-3609: delete-only user hit a permission error during delete"
fi

remaining=$(list_files_in_bucket)
if [ -n "$remaining" ]; then
    echo "objects left in bucket after the delete-only user ran delete:" >&2
    echo "$remaining" >&2
    die "PXB-3609: delete-only user left objects behind: $(echo $remaining)"
fi

vlog "delete-only user removed the whole backup including ${BACKUP_NAME}.md5"
