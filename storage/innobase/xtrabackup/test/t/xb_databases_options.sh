########################################################################
# Bug #569387: xtrabackup ignores --databases, i.e. when --databases
#
# Test following xtrabackup options
# --databases
# --databases-file
# --tables
# --tables-file
########################################################################

. inc/common.sh

start_server --innodb-file-per-table

function setup_test {

	cat <<EOF | run_cmd $MYSQL $MYSQL_ARGS

		create database database1;
		create database database2;
		create database test1;
		create database test2;
		create database thisisadatabase;
		create database testdatabase;

		create table database1.thisisatable (a int primary key) engine=InnoDB;
		create table database1.t (a int primary key) engine=InnoDB;
		create table database1.t1 (a int primary key) engine=InnoDB;
		create table database1.t2 (a int primary key) engine=InnoDB;
		create table database1.ancor (a int primary key) engine=InnoDB;
		create table database1.glow (a int primary key) engine=InnoDB;

		create table database2.thisisatable (a int primary key) engine=InnoDB;
		create table database2.t (a int primary key) engine=InnoDB;
		create table database2.t1 (a int primary key) engine=InnoDB;
		create table database2.t2 (a int primary key) engine=InnoDB;
		create table database2.ancor (a int primary key) engine=InnoDB;
		create table database2.glow (a int primary key) engine=InnoDB;

		create table test1.thisisatable (a int primary key) engine=InnoDB;
		create table test1.t (a int primary key) engine=InnoDB;
		create table test1.t1 (a int primary key) engine=InnoDB;
		create table test1.t2 (a int primary key) engine=InnoDB;
		create table test1.ancor (a int primary key) engine=InnoDB;
		create table test1.glow (a int primary key) engine=InnoDB;

		create table test2.thisisatable (a int primary key) engine=InnoDB;
		create table test2.t (a int primary key) engine=InnoDB;
		create table test2.t1 (a int primary key) engine=InnoDB;
		create table test2.t2 (a int primary key) engine=InnoDB;
		create table test2.ancor (a int primary key) engine=InnoDB;
		create table test2.glow (a int primary key) engine=InnoDB;

		create table thisisadatabase.thisisatable (a int primary key) engine=InnoDB;
		create table thisisadatabase.t (a int primary key) engine=InnoDB;
		create table thisisadatabase.t1 (a int primary key) engine=InnoDB;
		create table thisisadatabase.t2 (a int primary key) engine=InnoDB;
		create table thisisadatabase.ancor (a int primary key) engine=InnoDB;
		create table thisisadatabase.glow (a int primary key) engine=InnoDB;

		create table testdatabase.thisisatable (a int primary key) engine=InnoDB;
		create table testdatabase.t (a int primary key) engine=InnoDB;
		create table testdatabase.t1 (a int primary key) engine=InnoDB;
		create table testdatabase.t2 (a int primary key) engine=InnoDB;
		create table testdatabase.ancor (a int primary key) engine=InnoDB;
		create table testdatabase.glow (a int primary key) engine=InnoDB;

EOF

}

function ls_dir {
	( cd $1 ; find . -name '*.ibd' -type f | sort -d )
}

setup_test

backup_dir=$topdir/backup_dir
mkdir -p $backup_dir

# invalid characters
xtrabackup --backup --databases=a/b/c --target-dir=$backup_dir --datadir=$mysql_datadir 2>&1 | grep 'is not valid'

# too long name
xtrabackup --backup --databases=verylonglonglongname111111skjhkdjhfkjdhgkjdfh1555555555555511stillnotlongenoughsowilladdmore1111111111111111111114848484848484fkjhdjfhkdjfhd8484848aaaaaaaaaancnvjvifmifjhfkmfkfnbfifnfkfik4848484841111111prettyenoughnow --target-dir=$backup_dir --datadir=$mysql_datadir 2>&1 | grep 'is too long'

# too long name
xtrabackup --backup --databases=test1.verylonglonglongname111111skjhkdjhfkjdhgkjdfh1555555555555511stillnotlongenoughsowilladdmore1111111111111111111114848484848484fkjhdjfhkdjfhd8484848aaaaaaaaaancnvjvifmifjhfkmfkfnbfifnfkfik4848484841111111prettyenoughnow --target-dir=$backup_dir --datadir=$mysql_datadir 2>&1 | grep 'is too long'

# not fully qualified name
xtrabackup --backup --tables-file=<(echo verylonglonglongname111111skjhkdjhfkjdhgkjdfh1555555555555511stillnotlongenoughsowilladdmore1111111111111111111114848484848484fkjhdjfhkdjfhd8484848aaaaaaaaaancnvjvifmifjhfkmfkfnbfifnfkfik4848484841111111prettyenoughnow) --target-dir=$backup_dir --datadir=$mysql_datadir 2>&1 | grep 'is not fully qualified'

# again too long name
xtrabackup --backup --tables-file=<(echo test1.verylonglonglongname111111skjhkdjhfkjdhgkjdfh1555555555555511stillnotlongenoughsowilladdmore1111111111111111111114848484848484fkjhdjfhkdjfhd8484848aaaaaaaaaancnvjvifmifjhfkmfkfnbfifnfkfik4848484841111111prettyenoughnow) --target-dir=$backup_dir --datadir=$mysql_datadir 2>&1 | grep 'is too long'

# should go fine, we cannot validate regex against length
xtrabackup --backup --tables=verylonglonglongname111111skjhkdjhfkjdhgkjdfh1555555555555511stillnotlongenoughsowilladdmore1111111111111111111114848484848484fkjhdjfhkdjfhd8484848aaaaaaaaaancnvjvifmifjhfkmfkfnbfifnfkfik4848484841111111prettyenoughnow --target-dir=$backup_dir --datadir=$mysql_datadir
rm -rf $backup_dir/*

# should go fine, we cannot validate regex against length
xtrabackup --backup --tables=test1.verylonglonglongname111111skjhkdjhfkjdhgkjdfh1555555555555511stillnotlongenoughsowilladdmore1111111111111111111114848484848484fkjhdjfhkdjfhd8484848aaaaaaaaaancnvjvifmifjhfkmfkfnbfifnfkfik4848484841111111prettyenoughnow --target-dir=$backup_dir --datadir=$mysql_datadir
rm -rf $backup_dir/*

vlog "Testing with --databases=..."
$XB_BIN $XB_ARGS --backup --databases='database database2 test1.t test1.ancor thisisadatabase testdatabase.t testdatabase.t7' --target-dir=$backup_dir --datadir=$mysql_datadir
diff -u <(ls_dir $backup_dir) - <<EOF
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
xtrabackup --backup --databases-file=$topdir/list --target-dir=$backup_dir --datadir=$mysql_datadir
diff -u <(ls_dir $backup_dir) - <<EOF
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
run_cmd_expect_failure $XB_BIN $XB_ARGS --backup --tables-file=$topdir/list --target-dir=$backup_dir --datadir=$mysql_datadir
diff -u <(ls_dir $backup_dir) - <<EOF
EOF
rm -rf $backup_dir/*

vlog "Testing with --tables-file=..."
cat >$topdir/list <<EOF
test1.t
test1.ancor
testdatabase.t
testdatabase.t7
EOF
xtrabackup --backup --tables-file=$topdir/list --target-dir=$backup_dir --datadir=$mysql_datadir
diff -u <(ls_dir $backup_dir) - <<EOF
./test1/ancor.ibd
./test1/t.ibd
./testdatabase/t.ibd
EOF
rm -rf $backup_dir/*

vlog "Testing with --tables=..."
xtrabackup --backup --tables='dat.base,test1.t,test1.a' --target-dir=$backup_dir --datadir=$mysql_datadir
diff -u <(ls_dir $backup_dir) - <<EOF
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
