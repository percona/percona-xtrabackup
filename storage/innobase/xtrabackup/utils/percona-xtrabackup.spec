%define xb_version_major  @@XB_VERSION_MAJOR@@
%define xb_version_minor  @@XB_VERSION_MINOR@@
%define xb_version_patch  @@XB_VERSION_PATCH@@
%define xb_version_extra  @@XB_VERSION_EXTRA@@
%define xb_rpm_version_extra @@XB_RPM_VERSION_EXTRA@@
%define xb_revision       @@XB_REVISION@@
%global mysqldatadir /var/lib/mysql

#####################################
Name:           percona-xtrabackup
Version:        %{xb_version_major}.%{xb_version_minor}.%{xb_version_patch}
Release:        %{xb_rpm_version_extra}%{?dist}
Summary:        XtraBackup online backup for MySQL / InnoDB

Group:          Applications/Databases
License:        GPLv2
URL:            http://www.percona.com/software/percona-xtrabackup
Source:         percona-xtrabackup-%{version}%{xb_version_extra}.tar.gz

BuildRequires:  cmake, libaio-devel, libgcrypt-devel, ncurses-devel, readline-devel, zlib-devel, libev-devel openssl-devel
%if 0%{?rhel} > 5
BuildRequires:  libcurl-devel
%else
BuildRequires:  curl-devel
%endif
%if 0%{?rhel} > 6 
BuildRequires:  python-sphinx >= 1.0.1, python-docutils >= 0.6 
%endif
Conflicts:      percona-xtrabackup-21, percona-xtrabackup-22
Requires:       perl(DBD::mysql), rsync
Requires:	perl(Digest::MD5)
BuildRoot:      %{_tmppath}/%{name}-%{version}%{xb_version_extra}-root


%description
Percona XtraBackup is OpenSource online (non-blockable) backup solution for InnoDB and XtraDB engines

%package -n percona-xtrabackup-test
Summary:        Test suite for Percona XtraBackup
Group:          Applications/Databases
Requires:       percona-xtrabackup = %{version}-%{release}
Requires:       /usr/bin/mysql
AutoReqProv:    no

%description -n percona-xtrabackup-test
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
cmake -DBUILD_CONFIG=xtrabackup_release -DCMAKE_INSTALL_PREFIX=/opt/percona-xtrabackup/%{xb_version_major}.%{xb_version_minor} -DMYSQL_UNIX_ADDR="%{mysqldatadir}/mysql.sock" \
  -DWITH_SSL=system -DINSTALL_MYSQLTESTDIR=/opt/percona-xtrabackup/%{xb_version_major}.%{xb_version_minor}/percona-xtrabackup-test -DINSTALL_MANDIR=/opt/percona-xtrabackup/%{xb_version_major}.%{xb_version_minor} .
#
make %{?_smp_mflags}
#
%endif
#
%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT

%post
BIN_PATH=/opt/percona-xtrabackup/%{xb_version_major}.%{xb_version_minor}/bin
DOC_PATH=/opt/percona-xtrabackup/%{xb_version_major}.%{xb_version_minor}/man1
cmd=''
man_cmd=''
FILES=$BIN_PATH/*
for f in $FILES
do
  file=/usr/bin/$(basename $f)-%{xb_version_major}%{xb_version_minor}
  if [ -L $file ];
  then
    rm -rf $file
  fi
  ln -s $f $file
done
FILES=$DOC_PATH/*
for f in $FILES
do
  FileName=$(basename $f)
  file=/usr/share/man/man1/$FileName-%{xb_version_major}%{xb_version_minor}
  if [ -L $file ];
  then
    rm -rf $file
  fi
  ln -s $f $file
  man_cmd=" $man_cmd --slave /usr/share/man/man1/$FileName $FileName $file"
done
for f in /usr/bin/xtrabackup /usr/bin/innobackupex /usr/bin/xbstream /usr/bin/xbcrypt /usr/bin/xbcloud /usr/bin/xbcloud_osenv
do
  FileName=$(basename $f)
  if [ -L $f ]; then
    package=$(rpm -qfi $f | grep Name | awk '{print $3}')
    if [ -z "$package" ]; then
      package="alternatives"
    fi
    if [ $package == 'percona-xtrabackup-24' ]; then
      echo "You have percona-xtrabackup-24 installed please update it firstly!"
    else
      rm -rf $f
      if [ -z "$cmd" ]; then
        cmd="update-alternatives --install $f $FileName /usr/bin/$FileName-23 100"
      else
        cmd="$cmd --slave $f $FileName /usr/bin/$FileName-23 "
      fi
    fi
  elif [ -f $f ]; then
    echo "You have percona-xtrabackup installed please update it firstly!"
  else
    if [ -z "$cmd" ]; then
      cmd="update-alternatives --install $f $FileName /usr/bin/$FileName-23 100"
    else
      cmd="$cmd --slave $f $FileName /usr/bin/$FileName-23"
    fi
  fi
done
cmd="$cmd $man_cmd"
$cmd

%postun
if [ "$1" = 0 ]; then
  update-alternatives --remove xtrabackup "/usr/bin/xtrabackup-23"
fi

%post -n percona-xtrabackup-test
if [ ! -L /usr/share/percona-xtrabackup-test -a ! -f /usr/share/percona-xtrabackup-test ]; then
  ln -s /opt/percona-xtrabackup/%{xb_version_major}%{xb_version_minor}/percona-xtrabackup-test /usr/share/percona-xtrabackup-test-%{xb_version_major}%{xb_version_minor} || true
fi

%postun -n percona-xtrabackup-test
rm -f /usr/share/percona-xtrabackup-test-percona-xtrabackup-test-%{xb_version_major}%{xb_version_minor}

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
/opt/percona-xtrabackup/%{xb_version_major}.%{xb_version_minor}/bin/innobackupex
%doc COPYING
%doc /opt/percona-xtrabackup/%{xb_version_major}.%{xb_version_minor}/man1/*.1

%files -n percona-xtrabackup-test
%defattr(-,root,root,-)
/opt/percona-xtrabackup/%{xb_version_major}.%{xb_version_minor}/percona-xtrabackup-test

%triggerpostun -- percona-xtrabackup < 2.3.10
DOC_PATH=/opt/percona-xtrabackup/%{xb_version_major}.%{xb_version_minor}/man1
cmd=''
man_cmd=''
FILES=$DOC_PATH/*
for f in $FILES
do
  FileName=$(basename $f)
  file=/usr/share/man/man1/$FileName-%{xb_version_major}%{xb_version_minor}
  man_cmd=" $man_cmd --slave /usr/share/man/man1/$FileName $FileName $file"
done
for f in /usr/bin/xtrabackup /usr/bin/innobackupex /usr/bin/xbstream /usr/bin/xbcrypt /usr/bin/xbcloud /usr/bin/xbcloud_osenv
do
  FileName=$(basename $f)
  if [ -z "$cmd" ]; then
    cmd="update-alternatives --install $f $FileName /usr/bin/$FileName-23 100"
  else
    cmd="$cmd --slave $f $FileName /usr/bin/$FileName-23 "
  fi
done
cmd="$cmd $man_cmd"
$cmd


%changelog
* Mon Feb 13 2017 Evgeniy Patlan <evgeniy.patlan@percona.com>
- Update to new release Percona XtraBackup 2.3.7

* Mon Mar 14 2016 Tomislav Plavcic <tomislav.plavcic@percona.com>
- Update to new release Percona XtraBackup 2.3.4

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
