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

function ssl_version()
{
    sslv=$(ls -la {/,/usr/}{lib64,lib,lib/x86_64-linux-gnu}/libssl.so.1.* 2>/dev/null | sed 's/.*[.]so//; s/[^0-9]//g' | head -1)

    case $sslv in
        100|101|102)
        ;;
        *)
            if ! test -r "$1"
            then
                >&2 echo "tarball for your openssl version ($sslv) is not available"
                exit 1
            fi
            ;;
    esac

    echo $sslv
}

case "$1" in
    innodb80)
        url="https://dev.mysql.com/get/Downloads/MySQL-8.0"
        tarball="mysql-8.0.18-linux-glibc2.12-${arch}.tar.xz"
        ;;

    xtradb80)
        url="https://www.percona.com/downloads/Percona-Server-8.0/Percona-Server-8.0.15-6/binary/tarball"
        tarball="Percona-Server-8.0.15-6-Linux.${arch}.ssl$(ssl_version).tar.gz"
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
tar xf $tarball -C $destdir
sourcedir="$destdir/`ls $destdir`"
if test -n "$sourcedir"
then
    mv $sourcedir/* $destdir
    rm -rf $sourcedir
fi
