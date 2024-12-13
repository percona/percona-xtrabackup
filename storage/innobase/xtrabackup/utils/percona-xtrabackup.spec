%define xb_version_major  @@XB_VERSION_MAJOR@@
%define xb_version_minor  @@XB_VERSION_MINOR@@
%define xb_version_patch  @@XB_VERSION_PATCH@@
%define xb_version_extra  @@XB_VERSION_EXTRA@@
%define xb_rpm_version_extra @@XB_RPM_VERSION_EXTRA@@
%define xb_revision       @@XB_REVISION@@
%if 0%{?rhel} == 8
%define cmake_bin cmake
%else
%define cmake_bin cmake3
%endif
%global mysqldatadir /var/lib/mysql

# By default a build will be done in normal mode
%{?enable_fipsmode: %global enable_fipsmode 1}

%if 0%{?rhel} == 7
%global __python %{__python3}
%endif

%if 0%{?rhel} == 6
%global __python /opt/rh/rh-python36/root/usr/bin/python3
%endif

#####################################
Name:           percona-xtrabackup-%{xb_version_major}%{xb_version_minor}
Version:        %{xb_version_major}.%{xb_version_minor}.%{xb_version_patch}
Release:        %{xb_rpm_version_extra}%{?dist}
Summary:        XtraBackup online backup for MySQL / InnoDB

Group:          Applications/Databases
License:        GPLv2
URL:            http://www.percona.com/software/percona-xtrabackup
Source:         percona-xtrabackup-%{version}%{xb_version_extra}.tar.gz
Source999:      call-home.sh

BuildRequires:  %{cmake_bin}, libaio-devel, libgcrypt-devel, ncurses-devel, readline-devel, zlib-devel, libev-devel openssl-devel
BuildRequires:  libcurl-devel
Conflicts:      percona-xtrabackup-21, percona-xtrabackup-22, percona-xtrabackup, percona-xtrabackup-24, percona-xtrabackup-80, percona-xtrabackup-81, percona-xtrabackup-82
Conflicts:      percona-xtrabackup-pro-%{xb_version_major}%{xb_version_minor}
Requires:       perl(DBD::mysql), rsync, zstd
Requires:	perl(Digest::MD5), lz4
BuildRoot:      %{_tmppath}/%{name}-%{version}%{xb_version_extra}-root


%description
Percona XtraBackup is OpenSource online (non-blockable) backup solution for InnoDB and XtraDB engines

%package -n percona-xtrabackup-test-%{xb_version_major}%{xb_version_minor}
Summary:        Test suite for Percona XtraBackup
Group:          Applications/Databases
Conflicts:      percona-xtrabackup-test-pro-%{xb_version_major}%{xb_version_minor}
Requires:       percona-xtrabackup-%{xb_version_major}%{xb_version_minor} = %{version}-%{release}
Requires:       /usr/bin/mysql
AutoReqProv:    no

%description -n percona-xtrabackup-test-%{xb_version_major}%{xb_version_minor}
This package contains the test suite for Percona XtraBackup %{version}%{xb_version_extra}

%prep
%setup -q -n percona-xtrabackup-%{version}%{xb_version_extra}

%bcond_with dummy

%build
#
%if %{with dummy}
# Dummy binaries that avoid compilation
echo 'main() { return 300; }' | gcc -x c - -o storage/innobase/xtrabackup/src/xtrabackup
echo 'main() { return 300; }' | gcc -x c - -o storage/innobase/xtrabackup/src/xbstream
echo 'main() { return 300; }' | gcc -x c - -o storage/innobase/xtrabackup/src/xbcrypt
echo 'main() { return 300; }' | gcc -x c - -o storage/innobase/xtrabackup/src/xbcloud
#
%else
#
export CC=${CC-"gcc"}
export CXX=${CXX-"g++"}
export CFLAGS=${CFLAGS:-}
export CXXFLAGS=${CXXFLAGS:-}
#
sed -i 's:#!/usr/bin/env python:#!/usr/bin/env python2:g' storage/innobase/xtrabackup/test/subunit2junitxml
sed -i 's:#!/usr/bin/env python:#!/usr/bin/env python2:g' storage/innobase/xtrabackup/test/python/subunit/tests/sample-two-script.py
sed -i 's:#!/usr/bin/env python:#!/usr/bin/env python2:g' storage/innobase/xtrabackup/test/python/subunit/tests/sample-script.py
sed -i 's:#!/usr/bin/env python:#!/usr/bin/env python2:g' storage/innobase/xtrabackup/test/python/subunit/run.py
# debug
mkdir debug
(
  cd debug
  %{cmake_bin} .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_CONFIG=xtrabackup_release -DCMAKE_INSTALL_PREFIX=%{_prefix} \
  -DWITH_SSL=system -DINSTALL_MANDIR=%{_mandir} -DWITH_MAN_PAGES=1 \
  -DINSTALL_MYSQLTESTDIR=%{_datadir}/percona-xtrabackup-test-%{xb_version_major}%{xb_version_minor} \
  -DDOWNLOAD_BOOST=1 -DWITH_BOOST=libboost -DMINIMAL_RELWITHDEBINFO=OFF -DMYSQL_UNIX_ADDR="%{mysqldatadir}/mysql.sock" \
%if 0%{?enable_fipsmode}
  -DPROBUILD=1 \
%endif
  -DINSTALL_PLUGINDIR="%{_lib}/xtrabackup/plugin" -DFORCE_INSOURCE_BUILD=1 -DWITH_ZLIB=bundled -DWITH_ZSTD=bundled -DWITH_PROTOBUF=bundled
  make %{?_smp_mflags}
  cd ..
  ls -la debug
)
# release
%{cmake_bin} . -DBUILD_CONFIG=xtrabackup_release -DCMAKE_INSTALL_PREFIX=%{_prefix} \
  -DWITH_SSL=system -DINSTALL_MANDIR=%{_mandir} -DWITH_MAN_PAGES=1 \
  -DINSTALL_MYSQLTESTDIR=%{_datadir}/percona-xtrabackup-test-%{xb_version_major}%{xb_version_minor} \
  -DDOWNLOAD_BOOST=1 -DWITH_BOOST=libboost -DMINIMAL_RELWITHDEBINFO=OFF -DMYSQL_UNIX_ADDR="%{mysqldatadir}/mysql.sock" \
%if 0%{?enable_fipsmode}
  -DPROBUILD=1 \
%endif
  -DINSTALL_PLUGINDIR="%{_lib}/xtrabackup/plugin" -DFORCE_INSOURCE_BUILD=1 -DWITH_ZLIB=bundled -DWITH_ZSTD=bundled -DWITH_PROTOBUF=bundled
#
make %{?_smp_mflags}
#
%endif
#
%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT
cp -v debug/bin/xtrabackup $RPM_BUILD_ROOT/%{_bindir}/xtrabackup-debug
patchelf --debug --set-rpath '$ORIGIN/../lib/private' $RPM_BUILD_ROOT/%{_bindir}/xtrabackup-debug
rm -rf $RPM_BUILD_ROOT/%{_libdir}/libmysqlservices.a
rm -rf $RPM_BUILD_ROOT/usr/lib/libmysqlservices.a
rm -rf $RPM_BUILD_ROOT/usr/docs/INFO_SRC
rm -rf $RPM_BUILD_ROOT/%{_mandir}/man8
rm -rf $RPM_BUILD_ROOT/%{_mandir}/man1/c*
rm -rf $RPM_BUILD_ROOT/%{_mandir}/man1/m*
rm -rf $RPM_BUILD_ROOT/%{_mandir}/man1/i*
rm -rf $RPM_BUILD_ROOT/%{_mandir}/man1/l*
rm -rf $RPM_BUILD_ROOT/%{_mandir}/man1/p*
rm -rf $RPM_BUILD_ROOT/%{_mandir}/man1/z*

%post
cp %SOURCE999 /tmp/ 2>/dev/null ||
%if 0%{?enable_fipsmode}
bash /tmp/call-home.sh -f "PRODUCT_FAMILY_PXB" -v %{xb_version_major}.%{xb_version_minor}.%{xb_version_patch}-%{xb_version_extra}-%{xb_rpm_version_extra}-pro -d "PACKAGE" &>/dev/null || :
%else
bash /tmp/call-home.sh -f "PRODUCT_FAMILY_PXB" -v %{xb_version_major}.%{xb_version_minor}.%{xb_version_patch}-%{xb_version_extra}-%{xb_rpm_version_extra} -d "PACKAGE" &>/dev/null || :
%endif
rm -f /tmp/call-home.sh

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%{_bindir}/xtrabackup
%{_bindir}/xtrabackup-debug
%{_bindir}/xbstream
%{_bindir}/xbcrypt
%{_bindir}/xbcloud
%{_bindir}/xbcloud_osenv
/usr/lib/private/libprotobuf*
/usr/lib/private/icudt73l
/usr/lib/private/libabsl_*
%{_libdir}/xtrabackup/plugin/component_keyring_vault.so
%{_libdir}/xtrabackup/plugin/component_keyring_file.so
%{_libdir}/xtrabackup/plugin/component_keyring_kms.so
%{_includedir}/kmip.h
%{_includedir}/kmippp.h
/usr/lib/libkmip.a
/usr/lib/libkmippp.a
%{_libdir}/xtrabackup/plugin/component_keyring_kmip.so
%doc LICENSE
%doc %{_mandir}/man1/*.1.gz

%files -n percona-xtrabackup-test-%{xb_version_major}%{xb_version_minor}
%defattr(-,root,root,-)
%{_datadir}/percona-xtrabackup-test-%{xb_version_major}%{xb_version_minor}

%changelog
* Fri Aug 31 2018 Evgeniy Patlan <evgeniy.patlan@percona.com>
- Packaging for 8.0

* Wed Feb 03 2016 Tomislav Plavcic <tomislav.plavcic@percona.com>
- Packaging updates for version 2.4.0-rc1

* Mon Dec 14 2015 Tomislav Plavcic <tomislav.plavcic@percona.com>
- Update to new release Percona XtraBackup 2.3.3

* Fri Oct 16 2015 Tomislav Plavcic <tomislav.plavcic@percona.com>
- Update to new release Percona XtraBackup 2.3.2
- Renamed the package to percona-xtrabackup since 2.3 became GA

* Fri May 15 2015 Tomislav Plavcic <tomislav.plavcic@percona.com>
- Update to new release Percona XtraBackup 2.3.1beta1

* Thu Oct 30 2014 Tomislav Plavcic <tomislav.plavcic@percona.com>
- Update to new release Percona XtraBackup 2.3.0alpha1

* Mon Sep 29 2014 Tomislav Plavcic <tomislav.plavcic@percona.com>
- Update to new release Percona XtraBackup 2.2.6

* Fri Sep 26 2014 Tomislav Plavcic <tomislav.plavcic@percona.com>
- Update to new release Percona XtraBackup 2.2.5

* Thu Sep 11 2014 Tomislav Plavcic <tomislav.plavcic@percona.com>
- Changed options to build with system zlib

* Tue Jun 10 2014 Tomislav Plavcic <tomislav.plavcic@percona.com>
- renamed package from percona-xtrabackup-22 to percona-xtrabackup

* Wed Mar 26 2014 Alexey Bychko <alexey.bychko@percona.com>
- initial alpha release for 2.2 (2.2.1-alpha1)
