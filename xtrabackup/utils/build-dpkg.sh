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
go_out="$(getopt --options "k:KbBSnT"\
    --longoptions key:,nosign,binary,binarydep,source,dummy,notransitional \
    --name "$(basename "$0")" -- "$@")"
test $? -eq 0 || exit 1
eval set -- $go_out

BUILDPKG_KEY=''
DPKG_BINSRC=''
DUMMY=''
NOTRANSITIONAL=''

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
    esac
done

# Read XTRABACKUP_VERSION from the VERSION file
. VERSION

DEBIAN_VERSION="$(lsb_release -sc)"
REVISION="$(bzr revno 2>/dev/null || cat REVNO)"
FULL_VERSION="$XTRABACKUP_VERSION-$REVISION.$DEBIAN_VERSION"

# Build information
export CC=${CC:-gcc}
export CXX=${CXX:-g++}
export CFLAGS="-fPIC -Wall -O3 -g -static-libgcc -fno-omit-frame-pointer"
export CXXFLAGS="-O2 -fno-omit-frame-pointer -g -pipe -Wall -Wp,-D_FORTIFY_SOURCE=2"
export MAKE_JFLAG=-j4

export DEB_BUILD_OPTIONS='debug nocheck'
export DEB_CFLAGS_APPEND="$CFLAGS -DXTRABACKUP_REVISION=\\\"$REVISION\\\""
export DEB_CXXFLAGS_APPEND="$CXXFLAGS -DXTRABACKUP_REVISION=\\\"$REVISION\\\""
export DEB_DUMMY="$DUMMY"

# Build
(
    # we assume we're in the source directory, as we assume
    # that we've done "make dist" before.

    (
        T=`ls -1 ../percona-xtrabackup*tar.gz`; TO=`echo $T|sed -e 's/percona-xtrabackup-/percona-xtrabackup_/; s/\.tar\.gz/.orig.tar.gz/;'`; mv $T $TO

        # Move the debian dir to the appropriate place
        cp -a "utils/debian/" .

        # Don't build transitional packages if requested
        if test "x$NOTRANSITIONAL" = "xyes"
        then
            sed -i '/Package: xtrabackup/,/^$/d' debian/control
        fi

        # Update distribution
        dch -m -D "$DEBIAN_VERSION" --force-distribution -v "$XTRABACKUP_VERSION-$REVISION-1" 'Update distribution'
        # Issue dpkg-buildpackage command
        dpkg-buildpackage $DPKG_BINSRC $BUILDPKG_KEY
 
    )
)
