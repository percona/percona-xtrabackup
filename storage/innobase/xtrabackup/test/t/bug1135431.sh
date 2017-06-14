#
# Bug 1135431: Check privileges on backup start
#

start_server

readonly priv_file=$topdir/missing_privileges.txt
readonly user="test_user"

########################################################################
# Check privileges verification output
# Arguments:
#   case description string
#   FAILURE|SUCCESS - wheter pxb should fail or not
#   any extra arguments are passed to the PXB.
# Give list of permissions to check via stdin.
########################################################################
function check_privileges()
{
    local case_name="$1"
    local type="$2"
    shift 2
    if [ $type == "FAILURE" ]
    then
        local run_command='run_cmd_expect_failure'
    elif [ $type == "SUCCESS" ]
    then
        local run_command='run_cmd'
    fi
    vlog ""
    vlog "#############################################################"
    vlog "Case '$case_name' expecting $type, run with extra args: '$*'"
    vlog "#############################################################"

    $run_command ${XB_BIN} ${XB_ARGS} \
        --check-privileges \
        --backup \
        --datadir=$mysql_datadir \
        --target-dir=$topdir/backup \
        -u $user $* 2>&1 \
        | tee $topdir/pxb.log
    grep "missing required privilege" $topdir/pxb.log > $priv_file || echo "No required privileges detected"

    # Don't compare required and actual privileges directly, since it is
    # hard to find list that would match exactly on all platforms and configurations.
    while IFS= read -r line
    do
        run_cmd grep -qFi "$line" "$priv_file"
    done

    vlog "#############################################################"
}

mysql -e "CREATE USER $user;"
vlog "Verifying that xtrabackup fails when user has no required privileges."

check_privileges "PERCONA_SCHEMA db missing" FAILURE \
<<EOF
CREATE on *.*
LOCK TABLES on *.*
RELOAD on *.*
SELECT on INFORMATION_SCHEMA.PLUGINS
SHOW DATABASES on *.*
EOF

run_cmd mysql \
<<EOF
CREATE DATABASE PERCONA_SCHEMA;
GRANT SHOW DATABASES on *.* to $user@'localhost';
FLUSH PRIVILEGES;
EOF

check_privileges "PERCONA_SCHEMA.xtrabackup_history missing" FAILURE \
<<EOF
CREATE on PERCONA_SCHEMA.*
LOCK TABLES on *.*
RELOAD on *.*
SELECT on INFORMATION_SCHEMA.PLUGINS
EOF

# xtrabackup does not check validity of table, it is Ok to mock it.
run_cmd mysql \
<<EOF
GRANT CREATE on PERCONA_SCHEMA.* to $user@'localhost';
FLUSH PRIVILEGES;
CREATE TABLE IF NOT EXISTS PERCONA_SCHEMA.xtrabackup_history(id int);
EOF

check_privileges "PERCONA_SCHEMA.xtrabackup_history present" FAILURE \
<<EOF
LOCK TABLES on *.*
RELOAD on *.*
SELECT on INFORMATION_SCHEMA.PLUGINS
EOF

run_cmd mysql \
<<EOF
DROP DATABASE PERCONA_SCHEMA;
REVOKE CREATE ON PERCONA_SCHEMA.* FROM $user@'localhost';
EOF

check_privileges "Simple" FAILURE --no-lock \
<<EOF
CREATE on *.*
LOCK TABLES on *.*
RELOAD on *.*
SELECT on INFORMATION_SCHEMA.PLUGINS
EOF

check_privileges "Simple" FAILURE --safe-slave-backup \
<<EOF
CREATE on *.*
LOCK TABLES on *.*
RELOAD on *.*
SELECT on INFORMATION_SCHEMA.PLUGINS
SUPER on *.*
EOF

if is_galera
then
    check_privileges "Galera" FAILURE --galera-info \
<<EOF
CREATE on *.*
LOCK TABLES on *.*
RELOAD on *.*
REPLICATION CLIENT on *.*
SELECT on INFORMATION_SCHEMA.PLUGINS
EOF
fi


vlog "Granting some privileges to the user."
run_cmd mysql \
<<EOF
GRANT SELECT, CREATE, RELOAD, LOCK TABLES, REPLICATION CLIENT on *.* to $user@'localhost';
FLUSH PRIVILEGES;
EOF

check_privileges "Power user" SUCCESS \
    --slave-info \
    --safe-slave-backup \
    --no-lock \
<<EOF
SUPER on *.*
EOF

rm -rf $topdir/backup

vlog "Granting super privilege to the user."
run_cmd mysql \
<<EOF
GRANT SUPER on *.* to $user@'localhost';
FLUSH PRIVILEGES;
EOF

check_privileges "Super user" SUCCESS \
    --slave-info \
    --safe-slave-backup \
    --no-lock
