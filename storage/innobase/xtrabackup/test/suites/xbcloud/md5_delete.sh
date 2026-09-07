################################################################################
# PXB-3609: xbcloud delete must remove the <backup_name>.md5 file.
#
# `xbcloud put --md5` uploads the checksum file as <backup_name>.md5, next to
# the backup directory and not inside it.  `xbcloud delete` lists only what is
# under <backup_name>/, so before the fix that file was never deleted and
# stayed in the bucket after the backup itself was gone.
#
# Scenarios:
#   1. put --md5 then delete  -> the .md5 file goes with the backup.
#   2. put (no --md5) then delete -> completes cleanly; a .md5 file that was
#      never created must not turn into an error.
#
# Needs the MinIO client (mc) to look at individual object names, which
# xbcloud itself cannot show.  mc talks to MinIO over the network, so no
# docker is involved.  In CI the pipeline copies mc out of the MinIO image and
# points XBCLOUD_MC at it; developers can also just have mc in PATH.
################################################################################
. inc/xbcloud_common.sh
is_xbcloud_credentials_set
is_minio_server || skip_test "requires MinIO to list bucket contents"

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

# The test owns its bucket: nothing has to exist beforehand, and workers
# running in parallel cannot collide.  mb -p is happy if it is already there.
BUCKET="pxbmd5-${uuid}"
XB_FLAGS="--storage=s3 --s3-endpoint=${ENDPOINT} --s3-bucket=${BUCKET} \
--s3-access-key=${ROOT_KEY} --s3-secret-key=${ROOT_SECRET} --s3-bucket-lookup=path"

$MC mb -p "pxb/${BUCKET}" >/dev/null 2>&1 || die "could not create bucket ${BUCKET}"

# An alias mc cannot resolve is treated as a local path rather than an error,
# which would make every listing come back empty instead of failing.  Check it
# once here so the tests below cannot pass for that reason.
$MC ls "pxb/${BUCKET}" >/dev/null 2>&1 \
    || die "cannot reach bucket ${BUCKET} through mc -- check XBCLOUD_CREDENTIALS"

cleanup_md5_delete() {
    local rc=$?
    $MC rb --force "pxb/${BUCKET}" >/dev/null 2>&1 || true
    return $rc
}
trap cleanup_md5_delete EXIT

start_server --innodb_file_per_table

md5_backup="md5_backup_${uuid}"
plain_backup="plain_backup_${uuid}"

mysql -e "CREATE TABLE t (a INT PRIMARY KEY, b VARCHAR(64))" test
mysql -e "INSERT INTO t VALUES (1,'one'),(2,'two'),(3,'three')" test

# Every object key in the bucket, one per line.  mc ls prints the key last.
list_files_in_bucket() {
    $MC ls --recursive "pxb/${BUCKET}" 2>/dev/null | awk '{print $NF}'
}

# The keys belonging to one backup: everything under <name>/ plus the
# <name>.md5 file.  Other tests share this bucket, so the question can only be
# asked per backup, never by checking for an empty bucket.  grep exits 1 when
# nothing matches, which is the success case here, hence || true.
list_files_of_backup() {
    list_files_in_bucket | grep -E "^${1}(/|\.md5$)" || true
}

################################################################################
# Scenario 1: a backup taken with --md5 must lose its .md5 file on delete.
################################################################################

vlog "take a full backup and upload it with --md5"

md5_dir=$topdir/md5_backup
mkdir -p $md5_dir
xtrabackup --backup --stream=xbstream --extra-lsndir=$md5_dir \
    --target-dir=$md5_dir \
    | run_cmd xbcloud put --md5 --parallel=4 $XB_FLAGS ${md5_backup}

list_files_of_backup "$md5_backup" | grep -q "^${md5_backup}\.md5$" \
    || die "expected ${md5_backup}.md5 to exist after put --md5"
vlog "${md5_backup}.md5 present after put"

run_cmd xbcloud delete --parallel=4 $XB_FLAGS ${md5_backup}

remaining=$(list_files_of_backup "$md5_backup")
if [ -n "$remaining" ]; then
    echo "PXB-3609: objects left in bucket after delete:" >&2
    echo "$remaining" >&2
    die "PXB-3609: xbcloud delete left objects behind: $(echo $remaining)"
fi
vlog "delete removed the backup and ${md5_backup}.md5"

################################################################################
# Scenario 2: a backup taken without --md5 has no .md5 file.  The delete is
# issued unconditionally -- checking first would make delete require
# s3:GetObject -- so a file that is not there must not be treated as an error.
################################################################################

vlog "take a full backup and upload it without --md5"

plain_dir=$topdir/plain_backup
mkdir -p $plain_dir
xtrabackup --backup --stream=xbstream --extra-lsndir=$plain_dir \
    --target-dir=$plain_dir \
    | run_cmd xbcloud put --parallel=4 $XB_FLAGS ${plain_backup}

list_files_of_backup "$plain_backup" | grep -q "^${plain_backup}\.md5$" \
    && die "put without --md5 unexpectedly wrote ${plain_backup}.md5"

run_cmd xbcloud delete --parallel=4 $XB_FLAGS ${plain_backup}

remaining=$(list_files_of_backup "$plain_backup")
[ -z "$remaining" ] \
    || die "objects left after deleting a backup without --md5: $(echo $remaining)"

vlog "delete of a backup without --md5 completed cleanly"
