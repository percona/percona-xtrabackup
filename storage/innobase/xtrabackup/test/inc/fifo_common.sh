function create_base_table() {
  mysql -e "CREATE TABLE t (ID INT PRIMARY KEY AUTO_INCREMENT); INSERT INTO t VALUES();" test
}

function run_insert() {
  for i in {1..100} ; do
      mysql -e "INSERT INTO t VALUES ()" test
  done 1>/dev/null 2>/dev/null
}

function take_backup_fifo_xbstream()
{
  local stream_dir=$1
  local xtrabackup_args=$2
  local xbstream_args=$3

  xtrabackup --backup ${xtrabackup_args} --target-dir=${stream_dir} &
  pxb_pid=$!
  wait_for_file_to_generated ${stream_dir}/thread_0
  xbstream ${xbstream_args} --fifo-dir=${stream_dir}
  wait ${pxb_pid}
}

function take_backup_fifo_xbcloud()
{
  local stream_dir=$1
  local xtrabackup_args=$2
  local xbcloud_args=$3

  xtrabackup --backup ${xtrabackup_args} --target-dir=${stream_dir} &
  pxb_pid=$!
  wait_for_file_to_generated ${stream_dir}/thread_0
  xbcloud --defaults-file=$topdir/xbcloud.cnf put \
  --fifo-dir=${stream_dir} ${xbcloud_args}
  wait ${pxb_pid}
}

function download_fifo_xbcloud()
{
  local stream_dir=$1
  local xbcloud_args=$2
  local xbstream_args=$3

  xbcloud --defaults-file=$topdir/xbcloud.cnf get \
  --fifo-dir=${stream_dir} ${xbcloud_args} &
  xbcloud_pid=$!
  wait_for_file_to_generated ${stream_dir}/thread_0
  xbstream ${xbstream_args} --fifo-dir=${stream_dir}
  wait ${xbcloud_pid}
}
