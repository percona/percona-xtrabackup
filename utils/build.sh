#!/usr/bin/env bash

MYSQL_51_VERSION=5.1.59
MYSQL_55_VERSION=5.5.17

AUTO_DOWNLOAD=${AUTO_DOWNLOAD:-no}
MASTER_SITE="http://s3.amazonaws.com/percona.com/downloads/community"

set -e
export CFLAGS="$CFLAGS -g -O3"
export CXXFLAGS="$CXXFLAGS -g -O3"

MAKE_CMD=make
if gmake --version > /dev/null 2>&1
then
	MAKE_CMD=gmake
fi
MAKE_CMD="$MAKE_CMD -j6"

function usage()
{
    echo "Build an xtrabackup binary against the specified InnoDB flavor."
    echo
    echo "Usage: `basename $0` CODEBASE"
    echo "where CODEBASE can be one of the following values or aliases:"
    echo "  innodb51_builtin | 5.1	build against built-in InnoDB in MySQL 5.1"
    echo "  innodb55         | 5.5	build against InnoDB in MySQL 5.5"
    echo "  xtradb51         | xtradb   build against Percona Server with XtraDB 5.1"
    echo "  xtradb55         | xtradb55 build against Percona Server with XtraDB 5.5"
    exit -1
}

################################################################################
# Download files specified as arguments from $MASTER_SITE if $AUTO_DOWNLOAD is
# "yes". Otherwise print an error message and exit.
################################################################################
function auto_download()
{
    for i in $*
    do
	if ! test -f $i
	then
	    if [ "$AUTO_DOWNLOAD" = "yes" ]
	    then
		wget "$MASTER_SITE"/$i
	    else
		echo "Put $i in $top_dir or set environment variable \
AUTO_DOWNLOAD to \"yes\""
		exit -1
	    fi
	fi
    done
}

################################################################################
# Unpack the tarball specified as the first argument and apply the patch
# specified as the second argument to the resulting directory.
################################################################################
function unpack_and_patch()
{
    tar xzf $top_dir/$1
    cd `basename "$1" ".tar.gz"`
    patch -p1 < $top_dir/patches/$2
    cd ..
}

################################################################################
# Invoke 'make' in the specified directoies
################################################################################
function make_dirs()
{
    for d in $*
    do
	$MAKE_CMD -C $d
    done
}

function build_server()
{
    echo "Configuring the server"
    cd $server_dir
    BUILD/autorun.sh
    eval $configure_cmd

    echo "Building the server"
    make_dirs include zlib strings mysys dbug extra $innodb_dir
    cd $top_dir
}

function build_xtrabackup()
{
    echo "Building XtraBackup"
    mkdir $build_dir
    cp $top_dir/Makefile $top_dir/xtrabackup.c $build_dir

    # Read XTRABACKUP_VERSION from the VERSION file
    . $top_dir/VERSION

    cd $build_dir
    $MAKE_CMD $xtrabackup_target XTRABACKUP_VERSION=$XTRABACKUP_VERSION
    cd $top_dir
}

function build_tar4ibd()
{
    echo "Building tar4ibd"
    unpack_and_patch libtar-1.2.11.tar.gz tar4ibd_libtar-1.2.11.patch
    cd libtar-1.2.11
    ./configure
    $MAKE_CMD
    cd $topdir
}

################################################################################
# Do all steps to build the server, xtrabackup and tar4ibd
# Expects the following variables to be set before calling:
#   mysql_version	version string (e.g. "5.1.53")
#   server_patch	name of the patch to apply to server source before
#                       building (e.g. "xtradb51.patch")
#   innodb_name		either "innobase" or "innodb_plugin"
#   configure_cmd	server configure command
#   xtrabackup_target	'make' target to build in the xtrabackup build directory
#
################################################################################
function build_all()
{
    local mysql_version_short=${mysql_version:0:3}
    server_dir=$top_dir/mysql-$mysql_version_short
    server_tarball=mysql-$mysql_version.tar.gz
    innodb_dir=$server_dir/storage/$innodb_name
    build_dir=$innodb_dir/xtrabackup

    echo "Downloading sources"
    auto_download $server_tarball libtar-1.2.11.tar.gz

    test -d $server_dir && rm -r $server_dir

    echo "Preparing sources"
    unpack_and_patch $server_tarball $server_patch
    mv $top_dir/mysql-$mysql_version $server_dir

    build_server

    build_xtrabackup

    build_tar4ibd
}

if ! test -f xtrabackup.c
then
	echo "`basename $0` must be run from the directory with XtraBackup sources"
	usage
fi

type=$1
top_dir=`pwd`

case "$type" in
"innodb51_builtin" | "5.1")
	mysql_version=$MYSQL_51_VERSION
	server_patch=innodb51_builtin.patch
	innodb_name=innobase
	xtrabackup_target=5.1
	configure_cmd="./configure --enable-local-infile \
	    --enable-thread-safe-client \
	    --with-plugins=innobase \
	    --with-zlib-dir=bundled \
	    --enable-shared \
	    --with-extra-charsets=all"

	build_all
	;;

"innodb55" | "5.5")
	mysql_version=$MYSQL_55_VERSION
	server_patch=innodb55.patch
	innodb_name=innobase
	xtrabackup_target=5.5
	# We need to build with partitioning due to MySQL bug #58632
	configure_cmd="cmake . \
		-DENABLED_LOCAL_INFILE=ON \
		-DWITH_INNOBASE_STORAGE_ENGINE=ON \
		-DWITH_PARTITION_STORAGE_ENGINE=ON \
		-DWITH_ZLIB=bundled \
		-DWITH_EXTRA_CHARSETS=all \
		-DENABLE_DTRACE=OFF"

	build_all
	;;

"xtradb51" | "xtradb")
	server_dir=$top_dir/Percona-Server
	branch_dir=percona-server-5.1-xtrabackup
	innodb_dir=$server_dir/storage/innodb_plugin
	build_dir=$innodb_dir/xtrabackup
	xtrabackup_target=xtradb
	configure_cmd="./configure --enable-local-infile \
	    --enable-thread-safe-client \
	    --with-plugins=innodb_plugin \
	    --with-zlib-dir=bundled \
	    --enable-shared \
	    --with-extra-charsets=all"
	if [ "`uname -s`" = "Linux" ]
	then
		configure_cmd="LIBS=-lrt $configure_cmd"
	fi


	echo "Downloading sources"
	
	auto_download libtar-1.2.11.tar.gz

	# Get Percona Server
	if [ -d $branch_dir ]
	then
	    cd $branch_dir
	    bzr pull
	else
	    bzr branch lp:~percona-dev/percona-server/$branch_dir $branch_dir
	    cd $branch_dir
	fi

	$MAKE_CMD main
	rm -rf $server_dir
	mv Percona-Server $top_dir

	# Patch Percona Server
	cd $server_dir
	patch -p1 < $top_dir/patches/xtradb51.patch

	build_server

	build_xtrabackup

	build_tar4ibd
	;;
"xtradb55")
	server_dir=$top_dir/Percona-Server-5.5
	branch_dir=percona-server-5.5-xtrabackup
	innodb_dir=$server_dir/storage/innobase
	build_dir=$innodb_dir/xtrabackup
	xtrabackup_target=xtradb55
	# We need to build with partitioning due to MySQL bug #58632
	configure_cmd="cmake . \
		-DENABLED_LOCAL_INFILE=ON \
		-DWITH_INNOBASE_STORAGE_ENGINE=ON \
		-DWITH_PARTITION_STORAGE_ENGINE=ON \
		-DWITH_ZLIB=bundled \
		-DWITH_EXTRA_CHARSETS=all \
		-DENABLE_DTRACE=OFF"
	if [ "`uname -s`" = "Linux" ]
	then
		configure_cmd="LIBS=-lrt $configure_cmd"
	fi


	echo "Downloading sources"
	
	auto_download libtar-1.2.11.tar.gz

	# Get Percona Server
	if [ -d $branch_dir ]
	then
	    cd $branch_dir
	    bzr pull
	else
	    bzr branch lp:~percona-dev/percona-server/$branch_dir $branch_dir
	    cd $branch_dir
	fi

	$MAKE_CMD PERCONA_SERVER=Percona-Server-5.5 main
	rm -rf $server_dir
	mv Percona-Server-5.5 $top_dir

	# Patch Percona Server
	cd $server_dir
	patch -p1 < $top_dir/patches/xtradb55.patch

	build_server

	build_xtrabackup

	build_tar4ibd
	;;
*)
	usage
	;;
esac
