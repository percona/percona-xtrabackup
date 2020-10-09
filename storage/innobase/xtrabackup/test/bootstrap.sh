#!/bin/bash

set -e

function usage() {
    cat <<EOF
Usage: $0 [OPTIONS]
    The following options may be given :
        --type=             Pass innodb80 or xtradb80 for DB type(default=xtradb80)
        --version=          Version of tarball
        --destdir=          OPTIONAL: Destination directory where the tarball will be extracted(default=\"./server\")
        --help) usage ;;
EOF
    exit 1
}

function ssl_version() {
    sslv=$(readlink -f $(ldconfig -p | grep libssl.so | head -n1 | awk -F "=>" '{ print $2 }') | sed 's/.*[.]so//; s/[^0-9]//g')

    case ${sslv} in
        100|101) ;;
        102) unset sslv; sslv="102.$OS" ;;
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
    if test "${1}" = PICK-ARGS-FROM-ARGV; then
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
            if test -n "${pick_args}"; then
                append_arg_to_args "${arg}"
            fi
            ;;
        esac
    done
}

main () {
    if [ -f /etc/redhat-release ]; then
        OS="rpm"
    else
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
        innodb80)
            url="https://dev.mysql.com/get/Downloads/MySQL-8.0"
            tarball="mysql-${VERSION}-linux-glibc2.12-${arch}.tar.xz"
            ;;
        xtradb80)
            url="https://www.percona.com/downloads/Percona-Server-8.0/Percona-Server-${VERSION}/binary/tarball"
            if [[ $(echo ${VERSION} | awk -F "." '{ print $3 }' | cut -d '-' -f1) -lt "20" ]]; then
                tarball="Percona-Server-${VERSION}-Linux.${arch}.ssl$(ssl_version).tar.gz"
            elif [[ $(echo ${VERSION} | awk -F "." '{ print $3 }' | cut -d '-' -f1) -ge "20" ]]; then
                tarball="Percona-Server-${VERSION}-Linux.${arch}.glibc2.12.tar.gz"
            fi
            ;;
        *) 
            echo "Err: Specified unsupported ${TYPE}."
            echo "Supported types are: innodb80, xtradb80."
            echo "Example: $0 --type=xtradb80 --version=8.0.18-9"
            exit 1
            ;;
    esac

    # Check if tarball exist before any download
    if ! wget --spider "${url}/${tarball}" 2>/dev/null; then
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

TYPE="xtradb80"
VERSION="8.0.18-9"
DESTDIR="./server"
parse_arguments PICK-ARGS-FROM-ARGV "$@"
main