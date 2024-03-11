#!/usr/bin/env bash

shell_quote_string() {
  echo "$1" | sed -e 's,\([^a-zA-Z0-9/_.=-]\),\\\1,g'
}

usage () {
    cat <<EOF
Usage: $0 [OPTIONS]
    The following options may be given :
        --builddir=DIR      Absolute path to the dir where all actions will be performed
        --get_sources       Source will be downloaded from github
        --build_src_rpm     If it is 1 src rpm will be built
        --build_source_deb  If it is 1 source deb package will be built
        --build_rpm         If it is 1 rpm will be built
        --build_deb         If it is 1 deb will be built
        --build_tarball     If it is 1 tarball will be built
        --install_deps      Install build dependencies(root previlages are required)
        --branch            Branch for build
        --repo              Repo for build
        --rpm_release       RPM version( default = 1)
        --deb_release       DEB version( default = 1)
        --help) usage ;;
Example $0 --builddir=/tmp/PXB --get_sources=1 --build_src_rpm=1 --build_rpm=1
EOF
        exit 1
}

append_arg_to_args () {
    args="$args "$(shell_quote_string "$1")
}

parse_arguments() {
    pick_args=
    if test "$1" = PICK-ARGS-FROM-ARGV
    then
        pick_args=1
        shift
    fi

    for arg do
        val=$(echo "$arg" | sed -e 's;^--[^=]*=;;')
        case "$arg" in
            # these get passed explicitly to mysqld
            --builddir=*) WORKDIR="$val" ;;
            --build_src_rpm=*) SRPM="$val" ;;
            --build_source_deb=*) SDEB="$val" ;;
            --build_rpm=*) RPM="$val" ;;
            --build_deb=*) DEB="$val" ;;
            --get_sources=*) SOURCE="$val" ;;
            --build_tarball=*) TARBALL="$val" ;;
            --install_deps=*) INSTALL="$val" ;;
            --branch=*) BRANCH="$val" ;;
            --repo=*) REPO="$val" ;;
            --rpm_release=*) RPM_RELEASE="$val" ;;
            --deb_release=*) DEB_RELEASE="$val" ;;
            --help) usage ;;
            *)
              if test -n "$pick_args"
              then
                  append_arg_to_args "$arg"
              fi
              ;;
        esac
    done
}

check_workdir(){
    if [ "x$WORKDIR" = "x$CURDIR" ]
    then
        echo >&2 "Current directory cannot be used for building!"
        exit 1
    else
        if ! test -d "$WORKDIR"
        then
            echo >&2 "$WORKDIR is not a directory."
            exit 1
        fi
    fi
    return
}

add_percona_yum_repo(){
    if [ ! -f /etc/yum.repos.d/percona-dev.repo ]; then
        curl -o /etc/yum.repos.d/percona-dev.repo https://jenkins.percona.com/yum-repo/percona-dev.repo
        sed -i 's:$basearch:x86_64:g' /etc/yum.repos.d/percona-dev.repo
    fi
    return
}

add_raven_yum_repo(){
    if [ -n "$1" ]; then
        version="$1"
    else
        version="8"
    fi
    yum -y install https://pkgs.dyn.su/el${version}/base/x86_64/raven-release-1.0-3.el${version}.noarch.rpm
    yum -y update --enablerepo=raven
    return
}

get_sources(){
    cd $WORKDIR
    if [ $SOURCE = 0 ]
    then
        echo "Sources will not be downloaded"
        return 0
    fi
    git clone --depth 1 --branch $BRANCH "$REPO"
    retval=$?
    if [ $retval != 0 ]
    then
        echo "There were some issues during repo cloning from github. Please retry one more time"
        exit 1
    fi

    cd percona-xtrabackup
    if [ ! -z $BRANCH ]
    then
        git reset --hard
        git clean -xdf
        git checkout $BRANCH
    fi

    REVISION=$(git rev-parse --short HEAD)
    git reset --hard
    git submodule update --init
    #
    source XB_VERSION
    cat XB_VERSION > ../percona-xtrabackup-8.0.properties
    echo "REVISION=${REVISION}" >> ../percona-xtrabackup-8.0.properties
    BRANCH_NAME="${BRANCH}"
    echo "BRANCH_NAME=$(echo ${BRANCH_NAME} | awk -F '/' '{print $(NF)}')" >> ../percona-xtrabackup-8.0.properties
    echo "PRODUCT=Percona-XtraBackup-8.0" >> ../percona-xtrabackup-8.0.properties
    echo "PRODUCT_FULL=Percona-XtraBackup-${XB_VERSION_MAJOR}.${XB_VERSION_MINOR}.${XB_VERSION_PATCH}${XB_VERSION_EXTRA}" >> ../percona-xtrabackup-8.0.properties
    echo "PRODUCT_UL_DIR=Percona-XtraBackup-8.0" >> ../percona-xtrabackup-8.0.properties
    PRODUCT="Percona-XtraBackup-8.0"
    PRODUCT_FULL="Percona-XtraBackup-${XB_VERSION_MAJOR}.${XB_VERSION_MINOR}.${XB_VERSION_PATCH}${XB_VERSION_EXTRA}"
    if [ -z "${DESTINATION}" ]; then
        export DESTINATION=experimental
    fi 
    echo "DESTINATION=${DESTINATION}" >> percona-xtrabackup-8.0.properties

    enable_venv

    $CMAKE_BIN . -DDOWNLOAD_BOOST=1 -DWITH_BOOST=${WORKDIR}/boost -DFORCE_INSOURCE_BUILD=1 -DWITH_MAN_PAGES=1
    make dist
    #
    EXPORTED_TAR=$(basename $(find . -type f -name percona-xtrabackup*.tar.gz | sort | tail -n 1))
    #
    PXBDIR=${EXPORTED_TAR%.tar.gz}
    rm -rf ${PXBDIR}
    tar xzf ${EXPORTED_TAR}
    rm -f ${EXPORTED_TAR}

    cd ${PXBDIR}
    rsync -av ../extra/libkmip/* extra/libkmip/
    sed -i 's:-Wall -Wextra -Wformat-security -Wvla -Wundef:-Wextra -Wformat-security -Wvla -Wundef:g' cmake/maintainer.cmake
    sed -i '/Werror/d' cmake/maintainer.cmake
    sed -i 's:Wstringop-truncation:Wno-stringop-truncation:g' cmake/maintainer.cmake
    sed -i "s:@@XB_VERSION_MAJOR@@:${XB_VERSION_MAJOR}:g" storage/innobase/xtrabackup/utils/percona-xtrabackup.spec
    sed -i "s:@@XB_VERSION_MINOR@@:${XB_VERSION_MINOR}:g" storage/innobase/xtrabackup/utils/percona-xtrabackup.spec
    sed -i "s:@@XB_VERSION_PATCH@@:${XB_VERSION_PATCH}:g" storage/innobase/xtrabackup/utils/percona-xtrabackup.spec
    #
    if [ -z "${XB_VERSION_EXTRA}" ]; then
        EXTRAVER="%{nil}"
        RPM_EXTRAVER=${RPM_RELEASE}
    else 
        EXTRAVER=${XB_VERSION_EXTRA}
        RPM_EXTRAVER=${XB_VERSION_EXTRA#-}.${RPM_RELEASE}
    fi
    #
    sed -i "s:@@XB_VERSION_EXTRA@@:${EXTRAVER}:g" storage/innobase/xtrabackup/utils/percona-xtrabackup.spec
    sed -i "s:@@XB_RPM_VERSION_EXTRA@@:${RPM_EXTRAVER}:g" storage/innobase/xtrabackup/utils/percona-xtrabackup.spec
    sed -i "s:@@XB_REVISION@@:${REVISION}:g" storage/innobase/xtrabackup/utils/percona-xtrabackup.spec
    #
    # create a PXB tar
    cd ${WORKDIR}/percona-xtrabackup
    tar --owner=0 --group=0 --exclude=.bzr --exclude=.git -czf ${PXBDIR}.tar.gz ${PXBDIR}
    echo "UPLOAD=UPLOAD/experimental/BUILDS/${PRODUCT}/${PRODUCT_FULL}/${BRANCH}/${REVISION}/${BUILD_ID}" >> ../percona-xtrabackup-8.0.properties
    rm -rf ${PXBDIR}

    mkdir $WORKDIR/source_tarball
    mkdir $CURDIR/source_tarball
    cp ${PXBDIR}.tar.gz $WORKDIR/source_tarball
    cp ${PXBDIR}.tar.gz $CURDIR/source_tarball
    cd $CURDIR
    rm -rf percona-xtrabackup
    return
}

get_system(){
    if [ -f /etc/redhat-release ]; then
        GLIBC_VER_TMP="$(rpm glibc -qa --qf %{VERSION})"
        export RHEL=$(rpm --eval %rhel)
        export ARCH=$(echo $(uname -m) | sed -e 's:i686:i386:g')
        export OS_NAME="el$RHEL"
        export OS="rpm"
    else
        GLIBC_VER_TMP="$(dpkg-query -W -f='${Version}' libc6 | awk -F'-' '{print $1}')"
        export ARCH=$(uname -m)
        export OS_NAME="$(lsb_release -sc)"
        export OS="deb"
    fi
    export GLIBC_VER=".glibc${GLIBC_VER_TMP}"
    return
}

enable_venv(){
    if [ "$OS" == "rpm" ]; then
        if [ "${RHEL}" -eq 7 ]; then
            source /opt/rh/devtoolset-11/enable
            source /opt/rh/rh-python36/enable
            export CMAKE_BIN="cmake3"
        elif [ "${RHEL}" -eq 6 ]; then
            source /opt/rh/devtoolset-7/enable
            source /opt/rh/rh-python36/enable
            export CMAKE_BIN="cmake3"
        fi
    fi
}

install_deps() {
    if [ $INSTALL = 0 ]
    then
        echo "Dependencies will not be installed"
        return;
    fi
    if [ ! $( id -u ) -eq 0 ]
    then
        echo "It is not possible to instal dependencies. Please run as root"
        exit 1
    fi
    CURPLACE=$(pwd)
    if [ "$OS" == "rpm" ]
    then
        yum -y install git wget yum-utils curl
        yum install -y https://repo.percona.com/yum/percona-release-latest.noarch.rpm
        if [ $RHEL = 9 ]; then
            yum-config-manager --enable ol9_distro_builder
            yum-config-manager --enable ol9_codeready_builder
            yum -y install https://dl.fedoraproject.org/pub/epel/epel-release-latest-9.noarch.rpm
        #else
            # add_percona_yum_repo
        fi
        percona-release enable tools testing
        if [ ${RHEL} = 8 -o ${RHEL} = 9 ]; then
            PKGLIST+=" binutils-devel python3-pip python3-setuptools"
            PKGLIST+=" libcurl-devel cmake libaio-devel zlib-devel libev-devel bison make"
            PKGLIST+=" rpm-build libgcrypt-devel ncurses-devel readline-devel openssl-devel"
            PKGLIST+=" vim-common rpmlint patchelf python3-wheel libudev-devel"
            if [ $RHEL = 9 ]; then
                PKGLIST+=" rsync procps-ng-devel python3-sphinx gcc gcc-c++"
            else
                yum-config-manager --enable powertools
                #wget https://jenkins.percona.com/downloads/rpm/procps-ng-devel-3.3.15-6.el8.x86_64.rpm
                wget --no-check-certificate https://downloads.percona.com/downloads/TESTING/issue-CUSTO83/procps-ng-devel-3.3.15-6.el8.x86_64.rpm
                yum -y install ./procps-ng-devel-3.3.15-6.el8.x86_64.rpm
                rm procps-ng-devel-3.3.15-6.el8.x86_64.rpm
                PKGLIST+=" libarchive"
            fi
            until yum -y install ${PKGLIST}; do
                echo "waiting"
                sleep 1
            done
            if [ $RHEL = 8 ]; then
                DEVTOOLSET10_PKGLIST+=" gcc-toolset-10-gcc-c++ gcc-toolset-10-binutils"
                DEVTOOLSET10_PKGLIST+=" gcc-toolset-10-valgrind gcc-toolset-10-valgrind-devel gcc-toolset-10-libatomic-devel"
                DEVTOOLSET10_PKGLIST+=" gcc-toolset-10-libasan-devel gcc-toolset-10-libubsan-devel gcc-toolset-10-annobin"
                until yum -y install ${DEVTOOLSET10_PKGLIST}; do
                    echo "waiting"
                    sleep 1
                done
                update-alternatives --install /usr/bin/gcc gcc /opt/rh/gcc-toolset-10/root/usr/bin/gcc 10
                update-alternatives --install /usr/bin/g++ g++ /opt/rh/gcc-toolset-10/root/usr/bin/g++ 10
                gcc --version
            fi
        else
            until yum -y install epel-release centos-release-scl; do
                yum clean all
                sleep 1
                echo "waiting"
            done
            until yum -y makecache; do
                yum clean all
                sleep 1
                echo "waiting"
            done
            PKGLIST+=" wget libcurl-devel cmake cmake3 make gcc gcc-c++ libev-devel openssl-devel rpm-build"
            PKGLIST+=" libaio-devel perl-DBD-MySQL vim-common ncurses-devel readline-devel readline"
            PKGLIST+=" zlib-devel libgcrypt-devel bison patchelf"
            PKGLIST+=" socat numactli libudev-devel libicu-devel"
            PKGLIST+=" procps-ng-devel"
            if [[ "${RHEL}" -eq 7 ]]; then
                PKGLIST+=" numactl-libs perl-Digest-MD5  python3-pip python3-setuptools python3-wheel rh-python36-python-sphinx"
            elif [[ "${RHEL}" -eq 6 ]]; then
                PKGLIST+=" rh-python36-python-pip rh-python36-docutils rh-python36-python-setuptools"
            fi
            until yum -y install ${PKGLIST}; do
                echo "waiting"
                sleep 1
            done
            if [[ "${RHEL}" -eq 7 ]]; then
                yum -y --enablerepo=centos-sclo-rh-testing install devtoolset-11-gcc-c++ devtoolset-11-binutils devtoolset-11-valgrind devtoolset-11-valgrind-devel devtoolset-11-libatomic-devel
                yum -y --enablerepo=centos-sclo-rh-testing install devtoolset-11-libasan-devel devtoolset-11-libubsan-devel
                yum -y update nss
                scl enable devtoolset-11 bash
            elif [[ "${RHEL}" -eq 6 ]]; then
                source /opt/rh/rh-python36/enable
                pip install sphinx
            fi
        fi
    else
        apt-get update
        DEBIAN_FRONTEND=noninteractive apt-get -y install lsb-release gnupg git wget curl
        export OS_NAME="$(lsb_release -sc)"
        wget https://repo.percona.com/apt/percona-release_latest.$(lsb_release -sc)_all.deb && dpkg -i percona-release_latest.$(lsb_release -sc)_all.deb
        percona-release enable tools testing
        apt-get update

        PKGLIST+=" bison cmake devscripts debconf debhelper automake bison ca-certificates libcurl4-openssl-dev"
        PKGLIST+=" cmake debhelper libaio-dev libncurses-dev libtool libz-dev libsasl2-dev vim-common"
        PKGLIST+=" libgcrypt-dev libev-dev lsb-release libudev-dev"
        PKGLIST+=" build-essential rsync libdbd-mysql-perl libnuma1 socat libssl-dev patchelf libicu-dev"
        if [ "${OS_NAME}" != "bookworm" ]; then
            PKGLIST+=" libprocps-dev"
        else
            PKGLIST+=" libproc2-dev"
        fi
        if [ "${OS_NAME}" == "bionic" ]; then
            PKGLIST+=" gcc-8 g++-8"
	fi
        if [ "${OS_NAME}" == "focal" ]; then
            PKGLIST+=" gcc-10 g++-10"
        fi
        if [ "${OS_NAME}" == "focal" -o "${OS_NAME}" == "bullseye" -o "${OS_NAME}" == "bookworm" -o "${OS_NAME}" == "jammy" ]; then
            PKGLIST+=" python3-sphinx python3-docutils"
        else
            PKGLIST+=" python-sphinx python-docutils"
        fi

        until DEBIAN_FRONTEND=noninteractive apt-get -y install ${PKGLIST}; do
            sleep 1
            echo "waiting"
        done

        if [ "${OS_NAME}" == "focal" ]; then
            update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-10 10
            update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-10 10
            gcc --version
        fi
    fi
    return;
}

get_tar(){
    TARBALL=$1
    TARFILE=$(basename $(find $WORKDIR/$TARBALL -name 'percona-xtrabackup*.tar.gz' | sort | tail -n1))
    if [ -z $TARFILE ]
    then
        TARFILE=$(basename $(find $CURDIR/$TARBALL -name 'percona-xtrabackup*.tar.gz' | sort | tail -n1))
        if [ -z $TARFILE ]
        then
            echo "There is no $TARBALL for build"
            exit 1
        else
            cp $CURDIR/$TARBALL/$TARFILE $WORKDIR/$TARFILE
        fi
    else
        cp $WORKDIR/$TARBALL/$TARFILE $WORKDIR/$TARFILE
    fi
    return
}

get_deb_sources(){
    param=$1
    echo $param
    FILE=$(basename $(find $WORKDIR/source_deb -name "percona-xtrabackup*.$param" | sort | tail -n1))
    if [ -z $FILE ]
    then
        FILE=$(basename $(find $CURDIR/source_deb -name "percona-xtrabackup*.$param" | sort | tail -n1))
        if [ -z $FILE ]
        then
            echo "There is no sources for build"
            exit 1
        else
            cp $CURDIR/source_deb/$FILE $WORKDIR/
        fi
    else
        cp $WORKDIR/source_deb/$FILE $WORKDIR/
    fi
    return
}

build_srpm(){
    if [ $SRPM = 0 ]
    then
        echo "SRC RPM will not be created"
        return;
    fi
    if [ "$OS" == "deb" ]
    then
        echo "It is not possible to build src rpm here"
        exit 1
    fi
    cd $WORKDIR
    get_tar "source_tarball"
    if [ -d rpmbuild ]; then
        rm -fr rpmbuild
    fi
    ls | grep -v percona-xtrabackup*.tar.* | xargs rm -rf
    mkdir -vp rpmbuild/{SOURCES,SPECS,BUILD,SRPMS,RPMS}

    TARFILE=$(basename $(find . -name 'percona-xtrabackup-*.tar.gz' | sort | tail -n1))
    NAME=$(echo ${TARFILE}| awk -F '-' '{print $1"-"$2}')
    VERSION=$(echo ${TARFILE}| awk -F '-' '{print $3}')
    #
    SHORTVER=$(echo ${VERSION} | awk -F '.' '{print $1"."$2}')
    TMPREL=$(echo ${TARFILE}| awk -F '-' '{print $4}')
    RELEASE=${TMPREL%.tar.gz}
    #
    cd $WORKDIR/rpmbuild/SPECS
    tar vxzf $WORKDIR/$TARFILE --wildcards '*/storage/innobase/xtrabackup/utils/*.spec' --strip=5
    #
    cd $WORKDIR
    #
    mv -fv $TARFILE $WORKDIR/rpmbuild/SOURCES
    #
    enable_venv

    rpmbuild -bs --define "_topdir $WORKDIR/rpmbuild" --define "dist .generic" rpmbuild/SPECS/percona-xtrabackup.spec

    mkdir -p ${WORKDIR}/srpm
    mkdir -p ${CURDIR}/srpm
    cp $WORKDIR/rpmbuild/SRPMS/*.src.rpm $CURDIR/srpm
    cp $WORKDIR/rpmbuild/SRPMS/*.src.rpm $WORKDIR/srpm
    return
}

build_rpm(){
    if [ $RPM = 0 ]
    then
        echo "RPM will not be created"
        return;
    fi
    if [ "$OS" == "deb" ]
    then
        echo "It is not possible to build rpm here"
        exit 1
    fi
        SRC_RPM=$(basename $(find $WORKDIR/srpm -name 'percona-xtrabackup-*.src.rpm' | sort | tail -n1))
    if [ -z $SRC_RPM ]
    then
        SRC_RPM=$(basename $(find $CURDIR/srpm -name 'percona-xtrabackup-*.src.rpm' | sort | tail -n1))
        if [ -z $SRC_RPM ]
        then
            echo "There is no src rpm for build"
            echo "You can create it using key --build_src_rpm=1"
            exit 1
        else
            cp $CURDIR/srpm/$SRC_RPM $WORKDIR
        fi
    else
        cp $WORKDIR/srpm/$SRC_RPM $WORKDIR
    fi
    cd $WORKDIR

    if [ -d rpmbuild ]; then
        rm -fr rpmbuild
    fi

    mkdir -vp rpmbuild/{SOURCES,SPECS,BUILD,SRPMS,RPMS}
    cp $SRC_RPM rpmbuild/SRPMS/
    #
    echo "RHEL=${RHEL}" >> ${CURDIR}/percona-xtrabackup-8.0.properties
    echo "ARCH=${ARCH}" >> ${CURDIR}/percona-xtrabackup-8.0.properties
    #
    SRCRPM=$(basename $(find . -name '*.src.rpm' | sort | tail -n1))

    enable_venv

    rpmbuild --define "_topdir ${WORKDIR}/rpmbuild" --define "dist .el${RHEL}" --rebuild rpmbuild/SRPMS/${SRCRPM}
    return_code=$?
    if [ $return_code != 0 ]; then
        exit $return_code
    fi
    mkdir -p ${WORKDIR}/rpm
    mkdir -p ${CURDIR}/rpm
    cp rpmbuild/RPMS/*/*.rpm ${WORKDIR}/rpm
    cp rpmbuild/RPMS/*/*.rpm ${CURDIR}/rpm    
}

build_source_deb(){
    if [ $SDEB = 0 ]
    then
        echo "source deb package will not be created"
        return;
    fi
    if [ "$OS" == "rpm" ]
    then
        echo "It is not possible to build source deb here"
        exit 1
    fi
    rm -rf percona-xtrabackup*
    get_tar "source_tarball"
    rm -f *.dsc *.orig.tar.gz *.debian.tar* *.changes

    TARFILE=$(basename $(find . -name 'percona-xtrabackup-*.tar.gz' | sort | tail -n1))
    NAME=$(echo ${TARFILE}| awk -F '-' '{print $1"-"$2}')
    VERSION=$(echo ${TARFILE%.tar.gz} | awk -F '-' '{print $3"-"$4}')
    SHORTVER=$(echo ${VERSION} | awk -F '.' '{print $1"."$2}')

    echo "DEB_RELEASE=${DEB_RELEASE}" >> ${CURDIR}/percona-xtrabackup-8.0.properties

    NEWTAR=${NAME}-83_${VERSION}.orig.tar.gz
    mv ${TARFILE} ${NEWTAR}

    tar xzf ${NEWTAR}
    cd percona-xtrabackup-$VERSION
    cp -av storage/innobase/xtrabackup/utils/debian .
    dch -D unstable --force-distribution -v "${VERSION}-${DEB_RELEASE}" "Update to new upstream release Percona XtraBackup ${VERSION}"
    dpkg-buildpackage -S

    cd ${WORKDIR}

    mkdir -p $WORKDIR/source_deb
    mkdir -p $CURDIR/source_deb

    cp *.dsc $WORKDIR/source_deb
    cp *.orig.tar.gz $WORKDIR/source_deb
    cp *.debian.tar.* $WORKDIR/source_deb
    cp *.changes $WORKDIR/source_deb
    cp *.dsc $CURDIR/source_deb
    cp *.orig.tar.gz $CURDIR/source_deb
    cp *.debian.tar.* $CURDIR/source_deb
    cp *.changes $CURDIR/source_deb
    
}

build_deb(){
    if [ $DEB = 0 ]
    then
        echo "source deb package will not be created"
        return;
    fi
    if [ "$OS" == "rpm" ]
    then
        echo "It is not possible to build source deb here"
        exit 1
    fi
    for file in 'dsc' 'orig.tar.gz' 'debian.tar.*' 'changes'
    do
        get_deb_sources $file
    done
    cd $WORKDIR


    DSC=$(basename $(find . -name '*.dsc' | sort | tail -n 1))
    DIRNAME=$(echo $DSC | sed -e 's:_:-:g' | awk -F'-' '{print $1"-"$2"-"$3"-"$4"-"$5}')
    VERSION=$(echo $DSC | sed -e 's:_:-:g' | awk -F'-' '{print $4"-"$5}')
    #
    echo "DEB_RELEASE=${DEB_RELEASE}" >> ${CURDIR}/percona-xtrabackup-8.0.properties
    echo "DEBIAN_VERSION=${OS_NAME}" >> ${CURDIR}/percona-xtrabackup-8.0.properties
    echo "ARCH=${ARCH}" >> ${CURDIR}/percona-xtrabackup-8.0.properties

    dpkg-source -x $DSC
    cd $DIRNAME
    dch -m -D "$OS_NAME" --force-distribution -v "$VERSION-$DEB_RELEASE.$OS_NAME" 'Update distribution'
    dpkg-buildpackage -rfakeroot -uc -us -b

    cd ${WORKDIR}
    mkdir -p $CURDIR/deb
    mkdir -p $WORKDIR/deb
    cp $WORKDIR/*.deb $WORKDIR/deb
    cp $WORKDIR/*.deb $CURDIR/deb
}

build_tarball(){
    if [ $TARBALL = 0 ]
    then
        echo "Binary tarball will not be created"
        return;
    fi
    get_tar "source_tarball"
    cd $WORKDIR
    TARFILE=$(basename $(find . -name 'percona-xtrabackup-*.tar.gz' | sort | tail -n1))
    enable_venv
    #
    NAME=$(echo ${TARFILE%.tar.gz}| awk -F '-' '{print $1"-"$2}')
    VERSION=$(echo ${TARFILE%.tar.gz}| awk -F '-' '{print $3"-"$4}')
    #
    SHORTVER=$(echo ${VERSION} | awk -F '.' '{print $1"."$2}')
    TMPREL=$(echo ${TARFILE%.tar.gz}| awk -F '-' '{print $5}')
    RELEASE=${TMPREL%.tar.gz}

    rm -fr TARGET && mkdir TARGET
    rm -fr ${TARFILE%.tar.gz}
    tar xzf ${TARFILE}
    cd ${TARFILE%.tar.gz}
    #
    bash -x ./storage/innobase/xtrabackup/utils/build-binary.sh ${WORKDIR}/TARGET

    mkdir -p ${WORKDIR}/tarball
    mkdir -p ${CURDIR}/tarball
    cp $WORKDIR/TARGET/*.tar.gz ${WORKDIR}/tarball/
    cp $WORKDIR/TARGET/*.tar.gz ${CURDIR}/tarball/
}

CURDIR=$(pwd)
VERSION_FILE=$CURDIR/percona-xtrabackup-8.0.properties
args=
WORKDIR=
SRPM=0
SDEB=0
RPM=0
DEB=0
SOURCE=0
TARBALL=0
OS_NAME=
ARCH=
OS=
REVISION=0
BRANCH="8.0"
INSTALL=0
RPM_RELEASE=1
DEB_RELEASE=1
REPO="https://github.com/percona/percona-xtrabackup.git"
CMAKE_BIN="cmake"
parse_arguments PICK-ARGS-FROM-ARGV "$@"

check_workdir
get_system
install_deps
get_sources
build_tarball
build_srpm
build_source_deb
build_rpm
build_deb
