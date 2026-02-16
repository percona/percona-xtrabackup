#!/usr/bin/env bash
#
# Percona XtraBackup 8.0 - Build Script
# Builds source tarballs, SRPMs, RPMs, source DEBs, DEBs, and binary tarballs.
#
# Supported distributions:
#   RPM-based:  OracleLinux / RHEL 8, 9, 10; Amazon Linux 2023
#               CentOS 7 (install_deps + get_sources + build_src_rpm only)
#   DEB-based:  Debian 11 (bullseye), 12 (bookworm), 13 (trixie)
#               Ubuntu 20.04 (focal), 22.04 (jammy), 24.04 (noble), 26.04 (resolute)
#
set -euo pipefail

# =============================================================================
# Constants & Defaults
# =============================================================================
readonly SCRIPT_NAME="$(basename "$0")"
readonly PRODUCT="Percona-XtraBackup-8.0"
readonly PROPERTIES_FILE="percona-xtrabackup-8.0.properties"
readonly PXB_REPO_DEFAULT="https://github.com/percona/percona-xtrabackup.git"
readonly BOOST_URL="https://downloads.percona.com/downloads/packaging/boost/"
readonly BOOST_JFROG_URL="https://boostorg.jfrog.io/artifactory/main/release/1.77.0/source/"
readonly CALLHOME_URL="https://raw.githubusercontent.com/Percona-Lab/telemetry-agent/phase-0/call-home.sh"

# Supported distributions
readonly SUPPORTED_RPM_VERSIONS="7 8 9 10 2023"
readonly SUPPORTED_DEB_CODENAMES="focal bullseye bookworm trixie jammy noble resolute"

# =============================================================================
# Logging Helpers
# =============================================================================
log_info()  { echo "[INFO]  $*"; }
log_warn()  { echo "[WARN]  $*" >&2; }
log_error() { echo "[ERROR] $*" >&2; }
log_fatal() { echo "[FATAL] $*" >&2; exit 1; }

# =============================================================================
# Utility Functions
# =============================================================================
shell_quote_string() {
    echo "$1" | sed -e 's,\([^a-zA-Z0-9/_.=-]\),\\\1,g'
}

safe_cd() {
    cd "$1" || log_fatal "Failed to change directory to: $1"
}

# Retry a command with a simple backoff
retry_cmd() {
    local max_attempts="${1:-5}"
    shift
    local attempt=1
    until "$@"; do
        if (( attempt >= max_attempts )); then
            log_error "Command failed after ${max_attempts} attempts: $*"
            return 1
        fi
        log_warn "Attempt ${attempt} failed, retrying in 2s: $*"
        sleep 2
        (( attempt++ ))
    done
}

# Find a file matching a glob pattern, checking WORKDIR/subdir first, then CURDIR/subdir.
# Usage: find_artifact <subdir> <glob_pattern>
# Sets: FOUND_FILE (basename), FOUND_PATH (full path)
find_artifact() {
    local subdir="$1"
    local pattern="$2"

    FOUND_FILE=""
    FOUND_PATH=""

    # Search order: WORKDIR/subdir, CURDIR/subdir, WORKDIR root, CURDIR root
    local search_dirs=(
        "${WORKDIR}/${subdir}"
        "${CURDIR}/${subdir}"
        "${WORKDIR}"
        "${CURDIR}"
    )

    local candidate
    for dir in "${search_dirs[@]}"; do
        candidate="$(find "$dir" -maxdepth 1 -name "${pattern}" 2>/dev/null | sort | tail -n1)"
        if [[ -n "$candidate" ]]; then
            FOUND_FILE="$(basename "$candidate")"
            FOUND_PATH="$candidate"
            return 0
        fi
    done

    return 1
}

# Copy artifacts to both WORKDIR/<dir> and CURDIR/<dir>
publish_artifacts() {
    local subdir="$1"
    shift
    # remaining args are file globs/paths to copy

    mkdir -p "${WORKDIR}/${subdir}"
    mkdir -p "${CURDIR}/${subdir}"

    for src in "$@"; do
        # Handle globs: iterate over expanded paths
        for f in $src; do
            [[ -f "$f" ]] || continue
            cp "$f" "${WORKDIR}/${subdir}/"
            cp "$f" "${CURDIR}/${subdir}/"
        done
    done
}

# =============================================================================
# Usage
# =============================================================================
usage() {
    cat <<EOF
Usage: ${SCRIPT_NAME} [OPTIONS]

Options:
    --builddir=DIR          Absolute path to the build working directory
    --get_sources=0|1       Download sources from GitHub (default: 0)
    --build_src_rpm=0|1     Build source RPM (default: 0)
    --build_source_deb=0|1  Build source DEB package (default: 0)
    --build_rpm=0|1         Build RPM packages (default: 0)
    --build_deb=0|1         Build DEB packages (default: 0)
    --build_tarball=0|1     Build binary tarball (default: 0)
    --install_deps=0|1      Install build dependencies (requires root) (default: 0)
    --branch=BRANCH         Git branch to build (default: 8.0)
    --repo=URL              Git repository URL
    --rpm_release=N         RPM release number (default: 1)
    --deb_release=N         DEB release number (default: 1)
    --help                  Show this help message

Example:
    ${SCRIPT_NAME} --builddir=/tmp/PXB --get_sources=1 --build_src_rpm=1 --build_rpm=1

Supported platforms:
    RPM:  OracleLinux / RHEL 8, 9, 10; Amazon Linux 2023
          CentOS 7 (install_deps + get_sources + build_src_rpm only)
    DEB:  Debian 11 (bullseye), 12 (bookworm), 13 (trixie)
          Ubuntu 20.04 (focal), 22.04 (jammy), 24.04 (noble), 26.04 (resolute)
EOF
    exit 1
}

# =============================================================================
# Argument Parsing
# =============================================================================
parse_arguments() {
    local pick_args=""
    if [[ "${1:-}" == "PICK-ARGS-FROM-ARGV" ]]; then
        pick_args=1
        shift
    fi

    for arg in "$@"; do
        local val="${arg#*=}"
        case "$arg" in
            --builddir=*)         WORKDIR="$val" ;;
            --build_src_rpm=*)    SRPM="$val" ;;
            --build_source_deb=*) SDEB="$val" ;;
            --build_rpm=*)        RPM="$val" ;;
            --build_deb=*)        DEB="$val" ;;
            --get_sources=*)      SOURCE="$val" ;;
            --build_tarball=*)    BUILD_TARBALL="$val" ;;
            --install_deps=*)     INSTALL="$val" ;;
            --branch=*)           BRANCH="$val" ;;
            --repo=*)             REPO="$val" ;;
            --rpm_release=*)      RPM_RELEASE="$val" ;;
            --deb_release=*)      DEB_RELEASE="$val" ;;
            --help)               usage ;;
            *)
                if [[ -n "$pick_args" ]]; then
                    args="$args $(shell_quote_string "$arg")"
                fi
                ;;
        esac
    done
}

# =============================================================================
# System Detection
# =============================================================================
get_system() {
    if [[ -f /etc/redhat-release ]]; then
        GLIBC_VER_TMP="$(rpm -qa glibc --qf '%{VERSION}')"
        RHEL="$(rpm --eval '%{rhel}')"
        ARCH="$(uname -m | sed -e 's:i686:i386:g')"
        OS_NAME="el${RHEL}"
        OS="rpm"
    elif [[ -f /etc/amazon-linux-release ]]; then
        GLIBC_VER_TMP="$(rpm -qa glibc --qf '%{VERSION}')"
        RHEL="$(rpm --eval '%{amzn}')"
        ARCH="$(uname -m | sed -e 's:i686:i386:g')"
        OS_NAME="amzn${RHEL}"
        OS="rpm"
    else
        GLIBC_VER_TMP="$(dpkg-query -W -f='${Version}' libc6 | awk -F'-' '{print $1}')"
        ARCH="$(uname -m)"
	apt-get update
	apt-get -y install lsb-release
        OS_NAME="$(lsb_release -sc)"
        OS="deb"
    fi
    GLIBC_VER=".glibc${GLIBC_VER_TMP}"

    export RHEL ARCH OS_NAME OS GLIBC_VER
    log_info "Detected OS=${OS}, OS_NAME=${OS_NAME}, RHEL=${RHEL:-N/A}, ARCH=${ARCH}"

    _validate_distro
}

# =============================================================================
# Distro Validation
# =============================================================================
_validate_distro() {
    if [[ "$OS" == "rpm" ]]; then
        if ! echo "$SUPPORTED_RPM_VERSIONS" | grep -qw "${RHEL}"; then
            log_fatal "Unsupported RHEL/OL version: ${RHEL}. Supported: ${SUPPORTED_RPM_VERSIONS}"
        fi
    else
        if ! echo "$SUPPORTED_DEB_CODENAMES" | grep -qw "${OS_NAME}"; then
            log_fatal "Unsupported Debian/Ubuntu codename: ${OS_NAME}. Supported: ${SUPPORTED_DEB_CODENAMES}"
        fi
    fi
    log_info "Distribution validated: ${OS_NAME}"
}

# =============================================================================
# CentOS 7 Helpers
# =============================================================================
switch_to_vault_repo() {
    sed -i 's/mirrorlist/#mirrorlist/g' /etc/yum.repos.d/CentOS-* 2>/dev/null || true
    sed -i 's|#\s*baseurl=http://mirror.centos.org|baseurl=http://vault.centos.org|g' /etc/yum.repos.d/CentOS-* 2>/dev/null || true
}

# Activate SCL devtoolset and python36 on CentOS 7 (needed for cmake3 and rpmbuild)
enable_venv() {
    [[ "$OS" != "rpm" || "${RHEL}" != "7" ]] && return
    set +u
    # shellcheck disable=SC1091
    source /opt/rh/devtoolset-7/enable
    # shellcheck disable=SC1091
    source /opt/rh/rh-python36/enable
    set -u
    CMAKE_BIN="cmake3"
}

# =============================================================================
# Dependency Installation
# =============================================================================
install_deps() {
    if [[ "$INSTALL" == "0" ]]; then
        log_info "Skipping dependency installation"
        return
    fi
    if [[ "$(id -u)" -ne 0 ]]; then
        log_fatal "Root privileges required for --install_deps=1"
    fi

    if [[ "$OS" == "rpm" ]]; then
        _install_deps_rpm
    else
        _install_deps_deb
    fi
}

_install_deps_rpm() {
    if [[ "${RHEL}" == "7" ]]; then
        _install_deps_rpm_el7
        return
    fi

    # Amazon Linux 2023 Docker images ship curl-minimal which conflicts with curl.
    # Replace it with the full curl package before installing other deps.
    if [[ "${RHEL}" == "2023" ]]; then
        yum -y install --allowerasing git wget yum-utils curl
    else
        yum -y install git wget yum-utils curl
    fi
    yum install -y https://repo.percona.com/yum/percona-release-latest.noarch.rpm || true

    _configure_rpm_repos

    local PKGLIST=""
    PKGLIST+=" binutils-devel python3-pip python3-setuptools"
    PKGLIST+=" libcurl-devel cmake libaio-devel zlib-devel libev-devel bison make gcc"
    PKGLIST+=" rpm-build libgcrypt-devel ncurses-devel readline-devel openssl-devel gcc-c++"
    PKGLIST+=" vim-common rpmlint patchelf python3-wheel libudev-devel pkgconfig"

    case "${RHEL}" in
        9|10|2023)
            PKGLIST+=" rsync procps-ng-devel python3-sphinx"
            ;;
        8)
            if [[ "${ARCH}" == "x86_64" ]]; then
                yum-config-manager --enable powertools || true
                yum-config-manager --enable ol8_codeready_builder || true
                PKGLIST+=" libarchive procps-ng-devel"
            else
                PKGLIST+=" rsync python3-sphinx libarchive procps-ng-devel"
            fi
            ;;
    esac

    retry_cmd 5 yum -y install ${PKGLIST}
    _install_rpm_devtoolset
}

_configure_rpm_repos() {
    # Amazon Linux 2023: no OL repos or EPEL needed, packages available natively
    if [[ "${RHEL}" == "2023" ]]; then
        return
    fi

    # Enable CodeReady Builder / CRB equivalent for OL/RHEL
    yum-config-manager --enable "ol${RHEL}_codeready_builder" || true

    if [[ "${ARCH}" == "x86_64" ]]; then
        case "${RHEL}" in
            9|10)
                yum-config-manager --enable "ol${RHEL}_distro_builder" || true
                ;;
        esac
    fi

    # EPEL
    yum -y install "https://dl.fedoraproject.org/pub/epel/epel-release-latest-${RHEL}.noarch.rpm" || true
    yum -y install epel-release || true
}

_install_rpm_devtoolset() {
    local DT12_PKGS="gcc-toolset-12-gcc gcc-toolset-12-gcc-c++ gcc-toolset-12-binutils"
    DT12_PKGS+=" gcc-toolset-12-annobin-annocheck gcc-toolset-12-annobin-plugin-gcc"

    case "${RHEL}" in
        8)
            if [[ "${ARCH}" == "x86_64" ]]; then
                # EL8 x86_64: also install devtoolset-10 (used for valgrind/asan)
                local DT10_PKGS="gcc-toolset-10-gcc-c++ gcc-toolset-10-binutils"
                DT10_PKGS+=" gcc-toolset-10-valgrind gcc-toolset-10-valgrind-devel gcc-toolset-10-libatomic-devel"
                DT10_PKGS+=" gcc-toolset-10-libasan-devel gcc-toolset-10-libubsan-devel gcc-toolset-10-annobin"

                yum -y install centos-release-stream || true
                retry_cmd 5 yum -y install ${DT10_PKGS}
                yum -y remove centos-release-stream || true
            fi
            DT12_PKGS+=" gcc-toolset-12-libasan-devel gcc-toolset-12-libubsan-devel"
            retry_cmd 5 yum -y install ${DT12_PKGS}
            ;;
        9)
            retry_cmd 5 yum -y install ${DT12_PKGS}
            ;;
        *)
            # EL10, Amazon Linux 2023: system gcc is new enough
            return
            ;;
    esac
}

_install_deps_rpm_el7() {
    # CentOS 7: minimal deps for get_sources + build_src_rpm only
    switch_to_vault_repo

    retry_cmd 5 yum -y install epel-release centos-release-scl
    switch_to_vault_repo
    retry_cmd 5 yum -y makecache

    local PKGLIST=""
    PKGLIST+=" git wget curl rpm-build make gcc gcc-c++ cmake3"
    PKGLIST+=" devtoolset-7-gcc-c++ devtoolset-7-binutils"
    PKGLIST+=" libcurl-devel libaio-devel zlib-devel libev-devel openssl-devel"
    PKGLIST+=" libgcrypt-devel ncurses-devel readline-devel bison"
    PKGLIST+=" vim-common patchelf libudev-devel libicu-devel"
    PKGLIST+=" python3-pip python3-setuptools python3-wheel"
    PKGLIST+=" rh-python36-python-sphinx"
    PKGLIST+=" numactl numactl-libs perl-Digest-MD5 perl-DBD-MySQL"
    PKGLIST+=" procps-ng-devel"

    retry_cmd 5 yum -y install ${PKGLIST}

    yum -y --enablerepo=centos-sclo-rh-testing install \
        devtoolset-10-gcc-c++ devtoolset-10-binutils \
        devtoolset-10-valgrind devtoolset-10-valgrind-devel \
        devtoolset-10-libatomic-devel \
        devtoolset-10-libasan-devel devtoolset-10-libubsan-devel || true

    yum -y update nss
}

_install_deps_deb() {
    apt-get update
    DEBIAN_FRONTEND=noninteractive apt-get -y install devscripts dpkg-dev pkg-config lsb-release gnupg git wget curl

    # Re-detect OS_NAME after installing lsb-release
    OS_NAME="$(lsb_release -sc)"
    export OS_NAME

#    wget "https://repo.percona.com/apt/percona-release_latest.${OS_NAME}_all.deb" \
#        && dpkg -i "percona-release_latest.${OS_NAME}_all.deb"
    #percona-release enable tools testing
#    percona-release disable all
    apt-get update

    local PKGLIST=""
    PKGLIST+=" bison cmake devscripts debconf debhelper automake ca-certificates"
    PKGLIST+=" libcurl4-openssl-dev libaio-dev libncurses-dev libtool libz-dev libsasl2-dev"
    PKGLIST+=" vim-common libgcrypt-dev libev-dev lsb-release libudev-dev"
    PKGLIST+=" build-essential rsync libdbd-mysql-perl libnuma1 socat libssl-dev patchelf libicu-dev"
    PKGLIST+=" python3-sphinx python3-docutils"

    # procps dev package: libproc2-dev for bookworm+, libprocps-dev for older
    case "${OS_NAME}" in
        focal|bullseye|jammy) PKGLIST+=" libprocps-dev" ;;
        *)                    PKGLIST+=" libproc2-dev" ;;
    esac

    retry_cmd 5 env DEBIAN_FRONTEND=noninteractive apt-get -y install ${PKGLIST}

    # Trixie: pin GCC 13
    if [[ "${OS_NAME}" == "trixie" ]]; then
        apt-get -y install gcc-13 g++-13
        update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-13 100 \
            --slave /usr/bin/g++ g++ /usr/bin/g++-13
        update-alternatives --install /usr/bin/cc cc /usr/bin/gcc-13 100
    fi
}

# =============================================================================
# Workdir Validation
# =============================================================================
check_workdir() {
    if [[ -z "${WORKDIR:-}" ]]; then
        log_fatal "--builddir is required"
    fi
    if [[ "$WORKDIR" == "$CURDIR" ]]; then
        log_fatal "Current directory cannot be used as --builddir"
    fi
    if [[ ! -d "$WORKDIR" ]]; then
        log_fatal "${WORKDIR} is not a directory"
    fi
}

# =============================================================================
# Source Download & Tarball Preparation
# =============================================================================
get_sources() {
    if [[ "$SOURCE" == "0" ]]; then
        log_info "Skipping source download"
        return
    fi

    safe_cd "$WORKDIR"

    _clone_repo
    _load_version_info
    _write_properties
    _generate_source_tarball
    _patch_source_tarball
    _package_and_publish_sources
}

_clone_repo() {
    log_info "Cloning ${REPO} branch ${BRANCH}"
    git clone --depth 1 --no-tags --branch "$BRANCH" "$REPO" || \
        log_fatal "Failed to clone repository. Please retry."

    safe_cd percona-xtrabackup

    REVISION="$(git rev-parse --short HEAD)"
    git submodule update --init --depth 1 --jobs "$(nproc)"
}

_load_version_info() {
    # shellcheck disable=SC1091
    source XB_VERSION

    export XB_VERSION_MAJOR XB_VERSION_MINOR XB_VERSION_PATCH XB_VERSION_EXTRA
    PRODUCT_FULL="Percona-XtraBackup-${XB_VERSION_MAJOR}.${XB_VERSION_MINOR}.${XB_VERSION_PATCH}${XB_VERSION_EXTRA}"
}

_write_properties() {
    local props="${WORKDIR}/${PROPERTIES_FILE}"
    cat XB_VERSION > "$props"

    local branch_short
    branch_short="$(echo "${BRANCH}" | awk -F '/' '{print $(NF)}')"

    {
        echo "REVISION=${REVISION}"
        echo "BRANCH_NAME=${branch_short}"
        echo "PRODUCT=${PRODUCT}"
        echo "PRODUCT_FULL=${PRODUCT_FULL}"
        echo "PRODUCT_UL_DIR=${PRODUCT}"
        echo "DESTINATION=${DESTINATION:-experimental}"
    } >> "$props"
}

_generate_source_tarball() {
    # Replace Boost URL
    sed -i "s|${BOOST_JFROG_URL}|${BOOST_URL}|g" cmake/boost.cmake

    enable_venv

    "${CMAKE_BIN}" . \
        -DDOWNLOAD_BOOST=1 \
        -DWITH_BOOST="${WORKDIR}/boost" \
        -DFORCE_INSOURCE_BUILD=1 \
        -DWITH_MAN_PAGES=1

    make dist
}

_patch_source_tarball() {
    local exported_tar
    exported_tar="$(basename "$(find . -maxdepth 1 -name 'percona-xtrabackup*.tar.gz' | sort | tail -n1)")"
    PXBDIR="${exported_tar%.tar.gz}"

    rm -rf "${PXBDIR}"
    tar xzf "${exported_tar}"
    rm -f "${exported_tar}"

    safe_cd "${PXBDIR}"

    # Sync libkmip
    rsync -av ../extra/libkmip/* extra/libkmip/

    # Compiler warning adjustments
    sed -i 's:-Wall -Wextra -Wformat-security -Wvla -Wundef:-Wextra -Wformat-security -Wvla -Wundef:g' cmake/maintainer.cmake
    sed -i '/Werror/d' cmake/maintainer.cmake
    sed -i "s|${BOOST_JFROG_URL}|${BOOST_URL}|g" cmake/boost.cmake
    sed -i 's:Wstringop-truncation:Wno-stringop-truncation:g' cmake/maintainer.cmake

    # Fix ambiguous python shebangs (EL8+ brp-mangle-shebangs rejects '#!/usr/bin/env python')
    find . -name '*.py' -o -name 'subunit2junitxml' | \
        xargs sed -i 's|#!/usr/bin/env python$|#!/usr/bin/env python3|g' 2>/dev/null || true

    # Patch spec file version placeholders
    local specfile="storage/innobase/xtrabackup/utils/percona-xtrabackup.spec"
    sed -i "s:@@XB_VERSION_MAJOR@@:${XB_VERSION_MAJOR}:g" "$specfile"
    sed -i "s:@@XB_VERSION_MINOR@@:${XB_VERSION_MINOR}:g" "$specfile"
    sed -i "s:@@XB_VERSION_PATCH@@:${XB_VERSION_PATCH}:g" "$specfile"

    local extraver rpm_extraver
    if [[ -z "${XB_VERSION_EXTRA}" ]]; then
        extraver="%{nil}"
        rpm_extraver="${RPM_RELEASE}"
    else
        extraver="${XB_VERSION_EXTRA}"
        rpm_extraver="${XB_VERSION_EXTRA#-}.${RPM_RELEASE}"
    fi

    sed -i "s:@@XB_VERSION_EXTRA@@:${extraver}:g"         "$specfile"
    sed -i "s:@@XB_RPM_VERSION_EXTRA@@:${rpm_extraver}:g" "$specfile"
    sed -i "s:@@XB_REVISION@@:${REVISION}:g"              "$specfile"
    sed -i "s:@@RPM_RELEASE@@:${RPM_RELEASE}:g"           "$specfile"
}

_package_and_publish_sources() {
    safe_cd "${WORKDIR}/percona-xtrabackup"

    tar --owner=0 --group=0 --exclude=.bzr --exclude=.git \
        -czf "${PXBDIR}.tar.gz" "${PXBDIR}"

    echo "UPLOAD=UPLOAD/experimental/BUILDS/${PRODUCT}/${PRODUCT_FULL}/${BRANCH}/${REVISION}/${BUILD_ID:-}" \
        >> "${WORKDIR}/${PROPERTIES_FILE}"

    rm -rf "${PXBDIR}"

    publish_artifacts "source_tarball" "${PXBDIR}.tar.gz"

    safe_cd "$CURDIR"
    rm -rf percona-xtrabackup
}

# =============================================================================
# Tarball Retrieval Helper
# =============================================================================
get_tar() {
    local subdir="$1"
    if ! find_artifact "$subdir" 'percona-xtrabackup*.tar.gz'; then
        log_fatal "No tarball found in ${subdir} for build"
    fi
    cp "$FOUND_PATH" "${WORKDIR}/${FOUND_FILE}"
}

# =============================================================================
# DEB Source Retrieval Helper
# =============================================================================
get_deb_sources() {
    local param="$1"
    if ! find_artifact "source_deb" "percona-xtrabackup*.${param}"; then
        log_fatal "No source file (*.${param}) found for DEB build"
    fi
    cp "$FOUND_PATH" "${WORKDIR}/"
}

# =============================================================================
# Call-Home Script Injection (RPM spec)
# =============================================================================
_inject_callhome_rpm() {
    local specfile="$1"
    local specdir
    specdir="$(dirname "$specfile")"

    safe_cd "$specdir"
    wget -q "$CALLHOME_URL" -O call-home.sh

    local line_number
    line_number="$(grep -n 'SOURCE999' "$specfile" | awk -F ':' '{print $1}')"

    awk -v n="$line_number" 'NR <= n {print > "part1.txt"} NR > n {print > "part2.txt"}' "$specfile"
    head -n -1 part1.txt > temp && mv temp part1.txt

    {
        echo "cat <<'CALLHOME' > /tmp/call-home.sh"
        cat call-home.sh
        echo "CALLHOME"
        cat part2.txt
    } >> part1.txt

    rm -f call-home.sh part2.txt
    mv part1.txt "$specfile"
}

# =============================================================================
# Build: Source RPM
# =============================================================================
build_srpm() {
    if [[ "$SRPM" == "0" ]]; then
        log_info "Skipping SRPM build"
        return
    fi
    [[ "$OS" == "deb" ]] && log_fatal "Cannot build SRPM on a Debian-based system"

    safe_cd "$WORKDIR"
    get_tar "source_tarball"

    rm -fr rpmbuild
    mkdir -vp rpmbuild/{SOURCES,SPECS,BUILD,SRPMS,RPMS}

    local tarfile
    tarfile="$(basename "$(find . -maxdepth 1 -name 'percona-xtrabackup-*.tar.gz' | sort | tail -n1)")"
    local version
    version="$(echo "${tarfile}" | awk -F '-' '{print $3}')"
    local tmprel
    tmprel="$(echo "${tarfile}" | awk -F '-' '{print $4}')"
    local release="${tmprel%.tar.gz}"

    # Extract spec file from tarball (avoid --wildcards --strip, unreliable on older tar)
    local tardir="${tarfile%.tar.gz}"
    local spec_path="${tardir}/storage/innobase/xtrabackup/utils/percona-xtrabackup.spec"
    tar xzf "${WORKDIR}/${tarfile}" "${spec_path}"
    cp "${spec_path}" rpmbuild/SPECS/
    rm -rf "${tardir}"

    # Add changelog entry
    local specfile="rpmbuild/SPECS/percona-xtrabackup.spec"
    [[ -f "$specfile" ]] || log_fatal "Spec file not found after extraction from tarball"
    sed -i "/^%changelog/a - Release ${version}-${release}" "$specfile"
    sed -i "/^%changelog/a * $(date '+%a %b %d %Y') Percona Development Team <info@percona.com> - ${version}-${release}" "$specfile"

    # Inject call-home script
    _inject_callhome_rpm "${WORKDIR}/${specfile}"
    safe_cd "$WORKDIR"

    # Move source tarball and build
    mv -f "${tarfile}" rpmbuild/SOURCES/
    wget -q "$CALLHOME_URL" -O rpmbuild/SOURCES/call-home.sh

    enable_venv

    rpmbuild -bs \
        --define "_topdir ${WORKDIR}/rpmbuild" \
        --define "dist .generic" \
        rpmbuild/SPECS/percona-xtrabackup.spec

    publish_artifacts "srpm" "${WORKDIR}/rpmbuild/SRPMS/"*.src.rpm
}

# =============================================================================
# Build: RPM
# =============================================================================
build_rpm() {
    if [[ "$RPM" == "0" ]]; then
        log_info "Skipping RPM build"
        return
    fi
    [[ "$OS" == "deb" ]] && log_fatal "Cannot build RPM on a Debian-based system"
    [[ "$OS" == "rpm" && "${RHEL}" == "7" ]] && log_fatal "Binary RPM builds are not supported on CentOS 7. Use --build_src_rpm=1 only."
    if ! find_artifact "srpm" 'percona-xtrabackup-*.src.rpm'; then
        log_fatal "No source RPM found. Use --build_src_rpm=1 first."
    fi
    cp "$FOUND_PATH" "${WORKDIR}/"

    safe_cd "$WORKDIR"
    rm -fr rpmbuild
    mkdir -vp rpmbuild/{SOURCES,SPECS,BUILD,SRPMS,RPMS}
    cp "${FOUND_FILE}" rpmbuild/SRPMS/

    # Write build metadata
    {
        echo "RHEL=${RHEL}"
        echo "ARCH=${ARCH}"
    } >> "${CURDIR}/${PROPERTIES_FILE}"


    rpmbuild \
        --define "_topdir ${WORKDIR}/rpmbuild" \
        --define "dist .${OS_NAME}" \
        --rebuild "rpmbuild/SRPMS/${FOUND_FILE}"

    publish_artifacts "rpm" "${WORKDIR}/rpmbuild/RPMS/"*/*.rpm
}

# =============================================================================
# Build: Source DEB
# =============================================================================
build_source_deb() {
    if [[ "$SDEB" == "0" ]]; then
        log_info "Skipping source DEB build"
        return
    fi
    [[ "$OS" == "rpm" ]] && log_fatal "Cannot build source DEB on an RPM-based system"

    safe_cd "$WORKDIR"
    rm -rf percona-xtrabackup*
    get_tar "source_tarball"
    rm -f *.dsc *.orig.tar.gz *.debian.tar.* *.changes

    local tarfile
    tarfile="$(basename "$(find . -maxdepth 1 -name 'percona-xtrabackup-*.tar.gz' | sort | tail -n1)")"
    local name
    name="$(echo "${tarfile}" | awk -F '-' '{print $1"-"$2}')"
    local version
    version="$(echo "${tarfile%.tar.gz}" | awk -F '-' '{print $3"-"$4}')"

    echo "DEB_RELEASE=${DEB_RELEASE}" >> "${CURDIR}/${PROPERTIES_FILE}"

    local newtar="${name}-80_${version}.orig.tar.gz"
    mv "${tarfile}" "${newtar}"

    tar xzf "${newtar}"
    safe_cd "percona-xtrabackup-${version}"

    cp -av storage/innobase/xtrabackup/utils/debian .
    dch -D unstable --force-distribution \
        -v "${version}-${DEB_RELEASE}" \
        "Update to new upstream release Percona XtraBackup ${version}"

    dpkg-buildpackage -S

    safe_cd "$WORKDIR"
    publish_artifacts "source_deb" \
        "${WORKDIR}/"*.dsc \
        "${WORKDIR}/"*.orig.tar.gz \
        "${WORKDIR}/"*.debian.tar.* \
        "${WORKDIR}/"*.changes
}

# =============================================================================
# Build: DEB
# =============================================================================
build_deb() {
    if [[ "$DEB" == "0" ]]; then
        log_info "Skipping DEB build"
        return
    fi
    [[ "$OS" == "rpm" ]] && log_fatal "Cannot build DEB on an RPM-based system"

    for ext in dsc orig.tar.gz 'debian.tar.*' changes; do
        get_deb_sources "$ext"
    done

    safe_cd "$WORKDIR"

    local dsc
    dsc="$(basename "$(find . -maxdepth 1 -name '*.dsc' | sort | tail -n1)")"
    local dirname
    dirname="$(echo "$dsc" | sed -e 's:_:-:g' | awk -F'-' '{print $1"-"$2"-"$3"-"$4"-"$5}')"
    local version
    version="$(echo "$dsc" | sed -e 's:_:-:g' | awk -F'-' '{print $4"-"$5}')"

    {
        echo "DEB_RELEASE=${DEB_RELEASE}"
        echo "DEBIAN_VERSION=${OS_NAME}"
        echo "ARCH=${ARCH}"
    } >> "${CURDIR}/${PROPERTIES_FILE}"

    dpkg-source -x "$dsc"
    safe_cd "$dirname"

    dch -m -D "$OS_NAME" --force-distribution \
        -v "${version}-${DEB_RELEASE}.${OS_NAME}" \
        'Update distribution'

    # Inject call-home into postinst
    safe_cd debian/
    wget -q "$CALLHOME_URL" -O call-home.sh

    sed -i 's:exit 0::' percona-xtrabackup-80.postinst
    {
        echo "cat <<'CALLHOME' > /tmp/call-home.sh"
        cat call-home.sh
        echo "CALLHOME"
        echo "bash +x /tmp/call-home.sh -f \"PRODUCT_FAMILY_PXB\" -v \"${version}-${DEB_RELEASE}\" -d \"PACKAGE\" &>/dev/null || :"
        echo "rm -rf /tmp/call-home.sh"
        echo "exit 0"
    } >> percona-xtrabackup-80.postinst

    rm -f call-home.sh
    safe_cd ..

    dpkg-buildpackage -rfakeroot -uc -us -b

    safe_cd "$WORKDIR"
    publish_artifacts "deb" "${WORKDIR}/"*.deb
}

# =============================================================================
# Build: Binary Tarball
# =============================================================================
build_tarball() {
    if [[ "$BUILD_TARBALL" == "0" ]]; then
        log_info "Skipping binary tarball build"
        return
    fi
    [[ "$OS" == "rpm" && "${RHEL}" == "7" ]] && log_fatal "Binary tarball builds are not supported on CentOS 7."

    get_tar "source_tarball"
    safe_cd "$WORKDIR"

    local tarfile
    tarfile="$(basename "$(find . -maxdepth 1 -name 'percona-xtrabackup-*.tar.gz' | sort | tail -n1)")"


    rm -fr TARGET "${tarfile%.tar.gz}"
    mkdir TARGET
    tar xzf "${tarfile}"
    safe_cd "${tarfile%.tar.gz}"

    bash -x ./storage/innobase/xtrabackup/utils/build-binary.sh "${WORKDIR}/TARGET"

    publish_artifacts "tarball" "${WORKDIR}/TARGET/"*.tar.gz
}

# =============================================================================
# Main
# =============================================================================
main() {
    CURDIR="$(pwd)"
    args=""
    WORKDIR=""
    SRPM=0
    SDEB=0
    RPM=0
    DEB=0
    SOURCE=0
    BUILD_TARBALL=0
    OS_NAME=""
    ARCH=""
    OS=""
    RHEL=""
    REVISION=0
    BRANCH="8.0"
    INSTALL=0
    RPM_RELEASE=1
    DEB_RELEASE=1
    REPO="$PXB_REPO_DEFAULT"
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

    log_info "Build completed successfully"
}

main "$@"
