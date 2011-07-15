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
Name: xtrabackup
Version: %{xtrabackup_version}
Release: %{release}
Group: Server/Databases
License: GPLv2
Packager: Vadim Tkachenko <vadim@percona.com>
URL: http://www.percona.com/software/percona-xtrabackup/
Source: xtrabackup-%{xtrabackup_version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-root
Requires: mysql
BuildRequires: libaio-devel

%description
Percona XtraBackup is OpenSource online (non-blockable) backup solution for InnoDB and XtraDB engines.


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
export CC=${CC-"gcc"}
export CXX=$CC
export CFLAGS="$CFLAGS -DXTRABACKUP_VERSION=\\\"%{xtrabackup_version}\\\" -DXTRABACKUP_REVISION=\\\"%{xtrabackup_revision}\\\"" 
export CXXFLAGS="$CXXFLAGS -DXTRABACKUP_VERSION=\\\"%{xtrabackup_version}\\\" -DXTRABACKUP_REVISION=\\\"%{xtrabackup_revision}\\\" -fno-exceptions" 
cp $RPM_SOURCE_DIR/libtar-1.2.11.tar.gz $RPM_SOURCE_DIR/mysql-5.1.56.tar.gz \
    $RPM_SOURCE_DIR/mysql-5.5.10.tar.gz .
./utils/build.sh 5.1
./utils/build.sh xtradb
./utils/build.sh xtradb55

%install
[ "%{buildroot}" != '/' ] && rm -rf %{buildroot}
install -d %{buildroot}%{_bindir}
install -d %{buildroot}%{_datadir}
# install binaries and configs

install -m 755 Percona-Server/storage/innodb_plugin/xtrabackup/xtrabackup %{buildroot}%{_bindir}
install -m 755 Percona-Server-5.5/storage/innobase/xtrabackup/xtrabackup_55 %{buildroot}%{_bindir}
install -m 755 innobackupex %{buildroot}%{_bindir}
ln -s innobackupex %{buildroot}%{_bindir}/innobackupex-1.5.1
install -m 755 mysql-5.1/storage/innobase/xtrabackup/xtrabackup_51 %{buildroot}%{_bindir}
install -m 755 libtar-1.2.11/libtar/tar4ibd %{buildroot}%{_bindir}
cp -R test %{buildroot}%{_datadir}/xtrabackup-test

%clean
[ "%{buildroot}" != '/' ] && rm -rf %{buildroot}

%files
%defattr(-,root,root)
%{_bindir}/innobackupex
%{_bindir}/innobackupex-1.5.1
%{_bindir}/xtrabackup
%{_bindir}/xtrabackup_51
%{_bindir}/xtrabackup_55
%{_bindir}/tar4ibd
%{_datadir}/xtrabackup-test


###
### eof
###


