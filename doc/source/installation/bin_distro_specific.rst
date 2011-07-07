==========================================
 Distribution-specific installation notes
==========================================

SUSE 11
=======

Although there is no specific build for SUSE, the build for RHEL is suitable for this distribution.

* Download the RPM package of |XtraBackup| for your architecture at http://www.percona.com/downloads/XtraBackup/LATEST/

* Copy to a directory (like /tmp) and extract the RPM: ::

    rpm2cpio xtrabackup-1.5-9.rhel5.$ARCH.rpm | cpio -idmv

* Copy binaries to /usr/bin: ::

    cp ./usr/bin/xtrabackup_55 /usr/bin
    cp ./usr/bin/tar4ibd /usr/bin
    cp ./usr/bin/innobackupex-1.5.1 /usr/bin

* If you use a version prior to 1.6, the stock perl causes an issue with the backup scripts version detection. Edit :file:`/usr/bin/innobackupex-1.5.1`. Comment out the lines below as shown below ::

    $perl_version = chr($required_perl_version[0])
    . chr($required_perl_version[1])
    . chr($required_perl_version[2]);
    #if ($^V lt $perl_version) {    
    #my $version = chr(48 + $required_perl_version[0])    
    # . "." . chr(48 + $required_perl_version[1])    
    # . "." . chr(48 + $required_perl_version[2]);    
    #print STDERR "$prefix Warning: " .    
    # "Your perl is too old! Innobackup requires\n";    
    #print STDERR "$prefix Warning: perl $version or newer!\n";
    #}
