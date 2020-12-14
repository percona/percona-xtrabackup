#!/bin/bash

set -e

function usage() {
    cat <<EOF
Usage: $0 [OPTIONS]
    The following options may be given :
        --type=             Supported types are: innodb56 innodb57 xtradb56 xtradb57 mariadb100 mariadb101 as DB type(default=xtradb57)
        --version=          Version of tarball
        --destdir=          OPTIONAL: Destination directory where the tarball will be extracted(default=\"./server\")
        --help) usage ;;
EOF
    exit 1
}

function ssl_version() {
    sslv=$(ls -la {/,/usr/}{lib64,lib,lib/x86_64-linux-gnu}/libssl.so.1.* 2>/dev/null | sed 's/.*[.]so//; s/[^0-9]//g' | head -1)

    case ${sslv} in
        100|101|11) ;;
        102) 
            if [[ ${RHVER} -eq 7 ]] && [[ $(echo ${VERSION} | awk -F "." '{ print $3 }' | cut -d '-' -f1) -lt "50" ]]; then
                unset sslv; sslv=101
            elif [[ ${DEBVER} == "stretch" ]] && [[ $(echo ${VERSION} | awk -F "." '{ print $3 }' | cut -d '-' -f1) -lt "50" ]]; then
                unset sslv; sslv=102
            else
                unset sslv; sslv="102.$OS"
            fi
            ;;
        *)
            if ! test -r "${1}"; then
                >&2 echo "tarball for your openssl version (${sslv}) is not available"
                exit 1
            fi
            ;;
    esac
    echo ${sslv}
}

shell_quote_string() {
  echo "$1" | sed -e 's,\([^a-zA-Z0-9/_.=-]\),\\\1,g'
}

append_arg_to_args () {
    args="${args} "$(shell_quote_string "${1}")
}

parse_arguments() {
    pick_args=
    if test "${1}" = PICK-ARGS-FROM-ARGV
    then
        pick_args=1
        shift
    fi

    for arg do
        val=$(echo "${arg}" | sed -e 's;^--[^=]*=;;')
        case "${arg}" in
            # these get passed explicitly to mysqld
            --type=*) TYPE="${val}" ;;
            --version=*) VERSION="${val}" ;;
            --destdir=*) DESTDIR="${val}" ;;
            --help) usage ;;
            *)
            if test -n "${pick_args}"
            then
                append_arg_to_args "${arg}"
            fi
            ;;
        esac
    done
}

main () {
    if [ -f /etc/redhat-release ]; then
        OS="rpm"
        RHVER=$(rpm --eval %rhel)
    else
        DEBVER=$(lsb_release -sc)
        OS="deb"
    fi

    arch=$(uname -m)
    if [ "${arch}" == "i386" ]; then
        arch="i686"
    fi

    if [[ -d "${DESTDIR}" ]]; then
        rm -rf "${DESTDIR}"
    fi

    mkdir "${DESTDIR}"

    case "${TYPE}" in
        innodb55)
            url="http://s3.amazonaws.com/percona.com/downloads/community"
            tarball="mysql-5.5.42-linux2.6-$arch.tar.gz"
            ;;
        innodb56)
            url="https://dev.mysql.com/get/Downloads/MySQL-5.6/"
            fallback_url="https://downloads.mysql.com/archives/get/p/23/file"
            tarball="mysql-${VERSION}-linux-glibc2.12-${arch}.tar.gz"
                if ! wget --spider "${url}/${tarball}" 2>/dev/null; then
                    unset url
                    url=${fallback_url}
                fi
            ;;
        innodb57)
            url="https://dev.mysql.com/get/Downloads/MySQL-5.7/"
            fallback_url="https://downloads.mysql.com/archives/get/p/23/file"
            tarball="mysql-${VERSION}-linux-glibc2.12-${arch}.tar.gz"
                if ! wget --spider "${url}/${tarball}" 2>/dev/null; then
                    unset url
                    url=${fallback_url}
                fi
            ;;
        mariadb55)
            url="http://s3.amazonaws.com/percona.com/downloads/community"
            tarball="mariadb-5.5.40-linux-$arch.tar.gz"
            ;;
        mariadb100)
            url="http://s3.amazonaws.com/percona.com/downloads/community"
            tarball="mariadb-10.0.14-linux-${arch}.tar.gz"
            ;;
        mariadb101)
            url="https://downloads.mariadb.org/f/mariadb-${VERSION}/bintar-linux-glibc_214-${arch}/"
            tarball="mariadb-${VERSION}-linux-glibc_214-${arch}.tar.gz"
            ;;
        xtradb55)
            url="http://s3.amazonaws.com/percona.com/downloads/community/yassl"
            tarball="Percona-Server-5.5.41-rel37.0-727.Linux.$arch.tar.gz"
            ;;
        xtradb56)
            url="https://www.percona.com/downloads/Percona-Server-5.6/Percona-Server-${VERSION}/binary/tarball/"
            VERSION_PREFIX=$(echo ${VERSION} | awk -F '-' '{ print $1 }')
            VERSION_SUFFIX=$(echo ${VERSION} | awk -F '-' '{ print $2 }')
            tarball="Percona-Server-${VERSION_PREFIX}-rel${VERSION_SUFFIX}-Linux.${arch}.ssl$(ssl_version).tar.gz"
            ;;
        xtradb57)
            url="https://www.percona.com/downloads/Percona-Server-5.7/Percona-Server-${VERSION}/binary/tarball/"
            if [[ $(echo ${VERSION} | awk -F "." '{ print $3 }' | cut -d '-' -f1) -lt "31" ]]; then
                tarball="Percona-Server-${VERSION}-Linux.${arch}.ssl$(ssl_version).tar.gz"
            elif [[ $(echo ${VERSION} | awk -F "." '{ print $3 }' | cut -d '-' -f1) -ge "31" ]]; then
                tarball="Percona-Server-${VERSION}-Linux.${arch}.glibc2.12.tar.gz"
            fi
            ;;
        *)
            echo "Err: Specified unsupported ${TYPE}."
            echo "Supported types are: innodb56 innodb57 xtradb56 xtradb57 mariadb100 mariadb101"
            echo "Example: $0 --type=xtradb57 --version=5.7.31-34"
            exit 1
            ;;
    esac

    # Check if tarball exist before any download
    if ! wget --spider "${url}/${tarball}" 2>/dev/null; then
        echo "Version you specified(${VERSION}) is not exist on ${url}/${tarball}"
        exit 1
    elif [[ -z ${tarball} ]]; then
        echo "Version you specified(${VERSION}) is not exist on ${url}/${tarball}"
        exit 1
    else
        echo "Downloading ${tarball}"
        wget -qc "${url}/${tarball}"
    fi

    echo "Unpacking ${tarball} into ${DESTDIR}"
    tar xf "${tarball}" -C "${DESTDIR}"
    sourcedir="${DESTDIR}/$(ls ${DESTDIR})"
    if test -n "${sourcedir}"; then
        mv "${sourcedir}"/* "${DESTDIR}"
        rm -rf "${sourcedir}"
    fi
}

TYPE="xtradb57"
VERSION="5.7.31-34"
DESTDIR="./server"
parse_arguments PICK-ARGS-FROM-ARGV "$@"
main