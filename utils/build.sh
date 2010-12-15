#!/usr/bin/env bash

AUTO_DOWNLOAD=${AUTO_DOWNLOAD:-no}
MASTER_SITE="http://www.percona.com/downloads/community/"

set -e
export CFLAGS="$CFLAGS -g -O3"
export CXXFLAGS="$CXXFLAGS -g -O3"
MAKE_CMD=make
if [ "`uname -s`" = "FreeBSD" ]
then
	MAKE_CMD=gmake
fi

function usage(){
	echo "Usage: `basename $0` 5.1|5.5|plugin|xtradb"
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
"5.1")
	mysql_version=5.1.53
	reqs="mysql-$mysql_version.tar.gz libtar-1.2.11.tar.gz"
	for i in $reqs
	do
		if ! test -f $i
		then
			if [ "$AUTO_DOWNLOAD" = "yes" ]
			then
				wget "$MASTER_SITE"/$i
			else
				echo "Put $i in $top_dir or set environment variable AUTO_DOWNLOAD to \"yes\""
				exit -1
			fi
		fi
	done
	test -d mysql-$mysql_version && rm -r mysql-$mysql_version
	
	echo "Prepare sources"
	tar zxf mysql-$mysql_version.tar.gz
	cd $top_dir/mysql-$mysql_version
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
	$MAKE_CMD all
	
	echo "Compile XtraBackup"
	cd $top_dir/mysql-$mysql_version/storage/innobase/xtrabackup
	$MAKE_CMD 5.1
	
	echo "Compile tar4ibd"
	cd $top_dir/mysql-$mysql_version/libtar-1.2.11
	./configure
	$MAKE_CMD
	;;
"5.5")
	mysql_version=5.5.8
	reqs="mysql-$mysql_version.tar.gz libtar-1.2.11.tar.gz"
	for i in $reqs
	do
		if ! test -f $i
		then
			if [ "$AUTO_DOWNLOAD" = "yes" ]
			then
				wget "$MASTER_SITE"/$i
			else
				echo "Put $i in $top_dir or set environment variable AUTO_DOWNLOAD to \"yes\""
				exit -1
			fi
		fi
	done
	test -d mysql-$mysql_version && rm -r mysql-$mysql_version
	
	echo "Prepare sources"
	tar zxf mysql-$mysql_version.tar.gz
	cd $top_dir/mysql-$mysql_version
	patch -p1 < $top_dir/fix_innodb_for_backup55.patch

	cd $top_dir/mysql-$mysql_version
	tar zxf $top_dir/libtar-1.2.11.tar.gz
	cd $top_dir/mysql-$mysql_version/libtar-1.2.11
	patch -p1 < $top_dir/tar4ibd_libtar-1.2.11.patch

	mkdir $top_dir/mysql-$mysql_version/storage/innobase/xtrabackup
	cp $top_dir/Makefile $top_dir/xtrabackup.c $top_dir/innobackupex-1.5.1 $top_dir/mysql-$mysql_version/storage/innobase/xtrabackup

	echo "Compile MySQL"
	cd $top_dir/mysql-$mysql_version
	# We do not support CMake at the moment
	export HAVE_CMAKE=no
	# We need to build with partitioning due to MySQL bug #58632
	./configure --enable-local-infile \
	    --enable-thread-safe-client \
	    --with-plugins=innobase,partition \
	    --with-zlib-dir=bundled \
	    --enable-shared \
	    --with-extra-charsets=complex --disable-dtrace
	$MAKE_CMD all
	
	echo "Compile XtraBackup"
	cd $top_dir/mysql-$mysql_version/storage/innobase/xtrabackup
	$MAKE_CMD 5.5
	
	echo "Compile tar4ibd"
	cd $top_dir/mysql-$mysql_version/libtar-1.2.11
	./configure
	$MAKE_CMD
	;;
"plugin")
	mysql_version=5.1.53
	reqs="mysql-$mysql_version.tar.gz libtar-1.2.11.tar.gz"
	for i in $reqs
	do
		if ! test -f $i
		then
			if [ "$AUTO_DOWNLOAD" = "yes" ]
			then
				wget "$MASTER_SITE"/$i
			else
				echo "Put $i in $top_dir or set environment variable AUTO_DOWNLOAD to \"yes\""
				exit -1
			fi
		fi
	done
	test -d mysql-$mysql_version && rm -r mysql-$mysql_version
	
	echo "Prepare sources"
	tar zxf mysql-$mysql_version.tar.gz
	cd $top_dir/mysql-$mysql_version/
	patch -p1 < $top_dir/fix_innodb_for_backup_5.1_plugin.patch

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
	$MAKE_CMD all
	
	echo "Compile XtraBackup"
	cd $top_dir/mysql-$mysql_version/storage/innodb_plugin/xtrabackup
	$MAKE_CMD plugin
	
	echo "Compile tar4ibd"
	cd $top_dir/mysql-$mysql_version/libtar-1.2.11
	./configure
	$MAKE_CMD
	;;
"xtradb")
	mysql_version=5.1.53
	xtradb_version=12

	# Get Percona Server
	rm -rf release-$mysql_version-$xtradb_version 
	bzr branch  lp:percona-server/release-$mysql_version-$xtradb_version 
	cd release-$mysql_version-$xtradb_version 
	$MAKE_CMD
	rm -rf $top_dir/Percona-Server
	mv Percona-Server $top_dir
	cd $top_dir

	# Patch Percona Server
	cd Percona-Server
	patch -p1 < $top_dir/fix_innodb_for_backup_percona-server.patch
	
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
	LIBS=-lrt ./configure --enable-local-infile \
	    --enable-thread-safe-client \
	    --with-plugins=innodb_plugin \
	    --with-zlib-dir=bundled \
	    --enable-shared \
	    --with-extra-charsets=complex
	$MAKE_CMD all

	# Compile XtraBackup
	cd storage/innodb_plugin/xtrabackup
	$MAKE_CMD xtradb

	# Compile tar4ibd
	cd $top_dir/Percona-Server
	cd libtar-1.2.11
	./configure
	$MAKE_CMD all
	;;
*)
	usage
	;;
esac
