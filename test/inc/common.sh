#!/bin/env bash

set -eu
topdir="`pwd`/var"
mysql_datadir="$topdir/mysql"
mysql_port="3306"
mysql_socket="$topdir/mysql.sock"
MYSQL=mysql
MYSQL_ARGS="--socket=${mysql_socket} --user=root"
MYSQLD=mysqld_safe
MYSQLD_ARGS="--socket=${mysql_socket} --datadir=$mysql_datadir"


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
exit
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
    mysql_install_db --datadir="$mysql_datadir"
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
    ${MYSQLD} ${MYSQLD_ARGS} --port=$mysql_port &
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
    mysqladmin ${MYSQL_ARGS} shutdown

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
