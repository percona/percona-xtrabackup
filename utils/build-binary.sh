#!/bin/sh
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
    WORKDIR="$(pwd)"
    
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

SOURCEDIR="$(cd $(dirname "$0"); cd ..; pwd)"
test -e "$SOURCEDIR/Makefile" || exit 2

# Read XTRABACKUP_VERSION from the VERSION file
. $SOURCEDIR/VERSION

# Build information
REVISION="$(cd "$SOURCEDIR"; bzr log -r-1 | grep ^revno: | cut -d ' ' -f 2)"

# Compilation flags
export CC=${CC:-gcc}
export CXX=${CXX:-gcc}
export CFLAGS="-fPIC -Wall -O3 -g -static-libgcc -fno-omit-frame-pointer ${CFLAGS:-}"
export CXXFLAGS="-O2 -fno-omit-frame-pointer -g -pipe -Wall -Wp,-D_FORTIFY_SOURCE=2 -fno-exceptions ${CXXFLAGS:-}"
export MAKE_JFLAG=-j4

# Create a temporary working directory
INSTALLDIR="$(cd "$WORKDIR" && TMPDIR="$WORKDIR_ABS" mktemp -d xtrabackup-build.XXXXXX)"
INSTALLDIR="$WORKDIR_ABS/$INSTALLDIR/xtrabackup-$XTRABACKUP_VERSION"   # Make it absolute

mkdir "$INSTALLDIR"

export AUTO_DOWNLOAD=yes

# Build
(
    cd "$WORKDIR"

    # Make a copy of the source
    rm -f "xtrabackup-$XTRABACKUP_VERSION"

    cp -a "$SOURCEDIR" "xtrabackup-$XTRABACKUP_VERSION"

    # Build proper
    (
        cd "xtrabackup-$XTRABACKUP_VERSION"

        bash utils/build.sh xtradb55
        bash utils/build.sh xtradb
        bash utils/build.sh 5.1

        # Install the files
        mkdir "$INSTALLDIR/bin" "$INSTALLDIR/share"

        install -m 755 Percona-Server/storage/innodb_plugin/xtrabackup/xtrabackup \
            "$INSTALLDIR/bin"
        install -m 755 Percona-Server-5.5/storage/innobase/xtrabackup/xtrabackup_55 \
            "$INSTALLDIR/bin"
        install -m 755 innobackupex "$INSTALLDIR/bin"
        ln -s innobackupex "$INSTALLDIR/bin/innobackupex-1.5.1"
        install -m 755 mysql-5.1/storage/innobase/xtrabackup/xtrabackup_51 \
            "$INSTALLDIR/bin"
        install -m 755 libtar-1.2.11/libtar/tar4ibd "$INSTALLDIR/bin"
        cp -R test "$INSTALLDIR/share/xtrabackup-test"

    )

    $TAR czf "xtrabackup-$XTRABACKUP_VERSION.tar.gz" \
        --owner=0 --group=0 -C "$INSTALLDIR/../" "xtrabackup-$XTRABACKUP_VERSION"

    # Clean up build dir
    rm -rf "xtrabackup-$XTRABACKUP_VERSION"
    
)

# Clean up
rm -rf "$INSTALLDIR"

