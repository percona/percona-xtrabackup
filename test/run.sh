#!/bin/bash

. inc/common.sh

set +e

result=0
rm -rf results
mkdir results

function usage()
{
cat <<EOF
Usage: $0 [-g] [-h] [-s suite] [-t test_name] -d mysql_basedir
-d path     Server installation directory
-g          Output debug information to results/*.out
-t path     Run only a single named test
-h          Print this help message
-s suite    Select a test suite to run. Possible values: experimental, t. 
            Default is 't'.
EOF
}

function find_program()
{
    local VARNAME="$1"
    shift
    local PROGNAME="$1"
    shift
    local DIRLIST="$*"
    local found=""

    for dir in in $DIRLIST
    do
	if [ -d "$dir" -a -x "$dir/$PROGNAME" ]
	then
	    eval "$VARNAME=\"$dir/$PROGNAME\""
	    found="yes"
	    break
	fi
    done
    if [ -z "$found" ]
    then
	echo "Can't find $PROGNAME in $DIRLIST"
	exit -1
    fi
}

function set_vars()
{
    topdir="`pwd`/var"
    mysql_datadir="$topdir/mysql"
    mysql_port="3306"
    mysql_socket=`tempfile -p XBts --suffix .xtrabackup.mysql.sock`
    IB_ARGS="--defaults-file=$topdir/my.cnf --user=root --socket=$mysql_socket"
    XB_ARGS="--no-defaults"

    if gnutar --version > /dev/null 2>&1
    then
	TAR=gnutar
    else
	TAR=tar
    fi

    MYSQL_BASEDIR=${MYSQL_BASEDIR:-"$PWD/server"}

    MYSQL_ARGS="--no-defaults --socket=${mysql_socket} --user=root"

    MYSQLD_ARGS="--no-defaults --basedir=${MYSQL_BASEDIR} --socket=${mysql_socket} --datadir=$mysql_datadir --skip-networking"
    if [ "`whoami`" = "root" ]
    then
	MYSQLD_ARGS="$MYSQLD_ARGS --user=root"
    fi

    find_program MYSQL_INSTALL_DB mysql_install_db $MYSQL_BASEDIR/bin \
	$MYSQL_BASEDIR/scripts
    find_program MYSQLD mysqld $MYSQL_BASEDIR/bin/ $MYSQL_BASEDIR/libexec
    find_program MYSQL mysql $MYSQL_BASEDIR/bin
    find_program MYSQLADMIN mysqladmin $MYSQL_BASEDIR/bin

    export MYSQL_INSTALL_DB
    export MYSQL
    export MYSQLD
    export MYSQLADMIN

    # Check if we are running from a source tree and, if so, set PATH 
    # appropriately
    if test "`basename $PWD`" = "test"
    then
	PATH="$PWD/..:$PWD/../libtar-1.2.11/libtar:$PWD/../Percona-Server-5.5/storage/innobase/xtrabackup:$PWD/../Percona-Server/storage/innodb_plugin/xtrabackup:$PWD/../mysql-5.1/storage/innobase/xtrabackup:$PWD/../mysql-5.1/storage/innodb_plugin/xtrabackup:$PWD/../mysql-5.5/storage/innobase/xtrabackup:$PATH"
    fi

    PATH="${MYSQL_BASEDIR}/bin:$PATH"

    export topdir mysql_datadir mysql_port mysql_socket OUTFILE IB_ARGS \
	XB_ARGS TAR MYSQL_BASEDIR MYSQL MYSQLADMIN \
	MYSQL_ARGS MYSQLD_ARGS MYSQL_INSTALL_DB MYSQLD PATH
}

tname=""
XTRACE_OPTION=""
export SKIPPED_EXIT_CODE=200
export MYSQL_VERSION="system"
while getopts "gh?:t:s:d:" options; do
	case $options in
		t ) tname="$OPTARG";;
		g ) XTRACE_OPTION="-x";;
		h ) usage; exit;;
		s ) tname="$OPTARG/*.sh";;
	        d ) export MYSQL_BASEDIR="$OPTARG";;
		? ) echo "Use \`$0 -h' for the list of available options."
                    exit -1;;
	esac
done

set_vars

if [ -n "$tname" ]
then
   tests="$tname"
else
   tests="t/*.sh"
fi

failed_count=0
failed_tests=
total_count=0

export OUTFILE="$PWD/results/setup"
kill_leftovers
clean

source subunit.sh

SUBUNIT_OUT=test_results.subunit
rm -f $SUBUNIT_OUT

for t in $tests
do
   printf "%-40s" $t
   subunit_start_test $t >> $SUBUNIT_OUT
   name=`basename $t .sh`
   export OUTFILE="$PWD/results/$name"
   export SKIPPED_REASON="$PWD/results/$name.skipped"
   bash $XTRACE_OPTION $t > $OUTFILE 2>&1
   rc=$?

   stop_mysqld >/dev/null 2>&1
   clean >/dev/null 2>&1

   if [ $rc -eq 0 ]
   then
       echo "[passed]"
       subunit_pass_test $t >> $SUBUNIT_OUT
   elif [ $rc -eq $SKIPPED_EXIT_CODE ]
   then
       sreason=""
       test -r $SKIPPED_REASON && sreason=`cat $SKIPPED_REASON`
       echo "[skipped]    $sreason"
       subunit_skip_test $t >> $SUBUNIT_OUT
   else
       echo "[failed]"

       (
	   (echo "Something went wrong running $t. Exited with $rc";
	       echo; echo; cat $OUTFILE
	   ) | subunit_fail_test $t
       ) >> $SUBUNIT_OUT

       failed_count=$((failed_count+1))
       failed_tests="$failed_tests $t"
       result=1
   fi

   total_count=$((total_count+1))
done

kill_leftovers

if [ $result -eq 1 ]
then
    echo
    echo "$failed_count/$total_count tests have failed: $failed_tests"
    echo "See results/ for detailed output"
    exit -1
fi
