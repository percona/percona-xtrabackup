#!/bin/bash

set -e

function usage() {
    cat <<EOF
Usage: $0 [OPTIONS]
    The following options may be given :
        --type=             Pass innodb80 or xtradb80 for DB type(default=xtradb80)
        --pxb-type=         Specify release or debug build of PXB used for tests(default=release)
        --version=          Version of tarball
        --destdir=          OPTIONAL: Destination directory where the tarball will be extracted(default=\"./server\")
        --help) usage ;;
EOF
    exit 1
}

function ssl_version() {
    sslv=$(ls -la {/,/usr/}{lib64,lib,lib/x86_64-linux-gnu}/libssl.so.1.* 2>/dev/null | sed 's/.*[.]so//; s/[^0-9]//g' | head -1)

    case ${sslv} in
        100|101) ;;
        102) unset sslv; sslv="102.$OS" ;;
        *)
            >&2 echo "tarball for your openssl version (${sslv}) is not available"
            exit 1
            ;;
    esac
    echo ${sslv}
}

function glibc_version() {
    glibc=$(ldd --version | head -1 | awk '{print $NF}')
    case ${glibc} in
        2.12|2.17|2.27|2.28|2.31|2.34|2.35) ;;
        *)
            >&2 echo "tarball for your glibc version (${glibc}) is not available"
            exit 1
            ;;
    esac
    echo ${glibc}
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
            --pxb-type=*) PXB_TYPE="${val}" ;;
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

check_url() {
  url=$1
  tarball=$2
  retries=10
  # upstream sometimes reports file does not exists due to transient error
  # we should retry a few times before failing back
  tries=0
  while [[ ${tries} -lt ${retries} ]]; do
    if ! wget --spider "${url}/${tarball}" 2>/dev/null; then
      tries=$((tries+1))
    else
      return 0;
    fi
  done
  return 1;
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
            url="https://dev.mysql.com/get/Downloads/MySQL-8.4"
            fallback_url="https://downloads.mysql.com/archives/get/p/23/file"
	    # Strip '-<number>' where <number> is 1-100
            clean_version="${VERSION%-[0-9][0-9]}"
            clean_version="${clean_version%-[0-9]}"

	    if [ "${OS}" == "deb" ]; then
                tarball="mysql-${clean_version}-linux-glibc2.28-${arch}.tar.xz"
            else
                tarball="mysql-${clean_version}-linux-glibc2.17-${arch}.tar.xz"
            fi
            if ! check_url "${url}" "${tarball}"; then
                    unset url
                    url=${fallback_url}
            fi
            ;;
        xtradb80)
            url="https://www.percona.com/downloads/Percona-Server-8.4/Percona-Server-${VERSION}/binary/tarball"
            if [[ ${PXB_TYPE} == "Debug" ]] || [[ ${PXB_TYPE} == "debug" ]]; then
                SUFFIX="-debug"
              else
                SUFFIX="-minimal"
            fi
            tarball="Percona-Server-${VERSION}-Linux.${arch}.glibc$(glibc_version)${SUFFIX}.tar.gz"

            ;;
        *) 
            echo "Err: Specified unsupported ${TYPE}."
            echo "Supported types are: innodb80, xtradb80."
            echo "Example: $0 --type=xtradb80 --version=8.0.18-9"
            exit 1
            ;;
    esac

    # Check if tarball exist before any download
    if ! check_url "${url}" "${tarball}"; then
        echo "Version you specified(${VERSION}) does not exist on ${url}/${tarball}"
        exit 1
    else
        echo "Downloading ${tarball}"
        wget -qc "${url}/${tarball}"
    fi

    echo "Unpacking ${tarball} into ${DESTDIR}"
    # Separate the gunzip from the tar
    tar_file=${tarball}
    if [[ ${tar_file} =~ .*\.gz$ ]]; then
        gunzip "${tar_file}"
        tar_file=${tar_file%.gz}
    fi

    if [[ -x ${tar_file} ]]; then
        echo "Err: Cannot find the tar file : ${tar_file}"
        exit 1
    fi
    tar xf "${tar_file}" -C "${DESTDIR}"
    echo "Removing tar file ${tar_file}"
    rm "${tar_file}"

    sourcedir="${DESTDIR}/$(ls ${DESTDIR})"
    if test -n "${sourcedir}"; then
        mv "${sourcedir}"/* "${DESTDIR}"
        rm -rf "${sourcedir}"
    fi
}

TYPE="xtradb80"
PXB_TYPE="release"
VERSION="8.4.0-1"
DESTDIR="./server"
parse_arguments PICK-ARGS-FROM-ARGV "$@"
main
