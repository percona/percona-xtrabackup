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
    sslv=$(readlink -f $(ldconfig -p | grep libssl.so | head -n1 | awk -F "=>" '{ print $2 }') | sed 's/.*[.]so//; s/[^0-9]//g')

    case ${sslv} in
        100|101|102|11) ;;
        *)
            if ! test -r "${1}"; then
                >&2 echo "tarball for your openssl version (${sslv}) is not available"
                exit 1
            fi
            ;;
    esac
    echo ${sslv}
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
    arch=$(uname -m)
    if [ "${arch}" == "i386" ]; then
        arch="i686"
    fi

    if [[ -d "${DESTDIR}" ]]; then
        rm -rf "${DESTDIR}"
    fi

    mkdir "${DESTDIR}"

    case "${TYPE}" in
        innodb56)
            url="https://dev.mysql.com/get/Downloads/MySQL-5.6/"
            tarball="mysql-${VERSION}-linux-glibc2.12-${arch}.tar.gz"
            ;;
        innodb57)
            url="https://dev.mysql.com/get/Downloads/MySQL-5.7/" 
            tarball="mysql-${VERSION}-linux-glibc2.12-${arch}.tar.gz"
            ;;
        mariadb100)
            url="http://s3.amazonaws.com/percona.com/downloads/community"
            tarball="mariadb-10.0.14-linux-${arch}.tar.gz"
            ;;
        mariadb101)
            url="https://downloads.mariadb.org/f/mariadb-${VERSION}/bintar-linux-glibc_214-${arch}/"
            tarball="mariadb-${VERSION}-linux-glibc_214-${arch}.tar.gz"
            ;;
        xtradb56)
            url="https://www.percona.com/downloads/Percona-XtraDB-Cluster-56/Percona-XtraDB-Cluster-${VERSION}/binary/tarball/"
            vertmp_first=$(echo ${VERSION} | awk -F '-' '{print $1}')
            vertmp_suffix=$(echo ${VERSION} | awk -F '-' '{print $NF}')
            tarball="$(curl -s ${url} | grep -Eo Percona-XtraDB-Cluster-${vertmp_first}-rel[1-9][1-9].[0-9]-${vertmp_suffix}.[0-9].Linux.${arch}.ssl$(ssl_version).tar.gz | head -n1)"
            ;;
        xtradb57) 
            url="https://www.percona.com/downloads/Percona-XtraDB-Cluster-57/Percona-XtraDB-Cluster-${VERSION}/binary/tarball/"
            if [[ $(echo ${VERSION} | awk -F "." '{ print $3 }' | cut -d '-' -f1) -le "30" ]]; then
                vertmp_first=$(echo ${VERSION} | awk -F '-' '{print $1}')
                vertmp_suffix=$(echo ${VERSION} | awk -F '.' '{print $NF}')
                tarball="$(curl -s ${url} | grep -Eo Percona-XtraDB-Cluster-${vertmp_first}-rel[1-9][1-9]-${vertmp_suffix}.[0-9].Linux.${arch}.ssl$(ssl_version).tar.gz | head -n1)"
            elif [[ $(echo ${VERSION} | awk -F "." '{ print $3 }' | cut -d '-' -f1) -ge "31" ]]; then
                vertmp_first=$(echo ${VERSION} | awk -F '-' '{print $1}')
                vertmp_suffix=$(echo ${VERSION} | awk -F '.' '{print $NF}')
                tarball="$(curl -s ${url} | grep -Eo Percona-XtraDB-Cluster-${vertmp_first}-rel[1-9][1-9]-${vertmp_suffix}.[0-9].Linux.${arch}.glibc2.12.tar.gz | head -n1)"
            fi
            ;;
        *)
            echo "Err: Specified unsupported ${TYPE}."
            echo "Supported types are: innodb56 innodb57 xtradb56 xtradb57 mariadb100 mariadb101."
            echo "Example: $0 --type=xtradb57 --version=5.7.30-31.43"
            exit 1
            ;;
    esac

    # Check if tarball exist before any download
    if ! wget --spider "${url}/${tarball}" 2>/dev/null || [[ -z ${tarball} ]]; then
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
VERSION="5.7.31-31.45"
DESTDIR="./server"
parse_arguments PICK-ARGS-FROM-ARGV "$@"
main