#!/bin/bash

function check_libs {
    local elf_path=$1

    for elf in $(find $elf_path -maxdepth 1 -exec file {} \; | grep 'ELF ' | cut -d':' -f1); do
        echo "$elf"
        ldd "$elf"
    done
    return
}

function prepare {
    CURDIR=$(pwd)
    TMP_DIR="$CURDIR/temp"

    mkdir -p "$CURDIR"/temp

    if [ -f /etc/redhat-release ]; then 
        export PATH=$PATH:/usr/sbin
    fi

    TARBALLS=""
    for tarball in $(find . -name "*.tar.gz"); do
        TARBALLS+=" $(basename $tarball)"
    done

    DIRLIST="bin lib/private lib/plugin"
}

function install_deps {
    if [ -f /etc/redhat-release ]; then 
        yum install -y epel-release
        yum install -y perl-Time-HiRes perl numactl numactl-libs libaio libev libatomic libidn libcurl-devel || true
    else
        apt-get install -y perl numactl libaio-dev libev4 libatomic1 libidn11 libcurl4-openssl-dev || true

    fi
}

main () {
    prepare
    for tarfile in $TARBALLS; do
        echo "Unpacking tarball: $tarfile"
        cd "$TMP_DIR"
        tar xf "$CURDIR/$tarfile"
        cd "${tarfile%.tar.gz}"

        echo "Building ELFs ldd output list"
        for DIR in $DIRLIST; do
            if ! check_libs "$DIR" >> "$TMP_DIR"/libs_err.log; then
                echo "There is an error with libs linkage"
                echo "Displaying log: "
                cat "$TMP_DIR"/libs_err.log
                exit 1
            fi
        done

        echo "Checking for missing libraries"
        if [[ ! -z $(grep "not found" $TMP_DIR/libs_err.log) ]]; then
            echo "ERROR: There are missing libraries: "
            grep "not found" "$TMP_DIR"/libs_err.log
            echo "Log: "
            cat "$TMP_DIR"/libs_err.log
            exit 1
        fi

        # Run MTRs
        if [[ -z "${SKIP_MTR}" ]] && [[ "$SKIP_MTR" != "true" ]]; then
            export PATH=$PATH:$(pwd)/bin
            cd percona-xtrabackup-2.4-test
            ./bootstrap.sh xtradb57
            ./run.sh -j $(nproc) -f
        fi

        rm -rf "${TMP_DIR:?}/*"
    done
}

case "$1" in
    --install_deps) install_deps ;;
    --test) main ;;
    --help|*)
    cat <<EOF
Usage: $0 [OPTIONS]
    The following options may be given :
        --install_deps
        --test
        --help) usage ;;
Example $0 --install_deps 
EOF
    ;;
esac
