%define xb_version_major  @@XB_VERSION_MAJOR@@
%define xb_version_minor  @@XB_VERSION_MINOR@@
%define xb_version_patch  @@XB_VERSION_PATCH@@
%define xb_version_extra  @@XB_VERSION_EXTRA@@
%define xb_rpm_version_extra @@XB_RPM_VERSION_EXTRA@@
%define xb_revision       @@XB_REVISION@@
%define rpm_release       @@RPM_RELEASE@@

%global mysqldatadir /var/lib/mysql

#####################################
Name:           percona-xtrabackup-%{xb_version_major}%{xb_version_minor}
Version:        %{xb_version_major}.%{xb_version_minor}.%{xb_version_patch}
Release:        %{xb_rpm_version_extra}%{?dist}
Summary:        XtraBackup online backup for MySQL / InnoDB

License:        GPLv2
URL:            http://www.percona.com/software/percona-xtrabackup
Source:         percona-xtrabackup-%{version}%{xb_version_extra}.tar.gz
Source999:      call-home.sh
BuildRoot:      %{_tmppath}/%{name}-%{version}%{xb_version_extra}-root

BuildRequires:  cmake, libaio-devel, libgcrypt-devel, ncurses-devel, readline-devel
BuildRequires:  zlib-devel, libev-devel, openssl-devel, libcurl-devel, patchelf
Conflicts:      percona-xtrabackup-21, percona-xtrabackup-22, percona-xtrabackup, percona-xtrabackup-24
Requires:       perl(DBD::mysql), rsync, zstd
Requires:       perl(Digest::MD5), lz4


%description
Percona XtraBackup is OpenSource online (non-blockable) backup solution for InnoDB and XtraDB engines

%package -n percona-xtrabackup-test-%{xb_version_major}%{xb_version_minor}
Summary:        Test suite for Percona XtraBackup
Group:          Applications/Databases
Requires:       percona-xtrabackup-%{xb_version_major}%{xb_version_minor} = %{version}-%{release}
Requires:       /usr/bin/mysql
AutoReqProv:    no

%description -n percona-xtrabackup-test-%{xb_version_major}%{xb_version_minor}
This package contains the test suite for Percona XtraBackup %{version}%{xb_version_extra}

%prep
%setup -q -n percona-xtrabackup-%{version}%{xb_version_extra}

%bcond_with dummy

%build
%if %{with dummy}
echo 'int main() { return 300; }' | gcc -x c - -o storage/innobase/xtrabackup/src/xtrabackup
echo 'int main() { return 300; }' | gcc -x c - -o storage/innobase/xtrabackup/src/xbstream
echo 'int main() { return 300; }' | gcc -x c - -o storage/innobase/xtrabackup/src/xbcrypt
echo 'int main() { return 300; }' | gcc -x c - -o storage/innobase/xtrabackup/src/xbcloud
%else

export CC=${CC-"gcc"}
export CXX=${CXX-"g++"}
export CFLAGS=${CFLAGS:-}
export CXXFLAGS=${CXXFLAGS:-}

# Fix ambiguous python shebangs (EL8+ brp-mangle-shebangs rejects '#!/usr/bin/env python')
find . -name '*.py' -o -name 'subunit2junitxml' | \
  xargs sed -i 's|#!/usr/bin/env python$|#!/usr/bin/env python3|g' 2>/dev/null || true

# Use ccache if available for faster rebuilds
if command -v ccache &>/dev/null; then
  export CMAKE_C_COMPILER_LAUNCHER=ccache
  export CMAKE_CXX_COMPILER_LAUNCHER=ccache
fi

# Common cmake flags (shell variable — works on all rpmbuild versions)
CMAKE_FLAGS="-DBUILD_CONFIG=xtrabackup_release"
CMAKE_FLAGS+=" -DCMAKE_INSTALL_PREFIX=%{_prefix}"
CMAKE_FLAGS+=" -DWITH_SSL=system"
CMAKE_FLAGS+=" -DINSTALL_MANDIR=%{_mandir}"
CMAKE_FLAGS+=" -DWITH_MAN_PAGES=1"
CMAKE_FLAGS+=" -DMINIMAL_RELWITHDEBINFO=OFF"
CMAKE_FLAGS+=" -DINSTALL_MYSQLTESTDIR=%{_datadir}/percona-xtrabackup-test-%{xb_version_major}%{xb_version_minor}"
CMAKE_FLAGS+=" -DDOWNLOAD_BOOST=1"
CMAKE_FLAGS+=" -DWITH_BOOST=%{_builddir}/boost"
CMAKE_FLAGS+=" -DMYSQL_UNIX_ADDR=%{mysqldatadir}/mysql.sock"
CMAKE_FLAGS+=" -DINSTALL_PLUGINDIR=%{_lib}/xtrabackup/plugin"
CMAKE_FLAGS+=" -DFORCE_INSOURCE_BUILD=1"
CMAKE_FLAGS+=" -DWITH_ZLIB=bundled"
CMAKE_FLAGS+=" -DWITH_ZSTD=bundled"
CMAKE_FLAGS+=" -DWITH_PROTOBUF=bundled"

# Shared boost directory — downloaded once by debug build, reused by release
mkdir -p %{_builddir}/boost

# Debug build (runs first, downloads boost)
mkdir debug
cd debug
cmake .. ${CMAKE_FLAGS} -DCMAKE_BUILD_TYPE=Debug
make %{?_smp_mflags}
cd ..

# Release build (reuses cached boost)
cmake . ${CMAKE_FLAGS}
make %{?_smp_mflags}

%endif

%install
make install DESTDIR=%{buildroot}

cp -v debug/bin/xtrabackup %{buildroot}/%{_bindir}/xtrabackup-debug
patchelf --set-rpath '$ORIGIN/../lib/private' %{buildroot}/%{_bindir}/xtrabackup-debug

# Remove unwanted artifacts
rm -f  %{buildroot}/%{_libdir}/libmysqlservices.a \
       %{buildroot}/usr/lib/libmysqlservices.a
rm -rf %{buildroot}/usr/docs/INFO_SRC \
       %{buildroot}/%{_mandir}/man8

# Keep only xtrabackup/xbstream/xbcrypt/xbcloud man pages
find %{buildroot}/%{_mandir}/man1 -type f \
    ! -name 'xtrabackup*' \
    ! -name 'xbstream*' \
    ! -name 'xbcrypt*' \
    ! -name 'xbcloud*' \
    -delete

%post
cp %SOURCE999 /tmp/ 2>/dev/null ||
bash /tmp/call-home.sh -f "PRODUCT_FAMILY_PXB" -v %{xb_version_major}.%{xb_version_minor}.%{xb_version_patch}%{xb_version_extra}-%{rpm_release} -d "PACKAGE" &>/dev/null || :
rm -f /tmp/call-home.sh

%files
%license LICENSE
%{_bindir}/xtrabackup
%{_bindir}/xtrabackup-debug
%{_bindir}/xbstream
%{_bindir}/xbcrypt
%{_bindir}/xbcloud
%{_bindir}/xbcloud_osenv
/usr/lib/private/libprotobuf*
/usr/lib/private/icudt73l
%{_libdir}/xtrabackup/plugin/keyring_file.so
%{_libdir}/xtrabackup/plugin/keyring_vault.so
%{_libdir}/xtrabackup/plugin/component_keyring_file.so
%{_libdir}/xtrabackup/plugin/component_keyring_kms.so
%{_includedir}/kmip.h
%{_includedir}/kmippp.h
/usr/lib/libkmip.a
/usr/lib/libkmippp.a
%{_libdir}/xtrabackup/plugin/component_keyring_kmip.so
%{_mandir}/man1/*.1*

%files -n percona-xtrabackup-test-%{xb_version_major}%{xb_version_minor}
%{_datadir}/percona-xtrabackup-test-%{xb_version_major}%{xb_version_minor}

%changelog
* Fri Aug 31 2018 Evgeniy Patlan <evgeniy.patlan@percona.com>
- Packaging for 8.0

