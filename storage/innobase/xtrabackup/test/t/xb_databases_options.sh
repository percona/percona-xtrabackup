########################################################################
# Bug #569387: xtrabackup ignores --databases, i.e. when --databases
#
# Test following xtrabackup options
# --databases
# --databases-exclude
# --databases-file
# --tables
# --tables-file
# --tables-exclude
########################################################################

. inc/common.sh

start_server --innodb-file-per-table

function setup_test {

	local databases="database1 database2 test1 test2 thisisadatabase testdatabase"
	local tables="thisisatable t t1 t2 ancor glow"

	NEWLINE=$(printf '\n')
	for db in $databases
	do
		cmd="create database $db;$NEWLINE$NEWLINE"
		for table in $tables
		do
			cmd+="create table $db.$table (a int primary key) engine=InnoDB;$NEWLINE"
		done
		echo "$cmd" | run_cmd $MYSQL $MYSQL_ARGS
	done
}

function ls_dir {
	( cd $1 ; find . -name '*.ibd' -type f | sort -d )
}

function expect_dir_contents {
	local contents_diff
	contents_diff=`diff -U0 --label "expected" <(cat /dev/stdin) --label "actual" <(ls_dir "$1")`
	if [ $? -ne 0 ]; then
		echo "Actual contents of $1 does not match expected, diff: $contents_diff"
		exit -1
	fi
}

setup_test

very_long_name=verylonglonglongname111111skjhkdjhfkjdhgkjdfh1555555555555511stillnotlongenoughsowilladdmore1111111111111111111114848484848484fkjhdjfhkdjfhd8484848aaaaaaaaaancnvjvifmifjhfkmfkfnbfifnfkfik4848484841111111prettyenoughnow
backup_dir=$topdir/backup_dir
ignored_dir=to_be_ignored_on_backup
ignored_dir_full_path=$mysql_datadir/$ignored_dir

mkdir -p $backup_dir

# invalid characters
xtrabackup --backup --databases=a/b/c --target-dir=$backup_dir --datadir=$mysql_datadir 2>&1 | grep 'is not valid'

# too long name
xtrabackup --backup --databases=$very_long_name --target-dir=$backup_dir --datadir=$mysql_datadir 2>&1 | grep 'is too long'

# too long name
xtrabackup --backup --databases=test1.$very_long_name --target-dir=$backup_dir --datadir=$mysql_datadir 2>&1 | grep 'is too long'

# too long name
xtrabackup --backup --databases-exclude=$very_long_name --target-dir=$backup_dir --datadir=$mysql_datadir 2>&1 | grep 'is too long'

# too long name
xtrabackup --backup --databases-exclude=test1.$very_long_name --target-dir=$backup_dir --datadir=$mysql_datadir 2>&1 | grep 'is too long'

# not fully qualified name
xtrabackup --backup --tables-file=<(echo $very_long_name) --target-dir=$backup_dir --datadir=$mysql_datadir 2>&1 | grep 'is not fully qualified'

# again too long name
xtrabackup --backup --tables-file=<(echo test1.$very_long_name) --target-dir=$backup_dir --datadir=$mysql_datadir 2>&1 | grep 'is too long'

# should go fine, we cannot validate regex against length
xtrabackup --backup --tables=$very_long_name --target-dir=$backup_dir --datadir=$mysql_datadir
rm -rf $backup_dir/*

# should go fine, we cannot validate regex against length
xtrabackup --backup --tables=test1.$very_long_name --target-dir=$backup_dir --datadir=$mysql_datadir
rm -rf $backup_dir/*

vlog "Testing with --databases=..."
xtrabackup --backup \
	--databases='database database2 test1.t test1.ancor thisisadatabase testdatabase.t testdatabase.t7' \
	--target-dir=$backup_dir --datadir=$mysql_datadir

expect_dir_contents $backup_dir <<EOF
./database2/ancor.ibd
./database2/glow.ibd
./database2/t1.ibd
./database2/t2.ibd
./database2/thisisatable.ibd
./database2/t.ibd
./test1/ancor.ibd
./test1/t.ibd
./testdatabase/t.ibd
./thisisadatabase/ancor.ibd
./thisisadatabase/glow.ibd
./thisisadatabase/t1.ibd
./thisisadatabase/t2.ibd
./thisisadatabase/thisisatable.ibd
./thisisadatabase/t.ibd
EOF
rm -rf $backup_dir/*

vlog "Testing with --databases-file=..."
cat >$topdir/list <<EOF
database
database2
test1.t
test1.ancor
thisisadatabase
testdatabase.t
testdatabase.t7
EOF
xtrabackup --backup \
	--databases-file=$topdir/list \
	--target-dir=$backup_dir --datadir=$mysql_datadir
expect_dir_contents $backup_dir <<EOF
./database2/ancor.ibd
./database2/glow.ibd
./database2/t1.ibd
./database2/t2.ibd
./database2/thisisatable.ibd
./database2/t.ibd
./test1/ancor.ibd
./test1/t.ibd
./testdatabase/t.ibd
./thisisadatabase/ancor.ibd
./thisisadatabase/glow.ibd
./thisisadatabase/t1.ibd
./thisisadatabase/t2.ibd
./thisisadatabase/thisisatable.ibd
./thisisadatabase/t.ibd
EOF
rm -rf $backup_dir/*

vlog "Testing failure with --tables-file=..."
cat >$topdir/list <<EOF
database
database2
test1.t
test1.ancor
thisisadatabase
testdatabase.t
testdatabase.t7
EOF
run_cmd_expect_failure $XB_BIN $XB_ARGS --backup \
	--tables-file=$topdir/list \
	--target-dir=$backup_dir --datadir=$mysql_datadir
expect_dir_contents $backup_dir <<EOF
EOF
rm -rf $backup_dir/*

vlog "Testing with --tables-file=..."
cat >$topdir/list <<EOF
test1.t
test1.ancor
testdatabase.t
testdatabase.t7
EOF
xtrabackup --backup \
	--tables-file=$topdir/list \
	--target-dir=$backup_dir --datadir=$mysql_datadir
expect_dir_contents $backup_dir <<EOF
./test1/ancor.ibd
./test1/t.ibd
./testdatabase/t.ibd
EOF
rm -rf $backup_dir/*

vlog "Testing with --tables=..."
xtrabackup --backup \
	--tables='dat.base,test1.t,test1.a' \
	--target-dir=$backup_dir --datadir=$mysql_datadir
expect_dir_contents $backup_dir <<EOF
./database1/ancor.ibd
./database1/glow.ibd
./database1/t1.ibd
./database1/t2.ibd
./database1/thisisatable.ibd
./database1/t.ibd
./database2/ancor.ibd
./database2/glow.ibd
./database2/t1.ibd
./database2/t2.ibd
./database2/thisisatable.ibd
./database2/t.ibd
./test1/ancor.ibd
./test1/t1.ibd
./test1/t2.ibd
./test1/thisisatable.ibd
./test1/t.ibd
./testdatabase/ancor.ibd
./testdatabase/glow.ibd
./testdatabase/t1.ibd
./testdatabase/t2.ibd
./testdatabase/thisisatable.ibd
./testdatabase/t.ibd
./thisisadatabase/ancor.ibd
./thisisadatabase/glow.ibd
./thisisadatabase/t1.ibd
./thisisadatabase/t2.ibd
./thisisadatabase/thisisatable.ibd
./thisisadatabase/t.ibd
EOF
rm -rf $backup_dir/*

vlog "Testing with --tables-exclude=..."
xtrabackup --backup \
	--tables-exclude='ancor,glow,test,thisisa.*,ignore,mysql.*,sys.*' \
	--target-dir=$backup_dir --datadir=$mysql_datadir
expect_dir_contents $backup_dir <<EOF
./database1/t1.ibd
./database1/t2.ibd
./database1/t.ibd
./database2/t1.ibd
./database2/t2.ibd
./database2/t.ibd
EOF
rm -rf $backup_dir/*

vlog "Testing with both --tables and --tables-exclude=..."
xtrabackup --backup \
	--tables='ancor,glow,test,thisisa.*,ignore,t.*,mysql.*' \
	--tables-exclude='ancor,glow,test,thisisa.*,ignore,mysql.*,sys.*' \
	--target-dir=$backup_dir --datadir=$mysql_datadir
expect_dir_contents $backup_dir <<EOF
./database1/t1.ibd
./database1/t2.ibd
./database1/t.ibd
./database2/t1.ibd
./database2/t2.ibd
./database2/t.ibd
EOF
rm -rf $backup_dir/*


# Bug #1272329: innobackupex dies if a directory is not readable. i.e.: lost+found
# https://bugs.launchpad.net/percona-xtrabackup/2.3/+bug/1272329
vlog "Adding unreadable directory"
mkdir $ignored_dir_full_path
chmod 000 $ignored_dir_full_path

vlog "Testing failure with ureadable directory"
run_cmd_expect_failure $XB_BIN $XB_ARGS --backup \
	--target-dir=$backup_dir --datadir=$mysql_datadir
rm -rf $backup_dir/*

vlog "Testing with --databases-exclude=..."
xtrabackup --backup \
	--databases-exclude=$ignored_dir \
	--target-dir=$backup_dir --datadir=$mysql_datadir
rm -rf $backup_dir/*

vlog "Testing with both --databases=... and --databases-exclude=..."
xtrabackup --backup \
	--databases="database database2 ignore_db $ignored_dir" \
	--databases-exclude="ignore_db $ignored_dir database2.ancor"\
	--target-dir=$backup_dir --datadir=$mysql_datadir

expect_dir_contents $backup_dir <<EOF
./database2/glow.ibd
./database2/t1.ibd
./database2/t2.ibd
./database2/thisisatable.ibd
./database2/t.ibd
EOF
rm -rf $backup_dir/*

vlog "Removing unreadable directory"
chmod 777 $ignored_dir_full_path
rm -rf $ignored_dir_full_path/*
