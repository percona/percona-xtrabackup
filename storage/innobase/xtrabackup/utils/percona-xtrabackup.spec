%define xb_version_major  @@XB_VERSION_MAJOR@@
%define xb_version_minor  @@XB_VERSION_MINOR@@
%define xb_version_patch  @@XB_VERSION_PATCH@@
%define xb_version_extra  @@XB_VERSION_EXTRA@@
%define xb_rpm_version_extra @@XB_RPM_VERSION_EXTRA@@
%define xb_revision       @@XB_REVISION@@

#####################################
Name:           percona-xtrabackup-%{xb_version_major}%{xb_version_minor}
Version:        %{xb_version_major}.%{xb_version_minor}.%{xb_version_patch}
Release:        %{xb_rpm_version_extra}%{?dist}
Summary:        XtraBackup online backup for MySQL / InnoDB

Group:          Applications/Databases
License:        GPLv2
URL:            http://www.percona.com/software/percona-xtrabackup
Source:         percona-xtrabackup-%{version}%{xb_version_extra}.tar.gz

BuildRequires:  cmake, libaio-devel, libgcrypt-devel, ncurses-devel, readline-devel, zlib-devel, libev-devel
%if 0%{?rhel} > 5
BuildRequires:  libcurl-devel
%else
BuildRequires:  curl-devel
%endif
%if 0%{?rhel} > 6 
BuildRequires:  python-sphinx >= 1.0.1, python-docutils >= 0.6 
%endif
Conflicts:      percona-xtrabackup-21, percona-xtrabackup-22, percona-xtrabackup
Requires:       perl(DBD::mysql), rsync
BuildRoot:      %{_tmppath}/%{name}-%{version}%{xb_version_extra}-root

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
cmake -DBUILD_CONFIG=xtrabackup_release -DCMAKE_INSTALL_PREFIX=%{_prefix} \
  -DINSTALL_MYSQLTESTDIR=%{_datadir}/percona-xtrabackup-test-%{xb_version_major}%{xb_version_minor} -DINSTALL_MANDIR=%{_mandir} \
  -DDOWNLOAD_BOOST=1 -DWITH_BOOST=libboost .
#
make %{?_smp_mflags}
#
%endif
#
%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%{_bindir}/innobackupex
%{_bindir}/xtrabackup
%{_bindir}/xbstream
%{_bindir}/xbcrypt
%{_bindir}/xbcloud
%{_bindir}/xbcloud_osenv
%doc COPYING
%doc %{_mandir}/man1/*.1.gz

%files -n percona-xtrabackup-test-%{xb_version_major}%{xb_version_minor}
%defattr(-,root,root,-)
%{_datadir}/percona-xtrabackup-test-%{xb_version_major}%{xb_version_minor}

%changelog
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

* Wed Sep 29 2014 Tomislav Plavcic <tomislav.plavcic@percona.com>
- Update to new release Percona XtraBackup 2.2.6

* Fri Sep 26 2014 Tomislav Plavcic <tomislav.plavcic@percona.com>
- Update to new release Percona XtraBackup 2.2.5

* Thu Sep 11 2014 Tomislav Plavcic <tomislav.plavcic@percona.com>
- Changed options to build with system zlib

* Mon Jun 10 2014 Tomislav Plavcic <tomislav.plavcic@percona.com>
- renamed package from percona-xtrabackup-22 to percona-xtrabackup

* Wed Mar 26 2014 Alexey Bychko <alexey.bychko@percona.com>
- initial alpha release for 2.2 (2.2.1-alpha1)
