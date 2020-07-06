#!/bin/bash
#
# Execute this tool to setup the environment and build binary releases
# for xtrabackup starting from a fresh tree.
#
# Usage: build-binary.sh [target dir]
# The default target directory is the current directory. If it is not
# supplied and the current directory is not empty, it will issue an error in
# order to avoid polluting the current directory after a test run.
#

# Bail out on errors, be strict
set -ue

# Examine parameters
TARGET="$(uname -m)"
TARGET_CFLAGS=''

# Some programs that may be overriden
TAR=${TAR:-tar}

# Check if we have a functional getopt(1)
if ! getopt --test
then
    go_out="$(getopt --options="i" --longoptions=i686 \
        --name="$(basename "$0")" -- "$@")"
    test $? -eq 0 || exit 1
    eval set -- $go_out
fi

for arg
do
    case "$arg" in
    -- ) shift; break;;
    -i | --i686 )
        shift
        TARGET="i686"
        TARGET_CFLAGS="-m32 -march=i686"
        ;;
    esac
done

# Working directory
if test "$#" -eq 0
then
    WORKDIR="$(readlink -f $(dirname $0)/../../../../)"
    
    # Check that the current directory is not empty
    if test "x$(echo *)" != "x*"
    then
        echo >&2 \
            "Current directory is not empty. Use $0 . to force build in ."
        exit 1
    fi

    WORKDIR_ABS="$(cd "$WORKDIR"; pwd)"

elif test "$#" -eq 1
then
    WORKDIR="$1"

    # Check that the provided directory exists and is a directory
    if ! test -d "$WORKDIR"
    then
        echo >&2 "$WORKDIR is not a directory"
        exit 1
    fi

    WORKDIR_ABS="$(cd "$WORKDIR"; pwd)"

else
    echo >&2 "Usage: $0 [target dir]"
    exit 1

fi

SOURCEDIR="$(cd $(dirname "$0"); cd ../../../../; pwd)"
test -e "$SOURCEDIR/XB_VERSION" || exit 2

# Read version info from the XB_VERSION file
. $SOURCEDIR/XB_VERSION

XTRABACKUP_VERSION="${XB_VERSION_MAJOR}.${XB_VERSION_MINOR}.${XB_VERSION_PATCH}${XB_VERSION_EXTRA}"

# Compilation flags
export CC=${CC:-gcc}
export CXX=${CXX:-g++}
export CFLAGS=${CFLAGS:-}
export CXXFLAGS=${CXXFLAGS:-}
export MAKE_JFLAG=-j4

CMAKE_BIN='cmake'

if [ -f /etc/redhat-release ]; then
    RHEL=$(rpm --eval %rhel)
    if [[ "${RHEL}" -lt 8 ]]; then
        CMAKE_BIN='cmake3'
    fi
fi

# Create a temporary working directory
BASEINSTALLDIR="$(cd "$WORKDIR" && TMPDIR="$WORKDIR_ABS" mktemp -d xtrabackup-build.XXXXXX)"
INSTALLDIR="$WORKDIR_ABS/$BASEINSTALLDIR/percona-xtrabackup-$XTRABACKUP_VERSION-$(uname -s)-$(uname -m)$GLIBC_VER"   # Make it absolute

mkdir "$INSTALLDIR"

# Build
(
    cd "$WORKDIR"

    # Build proper
    (
	cd $SOURCEDIR

        # Install the files
        mkdir -p "$INSTALLDIR"
        $CMAKE_BIN -DBUILD_CONFIG=xtrabackup_release -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" \
          -DINSTALL_MYSQLTESTDIR=percona-xtrabackup-${XB_VERSION_MAJOR}.${XB_VERSION_MINOR}-test -DINSTALL_MANDIR=${INSTALLDIR}/man \
          -DDOWNLOAD_BOOST=1 -DWITH_BOOST=${WORKDIR_ABS}/libboost \
          -DMYSQL_UNIX_ADDR=/var/run/mysqld/mysqld.sock -DFORCE_INSOURCE_BUILD=1 .
        make $MAKE_JFLAG
        make install

    )
    exit_value=$?

    if test "x$exit_value" = "x0"
    then
        cd $INSTALLDIR
        if [ ! -d lib/private ]; then
            mkdir -p lib/private
        fi

        LIBLIST="libgcrypt.so libcrypto.so libssl.so libreadline.so libtinfo.so libsasl2.so libcurl.so libldap liblber libssh libbrotlidec.so libbrotlicommon.so libgssapi_krb5.so libkrb5.so libkrb5support.so libk5crypto.so librtmp.so libgssapi.so libcrypt.so libfreebl3.so libssl3.so libsmime3.so libnss3.so libnssutil3.so libplds4.so libplc4.so libnspr4.so libssl3.so libplds4.so"
        DIRLIST="bin lib lib/private lib/plugin"

        LIBPATH=""

        function gather_libs {
            local elf_path=$1
            for lib in $LIBLIST; do
                for elf in $(find $elf_path -maxdepth 1 -exec file {} \; | grep 'ELF ' | cut -d':' -f1); do
                    IFS=$'\n'
                    for libfromelf in $(ldd $elf | grep $lib | awk '{print $3}'); do
                        if [ ! -f lib/private/$(basename $(readlink -f $libfromelf)) ] && [ ! -L lib/$(basename $(readlink -f $libfromelf)) ]; then
                            echo "Copying lib $(basename $(readlink -f $libfromelf))"
                            cp $(readlink -f $libfromelf) lib/private

                            echo "Symlinking lib $(basename $(readlink -f $libfromelf))"
                            cd lib
                            ln -s private/$(basename $(readlink -f $libfromelf)) $(basename $(readlink -f $libfromelf))
                            cd -
                            
                            LIBPATH+=" $(echo $libfromelf | grep -v $(pwd))"
                        fi
                    done
                    unset IFS
                done
            done
        }

        function set_runpath {
            # Set proper runpath for bins but check before doing anything
            local elf_path=$1
            local r_path=$2
            for elf in $(find $elf_path -maxdepth 1 -exec file {} \; | grep 'ELF ' | cut -d':' -f1); do
                echo "Checking LD_RUNPATH for $elf"
                if [ -z $(patchelf --print-rpath $elf) ]; then
                    echo "Changing RUNPATH for $elf"
                    patchelf --set-rpath $r_path $elf
                fi
            done
        }

        function replace_libs {
            local elf_path=$1
            for libpath_sorted in $LIBPATH; do
                for elf in $(find $elf_path -maxdepth 1 -exec file {} \; | grep 'ELF ' | cut -d':' -f1); do
                    LDD=$(ldd $elf | grep $libpath_sorted|head -n1|awk '{print $1}')
                    if [[ ! -z $LDD  ]]; then
                        echo "Replacing lib $(basename $(readlink -f $libpath_sorted)) for $elf"
                        patchelf --replace-needed $LDD $(basename $(readlink -f $libpath_sorted)) $elf
                    fi
                    # Add if present in LDD to NEEDED
                    if [[ ! -z $LDD ]] && [[ -z "$(readelf -d $elf | grep $(basename $libpath_sorted | awk -F'.' '{print $1}'))" ]]; then
                        patchelf --add-needed $(basename $(readlink -f $libpath_sorted)) $elf
                    fi
                done
            done
        }

        # Gather libs
        for DIR in $DIRLIST; do
            gather_libs $DIR
        done

        # Set proper runpath
        set_runpath bin '$ORIGIN/../lib/private/'
        set_runpath lib '$ORIGIN/private/'
        set_runpath lib/plugin '$ORIGIN/../private/'
        set_runpath lib/private '$ORIGIN'

        # Replace libs
        for DIR in $DIRLIST; do
            replace_libs $DIR
        done

        cd "$WORKDIR"
        $TAR czf "percona-xtrabackup-$XTRABACKUP_VERSION-$(uname -s)-$(uname -m)$GLIBC_VER.tar.gz" \
                --owner=0 --group=0 -C "$INSTALLDIR/../" \
                "percona-xtrabackup-$XTRABACKUP_VERSION-$(uname -s)-$(uname -m)$GLIBC_VER"

        rm -rf $INSTALLDIR/percona-xtrabackup-8.0-test 2> /dev/null
        find $INSTALLDIR -type f -exec file '{}' \; | grep ': ELF ' | cut -d':' -f1 | xargs strip --strip-unneeded
        $TAR czf "percona-xtrabackup-$XTRABACKUP_VERSION-$(uname -s)-$(uname -m)$GLIBC_VER-minimal.tar.gz" \
            --owner=0 --group=0 -C "$INSTALLDIR/../" \
            "percona-xtrabackup-$XTRABACKUP_VERSION-$(uname -s)-$(uname -m)$GLIBC_VER"
    fi

    # Clean up build dir
    rm -rf "percona-xtrabackup-$XTRABACKUP_VERSION-$(uname -s)-$(uname -m)$GLIBC_VER"
    
    exit $exit_value
    
)
exit_value=$?

# Clean up
rm -rf "$WORKDIR_ABS/$BASEINSTALLDIR"

exit $exit_value

