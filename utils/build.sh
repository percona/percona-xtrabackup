#!/usr/bin/env bash

set -e

function usage(){
	echo "Usage: `basename $0` 5.0|5.1|plugin|xtradb"
	exit -1
}

if ! test -f xtrabackup.c
then
	echo "`basename $0` must be run from the directory with XtraBackup sources"
	usage
fi

type=$1
top_dir=`pwd`

case "$type" in
"5.0")
	mysql_version=5.0.91
	reqs="mysql-$mysql_version.tar.gz libtar-1.2.11.tar.gz"
	for i in $reqs
	do
		if ! test -f $i
		then
			echo "Put $i in $top_dir"
			exit -1
		fi
	done
	test -d mysql-$mysql_version && rm -r mysql-$mysql_version
	
	echo "Prepare sources"
	tar zxf mysql-$mysql_version.tar.gz
	cd mysql-$mysql_version
	patch -p1 < $top_dir/fix_innodb_for_backup.patch

	tar zxf $top_dir/libtar-1.2.11.tar.gz
	cd libtar-1.2.11
	patch -p1 < $top_dir/tar4ibd_libtar-1.2.11.patch

	mkdir $top_dir/mysql-$mysql_version/innobase/xtrabackup
	cp $top_dir/Makefile $top_dir/xtrabackup.c $top_dir/innobackupex-1.5.1 $top_dir/mysql-$mysql_version/innobase/xtrabackup

	echo "Compile MySQL"
	cd $top_dir/mysql-$mysql_version
	./configure --enable-local-infile \
	    --enable-thread-safe-client \
	    --enable-shared \
	    --with-extra-charsets=complex
	make all
	
	echo "Compile XtraBackup"
	cd $top_dir/mysql-$mysql_version/innobase/xtrabackup
	make 5.0
	
	echo "Compile tar4ibd"
	cd $top_dir/mysql-$mysql_version/libtar-1.2.11
	./configure
	make
	;;
"5.1")
	mysql_version=5.1.42
	reqs="mysql-$mysql_version.tar.gz libtar-1.2.11.tar.gz"
	for i in $reqs
	do
		if ! test -f $i
		then
			echo "Put $i in $top_dir"
			exit -1
		fi
	done
	test -d mysql-$mysql_version && rm -r mysql-$mysql_version
	
	echo "Prepare sources"
	tar zxf mysql-$mysql_version.tar.gz
	cd $top_dir/mysql-$mysql_version/storage
	patch -p1 < $top_dir/fix_innodb_for_backup51.patch

	cd $top_dir/mysql-$mysql_version
	tar zxf $top_dir/libtar-1.2.11.tar.gz
	cd $top_dir/mysql-$mysql_version/libtar-1.2.11
	patch -p1 < $top_dir/tar4ibd_libtar-1.2.11.patch

	mkdir $top_dir/mysql-$mysql_version/storage/innobase/xtrabackup
	cp $top_dir/Makefile $top_dir/xtrabackup.c $top_dir/innobackupex-1.5.1 $top_dir/mysql-$mysql_version/storage/innobase/xtrabackup

	echo "Compile MySQL"
	cd $top_dir/mysql-$mysql_version
	./configure --enable-local-infile \
	    --enable-thread-safe-client \
	    --with-plugins=innobase \
	    --with-zlib-dir=bundled \
	    --enable-shared \
	    --with-extra-charsets=complex
	make all
	
	echo "Compile XtraBackup"
	cd $top_dir/mysql-$mysql_version/storage/innobase/xtrabackup
	make 5.1
	
	echo "Compile tar4ibd"
	cd $top_dir/mysql-$mysql_version/libtar-1.2.11
	./configure
	make
	;;
"plugin")
	mysql_version=5.1.46
	reqs="mysql-$mysql_version.tar.gz libtar-1.2.11.tar.gz"
	for i in $reqs
	do
		if ! test -f $i
		then
			echo "Put $i in $top_dir"
			exit -1
		fi
	done
	test -d mysql-$mysql_version && rm -r mysql-$mysql_version
	
	echo "Prepare sources"
	tar zxf mysql-$mysql_version.tar.gz
	cd $top_dir/mysql-$mysql_version/
	patch -p1 < $top_dir/fix_innodb_for_backup_5.1_plugin_1.0.7.patch

	cd $top_dir/mysql-$mysql_version
	tar zxf $top_dir/libtar-1.2.11.tar.gz
	cd $top_dir/mysql-$mysql_version/libtar-1.2.11
	patch -p1 < $top_dir/tar4ibd_libtar-1.2.11.patch

	mkdir $top_dir/mysql-$mysql_version/storage/innodb_plugin/xtrabackup
	cp $top_dir/Makefile $top_dir/xtrabackup.c $top_dir/innobackupex-1.5.1 $top_dir/mysql-$mysql_version/storage/innodb_plugin/xtrabackup

	echo "Compile MySQL"
	cd $top_dir/mysql-$mysql_version
	./configure --enable-local-infile \
	    --enable-thread-safe-client \
	    --with-plugins=innodb_plugin \
	    --with-zlib-dir=bundled \
	    --enable-shared \
	    --with-extra-charsets=complex
	make all
	
	echo "Compile XtraBackup"
	cd $top_dir/mysql-$mysql_version/storage/innodb_plugin/xtrabackup
	make plugin
	
	echo "Compile tar4ibd"
	cd $top_dir/mysql-$mysql_version/libtar-1.2.11
	./configure
	make
	;;
"xtradb")
	mysql_version=5.1.47
	xtradb_version=11
	percona_server_dir=/home/akuzminsky/src/Percona-Server/release-5.1.47-11

	# Get Percona Server
	cd $percona_server_dir
	#rm -rf release-$mysql_version-$xtradb_version 
	#bzr branch  lp:percona-server/release-$mysql_version-$xtradb_version 
	#cd release-$mysql_version-$xtradb_version 
	make
	rm -rf $top_dir/Percona-Server
	mv Percona-Server $top_dir
	cd $top_dir

	# Patch Percona Server
	cd Percona-Server
	patch -p1 < $top_dir/fix_innodb_for_backup_percona-server-$xtradb_version.patch
	
	# Copy XtraBackup
	cd storage/innodb_plugin
	mkdir xtrabackup
	cd xtrabackup 
	cp $top_dir/Makefile $top_dir/xtrabackup.c $top_dir/innobackupex-1.5.1 .

	# Copy tar4ibd
	cd $top_dir/Percona-Server
	test -f $top_dir/libtar-1.2.11.tar.gz || 
		(cd $top_dir ; 
		wget http://www.percona.com/percona-builds/community/libtar-1.2.11.tar.gz ; 
		cd $top_dir/Percona-Server)
	rm -rf libtar-1.2.11
	tar zxf $top_dir/libtar-1.2.11.tar.gz
	cd libtar-1.2.11 
	patch -p1 < $top_dir/tar4ibd_libtar-1.2.11.patch

	# Compile MySQL
	cd $top_dir/Percona-Server
	./configure --enable-local-infile \
	    --enable-thread-safe-client \
	    --with-plugins=innodb_plugin \
	    --with-zlib-dir=bundled \
	    --enable-shared \
	    --with-extra-charsets=complex
	make all

	# Compile XtraBackup
	cd storage/innodb_plugin/xtrabackup
	make xtradb

	# Compile tar4ibd
	cd $top_dir/Percona-Server
	cd libtar-1.2.11
	./configure
	make all
	;;
*)
	usage
	;;
esac
