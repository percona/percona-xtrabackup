#!/bin/env bash

set -eu

function innobackupex()
{
    run_cmd `which innobackupex` $IB_ARGS $*
}

function xtrabackup()
{
    run_cmd `which $XB_BIN` $XB_ARGS $*
}

function vlog
{
    echo "`date +"%F %T"`: `basename "$0"`: $@"
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
    mkdir -p "$topdir"
    vlog "Creating MySQL data directory: $mysql_datadir"
    mkdir -p "$mysql_datadir"
}

function init_mysql_dir()
{
    vlog "Creating MySQL database"
    $MYSQL_INSTALL_DB --no-defaults --basedir=$MYSQL_BASEDIR --datadir="$mysql_datadir"
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

function get_xtrabackup_version()
{
    if [ "${MYSQL_VERSION:0:3}" = "5.1" ]
    then
	if [ -z "$INNODB_VERSION" ]
	then
	    XB_BIN="xtrabackup_51" # InnoDB 5.1 builtin
	else
	    XB_BIN="xtrabackup"    # InnoDB 5.1 plugin or Percona Server 5.1
	fi
    elif [ "${MYSQL_VERSION:0:3}" = "5.5" ]
    then
	XB_BIN="xtrabackup_55"
    else
	vlog "Uknown MySQL/InnoDB version: $MYSQL_VERSION/$INNODB_VERSION"
	exit -1
    fi
    XTRADB_VERSION=`echo $INNODB_VERSION  | sed 's/[0-9]\.[0-9]\.[0-9][0-9]*\(-[0-9][0-9]*\.[0-9][0-9]*\)*$/\1/'`
    vlog "Using '$XB_BIN' as xtrabackup binary"
    # Set the correct binary for innobackupex
    IB_ARGS="$IB_ARGS --ibbackup=$XB_BIN"
}

function kill_leftovers()
{
    while test -f "$PWD/mysqld.pid" 
    do
	vlog "Found a leftover mysqld processes with PID `cat $PWD/mysqld.pid`, stopping it"
	stop_mysqld
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
    # Get MySQL and InnoDB versions
    MYSQL_VERSION=`$MYSQL ${MYSQL_ARGS} -Nsf -e "SHOW VARIABLES LIKE 'version'"`
    MYSQL_VERSION=${MYSQL_VERSION#"version	"}
    INNODB_VERSION=`$MYSQL ${MYSQL_ARGS} -Nsf -e "SHOW VARIABLES LIKE 'innodb_version'"`
    INNODB_VERSION=${INNODB_VERSION#"innodb_version	"}
    get_xtrabackup_version
    vlog "MySQL started successfully"
}

function run_cmd()
{
  vlog "===> $@"
  "$@" || die "===> `basename $1` failed with exit code $?"
}

function stop_mysqld()
{
    if [ -f "$PWD/mysqld.pid" ]
    then
	${MYSQLADMIN} ${MYSQL_ARGS} shutdown 
	vlog "Database server has been stopped"
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
datadir=$mysql_datadir" > $topdir/my.cnf
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
