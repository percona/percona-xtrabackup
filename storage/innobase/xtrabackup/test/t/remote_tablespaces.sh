########################################################################
# Test remote tablespaces in InnoDB 5.6
########################################################################

. inc/common.sh

require_server_version_higher_than 5.6.0

remote_dir=$TEST_VAR_ROOT/var1/remote_dir/subdir
undo_dir=$TEST_VAR_ROOT/var1/undo_dir/subdir

mkdir -p $remote_dir
mkdir -p $undo_dir

start_server --innodb_file_per_table --innodb_directories="$remote_dir;$undo_dir"

$MYSQL $MYSQL_ARGS test <<EOF
CREATE TABLESPACE TS1 ADD DATAFILE '$remote_dir/ts1.ibd';
CREATE TABLESPACE TS2 ADD DATAFILE '$remote_dir/ts2_name_mismatch.ibd';
CREATE TABLE t(id INT AUTO_INCREMENT PRIMARY KEY, c INT)
  DATA DIRECTORY = '$remote_dir' ENGINE=InnoDB;
INSERT INTO t(c) VALUES (1), (2), (3), (4), (5), (6), (7), (8);
CREATE TABLE b(id INT AUTO_INCREMENT PRIMARY KEY, c INT)
  TABLESPACE TS1  ENGINE=InnoDB;
INSERT INTO b(c) VALUES (1), (2), (3), (4), (5), (6), (7), (8);
CREATE TABLE c(id INT AUTO_INCREMENT PRIMARY KEY, c INT)
  TABLESPACE TS2 ENGINE=InnoDB;
INSERT INTO c(c) VALUES (1), (2), (3), (4), (5), (6), (7), (8);
EOF

for ((i=0; i<12; i++))
do
    $MYSQL $MYSQL_ARGS test <<EOF
CREATE TABLE t$i(id INT AUTO_INCREMENT PRIMARY KEY, c INT)
  DATA DIRECTORY = '$remote_dir' ENGINE=InnoDB;
CREATE TABLE b$i(id INT AUTO_INCREMENT PRIMARY KEY, c INT)
  TABLESPACE TS1 ENGINE=InnoDB;
CREATE TABLE c$i(id INT AUTO_INCREMENT PRIMARY KEY, c INT)
  TABLESPACE TS2 ENGINE=InnoDB;
EOF
done

for ((i=0; i<12; i++))
do
    $MYSQL $MYSQL_ARGS test <<EOF
CREATE TABLE p$i(id INT AUTO_INCREMENT PRIMARY KEY, c INT)
  DATA DIRECTORY = '$undo_dir' ENGINE=InnoDB;
CREATE TABLE r$i(id INT AUTO_INCREMENT PRIMARY KEY, c INT)
  TABLESPACE TS1 ENGINE=InnoDB;
CREATE TABLE s$i(id INT AUTO_INCREMENT PRIMARY KEY, c INT)
  TABLESPACE TS2 ENGINE=InnoDB;
EOF
done

# create some undo tablespaces
mysql -e "CREATE UNDO TABLESPACE rundo1 ADD DATAFILE '$remote_dir/rundo1.ibu'"
mysql -e "CREATE UNDO TABLESPACE rundo2 ADD DATAFILE '$remote_dir/rundo2.ibu'"
mysql -e "CREATE UNDO TABLESPACE uundo1 ADD DATAFILE '$undo_dir/uundo1.ibu'"
mysql -e "CREATE UNDO TABLESPACE uundo2 ADD DATAFILE '$undo_dir/uundo2.ibu'"
mysql -e "CREATE UNDO TABLESPACE undo1 ADD DATAFILE 'undo1.ibu'"
mysql -e "CREATE UNDO TABLESPACE undo2 ADD DATAFILE 'undo2.ibu'"

# Generate some log data, as we also want to test recovery of remote tablespaces
for ((i=0; i<12; i++))
do
    $MYSQL $MYSQL_ARGS test <<EOF
      INSERT INTO t(c) SELECT c FROM t;
      INSERT INTO b(c) SELECT c FROM t;
      INSERT INTO c(c) SELECT c FROM t;
EOF
done

xtrabackup --backup --target-dir=$topdir/backup

record_db_state test

stop_server

rm -rf $mysql_datadir/*
rm -rf $remote_dir
rm -rf $undo_dir

for ((i=0; i<12; i++))
do
    test -f $topdir/backup/test/t${i}.ibd || die "Remote tablespace t${i} is missing from backup!"
done

for ((i=0; i<12; i++))
do
    test -f $topdir/backup/test/p${i}.ibd || die "Remote tablespace p${i} is missing from backup!"
done

xtrabackup --prepare --target-dir=$topdir/backup

for cmd in "--copy-back" "--move-back" ; do

    xtrabackup $cmd --target-dir=$topdir/backup

    if [ ! -f $remote_dir/test/t.ibd ] ; then
        vlog "Tablepace $remote_dir/t.ibd is missing!"
        exit -1
    fi
    for ((i=0; i<12; i++)) ; do
        if [ ! -f $remote_dir/test/t$i.ibd ] ; then
            vlog "Tablepace $remote_dir/t$i.ibd is missing!"
            exit -1
        fi

    done

    for ((i=0; i<12; i++)) ; do
        if [ ! -f $undo_dir/test/p$i.ibd ] ; then
            vlog "Tablepace $undo_dir/p$i.ibd is missing!"
            exit -1
        fi

    done

    for file in $mysql_datadir/rundo1.ibu $mysql_datadir/rundo2.ibu \
                $mysql_datadir/uundo1.ibu $mysql_datadir/uundo2.ibu \
                $mysql_datadir/undo1.ibu $mysql_datadir/undo2.ibu ; do
        if [ ! -f $file ] ; then
            vlog "Tablepace $file is missing!"
            exit -1
        fi
    done

    start_server

    verify_db_state test

    stop_server

    rm -rf $mysql_datadir/*
    rm -rf $remote_dir
    rm -rf $undo_dir

done
