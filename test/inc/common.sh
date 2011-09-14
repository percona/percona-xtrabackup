#!/bin/env bash

set -eu

function innobackupex()
{
    run_cmd $IB_BIN $IB_ARGS $*
}

function xtrabackup()
{
    run_cmd $XB_BIN $XB_ARGS $*
}

function vlog
{
    echo "`date +"%F %T"`: `basename "$0"`: $@" >&2
}

function clean_datadir()
{
    vlog "Removing MySQL data directory: $mysql_datadir"
    rm -rf "$mysql_datadir"
    vlog "Creating MySQL data directory: $mysql_datadir"
    mkdir -p "$mysql_datadir"
    init_mysql_dir
}

function clean()
{
    vlog "Removing temporary $topdir"
    rm -rf "$topdir"
}
function clean_on_error()
{
vlog "Exit on error"
clean
exit -1
}

function die()
{
  vlog "$*"
  exit 1
}

function initdir()
{
    if test -d "$topdir"
    then
        vlog "Directory $topdir exists. Removing it..."
        rm -r "$topdir"
    fi
    vlog "Creating temporary directory: $topdir"
    mkdir -p "$topdir/tmp"
    vlog "Creating MySQL data directory: $mysql_datadir"
    mkdir -p "$mysql_datadir"
}

function init_mysql_dir()
{
    vlog "Creating MySQL database"
    $MYSQL_INSTALL_DB --no-defaults --basedir=$MYSQL_BASEDIR --datadir="$mysql_datadir" --tmpdir="$mysql_tmpdir"
}
function set_mysl_port()
{
    i=$mysql_port
    while [ $i -lt 65536 ]
    do
        # check if port $i is used
        vlog "Checking port $i"
        port_status=`netstat -an | grep LISTEN | grep tcp | grep ":$i " || true`
        if test -z "$port_status"
        then
            # port is not used
            vlog "Port $i is free"
            mysql_port=$i
            break
        else
            vlog "Port $i is used"
            let i=$i+1
        fi
    done
}

# Checks whether MySQL is alive
function mysql_ping()
{
    local result="0"
    if test -S ${mysql_socket}
    then
        result=`${MYSQL} ${MYSQL_ARGS} -e "SELECT IF(COUNT(*)>=0, 1, 1) FROM user;" -s --skip-column-names mysql 2>/dev/null` || result="0"
    else
        result="0"
    fi
    echo $result
}

function kill_leftovers()
{
    while test -f "$PWD/mysqld.pid" 
    do
	vlog "Found a leftover mysqld processes with PID `cat $PWD/mysqld.pid`, stopping it"
	stop_mysqld 2> /dev/null
    done
}

function run_mysqld()
{
    local c=0
    kill_leftovers
    ${MYSQLD} ${MYSQLD_ARGS} $* --pid-file="$PWD/mysqld.pid" &
    while [ "`mysql_ping`" != "1" ]
    do
        if [ ${c} -eq 100 ]
        then
            vlog "Can't run MySQL!"
            clean_on_error
        fi
        let c=${c}+1
        sleep 1
    done
    if [ -n "$MYSQL_VERSION" -a -n "$INNODB_VERSION" ]
    then
	vlog "MySQL $MYSQL_VERSION (InnoDB $INNODB_VERSION) started successfully"
    fi
}

function run_cmd()
{
  vlog "===> $@"
  set +e
  "$@"
  local rc=$?
  set -e
  if [ $rc -ne 0 ]
  then
      die "===> `basename $1` failed with exit code $rc"
  fi
}

function run_cmd_expect_failure()
{
  vlog "===> $@"
  set +e
  "$@"
  local rc=$?
  set -e
  if [ $rc -eq 0 ]
  then
      die "===> `basename $1` succeeded when it was expected to fail"
  fi
}

function stop_mysqld()
{
    if [ -f "$PWD/mysqld.pid" ]
    then
	${MYSQLADMIN} ${MYSQL_ARGS} shutdown 
	vlog "Database server has been stopped"
	if [ -f "$PWD/mysqld.pid" ]
	then
	    sleep 1;
	    kill -9 `cat $PWD/mysqld.pid`
	    rm -f $PWD/mysqld.pid
	fi
    fi
}

function load_sakila()
{
vlog "Loading sakila"
${MYSQL} ${MYSQL_ARGS} -e "create database sakila"
vlog "Loading sakila scheme"
${MYSQL} ${MYSQL_ARGS} sakila < inc/sakila-db/sakila-schema.sql
vlog "Loading sakila data"
${MYSQL} ${MYSQL_ARGS} sakila < inc/sakila-db/sakila-data.sql
}

function load_dbase_schema()
{
vlog "Loading $1 database schema"
${MYSQL} ${MYSQL_ARGS} -e "create database $1"
${MYSQL} ${MYSQL_ARGS} $1 < inc/$1-db/$1-schema.sql
}

function load_dbase_data()
{
vlog "Loading $1 database data"
${MYSQL} ${MYSQL_ARGS} $1 < inc/$1-db/$1-data.sql
}

function drop_dbase()
{
  vlog "Dropping database $1"
  run_cmd ${MYSQL} ${MYSQL_ARGS} -e "DROP DATABASE $1"
}

function init()
{
initdir
init_mysql_dir
echo "
[mysqld]
datadir=$mysql_datadir
tmpdir=$mysql_tmpdir" > $topdir/my.cnf
}

function race_create_drop()
{
  vlog "Started create/drop table cycle"
  race_cycle_num=$1
  if [ -z $race_cycle_num ]
  then
    race_cycle_num=1000
  fi
  run_cmd ${MYSQL} ${MYSQL_ARGS} -e "create database race;"
  race_cycle_cnt=0;
  while [ "$race_cycle_num" -gt "$race_cycle_cnt"]
  do
	t1=tr$RANDOM
	t2=tr$RANDOM
	t3=tr$RANDOM
	${MYSQL} ${MYSQL_ARGS} -e "create table $t1 (a int) ENGINE=InnoDB;" race
	${MYSQL} ${MYSQL_ARGS} -e "create table $t2 (a int) ENGINE=InnoDB;" race
	${MYSQL} ${MYSQL_ARGS} -e "create table $t3 (a int) ENGINE=InnoDB;" race
	${MYSQL} ${MYSQL_ARGS} -e "drop table $t1;" race
	${MYSQL} ${MYSQL_ARGS} -e "drop table $t2;" race
	${MYSQL} ${MYSQL_ARGS} -e "drop table $t3;" race
	let "race_cycle_cnt=race_cycle_cnt+1"
  done
}

########################################################################
# Prints checksum for a given table.
# Expects 2 arguments: $1 is the DB name and $2 is the table to checksum
########################################################################
function checksum_table()
{
    $MYSQL $MYSQL_ARGS -Ns -e "CHECKSUM TABLE $2" $1 | awk {'print $2'}
}