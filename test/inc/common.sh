#!/bin/env bash

set -eu
topdir="`pwd`/var"
mysql_datadir="$topdir/mysql"
mysql_port="3306"
mysql_socket="/tmp/xtrabackup.mysql.sock"

function vlog
{
    #echo ""
    echo "`date +"%F %T"`: `basename "$0"`: $1"
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

function init_mysql()
{
	url="http://www.percona.com/downloads/community/"
	case "$MYSQL_VERSION" in
		system)
			echo "Using MySQL installed in the system"
			MYSQL=mysql
			MYSQLADMIN=mysqladmin
			MYSQL_INSTALL_DB=mysql_install_db
			MYSQL_ARGS="--no-defaults --socket=${mysql_socket} --user=root"
			MYSQLD=mysqld
			MYSQL_BASEDIR="/usr"
			MYSQLD_ARGS="--no-defaults --basedir=${MYSQL_BASEDIR} --socket=${mysql_socket} --datadir=$mysql_datadir"
			;;
		5.0)
			echo "Using MySQL 5.0"
			version="5.0.91-linux-`uname -m`-glibc23"
			cd $topdir
			wget "$url/mysql-$version.tar.gz"
			tar zxf mysql-$version.tar.gz
			MYSQL=$topdir/mysql-$version/bin/mysql
			MYSQLADMIN=$topdir/mysql-$version/bin/mysqladmin
			MYSQL_INSTALL_DB=$topdir/mysql-$version/scripts/mysql_install_db
			MYSQLD=$topdir/mysql-$version/bin/mysqld
			MYSQL_BASEDIR=$topdir/mysql-$version
			MYSQLD_ARGS="--no-defaults --basedir=${MYSQL_BASEDIR} --socket=${mysql_socket} --datadir=$mysql_datadir"
			MYSQL_ARGS="--no-defaults --socket=${mysql_socket} --user=root"
			cd -
			;;
		5.1)
			echo "Using MySQL 5.1"
			version="5.1.49-linux-`uname -m`-glibc23"
			cd $topdir
			wget "$url/mysql-$version.tar.gz"
			tar zxf mysql-$version.tar.gz
			MYSQL=$topdir/mysql-$version/bin/mysql
			MYSQLADMIN=$topdir/mysql-$version/bin/mysqladmin
			MYSQL_INSTALL_DB=$topdir/mysql-$version/scripts/mysql_install_db
			MYSQLD=$topdir/mysql-$version/bin/mysqld
			MYSQL_BASEDIR=$topdir/mysql-$version
			MYSQLD_ARGS="--no-defaults --basedir=${MYSQL_BASEDIR} --socket=${mysql_socket} --datadir=$mysql_datadir"
			MYSQL_ARGS="--no-defaults --socket=${mysql_socket} --user=root"
			cd -
			;;
		5.5)
			echo "Using MySQL 5.5"
			version="5.5.6-rc-linux2.6-`uname -m`"
			cd $topdir
			wget "$url/mysql-$version.tar.gz"
			tar zxf mysql-$version.tar.gz
			MYSQL=$topdir/mysql-$version/bin/mysql
			MYSQLADMIN=$topdir/mysql-$version/bin/mysqladmin
			MYSQL_INSTALL_DB=$topdir/mysql-$version/scripts/mysql_install_db
			MYSQLD=$topdir/mysql-$version/bin/mysqld
			MYSQL_BASEDIR=$topdir/mysql-$version
			MYSQLD_ARGS="--no-defaults --basedir=${MYSQL_BASEDIR} --socket=${mysql_socket} --datadir=$mysql_datadir"
			MYSQL_ARGS="--no-defaults --socket=${mysql_socket} --user=root"
			cd -
			;;
		percona)
			echo "Using Percona Server"
			version="5.1.51-rel11.5-132-Linux-`uname -m`"
			cd $topdir
			wget "$url/Percona-Server-$version.tar.gz"
			tar zxf Percona-Server-$version.tar.gz
			MYSQL=$topdir/Percona-Server-$version/bin/mysql
			MYSQLADMIN=$topdir/Percona-Server-$version/bin/mysqladmin
			MYSQL_INSTALL_DB=$topdir/Percona-Server-$version/bin/mysql_install_db
			MYSQLD=$topdir/Percona-Server-$version/libexec/mysqld
			MYSQL_BASEDIR=$topdir/Percona-Server-$version
			MYSQLD_ARGS="--no-defaults --basedir=${MYSQL_BASEDIR} --socket=${mysql_socket} --datadir=$mysql_datadir"
			MYSQL_ARGS="--no-defaults --socket=${mysql_socket} --user=root"
			set +u
			export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$topdir/Percona-Server-$version/lib/mysql
			set -u

			cd -
			;;
		*)
			usage
			exit -1
			;;
	esac
	if [ "`whoami`" = "root" ]
	then
		MYSQLD_ARGS="$MYSQLD_ARGS --user=root"
	fi
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


function run_mysqld()
{
    local c=0
    ${MYSQLD} ${MYSQLD_ARGS} $* --port=$mysql_port &
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
    vlog "MySQL started successfully"

}

function stop_mysqld()
{
    ${MYSQLADMIN} ${MYSQL_ARGS} shutdown

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


function init()
{
initdir
init_mysql
init_mysql_dir
set_mysl_port
}
