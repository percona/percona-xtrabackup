#!/bin/bash

. inc/common.sh

set +e

result=0
rm -rf results
mkdir results

function usage()
{
cat <<EOF
Usage: $0 [-f] [-g] [-h] [-s suite] [-t test_name] [-d mysql_basedir] [-c build_conf]
-f          Continue running tests after failures
-d path     Server installation directory. Default is './server'.
-g          Output debug information to results/*.out
-t path     Run only a single named test
-h          Print this help message
-s suite    Select a test suite to run. Possible values: experimental, t. 
            Default is 't'.
-c conf     XtraBackup build configuration as specified to build.sh.
            Default is 'autodetect', i.e. xtrabackup binary is determined based
            on the MySQL version.
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
    mysql_tmpdir="$topdir/tmp"
    mysql_port="3306"
    mysql_socket=`mktemp -t xtrabackup.mysql.sock.XXXXXX`
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

    MYSQLD_ARGS="--no-defaults --basedir=${MYSQL_BASEDIR} --socket=${mysql_socket} --datadir=$mysql_datadir --tmpdir=$mysql_tmpdir --skip-networking"
    if [ "`whoami`" = "root" ]
    then
	MYSQLD_ARGS="$MYSQLD_ARGS --user=root"
    fi

    find_program MYSQL_INSTALL_DB mysql_install_db $MYSQL_BASEDIR/bin \
	$MYSQL_BASEDIR/scripts
    find_program MYSQLD mysqld $MYSQL_BASEDIR/bin/ $MYSQL_BASEDIR/libexec
    find_program MYSQL mysql $MYSQL_BASEDIR/bin
    find_program MYSQLADMIN mysqladmin $MYSQL_BASEDIR/bin

    # Check if we are running from a source tree and, if so, set PATH 
    # appropriately
    if test "`basename $PWD`" = "test"
    then
	PATH="$PWD/..:$PWD/../libtar-1.2.11/libtar:$PWD/../Percona-Server-5.5/storage/innobase/xtrabackup:$PWD/../Percona-Server/storage/innodb_plugin/xtrabackup:$PWD/../mysql-5.1/storage/innobase/xtrabackup:$PWD/../mysql-5.1/storage/innodb_plugin/xtrabackup:$PWD/../mysql-5.5/storage/innobase/xtrabackup:$PATH"
    fi

    PATH="${MYSQL_BASEDIR}/bin:$PATH"

    export topdir mysql_datadir mysql_tmpdir mysql_port mysql_socket OUTFILE \
	IB_ARGS XB_ARGS TAR MYSQL_BASEDIR MYSQL MYSQLADMIN \
	MYSQL_ARGS MYSQLD_ARGS MYSQL_INSTALL_DB MYSQLD PATH
}

function get_version_info()
{
    if [ "$XB_BUILD" != "autodetect" ]
    then
	XB_BIN=""
	case "$XB_BUILD" in
	    "innodb51_builtin" )
		XB_BIN="xtrabackup_51";;
	    "innodb51" )
		XB_BIN="xtrabackup_plugin"
		MYSQLD_ARGS="$MYSQLD_ARGS --ignore-builtin-innodb --plugin-load=innodb=ha_innodb_plugin.so";;
	    "innodb55" )
		XB_BIN="xtrabackup_innodb55";;
	    "xtradb51" )
		XB_BIN="xtrabackup";;
	    "xtradb55" )
		XB_BIN="xtrabackup_55";;
	esac
	if [ -z "$XB_BIN" ]
	then
	    vlog "Unknown configuration: '$XB_BUILD'"
	    exit -1
	fi
    fi

    init >>$OUTFILE 2>&1

    MYSQL_VERSION=""
    INNODB_VERSION=""
    run_mysqld >>$OUTFILE 2>&1
    # Get MySQL and InnoDB versions
    MYSQL_VERSION=`$MYSQL ${MYSQL_ARGS} -Nsf -e "SHOW VARIABLES LIKE 'version'"`
    MYSQL_VERSION=${MYSQL_VERSION#"version	"}
    MYSQL_VERSION_COMMENT=`$MYSQL ${MYSQL_ARGS} -Nsf -e "SHOW VARIABLES LIKE 'version_comment'"`
    MYSQL_VERSION_COMMENT=${MYSQL_VERSION_COMMENT#"version_comment	"}
    INNODB_VERSION=`$MYSQL ${MYSQL_ARGS} -Nsf -e "SHOW VARIABLES LIKE 'innodb_version'"`
    INNODB_VERSION=${INNODB_VERSION#"innodb_version	"}
    XTRADB_VERSION="`echo $INNODB_VERSION  | sed 's/[0-9]\.[0-9]\.[0-9][0-9]*\(-[0-9][0-9]*\.[0-9][0-9]*\)*$/\1/'`"

    if [ "$XB_BUILD" = "autodetect" ]
    then
        # Determine xtrabackup build automatically
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
	    if [ -n "$XTRADB_VERSION" ]
	    then
		XB_BIN="xtrabackup_55"
	    else
		XB_BIN="xtrabackup_innodb55"
	    fi
	else
	    vlog "Uknown MySQL/InnoDB version: $MYSQL_VERSION/$INNODB_VERSION"
	    exit -1
	fi
    fi

    XB_PATH="`which $XB_BIN`"
    if [ -z "$XB_PATH" ]
    then
	vlog "Cannot find '$XB_BIN' in PATH"
	return 1
    fi
    XB_BIN="$XB_PATH"

    # Set the correct binary for innobackupex
    IB_BIN="`which innobackupex`"
    if [ -z "$IB_BIN" ]
    then
	vlog "Cannot find 'innobackupex' in PATH"
	return 1
    fi
    IB_ARGS="$IB_ARGS --ibbackup=$XB_BIN"

    export MYSQL_VERSION MYSQL_VERSION_COMMENT INNODB_VERSION XTRADB_VERSION \
	XB_BIN IB_BIN IB_ARGS
}

export SKIPPED_EXIT_CODE=200


tname=""
XTRACE_OPTION=""
XB_BUILD="autodetect"
force=""
while getopts "fgh?:t:s:d:c:" options; do
	case $options in
	        f ) force="yes";;
		t ) tname="$OPTARG";;
		g ) XTRACE_OPTION="-x";;
		h ) usage; exit;;
		s ) tname="$OPTARG/*.sh";;
	        d ) export MYSQL_BASEDIR="$OPTARG";;
	        c ) XB_BUILD="$OPTARG";;
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
export PIDDIR="$PWD"

if ! get_version_info
then
    echo "get_version_info failed. See $OUTFILE for details."
    exit -1
fi

if [ -n "$XTRADB_VERSION" ]
then
    echo "Running against Percona Server $MYSQL_VERSION (XtraDB $INNODB_VERSION)" | tee -a $OUTFILE
else
    echo "Running against MySQL $MYSQL_VERSION (InnoDB $INNODB_VERSION)" | tee -a $OUTFILE
fi
echo "Using '`basename $XB_BIN`' as xtrabackup binary" | tee -a $OUTFILE
echo | tee -a $OUTFILE

kill_leftovers >>$OUTFILE 2>&1
clean >>$OUTFILE 2>&1

source subunit.sh

SUBUNIT_OUT=test_results.subunit
rm -f $SUBUNIT_OUT

echo "========================================================================"

for t in $tests
do
   total_count=$((total_count+1))

   printf "%-40s" $t
   subunit_start_test $t >> $SUBUNIT_OUT
   name=`basename $t .sh`
   export OUTFILE="$PWD/results/$name"
   export SKIPPED_REASON="$PWD/results/$name.skipped"
   bash $XTRACE_OPTION $t > $OUTFILE 2>&1
   rc=$?

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
       if [ -z "$force" ]
       then
	   break;
       fi
   fi

   stop_mysqld >/dev/null 2>&1
   clean >/dev/null 2>&1
done

echo "========================================================================"
echo

if [ -n "$force" -o $failed_count -eq 0 ]
then
    kill_leftovers
fi

if [ $result -eq 1 ]
then
    echo
    echo "$failed_count/$total_count tests have failed: $failed_tests"
    echo "See results/ for detailed output"
    exit -1
fi
