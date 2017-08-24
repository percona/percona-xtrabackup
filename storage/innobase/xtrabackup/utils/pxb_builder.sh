#!/bin/sh
CURDIR=$(pwd)
VERSION_FILE=$CURDIR/percona-xtrabackup-2.4.properties
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
BRANCH="2.3"
INSTALL=0
RPM_RELEASE=1
DEB_RELEASE=1
REVISION=0

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
        --build_source_deb   If it is 1 source deb package will be built
        --build_rpm         If it is 1 rpm will be built
        --build_deb         If it is 1 deb will be built
        --build_tarball     If it is 1 tarball will be built
        --install_deps       Install build dependencies(root previlages are required)
        --help) usage ;;
Example $0 --builddir=/tmp/PXB --get_sources=1 --build_src_rpm=1 --build_rpm=1
EOF
        exit 1
}

append_arg_to_args () {
  args="$args "`shell_quote_string "$1"`
}

parse_arguments() {
    pick_args=
    if test "$1" = PICK-ARGS-FROM-ARGV
    then
        pick_args=1
        shift
    fi
  
    for arg do
        val=`echo "$arg" | sed -e 's;^--[^=]*=;;'`
        optname=`echo "$arg" | sed -e 's/^\(--[^=]*\)=.*$/\1/'`
        case "$arg" in
            # these get passed explicitly to mysqld
            --builddir=*) WORKDIR="$val" ;;
            --build_src_rpm=*) SRPM="$val" ;;
            --build_source_deb=*) SDEB="$val" ;;
            --build_rpm=*) RPM="$val" ;;
            --build_deb=*) DEB="$val" ;;
            --get_sources=*) SOURCE="$val" ;;
            --build_tarball=*) TARBALL="$val" ;;
            --branch=*) BRANCH="$val" ;;
            --install_deps=*) INSTALL="$val" ;;
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
    if [ ! -f /etc/yum.repos.d/percona-dev.repo ]
    then
        cat >/etc/yum.repos.d/percona-dev.repo <<EOL
[percona-dev-$basearch]
name=Percona internal YUM repository for build slaves \$releasever - \$basearch
baseurl=http://jenkins.percona.com/yum-repo/\$releasever/RPMS/\$basearch
gpgkey=http://jenkins.percona.com/yum-repo/PERCONA-PACKAGING-KEY
gpgcheck=0
enabled=1

[percona-dev-noarch]
name=Percona internal YUM repository for build slaves \$releasever - noarch
baseurl=http://jenkins.percona.com/yum-repo/\$releasever/RPMS/noarch
gpgkey=http://jenkins.percona.com/yum-repo/PERCONA-PACKAGING-KEY
gpgcheck=0
enabled=1
EOL
    fi
    return
}

add_percona_apt_repo(){
    if [ ! -f /etc/apt/sources.list.d/percona-dev.list ]
    then
        cat >/etc/apt/sources.list.d/percona-dev.list <<EOL
deb http://jenkins.percona.com/apt-repo/ @@DIST@@ main
deb-src http://jenkins.percona.com/apt-repo/ @@DIST@@ main
EOL
    sed -i "s:@@DIST@@:$OS_NAME:g" /etc/apt/sources.list.d/percona-dev.list
    fi
    return
}


get_sources(){
    cd $WORKDIR
    if [ $SOURCE = 0 ]
    then
        echo "Sources will not be downloaded"
        return 0
    fi
    git clone https://github.com/percona/percona-xtrabackup.git
    retval=$?
    if [ $retval != 0 ]
    then
        echo "There were some issues during repo cloning from github. Please retry one more time"
        exit 1
    fi
    cd percona-xtrabackup
    if [ ! -z $BRANCH ]
    then
        git checkout $BRANCH
    fi
    REVISION=$(git rev-parse --short HEAD)
    git reset --hard
    #
    source ./XB_VERSION
    cat XB_VERSION > $VERSION_FILE
    echo "REVISION=${REVISION}" >> $VERSION_FILE
    BRANCH_NAME="${BRANCH}"
    echo "BRANCH_NAME=$(echo ${BRANCH_NAME} | awk -F '/' '{print $(NF)}')" >> $VERSION_FILE
    echo "PRODUCT=Percona-XtraBackup" >> $VERSION_FILE
    echo "PRODUCT_FULL=Percona-XtraBackup-${XB_VERSION_MAJOR}.${XB_VERSION_MINOR}.${XB_VERSION_PATCH}${XB_VERSION_EXTRA}" >> $VERSION_FILE
    echo "PRODUCT_UL_DIR=XtraBackup" >> $VERSION_FILE
    cmake . -DDOWNLOAD_BOOST=1 -DWITH_BOOST=${WORKDIR}/boost
    make dist
    TARFILE=$(basename $(find . -name 'percona-xtrabackup-*.tar.gz' | sort | tail -n1))
    cp $TARFILE $WORKDIR
    cp $TARFILE $CURDIR
    cd $CURDIR
    rm -rf percona-xtrabackup
    return
}

get_system(){
    if [ -f /etc/redhat-release ]; then
        RHEL=$(rpm --eval %rhel)
        ARCH=$(echo $(uname -m) | sed -e 's:i686:i386:g')
        OS_NAME="el$RHEL"
        OS="rpm"
    fi
    if [ -f /etc/lsb-release ]; then
        ARCH=$(uname -m)
        OS_NAME="$(lsb_release -sc)"
        OS="deb"
    fi
    return
}

fix_spec() {
    PATH_TO_SPEC=$1
    PATH_TO_VERSION=$2
    source $PATH_TO_VERSION
    sed -i "s:@@XB_VERSION_MAJOR@@:${XB_VERSION_MAJOR}:g" $PATH_TO_SPEC
    sed -i "s:@@XB_VERSION_MINOR@@:${XB_VERSION_MINOR}:g" $PATH_TO_SPEC
    sed -i "s:@@XB_VERSION_PATCH@@:${XB_VERSION_PATCH}:g" $PATH_TO_SPEC
    if [ -z "${XB_VERSION_EXTRA}" ]; then
        EXTRAVER="%{nil}"
        RPM_EXTRAVER=${RPM_RELEASE}
    else 
        EXTRAVER=${XB_VERSION_EXTRA}
        RPM_EXTRAVER=${XB_VERSION_EXTRA#-}.${RPM_RELEASE}
    fi
    sed -i "s:@@XB_VERSION_EXTRA@@:${EXTRAVER}:g" $PATH_TO_SPEC
    sed -i "s:@@XB_RPM_VERSION_EXTRA@@:${RPM_EXTRAVER}:g" $PATH_TO_SPEC
    sed -i "s:@@XB_REVISION@@:${REVISION}:g" $PATH_TO_SPEC
    return
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
    if [ "x$OS" = "xrpm" ]
    then
        add_percona_yum_repo
        yum -y install git wget
        yum -y install gcc-c++ epel-release rpmdevtools bison
        if [ $RHEL = 7 ]
        then
            yum -y install python2-pip  python-sphinx_rtd_theme vim-common
        elif [ $RHEL = 6 ]
        then
            yum -y install python27
            wget http://jenkins.percona.com/downloads/setuptools-31.0.0.tar.gz
            tar -zxf setuptools-31.0.0.tar.gz
            cd setuptools-31.0.0
            python2.7 setup.py install
            ln -s /usr/local/bin/easy_install /usr/bin/easy_install-2.7
            /usr/bin/easy_install-2.7 pip
            ln -s /usr/local/bin/pip2.7 /usr/bin/pip2.7
            cd $CURDIR
        else
            echo "Unsupported OS version"
            exit 1
        fi
        pip2.7 install sphinx alabaster babel snowballstemmer Pygments Jinja2 six
        cd $WORKDIR
        link="https://raw.githubusercontent.com/percona/percona-xtrabackup/2.3/storage/innobase/xtrabackup/utils/percona-xtrabackup.spec"
        link_ver="https://raw.githubusercontent.com/percona/percona-xtrabackup/2.3/XB_VERSION"
        if [ ! -z $BRANCH ]
        then
            sed "s:2.3:$BRANCH:" $link
            sed "s:2.3:$BRANCH:" $link_ver
            wget $link
            wget $link_ver
            fix_spec $WORKDIR/percona-xtrabackup.spec $WORKDIR/XB_VERSION
            cd $CURPLACE
        fi
        yum-builddep -y $WORKDIR/percona-xtrabackup.spec
    else
        add_percona_apt_repo
        apt-get update
        apt-get -y install devscripts equivs
        CURPLACE=$(pwd)
        cd $WORKDIR
        link="https://raw.githubusercontent.com/percona/percona-xtrabackup/2.3/storage/innobase/xtrabackup/utils/debian/control"
        if [ ! -z $BRANCH ]
        then
            sed "s:2.3:$BRANCH:" $link
        fi
        wget $link
        cd $CURPLACE
        sed -i 's:apt-get :apt-get -y --force-yes :g' /usr/bin/mk-build-deps
        mk-build-deps --install $WORKDIR/control
    fi
    return;
}

get_tar(){
    TARFILE=$(basename $(find $WORKDIR -name 'percona-xtrabackup*.tar.gz' | sort | tail -n1))
    if [ -z $TARFILE ]
    then
        TARFILE=$(basename $(find $CURDIR -name 'percona-xtrabackup*.tar.gz' | sort | tail -n1))
        if [ -z $TARFILE ]
        then
            echo "There is no source tarball for build"
            echo "You can create it using key --get_sources=1"
            exit 1
        else
            cp $CURDIR/$TARFILE $WORKDIR
        fi
    fi
    return
}

get_deb_sources(){
    param=$1
    echo $param
    FILE=$(basename $(find $WORKDIR -name "percona-xtrabackup*.$param" | sort | tail -n1))
#    exit 127
    if [ -z $FILE ]
    then
        FILE=$(basename $(find $CURDIR -name "percona-xtrabackup*.$param" | sort | tail -n1))
        if [ -z $FILE ]
        then
            echo "There is no source tarball for build"
            echo "You can create it using key --get_sources=1"
            exit 1
        else
            cp $CURDIR/$FILE $WORKDIR
        fi
    fi
    return
}

build_srpm(){
    if [ $SRPM = 0 ]
    then
        echo "SRC RPM will not be created"
        return;
    fi
    if [ "x$OS" = "xdeb" ]
    then
        echo "It is not possible to build src rpm here"
        exit 1
    fi
    get_tar
    cd $WORKDIR
    mkdir -vp rpmbuild/{SOURCES,SPECS,BUILD,SRPMS,RPMS}
    cd $WORKDIR/rpmbuild/SPECS
    tar vxzf $WORKDIR/$TARFILE --wildcards '*/storage/innobase/xtrabackup/utils/*.spec'
    cp percona-xtrabackup*/storage/innobase/xtrabackup/utils/*.spec ./
    rm -rf percona-xtrabackup-*
    cd $WORKDIR
    tar vxzf $WORKDIR/$TARFILE --wildcards '*/XB_VERSION'
    cp percona-xtrabackup*/XB_VERSION ./
    mv $TARFILE $WORKDIR/rpmbuild/SOURCES/
    rm -rf percona-xtrabackup*
    fix_spec $WORKDIR/rpmbuild/SPECS/percona-xtrabackup.spec $WORKDIR/XB_VERSION
    rpmbuild -bs --define "_topdir $WORKDIR/rpmbuild" --define "dist .generic" rpmbuild/SPECS/percona-xtrabackup.spec
    cp $WORKDIR/rpmbuild/SRPMS/* $CURDIR
    cp $WORKDIR/rpmbuild/SRPMS/* $WORKDIR
    rm -rf $WORKDIR/rpmbuild
}

build_rpm(){
    if [ $RPM = 0 ]
    then
        echo "RPM will not be created"
        return;
    fi
    if [ "x$OS" = "xdeb" ]
    then
        echo "It is not possible to build rpm here"
        exit 1
    fi
        SRC_RPM=$(basename $(find $WORKDIR -name 'percona-xtrabackup*.src.rpm' | sort | tail -n1))
    if [ -z $SRC_RPM ]
    then
        SRC_RPM=$(basename $(find $CURDIR -name 'percona-xtrabackup*.src.rpm' | sort | tail -n1))
        if [ -z $SRC_RPM ]
        then
            echo "There is no src rpm for build"
            echo "You can create it using key --build_src_rpm=1"
            exit 1
        else
            cp $CURDIR/$SRC_RPM $WORKDIR
        fi
    fi
    cd $WORKDIR
    rm -fr rpmbuild
    mkdir -vp rpmbuild/{SOURCES,SPECS,BUILD,SRPMS,RPMS}
    cp $SRC_RPM rpmbuild/SRPMS/
    rpmbuild --define "_topdir ${WORKDIR}/rpmbuild" --define "dist .$OS_NAME" --rebuild rpmbuild/SRPMS/$SRC_RPM
    
}

build_source_deb(){
    if [ $SDEB = 0 ]
    then
        echo "source deb package will not be created"
        return;
    fi
    if [ "x$OS" = "xrmp" ]
    then
        echo "It is not possible to build source deb here"
        exit 1
    fi
    get_tar
    cd $WORKDIR
    tar vxzf $WORKDIR/$TARFILE --wildcards '*/XB_VERSION'
    dirname=$(echo $TARFILE | awk -F'.tar' '{print $1}')
    cd $dirname
    for line in $(cat XB_VERSION)
    do
        export $line
    done 
    cd ../
    NAME=percona-xtrabackup
    SHORTVER=$XB_VERSION_MAJOR$XB_VERSION_MINOR
    VERSION=$XB_VERSION_MAJOR.$XB_VERSION_MINOR.$XB_VERSION_PATCH$XB_VERSION_EXTRA
    if [ -z "$XB_VERSION_EXTRA" ]
    then
        DIRNAME=$NAME-$VERSION
    else
        DIRNAME=$NAME-$VERSION$EXTRAVER
    fi
    NEWTAR=$NAME'_'$VERSION.orig.tar.gz
    mv ${TARFILE} ${NEWTAR}
    tar xzf ${NEWTAR}
    cd ${DIRNAME}
    cp -av storage/innobase/xtrabackup/utils/debian .
    dch -D unstable --force-distribution -v "${VERSION}-${DEB_RELEASE}" "Update to new upstream release Percona XtraBackup ${VERSION}"
    dpkg-buildpackage -S
    cd $WORKDIR
    cp *.dsc $WORKDIR
    cp *.orig.tar.gz $WORKDIR
    cp *.debian.tar.* $WORKDIR
    cp *.changes $WORKDIR
    cp *.dsc $CURDIR
    cp *.orig.tar.gz $CURDIR
    cp *.debian.tar.* $CURDIR
    cp *.changes $CURDIR
}

build_deb(){
    if [ $DEB = 0 ]
    then
        echo "source deb package will not be created"
        return;
    fi
    if [ "x$OS" = "xrmp" ]
    then
        echo "It is not possible to build source deb here"
        exit 1
    fi
    for file in 'dsc' 'orig.tar.gz' 'debian.tar.*' 'changes'
    do
        get_deb_sources $file
    done
    cd $WORKDIR
    DEBIAN_VERSION="$(lsb_release -sc)"
    DSC=$(basename $(find . -name 'percona-xtrabackup*.dsc' | sort | tail -n 1))
    DIRNAME=$(echo $DSC | sed -e 's:_:-:g' | awk -F'-' '{print $1"-"$2"-"$3}')
    VERSION=$(echo $DSC | sed -e 's:_:-:g' | awk -F'-' '{print $4}' | awk -F'.' '{print $1}')
    ARCH=$(uname -m)
    dpkg-source -x $DSC
    cd $DIRNAME
    VER=$(echo $DIRNAME | sed -e 's:percona-xtrabackup-::')
    dch -m -D "$DEBIAN_VERSION" --force-distribution -v "$VER-$DEB_RELEASE.$DEBIAN_VERSION" 'Update distribution'
    dpkg-buildpackage -rfakeroot -uc -us -b
    #
    cd ${WORKDIR}
    #rm -fv *.dsc *.orig.tar.gz *.debian.tar.gz *.changes 
#    rm -fr ${DIRNAME}
}

build_tarball(){
    if [ $TARBALL = 0 ]
    then
        echo "Binary tarball will not be created"
        return;
    fi
    if [ "x$OS" = "xdeb" -o "x$RHEL" != "x6" ]
    then
        echo "It is not possible to build binary tarball here"
        exit 1
    fi
    get_tar
    cd $WORKDIR
    dirname=`tar -tzf $TARFILE | head -1 | cut -f1 -d"/"`
    tar xzf $TARFILE
    cd  $dirname
    mkdir $WORKDIR/BUILD_XYZ
    bash -x ./storage/innobase/xtrabackup/utils/build-binary.sh $WORKDIR/BUILD_XYZ
    cp $WORKDIR/BUILD_XYZ/percona-xtrabackup*.tar.gz $WORKDIR/
    cp $WORKDIR/BUILD_XYZ/percona-xtrabackup*.tar.gz $CURDIR/
    cd $CURDIR
}

parse_arguments PICK-ARGS-FROM-ARGV "$@"
check_workdir
get_system
install_deps
get_sources
build_srpm
build_source_deb
build_rpm
build_tarball
build_deb
