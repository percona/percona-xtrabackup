set -eu

function innobackupex()
{
    run_cmd $IB_BIN $IB_ARGS "$@"
}

function xtrabackup()
{
    run_cmd $XB_BIN $XB_ARGS "$@"
}

function vlog
{
    echo "`date +"%F %T"`: `basename "$0"`: $@" >&2
}


########################################################################
# Remove server var* directories from the worker's var root
########################################################################
function remove_var_dirs()
{
    rm -rf ${TEST_VAR_ROOT}/var[0-9]
}

function die()
{
  vlog "$*" >&2
  exit 1
}

function call_mysql_install_db()
{
        vlog "Calling mysql_install_db"

        cd $MYSQL_BASEDIR

        if ! $MYSQL_INSTALL_DB --defaults-file=${MYSQLD_VARDIR}/my.cnf \
            --basedir=${MYSQL_BASEDIR} \
            ${MYSQLD_EXTRA_ARGS}
        then
            vlog "mysql_install_db failed. Server log (if exists):"
            vlog "----------------"
            cat ${MYSQLD_ERRFILE} >&2 || true
            vlog "----------------"
            exit -1
        fi

        cd - >/dev/null 2>&1
}

########################################################################
# Checks whether MySQL with the PID specified as an argument is alive
########################################################################
function mysql_ping()
{
    local pid=$1
    local attempts=60
    local i

    for ((i=1; i<=attempts; i++))
    do
        $MYSQLADMIN $MYSQL_ARGS ping >/dev/null 2>&1 && return 0
        sleep 1
        # Is the server process still alive?
        if ! kill -0 $pid >/dev/null 2>&1
        then
            vlog "Server process PID=$pid died."
            wait $pid
            return 1
        fi
        vlog "Made $i attempts to connect to server"
    done

    return 1
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

########################################################################
# Choose a free port for a MySQL server instance
########################################################################
function get_free_port()
{
    local id=$1
    local port
    local lockfile

    for (( port=3307 + RANDOM; port < 65535; port++))
    do
	lockfile="/tmp/xtrabackup_port_lock.$port"
	# Try to atomically lock the current port number
	if ! (set -C; > $lockfile)
	then
	    continue;
	fi
	echo $$ > $lockfile
	if ! nc -z -w1 localhost $port >/dev/null 2>&1
	then
	    echo $port
	    return 0
	fi
	rm -f $lockfile
    done

    die "Could not find a free port for server id $id!"
}

function free_reserved_port()
{
   local port=$1

   rm -f /tmp/xtrabackup_port_lock.${port}
}


########################################################################
# Initialize server variables such as datadir, tmpdir, etc. and store
# them with the specified index in SRV_MYSQLD_* arrays to be used by
# switch_server() later
########################################################################
function init_server_variables()
{
    local id=$1

    if [ -n ${SRV_MYSQLD_IDS[$id]:-""} ]
    then
	die "Server with id $id has already been started"
    fi

    SRV_MYSQLD_IDS[$id]="$id"
    local vardir="${TEST_VAR_ROOT}/var${id}"
    SRV_MYSQLD_VARDIR[$id]="$vardir"
    SRV_MYSQLD_DATADIR[$id]="$vardir/data"
    SRV_MYSQLD_TMPDIR[$id]="$vardir/tmp"
    SRV_MYSQLD_PIDFILE[$id]="${TEST_VAR_ROOT}/mysqld${id}.pid"
    SRV_MYSQLD_ERRFILE[$id]="$vardir/data/mysqld${id}.err"
    SRV_MYSQLD_PORT[$id]=`get_free_port $id`
    SRV_MYSQLD_SOCKET[$id]=`mktemp -t mysql.sock.XXXXXX`
}

########################################################################
# Reset server variables
########################################################################
function reset_server_variables()
{
    local id=$1

    if [ -z ${SRV_MYSQLD_VARDIR[$id]:-""} ]
    then
	# Variables have already been reset
	return 0;
    fi

    SRV_MYSQLD_IDS[$id]=
    SRV_MYSQLD_VARDIR[$id]=
    SRV_MYSQLD_DATADIR[$id]=
    SRV_MYSQLD_TMPDIR[$id]=
    SRV_MYSQLD_PIDFILE[$id]=
    SRV_MYSQLD_ERRFILE[$id]=
    SRV_MYSQLD_PORT[$id]=
    SRV_MYSQLD_SOCKET[$id]=
}

##########################################################################
# Change the environment to make all utilities access the server with an
# id specified in the argument.
##########################################################################
function switch_server()
{
    local id=$1

    MYSQLD_VARDIR="${SRV_MYSQLD_VARDIR[$id]}"
    MYSQLD_DATADIR="${SRV_MYSQLD_DATADIR[$id]}"
    MYSQLD_TMPDIR="${SRV_MYSQLD_TMPDIR[$id]}"
    MYSQLD_PIDFILE="${SRV_MYSQLD_PIDFILE[$id]}"
    MYSQLD_ERRFILE="${SRV_MYSQLD_ERRFILE[$id]}"
    MYSQLD_PORT="${SRV_MYSQLD_PORT[$id]}"
    MYSQLD_SOCKET="${SRV_MYSQLD_SOCKET[$id]}"

    MYSQL_ARGS="--defaults-file=$MYSQLD_VARDIR/my.cnf "
    MYSQLD_ARGS="--defaults-file=$MYSQLD_VARDIR/my.cnf ${MYSQLD_EXTRA_ARGS}"
    if [ "`whoami`" = "root" ]
    then
	MYSQLD_ARGS="$MYSQLD_ARGS --user=root"
    fi

    IB_ARGS="--defaults-file=$MYSQLD_VARDIR/my.cnf --ibbackup=$XB_BIN \
--no-version-check ${IB_EXTRA_OPTS:-}"
    XB_ARGS="--defaults-file=$MYSQLD_VARDIR/my.cnf"

    # Some aliases for compatibility, as tests use the following names
    topdir="$MYSQLD_VARDIR"
    mysql_datadir="$MYSQLD_DATADIR"
    mysql_tmpdir="$MYSQLD_TMPDIR"
    mysql_socket="$MYSQLD_SOCKET"
}

########################################################################
# Start server with the id specified as the first argument
########################################################################
function start_server_with_id()
{
    local id=$1
    local attempts=0
    local max_attempts=5
    shift

    vlog "Starting server with id=$id..."

    while true
    do
        init_server_variables $id
        switch_server $id

        if [ ! -d "$MYSQLD_VARDIR" ]
        then
	    vlog "Creating server root directory: $MYSQLD_VARDIR"
	    mkdir "$MYSQLD_VARDIR"
        fi
        if [ ! -d "$MYSQLD_TMPDIR" ]
        then
	    vlog "Creating server temporary directory: $MYSQLD_TMPDIR"
	    mkdir "$MYSQLD_TMPDIR"
        fi

        if [ -f "$MYSQLD_ERRFILE" ]
        then
            rm "$MYSQLD_ERRFILE"
        fi

        # Create the configuration file used by mysql_install_db, the server
        # and the xtrabackup binary
        cat > ${MYSQLD_VARDIR}/my.cnf <<EOF
[mysqld]
socket=${MYSQLD_SOCKET}
port=${MYSQLD_PORT}
server-id=$id
basedir=${MYSQL_BASEDIR}
datadir=${MYSQLD_DATADIR}
tmpdir=${MYSQLD_TMPDIR}
log-error=${MYSQLD_ERRFILE}
log-bin=mysql-bin
relay-log=mysql-relay-bin
pid-file=${MYSQLD_PIDFILE}
replicate-ignore-db=mysql
innodb_log_file_size=48M
${MYSQLD_EXTRA_MY_CNF_OPTS:-}

[client]
socket=${MYSQLD_SOCKET}
user=root

[xtrabackup]
${XB_EXTRA_MY_CNF_OPTS:-}
EOF

        # Create datadir and call mysql_install_db if it doesn't exist
        if [ ! -d "$MYSQLD_DATADIR" ]
        then
            vlog "Creating server data directory: $MYSQLD_DATADIR"
            mkdir -p "$MYSQLD_DATADIR"
	    # Reserve 900 series for SST nodes
            if [[ $id -lt 900 ]];then
                call_mysql_install_db
            else 
                vlog "Skiping mysql_install_db of node $id for SST"
            fi
        fi

        # Start the server
        echo "Starting ${MYSQLD} ${MYSQLD_ARGS} $* "
        ${MYSQLD} ${MYSQLD_ARGS} $* &
        if ! mysql_ping $!
        then
            if grep "another mysqld server running on port" $MYSQLD_ERRFILE
            then
                if ((++attempts < max_attempts))
                then
                    vlog "Made $attempts attempts to find a free port"
                    reset_server_variables $id
                    free_reserved_port $MYSQLD_PORT
                    continue
                else
                    vlog "Failed to find a free port after $attempts attempts"
                fi
            fi
            vlog "Can't start the server. Server log (if exists):"
            vlog "----------------"
            cat ${MYSQLD_ERRFILE} >&2 || true
            vlog "----------------"
            exit -1
        else
            break
        fi
    done

     vlog "Server with id=$id has been started on port $MYSQLD_PORT, \
socket $MYSQLD_SOCKET"
}

########################################################################
# Stop server with the id specified as the first argument.  The server 
# is stopped in the fastest possible way.
########################################################################
function stop_server_with_id()
{
    local id=$1
    switch_server $id

    vlog "Killing server with id=$id..."

    if [ -f "${MYSQLD_PIDFILE}" ]
    then
        local pid=`cat $MYSQLD_PIDFILE`
        kill -9 $pid
        wait $pid || true
        rm -f ${MYSQLD_PIDFILE}
    else
        vlog "Server PID file '${MYSQLD_PIDFILE}' doesn't exist!"
    fi

    # Reset XB_ARGS so we can call xtrabackup in tests even without starting the
    # server
    XB_ARGS="--no-defaults"

    # unlock the port number
    free_reserved_port $MYSQLD_PORT

    reset_server_variables $id
}

########################################################################
# Start server with id=1 and additional command line arguments
# (if specified)
########################################################################
function start_server()
{
    start_server_with_id 1 $*
}

########################################################################
# Stop server with id=1
########################################################################
function stop_server()
{
    stop_server_with_id 1
}

########################################################################
# Shutdown cleanly server specified with the first argument
########################################################################
function shutdown_server_with_id()
{
    local id=$1
    switch_server $id

    vlog "Shutting down server with id=$id..."

    if [ -f "${MYSQLD_PIDFILE}" ]
    then
        ${MYSQLADMIN} ${MYSQL_ARGS} shutdown
    else
        vlog "Server PID file '${MYSQLD_PIDFILE}' doesn't exist!"
    fi

    # unlock the port number
    free_reserved_port $MYSQLD_PORT

    reset_server_variables $id
}

########################################################################
# Shutdown server with id=1 cleanly
########################################################################
function shutdown_server()
{
    shutdown_server_with_id 1
}

########################################################################
# Force a checkpoint for a server specified with the first argument
########################################################################
function force_checkpoint_with_server_id()
{
    local id=$1
    shift

    switch_server $id

    vlog "Forcing a checkpoint for server #$id"

    shutdown_server_with_id $id
    start_server_with_id $id $*
}

########################################################################
# Force a checkpoint for server id=1
########################################################################
function force_checkpoint()
{
    force_checkpoint_with_server_id 1 $*
}

########################################################################
# Configure a specified server as a slave
# Synopsis:
#   setup_slave <slave_id> <master_id>
#########################################################################
function setup_slave()
{
    local slave_id=$1
    local master_id=$2

    vlog "Setting up server #$slave_id as a slave of server #$master_id"

    switch_server $slave_id

    run_cmd $MYSQL $MYSQL_ARGS <<EOF
CHANGE MASTER TO
  MASTER_HOST='localhost',
  MASTER_USER='root',
  MASTER_PORT=${SRV_MYSQLD_PORT[$master_id]};

START SLAVE
EOF
}

########################################################################
# Wait until slave catches up with master.
# The timeout is hardcoded to 300 seconds
#
# Synopsis:
#   sync_slave_with_master <slave_id> <master_id>
#########################################################################
function sync_slave_with_master()
{
    local slave_id=$1
    local master_id=$2
    local count
    local master_file
    local master_pos

    vlog "Syncing slave (id=#$slave_id) with master (id=#$master_id)"

    # Get master log pos
    switch_server $master_id
    count=0
    while read line; do
      	if [ $count -eq 1 ] # File:
      	then
      	    master_file=`echo "$line" | sed s/File://`
      	elif [ $count -eq 2 ] # Position:
      	then
      	    master_pos=`echo "$line" | sed s/Position://`
      	fi
      	count=$((count+1))
    done <<< "`run_cmd $MYSQL $MYSQL_ARGS -Nse 'SHOW MASTER STATUS\G' mysql`"

    echo "master_file=$master_file, master_pos=$master_pos"

    # Wait for the slave SQL thread to catch up
    switch_server $slave_id

    run_cmd $MYSQL $MYSQL_ARGS <<EOF
SELECT MASTER_POS_WAIT('$master_file', $master_pos, 300);
EOF
}

########################################################################
# Prints checksum for a given table.
# Expects 2 arguments: $1 is the DB name and $2 is the table to checksum
########################################################################
function checksum_table()
{
    $MYSQL $MYSQL_ARGS -Ns -e "CHECKSUM TABLE $2 EXTENDED" $1 | awk {'print $2'}
}

##########################################################################
# Dumps a given database using mysqldump                                 #
##########################################################################
function record_db_state()
{
    $MYSQLDUMP $MYSQL_ARGS -t --compact --skip-extended-insert \
        $1 >"$topdir/tmp/$1_old.sql"
}


##########################################################################
# Compares the current dump of a given database with a state previously  #
# captured with record_db_state().					 #
##########################################################################
function verify_db_state()
{
    $MYSQLDUMP $MYSQL_ARGS -t --compact --skip-extended-insert \
        $1 >"$topdir/tmp/$1_new.sql"
    diff -u "$topdir/tmp/$1_old.sql" "$topdir/tmp/$1_new.sql"
}

########################################################################
# Workarounds for a bug in grep 2.10 when grep -q file > file would
# result in a failure.
########################################################################
function grep()
{
    command grep "$@" | cat
    return ${PIPESTATUS[0]}
}

function egrep()
{
    command egrep "$@" | cat
    return ${PIPESTATUS[0]}
}

####################################################
# Helper functions for testing incremental backups #
####################################################
function check_full_scan_inc_backup()
{
    local xb_performed_bmp_inc_backup="xtrabackup: using the full scan for incremental backup"
    local xb_performed_full_scan_inc_backup="xtrabackup: using the changed page bitmap"

    if ! grep -q "$xb_performed_bmp_inc_backup" $OUTFILE ;
    then
        vlog "xtrabackup did not perform a full scan for the incremental backup."
        exit -1
    fi
    if grep -q "$xb_performed_full_scan_inc_backup" $OUTFILE ;
    then
        vlog "xtrabackup appeared to use bitmaps instead of full scan for the incremental backup."
        exit -1
    fi
}

function check_bitmap_inc_backup()
{
    local xb_performed_bmp_inc_backup="xtrabackup: using the full scan for incremental backup"
    local xb_performed_full_scan_inc_backup="xtrabackup: using the changed page bitmap"

    if ! grep -q "$xb_performed_full_scan_inc_backup" $OUTFILE ;
    then
        vlog "xtrabackup did not use bitmaps for the incremental backup."
        exit -1
    fi
    if grep -q "$xb_performed_bmp_inc_backup" $OUTFILE ;
    then
        vlog "xtrabackup used a full scan instead of bitmaps for the incremental backup."
        exit -1
    fi
}

##############################################################
# Helper functions for xtrabackup process suspend and resume #
##############################################################
function wait_for_xb_to_suspend()
{
    local file=$1
    local i=0
    echo "Waiting for $file to be created"
    while [ ! -r $file ]
    do
        sleep 1
        i=$((i+1))
        echo "Waited $i seconds for xtrabackup_suspended to be created"
    done
}

function resume_suspended_xb()
{
    local file=$1
    echo "Removing $file"
    rm -f $file
}

########################################################################
# Skip the current test with a given comment
########################################################################
function skip_test()
{
    echo $1 > $SKIPPED_REASON
    exit $SKIPPED_EXIT_CODE
}

########################################################################
# Get version string in the XXYYZZ version
########################################################################
function get_version_str()
{
    printf %02d%02d%02d $1 $2 $3
}

#########################################################################
# Return 0 if the server version is higher than the first argument
#########################################################################
function is_server_version_higher_than()
{
    [[ $1 =~ ^([0-9]+)\.([0-9]+)\.([0-9]+)$ ]] || \
        die "Cannot parse server version: '$1'"

    local major=${BASH_REMATCH[1]}
    local minor=${BASH_REMATCH[2]}
    local patch=${BASH_REMATCH[3]}

    local server_str=`get_version_str $MYSQL_VERSION_MAJOR \
$MYSQL_VERSION_MINOR $MYSQL_VERSION_PATCH`
    local version_str=`get_version_str $major $minor $patch`

    [[ $server_str > $version_str ]]
}

#########################################################################
# Require a server version higher than the first argument
########################################################################
function require_server_version_higher_than()
{
    is_server_version_higher_than $1 || \
        skip_test "Requires server version higher than $1"
}

########################################################################
# Return 0 if the server version is lower than the first argument
#########################################################################
function is_server_version_lower_than()
{
    [[ $1 =~ ^([0-9]+)\.([0-9]+)\.([0-9]+)$ ]] || \
        die "Cannot parse server version: '$1'"

    local major=${BASH_REMATCH[1]}
    local minor=${BASH_REMATCH[2]}
    local patch=${BASH_REMATCH[3]}

    local server_str=`get_version_str $MYSQL_VERSION_MAJOR \
$MYSQL_VERSION_MINOR $MYSQL_VERSION_PATCH`
    local version_str=`get_version_str $major $minor $patch`

    [[ $server_str < $version_str ]]
}

#########################################################################
# Require a server version lower than the first argument
########################################################################
function require_server_version_lower_than()
{
    is_server_version_lower_than $1 || \
        skip_test "Requires server version lower than $1"
}

########################################################################
# Return 0 if the server is XtraDB-based
########################################################################
function is_xtradb()
{
    [ -n "$XTRADB_VERSION" ]
}

#########################################################################
# Skip the test if not running against XtraDB
########################################################################
function require_xtradb()
{
    is_xtradb || skip_test "Requires XtraDB"
}

########################################################################
# Return 0 if the server has Galera support
########################################################################
function is_galera()
{
    [ -n "$WSREP_READY" ]
}

########################################################################
# Skip the test if not running against a Galera-enabled server
########################################################################
function require_galera()
{
    is_galera || skip_test "Requires Galera support"
}

########################################################################
# Skip the test if qpress binary is not available
########################################################################
function require_qpress()
{
    if ! which qpress > /dev/null 2>&1 ; then
        skip_test "Requires qpress to be installed"
    fi
}

##############################################################################
# Execute a multi-row INSERT into a specified table.
#
# Arguments:
#
#   $1 -- table specification
#
#   all subsequent arguments represent tuples to insert in the form:
#   (value1, ..., valueN)
#
# Notes:
#
#   1. Bash special characters in the arguments must be quoted to screen them
#      from interpreting by Bash, i.e. \(1,...,\'a'\)
#
#   2. you can use Bash brace expansion to generate multiple tuples, e.g.:
#      \({1..1000},\'a'\) will generate 1000 tuples (1,'a'), ..., (1000, 'a')
##############################################################################
function multi_row_insert()
{
    local table=$1
    shift

    vlog "Inserting $# rows into $table..."
    (IFS=,; echo "INSERT INTO $table VALUES $*") | \
        $MYSQL $MYSQL_ARGS
    vlog "Done."
}

########################################################################
# Return 0 if the server has backup locks support
########################################################################
function has_backup_locks()
{
    if $MYSQL $MYSQL_ARGS -s -e "SHOW VARIABLES LIKE 'have_backup_locks'\G" \
              2> /dev/null | egrep -q "Value: YES$"
    then
        return 0
    fi

    # Was the server available?
    if [[ ${PIPESTATUS[0]} != 0 ]]
    then
        die "Server is unavailable"
    fi

    return 1
}

########################################################################
# Return 0 if the platform is 64-bit
########################################################################
function is_64bit()
{
    uname -m 2>&1 | grep 'x86_64'
}

# To avoid unbound variable error when no server have been started
SRV_MYSQLD_IDS=
