############################################################################
# Test kill-long-queries and kill-long-queries-timeout optins
############################################################################

. inc/common.sh

MYSQLD_EXTRA_MY_CNF_OPTS="
secure-file-priv=$TEST_VAR_ROOT
"

function bg_run()
{
    local varname=$1

    shift

    ( for cmd in "$@"
      do
        eval "$cmd"
      done ) &

    local pid=$!

    eval "$varname=$pid"

}

function mysql_select()
{
    vlog "Run select query with duration $1 seconds"
    ${MYSQL} ${MYSQL_ARGS} -c test  <<EOF
        /* Run background /*SELECT*\  */
        (
         SELECT SLEEP($1) FROM t1 FOR UPDATE
        ) UNION ALL
        (
         SELECT 1
        );
EOF
}

function mysql_update()
{
    vlog "Run update query with duration $1 seconds"
    ${MYSQL} ${MYSQL_ARGS} -c test <<EOF
        /* This is not SELECT but rather an /*UPDATE*\
        query  */
        UPDATE t1 SET a = SLEEP($1);
EOF
}


function bg_kill_ok()
{
    vlog "Killing $1, expecting it is alive"
    run_cmd kill $1
}


function bg_wait_ok()
{
    vlog "Waiting for $1, expecting it's success"
    run_cmd wait $1
}


function bg_wait_fail()
{
    vlog "Waiting for $1, expecting it would fail"
    run_cmd_expect_failure wait $1
}

function kill_all_queries()
{
  # we really want to ignore errors
  # some connections can die by themselves
  # between SELECT and killall
  run_cmd $MYSQL $MYSQL_ARGS --force --batch  test <<EOF
  select concat('KILL ',id,';') from information_schema.processlist
  where user='root' and time > 1 into outfile '$MYSQLD_TMPDIR/killall.sql';
  source $MYSQLD_TMPDIR/killall.sql;
EOF
  rm -f $MYSQLD_TMPDIR/killall.sql
}

function wait_for_connection_count()
{
  n=$1
  QUERY="SELECT COUNT(*) FROM PROCESSLIST \
WHERE CURRENT_USER() LIKE CONCAT(USER, '%') AND ID <> CONNECTION_ID()"
  for i in {1..200} ; do
    count=`${MYSQL} ${MYSQL_ARGS} -N -e "${QUERY}" information_schema`
    [ $count != $n ] || break
    sleep 0.3
  done
}

start_server

has_backup_locks && skip_test "Requires a server without backup locks support"

run_cmd $MYSQL $MYSQL_ARGS test <<EOF
CREATE TABLE t1(a INT) ENGINE=InnoDB;
INSERT INTO t1 VALUES (1);
CREATE TABLE t2(a INT) ENGINE=MyISAM;
INSERT INTO t2 VALUES (1);
EOF

mkdir $topdir/full

# ==============================================================
vlog "===================== case 1 ====================="
bg_run bg_select_pid "mysql_select 3"
bg_run bg_update_pid "mysql_update 3"

xtrabackup --backup --target-dir=$topdir/full/1 --kill-long-queries-timeout=50 \
           --kill-long-query-type=all

bg_wait_ok $bg_select_pid
bg_wait_ok $bg_update_pid
kill_all_queries


# ==============================================================
# this case can succeed for two reasons
# 1. update was waited for by xtrabackup an finished successfully
#    by the time of kill_all_queries
# 2. update was stared late and did not block FTWRL but was locked by
#    it. then it may be still alive by the time of kill_all_queries
vlog "===================== case 2 ====================="
bg_run bg_select_pid "mysql_select 200"
bg_run bg_update_pid "mysql_update 5"
wait_for_connection_count 2

xtrabackup --backup --target-dir=$topdir/full/2 --kill-long-queries-timeout=3 \
           --kill-long-query-type=select

bg_wait_fail $bg_select_pid
bg_wait_ok $bg_update_pid
kill_all_queries


# ==============================================================
vlog "===================== case 3 ====================="
bg_run bg_select_pid "mysql_select 200"
bg_run bg_update_pid "mysql_update 200"
wait_for_connection_count 2

xtrabackup --backup --target-dir=$topdir/full/3 --kill-long-queries-timeout=3 \
           --kill-long-query-type=all

bg_wait_fail $bg_select_pid
bg_wait_fail $bg_update_pid
kill_all_queries


# ==============================================================
vlog "===================== case 4 ====================="
bg_run bg_select_pid "mysql_select 200"
bg_run bg_update_pid "mysql_update 200"
wait_for_connection_count 2

run_cmd_expect_failure ${XB_BIN} ${XB_ARGS} --backup \
                          --target-dir=$topdir/full/4 \
                          --ftwrl-wait-timeout=3 \
                          --ftwrl-wait-query-type=all \
                          --ftwrl-wait-threshold=1 \
                          --kill-long-queries-timeout=1 \
                          --kill-long-query-type=all

bg_kill_ok $bg_select_pid
bg_kill_ok $bg_update_pid
kill_all_queries


# ==============================================================
vlog "===================== case 5 ====================="
bg_run bg_select_pid "mysql_select 200"
bg_run bg_update_pid "mysql_update 200"
wait_for_connection_count 2

# sleep at least --ftwrl-wait-threshold seconds
sleep 1

run_cmd_expect_failure ${XB_BIN} ${XB_ARGS} --backup \
                          --target-dir=$topdir/full/5 \
                          --ftwrl-wait-timeout=3 \
                          --ftwrl-wait-query-type=update \
                          --ftwrl-wait-threshold=1 \
                          --kill-long-queries-timeout=1 \
                          --kill-long-query-type=all

bg_kill_ok $bg_select_pid
bg_kill_ok $bg_update_pid
kill_all_queries


# ==============================================================
vlog "===================== case 6 ====================="
bg_run bg_update_pid "mysql_update 5"
wait_for_connection_count 1
bg_run bg_select_pid "mysql_select 200"
wait_for_connection_count 2

# sleep at least --ftwrl-wait-threshold seconds
sleep 1

xtrabackup --backup --target-dir=$topdir/full/6 \
           --ftwrl-wait-timeout=6 \
           --ftwrl-wait-query-type=update \
           --ftwrl-wait-threshold=1 \
           --kill-long-queries-timeout=1 \
           --kill-long-query-type=all

bg_wait_fail $bg_select_pid
bg_wait_ok $bg_update_pid
kill_all_queries
