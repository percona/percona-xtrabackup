#
# rpm spec for xtrabackup
#
%{!?redhat_version:%define redhat_version 5}
%{!?buildnumber:%define buildnumber 1}
%define distribution  rhel%{redhat_version}
%define release       %{buildnumber}.%{distribution}
%{!?xtrabackup_revision:%define xtrabackup_revision undefined}

%define __os_install_post /usr/lib/rpm/brp-compress

Summary: XtraBackup online backup for MySQL / InnoDB 
Name: percona-xtrabackup
Version: %{xtrabackup_version}
Release: %{release}
Group: Server/Databases
License: GPLv2
Packager: Percona Development Team <mysql-dev@percona.com>
URL: http://www.percona.com/software/percona-xtrabackup/
Source: percona-xtrabackup-%{xtrabackup_version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-root
Provides: xtrabackup
Obsoletes: xtrabackup
BuildRequires: libaio-devel, libgcrypt-devel
Requires: perl(DBD::mysql)

%description
Percona XtraBackup is OpenSource online (non-blockable) backup solution for InnoDB and XtraDB engines.

%package test
Summary: Test suite for Percona Xtrabackup
Group: Applications/Databases
Requires: percona-xtrabackup
AutoReqProv: no
Requires: /usr/bin/mysql

%description test
This package contains the test suite for Percona Xtrabackup


%changelog
* Mon Sep 27 2010 Aleksandr Kuzminsky
- Version 1.4

* Wed Jun 30 2010 Aleksandr Kuzminsky
- Version 1.3 ported on Percona Server 11

* Thu Mar 11 2010 Aleksandr Kuzminsky
- Ported to MySQL 5.1 with InnoDB plugin

* Fri Mar 13 2009 Vadim Tkachenko
- initial release


%prep
%setup -q


%build
set -ue
%if %{undefined dummy}
export CC=${CC-"gcc"}
export CXX=${CXX-"g++"}
export CFLAGS="$CFLAGS -DXTRABACKUP_VERSION=\\\"%{xtrabackup_version}\\\" -DXTRABACKUP_REVISION=\\\"%{xtrabackup_revision}\\\"" 
export CXXFLAGS="$CXXFLAGS -DXTRABACKUP_VERSION=\\\"%{xtrabackup_version}\\\" -DXTRABACKUP_REVISION=\\\"%{xtrabackup_revision}\\\"" 
AUTO_DOWNLOAD=yes ./utils/build.sh xtradb
cp src/xtrabackup . 
AUTO_DOWNLOAD=yes ./utils/build.sh xtradb55
cp src/xtrabackup_55 src/xbstream src/xbcrypt .
AUTO_DOWNLOAD=yes ./utils/build.sh xtradb56
cp src/xtrabackup_56 .
%else
# Dummy binaries that avoid compilation
echo 'main() { return 300; }' | gcc -x c - -o xtrabackup
echo 'main() { return 300; }' | gcc -x c - -o xtrabackup_51
echo 'main() { return 300; }' | gcc -x c - -o xtrabackup_55
echo 'main() { return 300; }' | gcc -x c - -o xtrabackup_56
echo 'main() { return 300; }' | gcc -x c - -o xbstream
echo 'main() { return 300; }' | gcc -x c - -o xbcrypt
%endif

%install
[ "%{buildroot}" != '/' ] && rm -rf %{buildroot}
install -d %{buildroot}%{_bindir}
install -d %{buildroot}%{_datadir}
# install binaries and configs

install -m 755 xtrabackup %{buildroot}%{_bindir}
install -m 755 xtrabackup_55 %{buildroot}%{_bindir}
install -m 755 xtrabackup_56 %{buildroot}%{_bindir}
install -m 755 innobackupex %{buildroot}%{_bindir}
ln -s innobackupex %{buildroot}%{_bindir}/innobackupex-1.5.1
install -m 755 xbstream %{buildroot}%{_bindir}
install -m 755 xbcrypt %{buildroot}%{_bindir}
cp -R test %{buildroot}%{_datadir}/percona-xtrabackup-test

%clean
[ "%{buildroot}" != '/' ] && rm -rf %{buildroot}

%files
%defattr(-,root,root)
%{_bindir}/innobackupex
%{_bindir}/innobackupex-1.5.1
%{_bindir}/xtrabackup
%{_bindir}/xtrabackup_55
%{_bindir}/xtrabackup_56
%{_bindir}/xbstream
%{_bindir}/xbcrypt
%doc COPYING

%files -n percona-xtrabackup-test
%{_datadir}/percona-xtrabackup-test

###
### eof
###


