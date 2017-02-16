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

# Create a temporary working directory
BASEINSTALLDIR="$(cd "$WORKDIR" && TMPDIR="$WORKDIR_ABS" mktemp -d xtrabackup-build.XXXXXX)"
INSTALLDIR="$WORKDIR_ABS/$BASEINSTALLDIR/percona-xtrabackup-$XTRABACKUP_VERSION-$(uname -s)-$(uname -m)"   # Make it absolute

mkdir "$INSTALLDIR"

# Build
(
    cd "$WORKDIR"

    # Build proper
    (
	cd $SOURCEDIR

        # Install the files
        mkdir -p "$INSTALLDIR"
        cmake -DBUILD_CONFIG=xtrabackup_release -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" \
          -DINSTALL_MYSQLTESTDIR=percona-xtrabackup-${XB_VERSION_MAJOR}.${XB_VERSION_MINOR}-test -DINSTALL_MANDIR=${INSTALLDIR}/man \
          -DDOWNLOAD_BOOST=1 -DWITH_BOOST=${WORKDIR_ABS}/libboost \
          -DMYSQL_UNIX_ADDR=/var/run/mysqld/mysqld.sock .
        make $MAKE_JFLAG
        make install

    )
    exit_value=$?

    if test "x$exit_value" = "x0"
    then
      $TAR czf "percona-xtrabackup-$XTRABACKUP_VERSION-$(uname -s)-$(uname -m).tar.gz" \
            --owner=0 --group=0 -C "$INSTALLDIR/../" \
            "percona-xtrabackup-$XTRABACKUP_VERSION-$(uname -s)-$(uname -m)"
    fi

    # Clean up build dir
    rm -rf "percona-xtrabackup-$XTRABACKUP_VERSION-$(uname -s)-$(uname -m)"
    
    exit $exit_value
    
)
exit_value=$?

# Clean up
rm -rf "$WORKDIR_ABS/$BASEINSTALLDIR"

exit $exit_value

