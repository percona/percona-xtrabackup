################################################################################
# PXB-3609: a user scoped to the backup prefix must not be disturbed by the
# .md5 file.
#
# <backup_name>.md5 sits next to the backup directory, so a user whose rights
# are scoped to <backup_name>/* has none on it at all.  The DELETE we now
# issue for it comes back 403, not 404.  That must not become a warning or a
# failure: such a user could never have uploaded a .md5 file either, and if
# one is there it belongs to somebody else.  xbcloud has to behave exactly as
# it did before this fix.
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
# policy all carry this test's own uuid.
BUCKET="pxbmd5pfx-${uuid}"
PFX_USER="md5pfx-${uuid}"
PFX_POLICY="${PFX_USER}-pol"
PFX_PASS="md5pfxsecret"
BACKUP_NAME="md5pfx_backup_${uuid}"

ROOT_FLAGS="--storage=s3 --s3-endpoint=${ENDPOINT} --s3-bucket=${BUCKET} \
--s3-access-key=${ROOT_KEY} --s3-secret-key=${ROOT_SECRET} --s3-bucket-lookup=path"
PFX_FLAGS="--storage=s3 --s3-endpoint=${ENDPOINT} --s3-bucket=${BUCKET} \
--s3-access-key=${PFX_USER} --s3-secret-key=${PFX_PASS} --s3-bucket-lookup=path"

$MC mb -p "pxb/${BUCKET}" >/dev/null 2>&1 || die "could not create bucket ${BUCKET}"

# An alias mc cannot resolve is treated as a local path rather than an error,
# which would make every listing come back empty instead of failing.  Check it
# once here so the tests below cannot pass for that reason.
$MC ls "pxb/${BUCKET}" >/dev/null 2>&1 \
    || die "cannot reach bucket ${BUCKET} through mc -- check XBCLOUD_CREDENTIALS"

# Rights on everything inside <backup>/ and nothing at the bucket root, so
# <backup>.md5 is out of reach.
cat > $topdir/${PFX_POLICY}.json <<EOF
{
  "Version": "2012-10-17",
  "Statement": [
    { "Effect": "Allow",
      "Action": "s3:ListBucket",
      "Resource": "arn:aws:s3:::${BUCKET}" },
    { "Effect": "Allow",
      "Action": ["s3:GetObject", "s3:PutObject", "s3:DeleteObject"],
      "Resource": "arn:aws:s3:::${BUCKET}/${BACKUP_NAME}/*" }
  ]
}
EOF

$MC admin policy create pxb "$PFX_POLICY" $topdir/${PFX_POLICY}.json >/dev/null \
    || die "could not create policy ${PFX_POLICY}"
$MC admin user add pxb "$PFX_USER" "$PFX_PASS" >/dev/null \
    || die "could not create user ${PFX_USER}"
$MC admin policy attach pxb "$PFX_POLICY" --user "$PFX_USER" >/dev/null \
    || die "could not attach policy ${PFX_POLICY} to ${PFX_USER}"

cleanup_md5_delete_prefix_scope() {
    local rc=$?
    $MC rb --force "pxb/${BUCKET}" >/dev/null 2>&1 || true
    $MC admin policy detach pxb "$PFX_POLICY" --user "$PFX_USER" >/dev/null 2>&1 || true
    $MC admin user remove pxb "$PFX_USER" >/dev/null 2>&1 || true
    $MC admin policy remove pxb "$PFX_POLICY" >/dev/null 2>&1 || true
    return $rc
}
trap cleanup_md5_delete_prefix_scope EXIT

start_server --innodb_file_per_table

mysql -e "CREATE TABLE t (a INT PRIMARY KEY, b VARCHAR(64))" test
mysql -e "INSERT INTO t VALUES (1,'one'),(2,'two'),(3,'three')" test

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

vlog "delete the backup as the prefix-scoped user"

LOG_DELETE=${OUTFILE}.xbcloud_delete.log
if ! xbcloud delete --parallel=4 $PFX_FLAGS "$BACKUP_NAME" > $LOG_DELETE 2>&1; then
    cat $LOG_DELETE >&2
    die "PXB-3609: delete failed for a prefix-scoped user -- being unable to \
remove the .md5 file must not fail the delete"
fi

# The .md5 file is out of this user's reach.  Saying so on every delete would
# be noise, and it is what xbcloud did before the fix, so the run has to be
# quiet about it.
if grep -qiE 'warning|failed to delete|access denied|forbidden|403' $LOG_DELETE; then
    cat $LOG_DELETE >&2
    die "PXB-3609: delete complained about ${BACKUP_NAME}.md5, which this user \
has no rights on"
fi

# The backup itself must be gone ...
remaining=$(list_files_in_bucket | grep -E "^${BACKUP_NAME}/" || true)
[ -z "$remaining" ] || die "backup objects left behind: $(echo $remaining)"

# ... and the .md5 file is expected to survive: nobody with these rights could
# have removed it, before or after this fix.
list_files_in_bucket | grep -q "^${BACKUP_NAME}\.md5$" \
    || die "unexpected: ${BACKUP_NAME}.md5 was removed by a user with no rights on it"

vlog "prefix-scoped delete removed the backup quietly and left ${BACKUP_NAME}.md5"
