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

# Enable Pro build
FIPSMODE=0

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
if [[ "x${FIPSMODE}" == "x1" ]]; then
    PRODUCT_FULL="percona-xtrabackup-pro-$XTRABACKUP_VERSION-$(uname -s)-$(uname -m)$GLIBC_VER"
else
    PRODUCT_FULL="percona-xtrabackup-$XTRABACKUP_VERSION-$(uname -s)-$(uname -m)$GLIBC_VER"
fi
BASEINSTALLDIR="$(cd "$WORKDIR" && TMPDIR="$WORKDIR_ABS" mktemp -d xtrabackup-build.XXXXXX)"
INSTALLDIR="$WORKDIR_ABS/$BASEINSTALLDIR/$PRODUCT_FULL"   # Make it absolute

mkdir "$INSTALLDIR"

# Build
    BUILD_PARAMETER=""
    if [[ "x${FIPSMODE}" == "x1" ]]; then
        BUILD_PARAMETER="-DPROBUILD=1"
    fi
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
          -DMINIMAL_RELWITHDEBINFO=OFF ${BUILD_PARAMETER}\
          -DMYSQL_UNIX_ADDR=/var/run/mysqld/mysqld.sock -DFORCE_INSOURCE_BUILD=1 -DWITH_ZLIB=bundled -DWITH_ZSTD=bundled .
        make $MAKE_JFLAG
        make install

    )
    exit_value=$?

    if test "x$exit_value" = "x0"
    then

        LIBLIST="libgcrypt.so libaio.so libprocps.so libreadline.so libtinfo.so libsasl2.so librtmp.so libssl3.so libsmime3.so libnss3.so libnssutil3.so libplds4.so libplc4.so libnspr4.so"
        DIRLIST="bin lib lib/private lib/plugin"

        LIBPATH=""

    function gather_libs {
        local elf_path=$1
        for lib in ${LIBLIST}; do
            for elf in $(find ${elf_path} -maxdepth 1 -exec file {} \; | grep 'ELF ' | cut -d':' -f1); do
                IFS=$'\n'
                for libfromelf in $(ldd ${elf} | grep ${lib} | awk '{print $3}'); do
                    lib_realpath="$(readlink -f ${libfromelf})"
                    lib_realpath_basename="$(basename $(readlink -f ${libfromelf}))"
                    lib_without_version_suffix=$(echo ${lib_realpath_basename} | awk -F"." 'BEGIN { OFS = "." }{ print $1, $2}')

                    if [ ! -f "lib/private/${lib_realpath_basename}" ] && [ ! -L "lib/private/${lib_without_version_suffix}" ]; then
                    
                        echo "Copying lib ${lib_realpath_basename}"
                        cp ${lib_realpath} lib/private

                        if [ ${lib_realpath_basename} != ${lib_without_version_suffix} ] && [ ! -L lib/private/${lib_without_version_suffix} ]; then
                            echo "Symlinking lib from ${lib_realpath_basename} to ${lib_without_version_suffix}"
                            cd lib/private
                            ln -s ${lib_realpath_basename} ${lib_without_version_suffix}
                            cd -
                        fi

                        patchelf --set-soname ${lib_without_version_suffix} lib/private/${lib_realpath_basename}

                        LIBPATH+=" $(echo ${libfromelf} | grep -v $(pwd))"
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
            for elf in $(find ${elf_path} -maxdepth 1 -exec file {} \; | grep 'ELF ' | cut -d':' -f1); do
                echo "Checking LD_RUNPATH for ${elf}"
                if [ -z $(patchelf --print-rpath ${elf}) ]; then
                    echo "Changing RUNPATH for ${elf}"
                    patchelf --set-rpath ${r_path} ${elf}
                fi
            done
        }

        function replace_libs {
            local elf_path=$1
            for libpath_sorted in ${LIBPATH}; do
                for elf in $(find ${elf_path} -maxdepth 1 -exec file {} \; | grep 'ELF ' | cut -d':' -f1); do
                    LDD=$(ldd ${elf} | grep ${libpath_sorted}|head -n1|awk '{print $1}')
                    lib_realpath_basename="$(basename $(readlink -f ${libpath_sorted}))"
                    lib_without_version_suffix="$(echo ${lib_realpath_basename} | awk -F"." 'BEGIN { OFS = "." }{ print $1, $2}')"
                    if [[ ! -z $LDD  ]]; then
                        echo "Replacing lib ${lib_realpath_basename} to ${lib_without_version_suffix} for ${elf}"
                        patchelf --replace-needed ${LDD} ${lib_without_version_suffix} ${elf}
                    fi
                done
            done
        }

        function check_libs {
            local elf_path=$1
            for elf in $(find ${elf_path} -maxdepth 1 -exec file {} \; | grep 'ELF ' | cut -d':' -f1); do
                if ! ldd ${elf}; then
                    exit 1
                fi
            done
        }

        function link {
            if [ ! -d lib/private ]; then
                mkdir -p lib/private
            fi
            # Gather libs
            for DIR in ${DIRLIST}; do
                gather_libs ${DIR}
            done

            # Set proper runpath
            set_runpath bin '$ORIGIN/../lib/private/'
            set_runpath lib '$ORIGIN/private/'
            set_runpath lib/plugin '$ORIGIN/../private/'
            set_runpath lib/private '$ORIGIN'

            # Replace libs
            for DIR in ${DIRLIST}; do
                replace_libs ${DIR}
            done

            # Make final check in order to determine any error after linkage
            for DIR in ${DIRLIST}; do
                check_libs ${DIR}
            done
        }

        cd "$WORKDIR"

        mkdir "$WORKDIR_ABS/$BASEINSTALLDIR/minimal"
        cp -r "$WORKDIR_ABS/$BASEINSTALLDIR/$PRODUCT_FULL" "$WORKDIR_ABS/$BASEINSTALLDIR/minimal/$PRODUCT_FULL-minimal"

        # NORMAL TARBALL
        cd "$INSTALLDIR"
        link

        cd "$WORKDIR_ABS/$BASEINSTALLDIR/minimal/$PRODUCT_FULL-minimal"
        rm -rf percona-xtrabackup-8.0-test 2> /dev/null
        find . -type f -exec file '{}' \; | grep ': ELF ' | cut -d':' -f1 | xargs strip --strip-unneeded
        link

        cd "$WORKDIR"
        if [[ "x${FIPSMODE}" == "x1" ]]; then
            $TAR czf "percona-xtrabackup-pro-$XTRABACKUP_VERSION-$(uname -s)-$(uname -m)$GLIBC_VER.tar.gz" \
                    --owner=0 --group=0 -C "$INSTALLDIR/../" \
                    "percona-xtrabackup-$XTRABACKUP_VERSION-$(uname -s)-$(uname -m)$GLIBC_VER"

            $TAR czf "percona-xtrabackup-pro-$XTRABACKUP_VERSION-$(uname -s)-$(uname -m)$GLIBC_VER-minimal.tar.gz" \
                    --owner=0 --group=0 -C "$INSTALLDIR/../minimal/" \
                    "percona-xtrabackup-$XTRABACKUP_VERSION-$(uname -s)-$(uname -m)$GLIBC_VER-minimal"
        else
            $TAR czf "percona-xtrabackup-$XTRABACKUP_VERSION-$(uname -s)-$(uname -m)$GLIBC_VER.tar.gz" \
                    --owner=0 --group=0 -C "$INSTALLDIR/../" \
                    "percona-xtrabackup-$XTRABACKUP_VERSION-$(uname -s)-$(uname -m)$GLIBC_VER"

            $TAR czf "percona-xtrabackup-$XTRABACKUP_VERSION-$(uname -s)-$(uname -m)$GLIBC_VER-minimal.tar.gz" \
                    --owner=0 --group=0 -C "$INSTALLDIR/../minimal/" \
                    "percona-xtrabackup-$XTRABACKUP_VERSION-$(uname -s)-$(uname -m)$GLIBC_VER-minimal"
        fi
    fi

    # Clean up build dir
    rm -rf "percona-xtrabackup-$XTRABACKUP_VERSION-$(uname -s)-$(uname -m)$GLIBC_VER"
    
    exit $exit_value
    
)
exit_value=$?

# Clean up
rm -rf "$WORKDIR_ABS/$BASEINSTALLDIR"

exit $exit_value

