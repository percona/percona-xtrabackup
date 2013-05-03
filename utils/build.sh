#!/usr/bin/env bash

set -e

MYSQL_51_VERSION=5.1.59
MYSQL_55_VERSION=5.5.17
MYSQL_56_VERSION=5.6.10
PS_51_VERSION=5.1.59-13.0
PS_55_VERSION=5.5.16-22.0

AUTO_DOWNLOAD=${AUTO_DOWNLOAD:-no}
MASTER_SITE="http://s3.amazonaws.com/percona.com/downloads/community"

top_dir=`pwd`

# Set the build type and resolve synonyms
case "$1" in
"plugin" )
        type="innodb51"
        ;;
"5.5" )
        type="innodb55"
        ;;
"5.6" | "xtradb56" | "mariadb100" )
        type="innodb56"
        ;;
"xtradb" | "mariadb51" | "mariadb52" | "mariadb53" )
        type="xtradb51"
        ;;
"galera55" | "mariadb55" )
        type="xtradb55"
        ;;
*)
        type=$1
        ;;
esac

# Percona Server 5.5 does not build with -Werror, so ignore DEBUG for now
if [ -n "$DEBUG" -a "$type" != "xtradb55" -a "$type" != "xtradb51" ]
then
    # InnoDB extra debug flags
    innodb_extra_debug="-DUNIV_DEBUG -DUNIV_MEM_DEBUG \
-DUNIV_DEBUG_THREAD_CREATION -DUNIV_DEBUG_LOCK_VALIDATE -DUNIV_DEBUG_PRINT \
-DUNIV_DEBUG_FILE_ACCESS -DUNIV_SEARCH_DEBUG -DUNIV_LOG_LSN_DEBUG \
-DUNIV_ZIP_DEBUG -DUNIV_AHI_DEBUG -DUNIV_SQL_DEBUG -DUNIV_AIO_DEBUG \
-DUNIV_LRU_DEBUG -DUNIV_BUF_DEBUG -DUNIV_HASH_DEBUG -DUNIV_IBUF_DEBUG"

    if [ "$type" = "innodb56" ]
    then
	innodb_extra_debug="$innodb_extra_debug -DUNIV_LIST_DEBUG"
    fi

    CFLAGS="$CFLAGS -g -O0 $innodb_extra_debug -DSAFE_MUTEX -DSAFEMALLOC"
    CXXFLAGS="$CXXFLAGS -g -O0 $innodb_extra_debug -DSAFE_MUTEX -DSAFEMALLOC"
    extra_config_51="--with-debug=full"
    extra_config_55plus="-DWITH_DEBUG=ON -DMYSQL_MAINTAINER_MODE=OFF"
else
    CFLAGS="$CFLAGS -g -O3"
    CXXFLAGS="$CXXFLAGS -g -O3"
    extra_config_51=
    extra_config_55plus=
fi

if [ "$type" = "innodb51_builtin" ]
then
    # include/*.ic in pre-5.1-plugin InnoDB do not compile well in C++.
    CXXFLAGS="$CXXFLAGS -fpermissive"
fi

xtrabackup_include_dir="$top_dir/src"
CFLAGS="$CFLAGS -I$xtrabackup_include_dir"
CXXFLAGS="$CXXFLAGS -I$xtrabackup_include_dir"

export CFLAGS CXXFLAGS

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
    echo "  innodb51         | plugin                build against InnoDB plugin in MySQL 5.1"
    echo "  innodb55         | 5.5                   build against InnoDB in MySQL 5.5"
    echo "  innodb56         | 5.6,xtradb56,         build against InnoDB in MySQL 5.6"
    echo "                   | mariadb100"
    echo "  xtradb51         | xtradb,mariadb51      build against Percona Server with XtraDB 5.1"
    echo "                   | mariadb52,mariadb53"
    echo "  xtradb55         | galera55,mariadb55    build against Percona Server with XtraDB 5.5"
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
    local $type=$1
    echo "Configuring the server"
    cd $server_dir
    BUILD/autorun.sh
    eval $configure_cmd

    echo "Building the server"
    if [ "$type" = "innodb56" ]
    then
        make_dirs libmysqld
    else
        make_dirs include zlib strings mysys dbug extra
        make_dirs $innodb_dir
    fi
    cd $top_dir
}

function build_libarchive()
{
	echo "Building libarchive"
	cd $top_dir/src/libarchive
	
	cmake  . \
	    -DENABLE_CPIO=OFF \
	    -DENABLE_OPENSSL=OFF \
	    -DENABLE_TAR=OFF \
	    -DENABLE_TEST=OFF
	$MAKE_CMD || exit -1
}

function build_xtrabackup()
{
    build_libarchive
    echo "Building XtraBackup"

    # Read XTRABACKUP_VERSION from the VERSION file
    . $top_dir/VERSION

    cd $top_dir/src
    if [ "`uname -s`" = "Linux" ]
    then
	export LIBS="$LIBS -lrt"
    fi
    $MAKE_CMD MYSQL_ROOT_DIR=$server_dir clean
    cat > build.sh <<EOF
export CFLAGS="$CFLAGS"
export CXXFLAGS="$CXXFLAGS"
$MAKE_CMD MYSQL_ROOT_DIR=$server_dir \
  XTRABACKUP_VERSION=$XTRABACKUP_VERSION $xtrabackup_target
EOF
    chmod +x build.sh
    $MAKE_CMD MYSQL_ROOT_DIR=$server_dir XTRABACKUP_VERSION=$XTRABACKUP_VERSION $xtrabackup_target
    cd $top_dir
}


################################################################################
# Do all steps to build the server, xtrabackup and xbstream
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
    local type=$1
    local mysql_version_short=${mysql_version:0:3}
    server_dir=$top_dir/mysql-$mysql_version_short
    server_tarball=mysql-$mysql_version.tar.gz
    innodb_dir=$server_dir/storage/$innodb_name

    echo "Downloading sources"
    auto_download $server_tarball

    test -d $server_dir && rm -r $server_dir

    echo "Preparing sources"
    unpack_and_patch $server_tarball $server_patch
    mv $top_dir/mysql-$mysql_version $server_dir

    build_server $type

    build_xtrabackup
}

if ! test -f src/xtrabackup.cc
then
	echo "`basename $0` must be run from the directory with XtraBackup sources"
	usage
fi

case "$type" in
"innodb51")
       mysql_version=$MYSQL_51_VERSION
       server_patch=innodb51.patch
       innodb_name=innodb_plugin
       xtrabackup_target=plugin
       configure_cmd="./configure --enable-local-infile \
           --enable-thread-safe-client \
           --with-plugins=innodb_plugin \
           --with-zlib-dir=bundled \
           --enable-shared \
           --with-extra-charsets=all $extra_config_51"

       build_all $type
       ;;
"innodb55")
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
		-DENABLE_DTRACE=OFF $extra_config_55plus"

	build_all $type
	;;

"innodb56" )
        mysql_version=$MYSQL_56_VERSION
        server_patch=innodb56.patch
        innodb_name=innobase
        xtrabackup_target=5.6
        configure_cmd="cmake . \
                -DWITH_INNOBASE_STORAGE_ENGINE=ON \
                -DWITH_ZLIB=bundled \
                -DWITH_EXTRA_CHARSETS=all \
                -DWITH_EMBEDDED_SERVER=1 \
                -DENABLE_DTRACE=OFF $extra_config_55plus"
        build_all $type
        ;;

"xtradb51" )
	server_dir=$top_dir/Percona-Server
	branch_dir=percona-server-5.1-xtrabackup
	innodb_dir=$server_dir/storage/innodb_plugin
	xtrabackup_target=xtradb
	configure_cmd="./configure --enable-local-infile \
	    --enable-thread-safe-client \
	    --with-plugins=innodb_plugin \
	    --with-zlib-dir=bundled \
	    --enable-shared \
	    --with-extra-charsets=all $extra_config_51"
	if [ "`uname -s`" = "Linux" ]
	then
		configure_cmd="LIBS=-lrt $configure_cmd"
	fi


	echo "Downloading sources"
	
	# Get Percona Server
	if [ -d $branch_dir ]
	then
	    rm -rf $branch_dir
	fi
        if [ -d $branch_dir ]
        then
	    cd $branch_dir
	    (bzr upgrade || true)
	    bzr clean-tree --force --ignored
	    bzr revert
	    bzr pull --overwrite
	else
	    bzr branch -r tag:Percona-Server-$PS_51_VERSION \
		lp:percona-server/5.1 $branch_dir
	    cd $branch_dir
	fi

	$MAKE_CMD main
	cd $top_dir
	rm -rf $server_dir
	ln -s $branch_dir/Percona-Server $server_dir

	# Patch Percona Server
	cd $server_dir
	patch -p1 < $top_dir/patches/xtradb51.patch

	build_server $type

	build_xtrabackup

	;;
"xtradb55" )
	server_dir=$top_dir/Percona-Server-5.5
	branch_dir=percona-server-5.5-xtrabackup
	innodb_dir=$server_dir/storage/innobase
	xtrabackup_target=xtradb55
	# We need to build with partitioning due to MySQL bug #58632
	configure_cmd="cmake . \
		-DENABLED_LOCAL_INFILE=ON \
		-DWITH_INNOBASE_STORAGE_ENGINE=ON \
		-DWITH_PARTITION_STORAGE_ENGINE=ON \
		-DWITH_ZLIB=bundled \
		-DWITH_EXTRA_CHARSETS=all \
		-DENABLE_DTRACE=OFF $extra_config_55plus"
	if [ "`uname -s`" = "Linux" ]
	then
		configure_cmd="LIBS=-lrt $configure_cmd"
	fi


	echo "Downloading sources"
	
	# Get Percona Server
	if [ -d $branch_dir ]
	then
	    rm -rf $branch_dir
	fi
        if [ -d $branch_dir ]
        then
	    cd $branch_dir
	    yes | bzr break-lock
	    (bzr upgrade || true)
	    bzr clean-tree --force --ignored
	    bzr revert
	    bzr pull --overwrite
	else
	    bzr branch -r tag:Percona-Server-$PS_55_VERSION \
		lp:percona-server $branch_dir
	    cd $branch_dir
	fi

	$MAKE_CMD PERCONA_SERVER=Percona-Server-5.5 main
	cd $top_dir
	rm -rf $server_dir
	ln -s $branch_dir/Percona-Server $server_dir

	# Patch Percona Server
	cd $server_dir
	patch -p1 < $top_dir/patches/xtradb55.patch

	build_server $type

	build_xtrabackup

	;;
*)
	usage
	;;
esac
