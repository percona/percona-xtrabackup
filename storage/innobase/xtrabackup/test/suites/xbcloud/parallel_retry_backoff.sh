################################################################################
# PXB-3885: xbcloud serializes waits of parallel threads
#
# When parallel requests fail at the same time, their retry backoffs must
# overlap. They used to be slept one after another on the event loop thread,
# so N failed requests cost the sum of their delays and stalled every other
# transfer meanwhile.
#
# socat runs as a proxy in front of the S3 endpoint. The test feeds xbcloud
# (--parallel=16) the first part of a backup, freezes socat with SIGSTOP and
# then feeds the rest, so every upload of the rest starts while connections
# stay open but no byte moves, and they all time out together and back off.
# Their backoffs must start together, not spread over the sum of their delays.
################################################################################

. inc/xbcloud_common.sh
is_xbcloud_credentials_set
require_commands socat nc pkill

start_server

write_credentials

start_proxy

vlog "Create about 512MB of table data"
mysql -e "CREATE TABLE t (id INT AUTO_INCREMENT PRIMARY KEY, b LONGBLOB)" test
mysql -e "INSERT INTO t (b) VALUES (REPEAT('a', 1048576))" test
for i in $(seq 1 9); do
  mysql -e "INSERT INTO t (b) SELECT b FROM t" test
done

vlog "Take a backup"
xtrabackup --backup --stream=xbstream --target-dir=$topdir/backup \
  > $topdir/backup.xbs

# Write the backup into xbcloud's input in two parts, freezing the network for
# 15 seconds in between, so that every upload of the second part starts while
# the network is frozen and they all time out together.
function feed_backup_with_stall() {
  local first_part=$((50 * 1024 * 1024))
  # xbcloud reads its input only after its startup requests, and a pipe holds
  # only 64KB, so head returns once xbcloud is uploading the first part
  head -c $first_part $topdir/backup.xbs
  stall_proxy
  (sleep 15; resume_proxy) &
  tail -c +$((first_part + 1)) $topdir/backup.xbs
  wait
}

vlog "Upload it through the proxy, which stalls for 15 seconds after 50MB"
rc=0
feed_backup_with_stall | xbcloud --defaults-file=$topdir/xbcloud.cnf put \
    --s3-endpoint=$proxy_endpoint \
    --parallel=16 --timeout=5 --max-retries=10 --max-backoff=3000 \
    ${full_backup_name} 2> $topdir/put.log || rc=$?
stop_proxy
if [ $rc -ne 0 ]; then
  grep -v "successfully uploaded" $topdir/put.log | tail -5
  die "xbcloud put failed with exit code $rc"
fi

# The first backoff of each request that failed while the network stalled, as
# xbcloud logs it: "<date> <time> ... Sleeping for <ms> ms before retrying
# <object> [1]". Run one after another, they start over about the sum of their
# delays; overlapping, they all start within a few seconds.
first_backoffs=$(grep "before retrying .* \[1\]$" $topdir/put.log || true)
count=$(echo -n "$first_backoffs" | grep -c . || true)
if [ "$count" -lt 8 ]; then
  cat $topdir/put.log
  die "Expected at least 8 parallel requests to back off, got $count"
fi
total_ms=$(echo "$first_backoffs" | awk '{ms += $(NF - 5)} END {print ms}')
first=$(echo "$first_backoffs" | head -1 | cut -c1-15)
last=$(echo "$first_backoffs" | tail -1 | cut -c1-15)
spread=$(($(date -d "20$last" +%s) - $(date -d "20$first" +%s)))
vlog "First backoffs of $count requests: $((total_ms / 1000)) seconds in total," \
  "from $first to $last ($spread seconds)"
if [ $((spread * 2000)) -ge $total_ms ]; then
  echo "$first_backoffs"
  die "Backoffs of parallel requests ran one after another"
fi

vlog "Download and prepare the backup"
mkdir $topdir/downloaded
run_cmd xbcloud --defaults-file=$topdir/xbcloud.cnf get --parallel=8 \
  ${full_backup_name} | xbstream -x -C $topdir/downloaded
xtrabackup --prepare --target-dir=$topdir/downloaded

run_cmd xbcloud --defaults-file=$topdir/xbcloud.cnf delete --parallel=8 \
  ${full_backup_name}
