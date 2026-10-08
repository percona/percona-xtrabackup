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
  if [ -z ${XBCLOUD_CREDENTIALS+x} ];
  then
    skip_test "Requires XBCLOUD_CREDENTIALS"
  fi
}

now=$(date +%s)
uuid=($(cat /proc/sys/kernel/random/uuid))
full_backup_name=${now}-${uuid}-full_backup
inc_backup_name=${now}-${uuid}-inc_backup
inc2_backup_name=${now}-${uuid}-inc2_backup

full_backup_dir=$topdir/${full_backup_name}
inc_backup_dir=$topdir/${inc_backup_name}

function write_credentials() {
  # write credentials into xbcloud.cnf
  echo '[xbcloud]' > $topdir/xbcloud.cnf
  echo ${XBCLOUD_CREDENTIALS} | sed 's/ *--/\'$'\n/g' >> $topdir/xbcloud.cnf
}

function is_minio_server() {
  if [[ "$XBCLOUD_CREDENTIALS" =~ .*"s3-endpoint".* ]]; then
    ENDPOINT=$(echo ${XBCLOUD_CREDENTIALS} | awk -F's3-endpoint=' '{print $2}' | awk '{print $1}' | tr -d "'" | tr -d '\\')
    SERVER=$(curl -sI ${ENDPOINT} | grep Server)
    if [[ "$SERVER" =~ .*"MinIO".* ]]; then
      return 0
    fi
  fi
  return 1
}

function is_ec2_with_profile() {
  TOKEN=`curl -sS --connect-timeout 3 -X PUT "http://169.254.169.254/latest/api/token" -H "X-aws-ec2-metadata-token-ttl-seconds: 21600"` || \
  return 1

  PROFILE=`curl -sS --connect-timeout 3 -H "X-aws-ec2-metadata-token: $TOKEN" http://169.254.169.254/latest/meta-data/iam/security-credentials/` || \
  return 1

  curl -sS --connect-timeout 3 -H "X-aws-ec2-metadata-token: $TOKEN" http://169.254.169.254/latest/meta-data/iam/security-credentials/${PROFILE} || \
  return 1
}

xbcloud_cleanup() {
     xbcloud --defaults-file=$topdir/xbcloud.cnf delete --parallel=10 ${full_backup_name}
     xbcloud --defaults-file=$topdir/xbcloud.cnf delete --parallel=10 ${inc_backup_name}
     xbcloud --defaults-file=$topdir/xbcloud.cnf delete --parallel=10 ${inc2_backup_name}
}

# Print host:port of the plain http S3 endpoint of XBCLOUD_CREDENTIALS.
# Prints nothing without one.
function s3_endpoint_upstream() {
  local endpoint
  endpoint=$(echo ${XBCLOUD_CREDENTIALS} | awk -F's3-endpoint=' '{print $2}' | \
    awk '{print $1}' | tr -d "'" | tr -d '\\')
  python3 -c "
import sys, urllib.parse
u = urllib.parse.urlparse(sys.argv[1])
if u.scheme == 'http' and u.hostname:
    print('%s:%d' % (u.hostname, u.port or 80))
" "$endpoint"
}

# Skip the test unless all the given commands are available
function require_commands() {
  local command
  for command in "$@"; do
    command -v $command > /dev/null || skip_test "Requires $command"
  done
}

# Run a command when the test exits, in addition to the ones added before
function run_at_exit() {
  exit_commands="${exit_commands:-}$1; "
  trap "$exit_commands" EXIT
}

# Start socat as a proxy between xbcloud and the S3 endpoint of
# XBCLOUD_CREDENTIALS. Pass --s3-endpoint=$proxy_endpoint to xbcloud to go
# through it. Needs socat, nc and pkill: call require_commands first.
function start_proxy() {
  local upstream port
  upstream=$(s3_endpoint_upstream)
  if [ -z "$upstream" ]; then
    skip_test "Requires a plain http --s3-endpoint in XBCLOUD_CREDENTIALS"
  fi

  port=$(get_free_port proxy)
  socat TCP-LISTEN:$port,bind=127.0.0.1,fork,reuseaddr TCP:$upstream \
    > $topdir/proxy.log 2>&1 &
  proxy_pid=$!
  run_at_exit stop_proxy
  for i in $(seq 1 50); do
    nc -z 127.0.0.1 $port && break
    sleep 0.1
  done
  nc -z 127.0.0.1 $port || die "socat did not start"
  proxy_endpoint="http://127.0.0.1:$port/"
  vlog "Proxy at $proxy_endpoint to $upstream"
}

# Freeze socat and its per-connection children: connections stay open but no
# byte moves, like a network that silently drops packets. Stop the parent
# first, so that it cannot fork new children. resume_proxy undoes it.
function stall_proxy() {
  vlog "Stall the proxy"
  kill -STOP $proxy_pid
  pkill -STOP -P $proxy_pid || true
}

function resume_proxy() {
  vlog "Resume the proxy"
  pkill -CONT -P $proxy_pid || true
  kill -CONT $proxy_pid
}

# Stop socat and its children. Also run when the test exits.
function stop_proxy() {
  [ -n "${proxy_pid:-}" ] || return 0
  # a stopped process acts on SIGTERM only once it continues
  pkill -TERM -P $proxy_pid || true
  pkill -CONT -P $proxy_pid || true
  kill -TERM $proxy_pid 2> /dev/null || true
  kill -CONT $proxy_pid 2> /dev/null || true
  proxy_pid=
}
