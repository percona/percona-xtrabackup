#
# rpm spec for xtrabackup
#
%define distribution  rhel5
%define release       1.%{distribution}

Summary: XtraBackup online backup for MySQL / InnoDB 
Name: xtrabackup
Version: 0.3
Release: %{release}
Group: Server/Databases
License: GPLv2
Packager: Vadim Tkachenko <vadim@percona.com>
URL: http://percona.com/percona-lab.html
Source: %{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-root

%description
Percona XtraBackup is OpenSource online (non-blockable) backup solution for InnoDB and XtraDB engines.


%changelog
* Fri Mar 13 2009 Vadim Tkachenko
- initial release


%prep
%setup -q


%build
CC="ccache gcc" CXX="ccache gcc" ./configure \
  --prefix=%{_prefix} 
make -j8
cd innobase/xtrabackup
make


%install
[ "%{buildroot}" != '/' ] && rm -rf %{buildroot}
install -d %{buildroot}%{_bindir}
# install binaries and configs
install -m 755 innobase/xtrabackup/{innobackupex-1.5.1,xtrabackup} %{buildroot}%{_bindir}

%clean
[ "%{buildroot}" != '/' ] && rm -rf %{buildroot}

%files
%defattr(-,root,root)
%{_bindir}/innobackupex-1.5.1
%{_bindir}/xtrabackup


###
### eof
###


