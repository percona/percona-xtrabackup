#
# rpm spec for xtrabackup
#
%{!?redhat_version:%define redhat_version 5}
%{!?buildnumber:%define buildnumber 1}
%define distribution  rhel%{redhat_version}
%define release       %{buildnumber}.%{distribution}
%define xtrabackup_version 1.3
%define xtradb_version 11
%{!?xtrabackup_revision:%define xtrabackup_revision undefined}

Summary: XtraBackup online backup for MySQL / InnoDB 
Name: xtrabackup
Version: %{xtrabackup_version}
Release: %{release}
Group: Server/Databases
License: GPLv2
Packager: Vadim Tkachenko <vadim@percona.com>
URL: http://www.percona.com/software/percona-xtrabackup/
Source00: Percona-Server.tar.gz
Source01: xtrabackup-%{xtrabackup_version}.tar.gz
Source02: libtar-1.2.11.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-root
Requires: mysql-client|mysql

%description
Percona XtraBackup is OpenSource online (non-blockable) backup solution for InnoDB and XtraDB engines.


%changelog
* Wed Jun 30 2010 Aleksandr Kuzminsky
- Version 1.3 ported on Percona Server 11

* Thu Mar 11 2010 Aleksandr Kuzminsky
- Ported to MySQL 5.1 with InnoDB plugin

* Fri Mar 13 2009 Vadim Tkachenko
- initial release


%prep
%setup -q -n Percona-Server
cd storage/innodb_plugin
tar zxf $RPM_SOURCE_DIR/xtrabackup-%{xtrabackup_version}.tar.gz
mv xtrabackup-%{xtrabackup_version} xtrabackup
cd -
tar zxf $RPM_SOURCE_DIR/libtar-1.2.11.tar.gz
cd libtar-1.2.11
patch -p1 < ../storage/innodb_plugin/xtrabackup/tar4ibd_libtar-1.2.11.patch
cd ..
patch -p1 < storage/innodb_plugin/xtrabackup/fix_innodb_for_backup_percona-server-%{xtradb_version}.patch


%build
export CC=${CC-"gcc"} 
export CXX=$CC 
export CFLAGS="$CFLAGS -DXTRABACKUP_VERSION=\\\"%{xtrabackup_version}\\\" -DXTRABACKUP_REVISION=\\\"%{xtrabackup_revision}\\\"" 
./configure \
  --prefix=%{_prefix} --enable-local-infile --enable-thread-safe-client --with-plugins=innodb_plugin --with-zlib-dir=bundled --with-extra-charsets=complex
make -j`if [ -f /proc/cpuinfo ] ; then grep -c processor.* /proc/cpuinfo ; else echo 1 ; fi`
cd storage/innodb_plugin/xtrabackup
make
cd ../../..
cd libtar-1.2.11
./configure --prefix=%{_prefix}
make

%install
[ "%{buildroot}" != '/' ] && rm -rf %{buildroot}
install -d %{buildroot}%{_bindir}
# install binaries and configs
install -m 755 storage/innodb_plugin/xtrabackup/{innobackupex-1.5.1,xtrabackup} %{buildroot}%{_bindir}
install -m 755 libtar-1.2.11/libtar/tar4ibd %{buildroot}%{_bindir}

%clean
[ "%{buildroot}" != '/' ] && rm -rf %{buildroot}

%files
%defattr(-,root,root)
%{_bindir}/innobackupex-1.5.1
%{_bindir}/xtrabackup
%{_bindir}/tar4ibd


###
### eof
###


