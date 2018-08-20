#!/bin/bash
# Usage: build-dpkg.sh [target dir]
# The default target directory is the current directory. If it is not
# supplied and the current directory is not empty, it will issue an error in
# order to avoid polluting the current directory after a test run.
#
# The program will setup the dpkg building environment and ultimately call
# dpkg-buildpackage with the appropiate parameters.
#

# Bail out on errors, be strict
set -ue

# Examine parameters
go_out="$(getopt --options "k:KbBSnTtD" --longoptions \
    key:,nosign,binary,binarydep,source,dummy,notransitional,transitional \
    --name "$(basename "$0")" -- "$@")"
test $? -eq 0 || exit 1
eval set -- $go_out

BUILDPKG_KEY=''
DPKG_BINSRC=''
DUMMY=''
NOTRANSITIONAL='yes'
PACKAGE_SUFFIX='-1'

for arg
do
    case "$arg" in
    -- ) shift; break;;
    -k | --key ) shift; BUILDPKG_KEY="-pgpg -k$1"; shift;;
    -K | --nosign ) shift; BUILDPKG_KEY="-uc -us";;
    -b | --binary ) shift; DPKG_BINSRC='-b';;
    -B | --binarydep ) shift; DPKG_BINSRC='-B';;
    -S | --source ) shift; DPKG_BINSRC='-S';;
    -n | --dummy ) shift; DUMMY='yes';;
    -T | --notransitional ) shift; NOTRANSITIONAL='yes';;
    -t | --transitional ) shift; NOTRANSITIONAL='';;
    -D ) shift; PACKAGE_SUFFIX="$PACKAGE_SUFFIX.$(lsb_release -sc)";;
    esac
done

SOURCEDIR="$(readlink -f $(dirname "$0")/../../../../)"

# Read version info from the XB_VERSION file
. $SOURCEDIR/XB_VERSION

XTRABACKUP_VERSION="${XB_VERSION_MAJOR}.${XB_VERSION_MINOR}.${XB_VERSION_PATCH}${XB_VERSION_EXTRA}"

DEBIAN_VERSION="$(lsb_release -sc)"
REVISION="$(cd "$SOURCEDIR"; git rev-parse --short HEAD 2>/dev/null || cat REVNO)"
FULL_VERSION="$XTRABACKUP_VERSION-$REVISION.$DEBIAN_VERSION"

# Working directory
if test "$#" -eq 0
then
    # We build in the sourcedir
    WORKDIR="$(cd "$SOURCEDIR/.."; pwd)"

elif test "$#" -eq 1
then
    WORKDIR="$1"

    # Check that the provided directory exists and is a directory
    if ! test -d "$WORKDIR"
    then
        echo >&2 "$WORKDIR is not a directory"
        exit 1
    fi

else
    echo >&2 "Usage: $0 [target dir]"
    exit 1

fi

# Build information
export CC=${CC:-gcc}
export CXX=${CXX:-g++}
export CFLAGS=${CFLAGS:-}
export CXXFLAGS=${CXXFLAGS:-}
export MAKE_JFLAG=-j4

export DEB_BUILD_OPTIONS='debug nocheck'
export DEB_CFLAGS_APPEND="$CFLAGS"
export DEB_CXXFLAGS_APPEND="$CXXFLAGS"
export DEB_DUMMY="$DUMMY"

# Build
    # Prepare source directory for dpkg-source
        TMPDIR=$(mktemp -d)
        cd ${TMPDIR}
        cmake "$SOURCEDIR"
        make DUMMY="$DUMMY" dist
    # Create the original tarball
        mv "${TMPDIR}/percona-xtrabackup-$XTRABACKUP_VERSION.tar.gz" \
        "$WORKDIR/percona-xtrabackup_$XTRABACKUP_VERSION-$REVISION.orig.tar.gz"
        rm -fr ${TMPDIR}
    #
        cd "$WORKDIR"
        rm -fr percona-xtrabackup-$XTRABACKUP_VERSION
        tar xzf percona-xtrabackup_$XTRABACKUP_VERSION-$REVISION.orig.tar.gz
        cd percona-xtrabackup-$XTRABACKUP_VERSION
        cp -a storage/innobase/xtrabackup/utils/debian .

    # Update distribution
        dch -m -D "$DEBIAN_VERSION" --force-distribution \
            -v "$XTRABACKUP_VERSION-$REVISION$PACKAGE_SUFFIX" 'Update distribution'

    #Do not build transitional packages if requested
        if test "x$NOTRANSITIONAL" = "xyes"
        then
          sed -i '/Package: xtrabackup/,/^$/d' debian/control
        fi

    # Issue dpkg-buildpackage command
       dpkg-buildpackage $DPKG_BINSRC $BUILDPKG_KEY
       rm -rf "percona-xtrabackup-$XTRABACKUP_VERSION"
 
