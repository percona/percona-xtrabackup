#!/bin/bash -x
#
# Execute this tool to setup and build RPMs for XtraBackup starting
# from a fresh tree
#
# Usage: build-rpm.sh [--i686] [--nosign] [--test] [target dir]
# The default target directory is the current directory. If it is not
# supplied and the current directory is not empty, it will issue an error in
# order to avoid polluting the current directory after a test run.
#
# The program will setup the rpm building environment and ultimately call
# rpmbuild with the appropiate parameters.
#

# Bail out on errors, be strict
set -ue

# Examine parameters
TARGET=''
TARGET_CFLAGS=''
TARGET_ARG=''
SIGN='--sign'  # We sign by default
TEST='no'    # We don't test by default
DUMMY=''

# Check if we have got a functional getopt(1)
if ! getopt --test
then
  go_out="$(getopt --options="iKtn" --longoptions=i686,nosign,test,dummy \
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
    TARGET="--target i686"
    TARGET_CFLAGS="-m32 -march=i686"
    TARGET_ARG='i686'
    ;;
    -K | --nosign )
    shift
    SIGN=''
    ;;
    -t | --test )
    shift
    TEST='yes'
    ;;
    -n | --dummy )
    shift
    DUMMY='--define=dummy=1'
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

SOURCEDIR="$(readlink -f $(dirname "$0")/../../../..)"
test -e "$SOURCEDIR/XB_VERSION" || exit 2

# Read version info from the XB_VERSION file
. $SOURCEDIR/XB_VERSION

XTRABACKUP_VERSION="${XB_VERSION_MAJOR}.${XB_VERSION_MINOR}.${XB_VERSION_PATCH}${XB_VERSION_EXTRA}"

# Build information
REDHAT_RELEASE="$(grep -o 'release [0-9][0-9]*' /etc/redhat-release | \
cut -d ' ' -f 2)"

if [ -z "${REVISION:-}" ]; then
  REVISION="$(cd "$SOURCEDIR"; (git rev-parse --short HEAD 2>/dev/null || cat REVNO))"
fi

# Fix problems in rpmbuild for rhel4: _libdir and _arch are not correctly set.
if test "x$REDHAT_RELEASE" == "x4" && test "x$TARGET_ARG" == "xi686"
then
  TARGET_LIBDIR='--define=_libdir=/usr/lib'
  TARGET_ARCH='--define=_arch=i386'
else
  TARGET_LIBDIR=''
  TARGET_ARCH=''
fi

# Compilation flags
export CC=${CC:-gcc}
export CXX=${CXX:-g++}
export CFLAGS="$TARGET_CFLAGS"
export CXXFLAGS="$TARGET_CFLAGS"
export MAKE_JFLAG=-j4

export MYSQL_RPMBUILD_TEST="$TEST"

(
cd "$WORKDIR"

mkdir -p BUILD SOURCES RPMS SRPMS SPECS

# Create the source archive
(cd "$SOURCEDIR"; cmake .; make DUMMY="$DUMMY" dist)

cp $SOURCEDIR/percona-xtrabackup-$XTRABACKUP_VERSION.tar.gz SOURCES/
cp $SOURCEDIR/storage/innobase/xtrabackup/utils/percona-xtrabackup.spec SPECS/
#
if [ -z "${XB_VERSION_EXTRA:-}" ]; then
  EXTRAVER="%{nil}"
  RPM_EXTRAVER=1
else 
  EXTRAVER=${XB_VERSION_EXTRA}
  RPM_EXTRAVER=${XB_VERSION_EXTRA#-}.1
fi
#

sed -i "s:@@XB_VERSION_EXTRA@@:${EXTRAVER}:g" SPECS/percona-xtrabackup.spec
sed -i "s:@@XB_RPM_VERSION_EXTRA@@:${RPM_EXTRAVER}:g" SPECS/percona-xtrabackup.spec
sed -i "s:@@XB_VERSION_MAJOR@@:${XB_VERSION_MAJOR}:g" SPECS/percona-xtrabackup.spec
sed -i "s:@@XB_VERSION_MINOR@@:${XB_VERSION_MINOR}:g" SPECS/percona-xtrabackup.spec
sed -i "s:@@XB_VERSION_PATCH@@:${XB_VERSION_PATCH}:g" SPECS/percona-xtrabackup.spec
sed -i "s:@@XB_REVISION@@:${REVISION}:g" SPECS/percona-xtrabackup.spec

# Issue RPM command
rpmbuild --define "_topdir ${WORKDIR_ABS}" $SIGN $TARGET $TARGET_LIBDIR $TARGET_ARCH $DUMMY -ba --clean SPECS/percona-xtrabackup.spec 

)
