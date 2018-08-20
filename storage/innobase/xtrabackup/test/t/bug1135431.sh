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
        if [ -n "$line"]
        then
            run_cmd grep -qFi "$line" "$priv_file"
        fi
    done

    vlog "#############################################################"
}

user_string="$user@localhost"
if is_server_version_higher_than 5.7.0
then
    user_string="$user"
fi

vlog "Creating user."
mysql -e "CREATE USER $user;"
mysql \
<<EOF
GRANT PROCESS on *.* to $user_string;
FLUSH PRIVILEGES;
EOF

vlog "Verifying that xtrabackup fails when user has no required privileges."

default_privileges="
SELECT on INFORMATION_SCHEMA.PLUGINS
SHOW DATABASES on *.*
"

if has_feature_enabled "have_backup_locks"
then
    default_privileges+="
LOCK TABLES on *.*
REPLICATION CLIENT on *.*
"
fi

if is_server_version_higher_than 5.5.0
then
    default_privileges+="
RELOAD on *.*
"
fi

check_privileges "PERCONA_SCHEMA db missing" FAILURE \
<<EOF
$default_privileges
CREATE on *.*
EOF

run_cmd mysql \
<<EOF
CREATE DATABASE PERCONA_SCHEMA;
GRANT SHOW DATABASES on *.* to $user_string;
FLUSH PRIVILEGES;
EOF

check_privileges "PERCONA_SCHEMA.xtrabackup_history missing" FAILURE \
<<EOF
$default_privileges
CREATE on PERCONA_SCHEMA.*
EOF

# xtrabackup does not check validity of table, it is Ok to mock it.
run_cmd mysql \
<<EOF
GRANT CREATE on PERCONA_SCHEMA.* to $user_string;
FLUSH PRIVILEGES;
CREATE TABLE IF NOT EXISTS PERCONA_SCHEMA.xtrabackup_history(id int);
EOF

check_privileges "PERCONA_SCHEMA.xtrabackup_history present" FAILURE \
<<EOF
$default_privileges
EOF

run_cmd mysql \
<<EOF
DROP DATABASE PERCONA_SCHEMA;
REVOKE CREATE ON PERCONA_SCHEMA.* FROM $user_string;
EOF

check_privileges "Simple" FAILURE --safe-slave-backup \
<<EOF
$default_privileges
CREATE on *.*
SUPER on *.*
EOF

if is_galera
then
    check_privileges "Galera" FAILURE --galera-info \
<<EOF
$default_privileges
CREATE on *.*
REPLICATION CLIENT on *.*
EOF
fi

vlog "Granting some privileges to the user."
run_cmd mysql \
<<EOF
GRANT SELECT, CREATE, RELOAD, LOCK TABLES, REPLICATION CLIENT on *.* to $user_string;
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
GRANT SUPER on *.* to $user_string;
FLUSH PRIVILEGES;
EOF

check_privileges "Super user" SUCCESS \
    --slave-info \
    --safe-slave-backup \
    --no-lock
