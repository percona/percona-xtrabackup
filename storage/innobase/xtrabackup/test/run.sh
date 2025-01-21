#!/bin/bash

export DEBUG=

shopt -s extglob

set +e

. inc/common.sh
. subunit.sh

trap cleanup_on_exit EXIT
trap terminate SIGHUP SIGINT SIGQUIT SIGTERM

result=0

# Default test timeout in seconds
TEST_TIMEOUT=1800

# Magic exit code to indicate a skipped test
export SKIPPED_EXIT_CODE=200

# Default server installation directory (-d option)
MYSQL_BASEDIR=${MYSQL_BASEDIR:-"$PWD/server/"}

TEST_BASEDIR="$PWD"
TEST_VAR_ROOT="$TEST_BASEDIR/var"

# Global statistics
FAILED_COUNT=0
FAILED_TESTS=
SKIPPED_COUNT=0
SKIPPED_TESTS=
SUCCESSFUL_COUNT=0
TOTAL_COUNT=0
TOTAL_TIME=0

function usage()
{
cat <<EOF
Usage: $0 [-f] [-g] [-h] [-s suite] [-t test_name] [-d mysql_basedir] [-c build_conf]
-f          Continue running tests after failures
-d path     Server installation directory. Default is './server'.
-g          Debug mode
-k          Make a copy of failed var directory
-t path     Run only a single named test. This option can be passed multiple times.
-h          Print this help message
-s suite    Select a test suite to run. Possible values: binlog, experimental, gr, keyring, rocksdb, pagetracking,  main, compression and xbcloud.
            Default is 'binlog, main, gr, pagetracking, rocksdb, keyring, compression and xbcloud'.
-j N        Run tests in N parallel processes.
-T seconds  Test timeout (default is $TEST_TIMEOUT seconds).
-x options  Extra options to pass to xtrabackup
-X options  Extra options to pass to mysqld
-r path     Use specified path as root directory for test workers.
-R          Run xtrabackup and mysqld under Record and Replay
-D          Use mysqld-debug for debug test run.
EOF
}

###############################################################################
# Calculate the number of parallel workers automatically based on the number of
# available cores
###############################################################################
function autocalc_nworkers()
{
    if [ -r /proc/cpuinfo ]
    then
        NWORKERS=`grep processor /proc/cpuinfo | wc -l`
    elif which sysctl >/dev/null 2>&1
    then
        NWORKERS=`sysctl -n hw.ncpu`
    fi

    if [[ ! $NWORKERS =~ ^[0-9]+$ || $NWORKERS < 1 ]]
    then
        echo "Cannot determine the number of available CPU cures!"
        exit -1
    fi

    XB_TEST_MAX_WORKERS=${XB_TEST_MAX_WORKERS:-16}
    if [ "$NWORKERS" -gt $XB_TEST_MAX_WORKERS ]
    then
        cat <<EOF
Autodetected number of cores: $NWORKERS
Limiting to $XB_TEST_MAX_WORKERS to avoid excessive resource consumption
EOF
        NWORKERS=$XB_TEST_MAX_WORKERS
    fi
}

###############################################################################
# Kill a specified worker
###############################################################################
function kill_worker()
{
    local worker=$1
    local pid=${worker_pids[$worker]}
    local cpids=`pgrep -P $pid`

    worker_pids[$worker]=""

    if ! kill -0 $pid >/dev/null 2>&1
    then
        wait $pid >/dev/null 2>&1 || true
        return 0
    fi

    # First send SIGTERM to let worker exit gracefully
    kill -SIGTERM $pid >/dev/null 2>&1 || true
    for cpid in $cpids ; do kill -SIGTERM $cpid >/dev/null 2>&1 ; done

    sleep 1

    if ! kill -0 $pid >/dev/null 2>&1
    then
        wait $pid >/dev/null 2>&1 || true
        return 0
    fi

    # Now kill with SIGKILL
    kill -SIGKILL $pid >/dev/null 2>&1 || true
    for cpid in $cpids ; do kill -SIGKILL $cpid >/dev/null 2>&1 ; done
    wait $pid >/dev/null 2>&1 || true
    release_port_locks $pid
}

########################################################################
# Kill all running workers
########################################################################
function kill_all_workers()
{
    while true
    do
        found=""

        # First send SIGTERM to let workers exit gracefully
        for ((i = 1; i <= NWORKERS; i++))
        do
            [ -z ${worker_pids[$i]:-""} ] && continue

            found="yes"

           if ! kill -0 ${worker_pids[$i]} >/dev/null 2>&1
           then
               worker_pids[$i]=""
               continue
           fi

            kill -SIGTERM ${worker_pids[$i]} >/dev/null 2>&1 || true
        done

        [ -z "$found" ] && break

        sleep 1

        # Now kill with SIGKILL
        for ((i = 1; i <= NWORKERS; i++))
        do
            [ -z ${worker_pids[$i]:-""} ] && continue

            if ! kill -0 ${worker_pids[$i]} >/dev/null 2>&1
            then
                wait ${worker_pids[$i]} >/dev/null 2>&1 || true
                worker_pids[$i]=""
                continue
            fi

            kill -SIGKILL ${worker_pids[$i]} >/dev/null 2>&1 || true
            wait ${worker_pids[$i]} >/dev/null 2>&1 || true
            release_port_locks ${worker_pids[$i]}
            worker_pids[$i]=""
        done
    done
}

########################################################################
# Handler called from a fatal signal handler trap
########################################################################
function terminate()
{
    echo "Terminated, cleaning up..."

    # The following will call cleanup_on_exit()
    exit 2
}

########################################################################
# Display the test run summary and exit
########################################################################
function print_status_and_exit()
{
    local test_time=$((`now` - TEST_START_TIME))
    cat <<EOF
==============================================================================
Spent $TOTAL_TIME of $test_time seconds executing testcases

SUMMARY: $TOTAL_COUNT run, $SUCCESSFUL_COUNT successful, $SKIPPED_COUNT skipped, $FAILED_COUNT failed

EOF

    if [ -n "$SKIPPED_TESTS" ]
    then
        echo "Skipped tests: $SKIPPED_TESTS"
        echo
    fi

    if [ -n "$FAILED_TESTS" ]
    then
        echo "Failed tests: $FAILED_TESTS"
        echo
    fi

    echo "See results/ for detailed output"

    if [ "$FAILED_COUNT" = 0 ]
    then
        exit 0
    fi

    exit 1
}

########################################################################
# Cleanup procedure invoked on process exit
########################################################################
function cleanup_on_exit()
{
    kill_servers $TEST_VAR_ROOT

    remove_var_dirs

    release_port_locks $$

    kill_all_workers

    cleanup_all_workers
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

########################################################################
# Explore environment and setup global variables
########################################################################
function set_vars()
{
    if gnutar --version > /dev/null 2>&1
    then
        TAR=gnutar
    elif gtar --version > /dev/null 2>&1
    then
        TAR=gtar
    else
        TAR=tar
    fi

    if gnused --version > /dev/null 2>&1
    then
        SED=gnused
    elif gsed --version > /dev/null 2>&1
    then
        SED=gsed
    else
        SED=sed
    fi

    if [ "$MYSQL_DEBUG_MODE" = "on" ]; then
        find_program MYSQLD mysqld-debug $MYSQL_BASEDIR/bin/ $MYSQL_BASEDIR/libexec
    else
        find_program MYSQLD mysqld $MYSQL_BASEDIR/bin/ $MYSQL_BASEDIR/libexec
    fi
    find_program MYSQL mysql $MYSQL_BASEDIR/bin
    find_program MYSQLADMIN mysqladmin $MYSQL_BASEDIR/bin
    find_program MYSQLDUMP mysqldump $MYSQL_BASEDIR/bin

    # Check if we are running from a source tree and, if so, set PATH 
    # appropriately
    if test -d $PWD/../src
    then
	PATH="$PWD/../../../../runtime_output_directory:$PWD/..:$PWD/../src/xbcloud:$PATH"
        XTRABACKUP_BASEDIR="$PWD/../src"
    fi

    if test -d $PWD/../bin
    then
        PATH="$PWD/../bin:$PATH"
    fi

    # 8.0 branch run test at xtrabackup-test/test folder
    if test -d $PWD/../../bin
    then
        PATH="$PWD/../../bin:$PATH"
    fi

    PATH="${MYSQL_BASEDIR}/bin:$PATH"

    if [ -z "${LD_LIBRARY_PATH:-}" ]; then
	LD_LIBRARY_PATH=$MYSQL_BASEDIR/lib/mysql
    else
	LD_LIBRARY_PATH=$MYSQL_BASEDIR/lib/mysql:$LD_LIBRARY_PATH
    fi
    DYLD_LIBRARY_PATH="$LD_LIBRARY_PATH"

    export TAR SED MYSQL_BASEDIR MYSQL MYSQLD MYSQLADMIN \
MYSQL_INSTALL_DB PATH LD_LIBRARY_PATH DYLD_LIBRARY_PATH MYSQLDUMP \
XTRABACKUP_BASEDIR

    # Use stage-v.percona.com as a VersionCheck server
    export PERCONA_VERSION_CHECK_URL=https://stage-v.percona.com
}

########################################################################
# Configura ASAN suppress list
########################################################################
function config_asan()
{
  IS_ASAN=$(ldd $XB_BIN | grep asan | wc -l)
  if [[ $IS_ASAN != 0 ]];
  then
    LSAN_OPTIONS=suppressions=${PWD}/asan.supp
    export LSAN_OPTIONS
    echo "This is ASAN build"
  fi
}


# Configure mysql to read local component files if created by test cases.
function config_local_components()
{
  local FILE="${MYSQLD}.my"
  if [[ ! -f "${FILE}" ]]; then
    cat <<EOF > "${FILE}"
{
  "read_local_manifest": true
}
EOF
  fi

  FILE="$MYSQL_BASEDIR/lib/plugin/component_keyring_file.cnf"
  if [[ ! -f "${FILE}" ]]; then
    cat <<EOF > "${FILE}"
{
  "read_local_config": true
}
EOF
  fi

  FILE="$MYSQL_BASEDIR/lib/plugin/component_keyring_kmip.cnf"
  if [[ ! -f "${FILE}" ]]; then
    cat <<EOF > "${FILE}"
{
 "read_local_config": true
}
EOF
  fi

  FILE="$MYSQL_BASEDIR/lib/plugin/component_keyring_kms.cnf"
  if [[ ! -f "${FILE}" ]]; then
    cat <<EOF > "${FILE}"
{
  "read_local_config": true
}
EOF
  fi

  FILE="$MYSQL_BASEDIR/lib/plugin/component_keyring_vault.cnf"
  if [[ ! -f "${FILE}" ]]; then
    cat <<EOF > "${FILE}"
{
  "read_local_config": true
}
EOF
  fi
}

# Fix innodb51 test failures on Centos5-32 Jenkins slaves due to SELinux
# preventing shared symbol relocations in ha_innodb_plugin.so.0.0.0
function fix_selinux()
{
    if which lsb_release &>/dev/null && \
        lsb_release -d | grep CentOS &>/dev/null && \
        lsb_release -r | egrep '5.[0-9]' &>/dev/null && \
        which chcon &>/dev/null
    then
        chcon -t textrel_shlib_t $MYSQL_BASEDIR/lib/plugin/ha_innodb_plugin.so.0.0.0
    fi
}


function get_version_info()
{
    MYSQLD_EXTRA_ARGS=

    XB_BIN=""
    XB_ARGS="--no-defaults"
    XB_BIN="xtrabackup"

    MYSQL_VERSION=""
    INNODB_VERSION=""

    start_server >>$OUTFILE 2>&1

    # Get MySQL and InnoDB versions
    MYSQL_VERSION=`$MYSQL ${MYSQL_ARGS} -Nsf -e "SHOW VARIABLES LIKE 'version'"`
    MYSQL_VERSION=${MYSQL_VERSION#"version	"}
    MYSQL_VERSION_COMMENT=`$MYSQL ${MYSQL_ARGS} -Nsf -e "SHOW VARIABLES LIKE 'version_comment'"`
    MYSQL_VERSION_COMMENT=${MYSQL_VERSION_COMMENT#"version_comment	"}

    # Split version info into components for easier compatibility checks
    [[ $MYSQL_VERSION =~ ^([0-9]+)\.([0-9]+)\.([0-9]+) ]] || \
        die "Cannot parse server version: '$MYSQL_VERSION'"
    MYSQL_VERSION_MAJOR=${BASH_REMATCH[1]}
    MYSQL_VERSION_MINOR=${BASH_REMATCH[2]}
    MYSQL_VERSION_PATCH=${BASH_REMATCH[3]}

    INNODB_VERSION=`$MYSQL ${MYSQL_ARGS} -Nsf -e "SHOW VARIABLES LIKE 'innodb_version'"`
    INNODB_VERSION=${INNODB_VERSION#"innodb_version	"}
    XTRADB_VERSION="`echo $INNODB_VERSION  | sed 's/[0-9]\.[0-9]\.[0-9][0-9]*\(-[0-9][0-9]*\.[0-9][0-9]*\)*$/\1/'`"

    WSREP_ENABLED=`$MYSQL ${MYSQL_ARGS} -Nsf -e "SHOW VARIABLES" | grep -i wsrep`

    WSREP_READY=`$MYSQL ${MYSQL_ARGS} -Nsf -e "SHOW STATUS LIKE 'wsrep_ready'"`
    WSREP_READY=${WSREP_READY#"wsrep_ready	"}

    if [ -n "$WSREP_ENABLED" -a -z "$WSREP_READY" ]
    then
        die "Galera initialization failed!"
    fi

    if [ -n "$WSREP_READY" ]
    then
       if [ -f ${MYSQL_BASEDIR}/lib/libgalera_smm.so ]
       then
           LIBGALERA_PATH=${MYSQL_BASEDIR}/lib/libgalera_smm.so
       elif [ -f ${MYSQL_BASEDIR}/lib/galera3/libgalera_smm.so ]
       then
           LIBGALERA_PATH=${MYSQL_BASEDIR}/lib/galera3/libgalera_smm.so
       else
           die "Cannot find libgalera_smm.so"
       fi
    fi

    # Determine MySQL flavor
    if [[ "$MYSQL_VERSION" =~ "MariaDB" ]]
    then
	MYSQL_FLAVOR="MariaDB"
    elif [ -n "$XTRADB_VERSION" ]
    then
	MYSQL_FLAVOR="Percona Server"
    else
	MYSQL_FLAVOR="MySQL"
    fi

    # Determine InnoDB flavor
    if [ -n "$XTRADB_VERSION" ]
    then
	INNODB_FLAVOR="XtraDB"
    else
	INNODB_FLAVOR="InnoDB"
    fi

    # version check - Allow 8.1 to 8.4
    if [[ "$MYSQL_VERSION_MAJOR" -ne 8 ]] || [[ "$MYSQL_VERSION_MINOR" -lt 1 ]] && [[ "$MYSQL_VERSION_MINOR" -gt 4 ]]
    then
        vlog "Unknown MySQL/InnoDB version: $MYSQL_VERSION/$INNODB_VERSION"
        exit -1
    fi

    XB_PATH="`which $XB_BIN`"
    if [ -z "$XB_PATH" ]
    then
	vlog "Cannot find '$XB_BIN' in PATH"
	return 1
    fi
    XB_BIN="$XB_PATH"

    stop_server >>$OUTFILE 2>&1

    export MYSQL_VERSION MYSQL_VERSION_COMMENT MYSQL_FLAVOR \
	INNODB_VERSION XTRADB_VERSION INNODB_FLAVOR \
	XB_BIN XB_ARGS MYSQLD_EXTRA_ARGS WSREP_READY LIBGALERA_PATH
}

############################################################################
# Make a copy of var directory
############################################################################
function copy_worker_var_dir()
{
  local keep_worker_number=$1
  [ -d $PWD/results/debug/ ] || mkdir $PWD/results/debug/
  for worker_dir in $TEST_BASEDIR/var/w+([0-9])
  do
      [ -d $worker_dir ] || continue
      [[ $worker_dir =~ w([0-9]+)$ ]] || continue

      local worker=${BASH_REMATCH[1]}

      if [ "$worker" = "$keep_worker_number" ]
      then
          local tname=`basename $tpath .sh`
          local debug_dir=$(mktemp -d  $PWD/results/debug/${tname}.XXXXXX)
          cp -R ${worker_outfiles[$worker]} ${debug_dir}
          cp -R $TEST_BASEDIR/var/w${worker} ${debug_dir}
      fi
  done
}

###########################################################################
# Kill all server processes started by a worker specified with its var root
# directory
###########################################################################
function kill_servers()
{
    local var_root=$1
    local file

    [ -d $var_root ] || return 0

    cd $var_root

    for file in mysqld*.pid
    do
        if [ -f $file ]
        then
            vlog "Found a leftover mysqld processes with PID `cat $file`, \
stopping it"
            kill -9 `cat $file` 2>/dev/null || true
            rm -f $file
        fi
    done

    cd - >/dev/null 2>&1
}

###########################################################################
# Kill all server processes started by a worker specified with its number
###########################################################################
function kill_servers_for_worker()
{
    local worker=$1

    kill_servers $TEST_BASEDIR/var/w$worker
}

################################################################################
# Clean up all workers (except DEBUG_WORKER if set) We can't use worker* arrays
# here and examine the directory structure, because the same functions is called
# on startup when we want to cleanup all workers started on previous invokations
################################################################################
function cleanup_all_workers() { local worker

    for worker_dir in $TEST_BASEDIR/var/w+([0-9])
    do
        [ -d $worker_dir ] || continue
        [[ $worker_dir =~ w([0-9]+)$ ]] || continue

        worker=${BASH_REMATCH[1]}

        if [ "$worker" = "$DEBUG_WORKER" ]
        then
            echo
            echo "Skipping cleanup for worker #$worker due to debug mode."
            echo "You can do post-mortem analysis by examining test data in \
$worker_dir"
            echo "and the server process if it was running at the failure time."
            continue
        fi

        cleanup_worker $worker
    done
}
########################################################################
# Clean up a specified worker
########################################################################
function cleanup_worker()
{
    local worker=$1
    local tmpdir

    kill_servers_for_worker $worker >>$OUTFILE 2>&1

    # It is possible that a file is created while rm is in progress
    # which results in "rm: cannot remove ...: Directory not empty
    # hence the loop below
    while true
    do
        # Fix permissions as some tests modify them so the following 'rm' fails
        chmod -R 0700 $TEST_BASEDIR/var/w$worker >/dev/null 2>&1
        rm -rf $TEST_BASEDIR/var/w$worker && break
    done

    tmpdir=${worker_tmpdirs[$worker]:-""}
    if [ -n "$tmpdir" ]
    then
        rm -rf $tmpdir
    fi
}

########################################################################
# Return the number of seconds since the Epoch, UTC
########################################################################
function now()
{
    date '+%s'
}

########################################################################
# Process the exit code of a specified worker
########################################################################
function reap_worker()
{
    local worker=$1
    local pid=${worker_pids[$worker]}
    local skip_file=${worker_skip_files[$worker]}
    local tpath=${worker_names[$worker]}
    local tname=`basename $tpath .sh`
    local suite=`dirname $tpath | awk -F'/' '{print $NF}'`
    if [[ "${suite}" = "t" ]];
    then
      suite="main"
    fi
    if [[ $NOFSUITES -ge 1 ]];
    then
      tname="${suite}.${tname}"
    fi
    local test_time=$((`now` - worker_stime[$worker]))
    local status_file=${worker_status_files[$worker]}
    local rc

    if [ -f "$status_file" ]
    then
        rc=`cat $status_file`
        # Assume exit code 1 if status file is empty
        rc=${rc:-"1"}
    else
        # Assume exit code 1 if status file does not exist
        rc="1"
    fi

    printf "%-40s w%d\t" $tname $worker

    ((TOTAL_TIME+=test_time))

    # Have to call subunit_start_test here, as currently tests cannot be
    # interleaved in the subunit output
    subunit_start_test $tpath "${worker_stime_txt[$worker]}" >> $SUBUNIT_OUT

    if [ $rc -eq 0 ]
    then
        echo "[passed]    $test_time"

        SUCCESSFUL_COUNT=$((SUCCESSFUL_COUNT + 1))
        subunit_pass_test $tpath >> $SUBUNIT_OUT

        cleanup_worker $worker
    elif [ $rc -eq $SKIPPED_EXIT_CODE ]
    then
        sreason=""
        test -r $skip_file && sreason=`cat $skip_file`

        SKIPPED_COUNT=$((SKIPPED_COUNT + 1))
        SKIPPED_TESTS="$SKIPPED_TESTS $tname"

        echo "[skipped]   $sreason"

        subunit_skip_test $tpath >> $SUBUNIT_OUT

        cleanup_worker $worker
    else
        echo "[failed]    $test_time"

        (
            (echo "Something went wrong running $tpath. Exited with $rc";
                echo; echo; cat ${worker_outfiles[$worker]}
            ) | subunit_fail_test $tpath
        ) >> $SUBUNIT_OUT

       FAILED_COUNT=$((FAILED_COUNT + 1))
       FAILED_TESTS="$FAILED_TESTS $tname"

       # Return 0 on failed tests in the -f mode
       if [ -z "$force" ]
       then

           if [ -z "$DEBUG" ]
           then
                if [ ! -z "$KEEP_VAR" ]
                then
                  copy_worker_var_dir $worker
                fi
               cleanup_worker $worker
           else
               DEBUG_WORKER=$worker
           fi

           return 1
       else
           if [ ! -z "$KEEP_VAR" ]
           then
             copy_worker_var_dir $worker
           fi
           cleanup_worker $worker

           return 0
       fi
    fi

}

#############################################################################
# Check if a specified worker has exceed TEST_TIMEOUT and if so, terminate it
#############################################################################
function check_timeout_for_worker()
{
    local worker=$1
    local tpath=${worker_names[$worker]}
    local tname=`basename $tpath .sh`

    if (( `now` - worker_stime[$worker] > TEST_TIMEOUT ))
    then
        kill_worker $worker
        printf "%-40s w%d\t" $tname $worker
        echo "[failed]    Timed out after $TEST_TIMEOUT seconds."

        (
            # Have to call subunit_start_test here, as currently tests cannot be
            # interleaved in the subunit output
            subunit_start_test $tpath "${worker_stime_txt[$worker]}"

            (echo "Timeout exceeded running $tpath.";
                echo; echo; cat ${worker_outfiles[$worker]}
            ) | subunit_fail_test $tpath
        ) >> $SUBUNIT_OUT

        FAILED_COUNT=$((FAILED_COUNT + 1))
        FAILED_TESTS="$FAILED_TESTS $tname"

        # Return 0 on failed tests in the -f mode
        if [ -z "$force" ]
        then

            if [ -z "$DEBUG" ]
           then
                cleanup_worker $worker
            else
               DEBUG_WORKER=$worker
           fi

            return 1
        else
            cleanup_worker $worker

            return 0
       fi
    fi
}
#########################################################################
# Wait for all currently running worker to finish
#########################################################################
function reap_all_workers()
{
    while true
    do
        found=""

        for ((i = 1; i <= NWORKERS; i++))
        do
            [ -z ${worker_pids[$i]:-""} ] && continue

            found="yes"

            # Check if it's alive
            if kill -0 ${worker_pids[$i]} >/dev/null 2>&1
            then
                check_timeout_for_worker $i || print_status_and_exit
                continue
            fi

            reap_worker $i || print_status_and_exit

            worker_pids[$i]=""
        done

        [ -z "$found" ] && break

        sleep 1
    done
}

########################################################################
# Release all port locks reserved by the current process
# Used by the EXIT trap (in both normal and abnormal shell termination)
########################################################################
function release_port_locks()
{
    local process=$1
    local lockfile

    # Suppress errors when no port lock files are found
    shopt -s nullglob

    for lockfile in /tmp/xtrabackup_port_lock.*
    do
        if [ "`cat $lockfile 2>/dev/null`" = $process ]
        then
            rm -rf $lockfile
        fi
    done

    shopt -u nullglob
}

########################################################################
# Report status and release port locks on exit
########################################################################
function cleanup_on_test_exit()
{
    local rc=$?

    echo $rc > $STATUS_FILE

    release_port_locks $$
}

########################################################################
# Script body
########################################################################

TEST_START_TIME=`now`

tname=""
suites=""
NOFSUITES=0
XTRACE_OPTION=""
force=""
KEEP_VAR=""
SUBUNIT_OUT=test_results.subunit
NWORKERS=
DEBUG_WORKER=""
MYSQL_DEBUG_MODE=off

while getopts "fgkhDR?:t:s:d:c:j:T:x:X:i:r:" options; do
        case $options in
            f ) force="yes";;
            t )

                tname="${tname} $OPTARG";
                if ! [ -r "$OPTARG" ]
                then
                    echo "Cannot find test $OPTARG."
                    exit -1
                fi
                ;;

            g ) DEBUG=on;;
            k ) KEEP_VAR=on;;
            h ) usage; exit;;
            s )
                if [[ "$OPTARG" = "default" ]];
                then
                  tname="${tname} t/*.sh";
                else
                  tname="${tname} suites/$OPTARG/*.sh";
                fi
                suites="${suites} $OPTARG"
                ;;
            d ) export MYSQL_BASEDIR="$OPTARG";;
            D ) export MYSQL_DEBUG_MODE=on ;;
            c ) echo "Warning: -c does not have any effect and is only \
recognized for compatibility";;
            j )

                if [[ ! $OPTARG =~ ^[0-9]+$ || $OPTARG < 1 ]]
                then
                    echo "Wrong -j argument: $OPTARG"
                    exit -1
                fi
                NWORKERS="$OPTARG"
                ;;

            T )
                if [[ ! $OPTARG =~ ^[0-9]+$ ]]
                then
                    echo "Wrong -T argument: $OPTARG"
                    exit -1
                fi
                TEST_TIMEOUT="$OPTARG"
                ;;

            x )
                XB_EXTRA_MY_CNF_OPTS="$OPTARG"
                ;;

            X )
                MYSQLD_EXTRA_MY_CNF_OPTS="$OPTARG"
                ;;

            r )
                if [ ! -d $OPTARG ]
                then
                    echo "Wrong -r argument: $OPTARG make sure that directory exists"
                    exit -1
                fi
                TEST_BASEDIR="$OPTARG"
                TEST_VAR_ROOT="$TEST_BASEDIR/var"
                ;;

            R ) export WITH_RR=1 ;;
            ? ) echo "Use \`$0 -h' for the list of available options."
                    exit -1;;
        esac
done

set_vars

if [ -n "$tname" ]
then
   tests="$tname"
else
   tests="suites/binlog/* suites/pagetracking/* suites/gr/*.sh suites/keyring/*.sh suites/xbcloud/*.sh suites/compression/*.sh suites/rocksdb/*.sh t/*.sh"
   suites=" pagetracking binlog gr keyring compression rocksdb main xbcloud"
fi

export OUTFILE="$PWD/results/setup"

rm -rf results
mkdir results

cleanup_all_workers >>$OUTFILE 2>&1

echo "Using $TEST_VAR_ROOT as test root"
rm -rf $TEST_VAR_ROOT test_results.subunit
mkdir -p $TEST_VAR_ROOT

echo "Detecting server version..." | tee -a $OUTFILE

if ! get_version_info
then
    echo "get_version_info failed. See $OUTFILE for details."
    exit -1
fi

config_asan

if is_server_version_higher_than 8.0.23
then
  config_local_components
fi

if test -d $PWD/../../../../plugin_output_directory
then
  plugin_dir=$PWD/../../../../plugin_output_directory
else
  plugin_dir=$PWD/../../lib/plugin/
fi

echo "Running against $MYSQL_FLAVOR $MYSQL_VERSION ($INNODB_FLAVOR $INNODB_VERSION)" |
  tee -a $OUTFILE

echo "Using '`basename $XB_BIN`' as xtrabackup binary" | tee -a $OUTFILE

[ -z "$NWORKERS" ] && autocalc_nworkers

if [ "$NWORKERS" -gt 1 ]
then
    echo "Using $NWORKERS parallel workers" | tee -a $OUTFILE
fi

if [ -n  "$suites" ];
then
  echo "Running suite(s):${suites}"
  NOFSUITES=$(echo ${suites}| tr -cd ' \t' | wc -c)
fi
echo | tee -a $OUTFILE

cat <<EOF
==============================================================================
TEST                                   WORKER    RESULT     TIME(s) or COMMENT
------------------------------------------------------------------------------
EOF

for t in $tests
do
   # Check if we have available workers
   found=""

   while [ -z "$found" ]
   do
       for ((i = 1; i <= NWORKERS; i++))
       do
           if [ -z ${worker_pids[$i]:-""} ]
           then
               found="yes"
               break
           else
               # Check if it's alive
               if kill -0 ${worker_pids[$i]} >/dev/null 2>&1
               then
                   check_timeout_for_worker $i || print_status_and_exit
                   continue
               fi
               reap_worker $i || print_status_and_exit

               worker_pids[$i]=""
               found="yes"

               break
           fi
       done

       if [ -z "$found" ]
       then
           sleep 1
       fi
   done

   worker=$i

   TOTAL_COUNT=$((TOTAL_COUNT+1))

   name=`basename $t .sh`
   suite=`dirname $t | awk -F'/' '{print $NF}'`
   if [[ "${suite}" = "t" ]];
   then
     suite="main"
   fi
   if [[ $NOFSUITES -ge 1 ]];
   then
     name="${suite}.${name}"
   fi
   worker_names[$worker]=$t
   worker_outfiles[$worker]="$PWD/results/$name"
   worker_skip_files[$worker]="$PWD/results/$name.skipped"
   worker_status_files[$worker]="$PWD/results/$name.status"
   # Create a unique TMPDIR for each worker so that it can be removed as a part
   # of the cleanup procedure. Server socket files will also be created there.
   worker_tmpdirs[$worker]="`mktemp -d -t xbtemp.XXXXXX`"

   (
       set -eu
       if [ -n "$DEBUG" ]
       then
           set -x
       fi

       trap "cleanup_on_test_exit" EXIT

       . inc/common.sh

       export OUTFILE=${worker_outfiles[$worker]}
       export SKIPPED_REASON=${worker_skip_files[$worker]}
       export TEST_VAR_ROOT=$TEST_BASEDIR/var/w$worker
       export TMPDIR=${worker_tmpdirs[$worker]}
       export STATUS_FILE=${worker_status_files[$worker]}

       mkdir $TEST_VAR_ROOT

       # Execute the test in a subshell. This is required to catch syntax
       # errors, as otherwise $? would be 0 in cleanup_on_test_exit resulting in
       # passed test
       (. $t) || exit $?
   ) > ${worker_outfiles[$worker]} 2>&1 &

   worker_pids[$worker]=$!
   worker_stime[$worker]="`now`"
   # Used in subunit reports
   worker_stime_txt[$worker]="`date -u '+%Y-%m-%d %H:%M:%S'`"
done

# Wait for in-progress workers to finish
reap_all_workers

print_status_and_exit

