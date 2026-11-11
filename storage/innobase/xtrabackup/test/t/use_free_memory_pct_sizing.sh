########################################################################
# PXB-3889 : --prepare --use-free-memory-pct must never turn a failed,
# zero or tiny free-memory read into a negative or wrapped buffer pool
# size. It falls back to --use-memory with a warning instead.
#
# The debug keyword xb_simulate_free_memory replaces the free and total
# memory that xtrabackup reads with XB_DEBUG_FREE_MEMORY and
# XB_DEBUG_TOTAL_MEMORY (bytes).
########################################################################

. inc/common.sh

require_debug_pxb_version

start_server

run_cmd $MYSQL $MYSQL_ARGS test <<EOF
CREATE TABLE t1 (a INT PRIMARY KEY AUTO_INCREMENT, b VARCHAR(100));
INSERT INTO t1 (b) VALUES ('a'), ('b'), ('c');
EOF

backup_dir=$topdir/backup
xtrabackup --backup --target-dir=$backup_dir
record_db_state test
stop_server
rm -rf $mysql_datadir

# Make --prepare ask for 2G (redo_memory) + 100 pages, which the smart
# memory code rounds up to 17 chunks of 128M.
sed -i -e '/^redo_memory =/ s/= .*/= 2147483648/' \
       -e '/^redo_frames =/ s/= .*/= 100/' \
       $backup_dir/xtrabackup_checkpoints
grep -q '^redo_memory = 2147483648' $backup_dir/xtrabackup_checkpoints ||
  die "could not set redo_memory in xtrabackup_checkpoints"
required=2281701376
use_memory_default=104857600

case_no=0

# Run --prepare on a fresh copy of the backup. Extra arguments go to
# xtrabackup. The output is in $prep_log.
function prepare_copy()
{
  case_no=$((case_no + 1))
  local dir=$topdir/prep_$case_no
  prep_log=$topdir/prep_$case_no.log
  rm -rf $dir
  cp -a $backup_dir $dir
  vlog "===> xtrabackup --prepare --target-dir=$dir $*"
  if ! $XB_BIN $XB_ARGS --prepare --target-dir=$dir "$@" > $prep_log 2>&1; then
    cat $prep_log
    die "--prepare failed in case $case_no"
  fi
  prepared_dir=$dir
}

# expect_buffer_pool <bytes> <message>: the smart memory code chose <bytes>
# and never printed a negative size.
function expect_buffer_pool()
{
  expect_log "Setting buffer pool to: $1\$" "$2" $prep_log
  expect_no_log "Setting buffer pool to: -" "$2" $prep_log
}

# expect_fallback <message>: --use-memory was used instead.
function expect_fallback()
{
  expect_log "-use-memory=$use_memory_default bytes for the buffer pool" "$1" \
    $prep_log
  expect_buffer_pool $use_memory_default "$1"
  expect_log \
    "Using $use_memory_default bytes for buffer pool (set by --use-memory parameter)" \
    "$1" $prep_log
}

vlog "Case 1: real host memory is read and is not 0 (the PXB-3889 bug)"
mem_avail=$(awk '/^MemAvailable:/ {print $2 * 1024}' /proc/meminfo)
prepare_copy --use-free-memory-pct=1
msg="case 1: the free memory of the host must be read as non-zero"
expect_log "Free memory of this host: [1-9][0-9]* bytes" "$msg" $prep_log
expect_log "Total memory: [1-9][0-9]* bytes" \
  "case 1: the total memory of the host must be read as non-zero" $prep_log
expect_no_log "Ignoring --use-free-memory-pct" "$msg" $prep_log
read_free=$(sed -n 's/.*Free memory of this host: \([0-9]*\) bytes.*/\1/p' $prep_log | head -1)
vlog "MemAvailable before --prepare: $mem_avail, xtrabackup read: $read_free"

# Run --prepare with free memory simulated as $1 bytes,
# --use-free-memory-pct=$2 and total memory simulated as $3 bytes
# (default 16G, so the result does not depend on the host).
function prepare_with_free()
{
  XB_DEBUG_FREE_MEMORY=$1 XB_DEBUG_TOTAL_MEMORY=${3:-17179869184} \
    prepare_copy --use-free-memory-pct=$2 --debug=d,xb_simulate_free_memory
  expect_log "Free memory of this host: $1 bytes" \
    "case $case_no: xb_simulate_free_memory must set the free memory" $prep_log
}

vlog "Case 2: a failed (0) free memory read falls back to --use-memory"
# The old code printed "Setting buffer pool to: -134217728" and InnoDB got
# SIGSEGV while it allocated a 16 EiB buffer pool. Case 3 crashed the same.
prepare_with_free 0 50
msg="case 2: a 0 free memory read must fall back to --use-memory"
expect_log "the free memory was read as 0" "$msg" $prep_log
expect_fallback "$msg"

vlog "Case 2b: a failed (0) total memory read falls back to --use-memory"
# Without the total memory the free memory cannot be validated.
prepare_with_free 8589934592 50 0
msg="case 2b: a 0 total memory read must fall back to --use-memory"
expect_log "the total memory was read as 0" "$msg" $prep_log
expect_fallback "$msg"

vlog "Case 3: free memory below one buffer pool chunk"
# 1M free. The old code rounded 50% of it (512K) down with
# buf_pool_size_align_down(), which wrapped around to -128M, and InnoDB got
# SIGSEGV like in case 2.
prepare_with_free 1048576 50
msg="case 3: below one chunk must fall back to --use-memory"
expect_log "the free memory is below one buffer pool chunk" "$msg" $prep_log
expect_fallback "$msg"

vlog "Case 4: rounding down keeps whole chunks"
# 300M free, 100% = 300M. Rounding up (384M) exceeds the free memory, so
# the size is rounded down to 256M. The old code dropped two chunks and
# used 128M.
prepare_with_free 314572800 100
msg="case 4: 300M at 100% must round down to 256M"
expect_buffer_pool 268435456 "$msg"
expect_log \
  "Using 268435456 bytes for buffer pool (set by --use-free-memory-pct parameter)" \
  "$msg" $prep_log

vlog "Case 5: enough free memory gives the estimated size"
# Control case: the old code passes it too. It shows the normal path is
# unchanged.
prepare_with_free 8589934592 50
msg="case 5: enough free memory must give the estimated size"
expect_no_log "Required memory will exceed" "$msg" $prep_log
expect_buffer_pool $required "$msg"
expect_log \
  "Using $required bytes for buffer pool (set by --use-free-memory-pct parameter)" \
  "$msg" $prep_log

vlog "Case 6: free memory not below the total memory is not a valid read"
# 2^63 bytes free on a 16G host. The old code computed free * pct, which
# wrapped around to exactly 0 at 50%, and logged "Setting buffer pool to: 0".
prepare_with_free 9223372036854775808 50
msg="case 6: free memory above total memory must fall back to --use-memory"
expect_log "the free memory is not below the total memory (free [0-9]* bytes, total 17179869184 bytes)" \
  "$msg" $prep_log
expect_fallback "$msg"

vlog "Case 6b: free memory equal to the total memory is not a valid read"
prepare_with_free 17179869184 50
msg="case 6b: free memory equal to total memory must fall back to --use-memory"
expect_log "the free memory is not below the total memory (free [0-9]* bytes, total 17179869184 bytes)" \
  "$msg" $prep_log
expect_fallback "$msg"

vlog "Case 7: the fallback prepare gives a usable backup"
prepare_with_free 0 50
expect_fallback "case 7: a 0 free memory read must fall back to --use-memory"

xtrabackup --copy-back --target-dir=$prepared_dir
start_server
verify_db_state test
stop_server
