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
(i.e. innodb51 innodb55 innodb56 xtradb51 xtradb55) then the
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

if [ "$arch" = "i686" ]
then
    maria_arch_path=x86
else
    maria_arch_path=amd64
fi

case "$1" in
    innodb51)
        url="http://s3.amazonaws.com/percona.com/downloads/community"
        tarball="mysql-5.1.73-linux-$arch-glibc23.tar.gz"
        ;;

    innodb55)
        url="http://s3.amazonaws.com/percona.com/downloads/community"
        tarball="mysql-5.5.42-linux2.6-$arch.tar.gz"
        ;;

    innodb56)
        url="http://s3.amazonaws.com/percona.com/downloads/community"
        tarball="mysql-5.6.23-linux-glibc2.5-$arch.tar.gz"
        ;;

    innodb57)
        url="http://s3.amazonaws.com/percona.com/downloads/community"
        tarball="mysql-5.7.21-linux-glibc2.12-$arch.tar.gz"
        ;;

    xtradb51)
        url="http://s3.amazonaws.com/percona.com/downloads/community/yassl"
        tarball="Percona-Server-5.1.73-rel14.12-625.Linux.$arch.tar.gz"
        ;;

    xtradb55)
        url="http://s3.amazonaws.com/percona.com/downloads/community/yassl"
        tarball="Percona-Server-5.5.41-rel37.0-727.Linux.$arch.tar.gz"
        ;;

    xtradb56)
        url="http://s3.amazonaws.com/percona.com/downloads/community/yassl"
        tarball="Percona-Server-5.6.22-rel72.0-.Linux.$arch.tar.gz"
        ;;

    xtradb57)
        url="http://s3.amazonaws.com/percona.com/downloads/community/yassl"
        tarball="Percona-Server-5.7.21-20-Linux.$arch.tar.gz"
        ;;

    mariadb51)
        url="http://s3.amazonaws.com/percona.com/downloads/community"
        tarball="mariadb-5.1.67-Linux-$arch.tar.gz"
        ;;

    mariadb52)
        url="http://s3.amazonaws.com/percona.com/downloads/community"
        tarball="mariadb-5.2.14-Linux-$arch.tar.gz"
        ;;

    mariadb53)
        url="http://s3.amazonaws.com/percona.com/downloads/community"
        tarball="mariadb-5.3.12-Linux-$arch.tar.gz"
        ;;

    mariadb55)
        url="http://s3.amazonaws.com/percona.com/downloads/community"
        tarball="mariadb-5.5.40-linux-$arch.tar.gz"
        ;;

    mariadb100)
        url="http://s3.amazonaws.com/percona.com/downloads/community"
        tarball="mariadb-10.0.14-linux-$arch.tar.gz"
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

if test -d "$destdir"
then
    rm -rf "$destdir"
fi
mkdir "$destdir"

if test -n "$url"
then
    echo "Downloading $tarball"
    wget -qc "$url/$tarball"
fi

echo "Unpacking $tarball into $destdir"
tar zxf $tarball -C $destdir
sourcedir="$destdir/`ls $destdir`"
if test -n "$sourcedir"
then
    mv $sourcedir/* $destdir
    rm -rf $sourcedir
fi
