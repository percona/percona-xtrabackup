#!/bin/bash

function check_libs {
    for elf in $(find $1 -maxdepth 1 -exec file {} \; | grep 'ELF ' | cut -d':' -f1); do
        echo $elf
        ldd $elf | grep "not found"
    done
    return
}

function prepare {
    CURDIR=$(pwd)

    mkdir -p $CURDIR/temp
    TMP_DIR=$CURDIR/temp
    
    TARBALL=$(basename $(find . -name "*glibc2.12.tar.gz" | sort))
}

function install_deps {
    if [ -f /etc/redhat-release ]; then 
        yum install -y perl-Time-HiRes perl numactl numactl-libs libaio libev libatomic libidn || true
    else
        apt-get install -y perl numactl libaio-dev libev4 libatomic1 libidn11 || true

    fi
}

main () {

    DIRLIST="bin lib lib/private lib/plugin"

    prepare

    echo "Unpacking tarball"
    cd $TMP_DIR
    tar xf $CURDIR/$TARBALL
    cd ${TARBALL%.tar.gz}

    echo "Checking ELFs for not found"
    for DIR in $DIRLIST; do
        check_libs $DIR | tee -a $TMP_DIR/libs_err.log
    done

    if [[ ! -z $(cat $TMP_DIR/libs_err.log | grep "not found") ]]; then
        echo "ERROR: There are missing libraries: "
        cat $TMP_DIR/libs_err.log
        exit 1
    fi

    # Run MTRs
    export PATH=$PATH:$(pwd)/bin
    cd percona-xtrabackup-8.0-test/test
    ./bootstrap.sh xtradb80
    ./run.sh -j $(nproc) -f
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
