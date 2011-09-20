#!/bin/bash

set -e

function usage()
{
    cat <<EOF
Usage: 
$0 xtrabackup_target [installation_directory]
$0 path_to_server_tarball [installation_directory]

Prepares a server binary directory to be used by run.sh when running XtraBackup
tests.

If the argument is one of the build targets passed to build.sh 
(i.e. innodb51_builtin innodb51 innodb55 xtradb51 xtradb55) then the 
appropriate Linux tarball is downloaded from a pre-defined location and
unpacked into the specified installation  directory ('./server' by default).

Otherwise the argument is assumed to be a path to a server binary tarball.
EOF
}

if [ -z "$1" ]
then
    usage
fi

arch="`uname -m`"
if [ "$arch" = "i386" ]
then
    arch="i686"
fi

case "$1" in
    innodb51_builtin | innodb51)
	url="http://s3.amazonaws.com/percona.com/downloads/community"
	tarball="mysql-5.1.49-linux-$arch-glibc23.tar.gz"
	;;
    innodb55)
	url="http://s3.amazonaws.com/percona.com/downloads/community"
	tarball="mysql-5.5.16-linux2.6-$arch.tar.gz"
	;;
    xtradb51)
	url="http://s3.amazonaws.com/percona.com/downloads/Percona-Server-5.1/Percona-Server-5.1.57-12.8/Linux/binary"
	tarball="Percona-Server-5.1.57-rel12.8-233-Linux-$arch.tar.gz"
	;;
    xtradb55)
	url="http://s3.amazonaws.com/percona.com/downloads/Percona-Server-5.5/Percona-Server-5.5.11-20.2/Linux/binary"
	tarball="Percona-Server-5.5.11-rel20.2-116.Linux.$arch.tar.gz"
	;;
    *)
	if ! test -r "$1"
	then
	    echo "$1 does not exist"
	    exit 1
	fi
	tarball="$1"
	;;
esac

if test -n "$2"
then
    destdir="$2"
else
    destdir="./server"
fi

if test -n "$url"
then
    echo "Downloading $tarball"
    wget -qc "$url/$tarball"
fi

if test -d "$destdir"
then
    rm -rf "$destdir"
fi
mkdir "$destdir"

echo "Unpacking $tarball into $destdir"
tar zxf $tarball -C $destdir 
sourcedir="$destdir/`ls $destdir`"
if test -n "$sourcedir"
then
    mv $sourcedir/* $destdir
    rm -rf $sourcedir
fi
