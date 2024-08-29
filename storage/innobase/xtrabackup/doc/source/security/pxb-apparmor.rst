.. _pxb-apparmor:

=============================================
Working with AppArmor 
=============================================

The Linux Security Module implements mandatory access controls (MAC) with AppArmor. Debian and Ubuntu systems install AppArmor by default. AppArmor uses profiles which define which files and permissions are needed for application.

*Percona XtraBackup* does not have a profile and is not confined by AppArmor. 

For a list of common AppArmor commands, see `Percona Server for MySQL - AppArmor <https://www.percona.com/doc/percona-server/LATEST/security/apparmor.html>`_

Develop a profile
-------------------

Download the profile from:

https://github.com/percona/percona-xtrabackup/tree/8.0/packaging/percona/apparmor/apparmor.d

The following profile sections should be updated with your system information, such as location of the backup destination directory.

.. sourcecode:: text

    # enable storing backups only in /backups directory
    # /backups/** rwk,

    # enable storing backups anywhere in caller user home directory
    /@{HOME}/** rwk,


    # enable storing backups only in /backups directory
    # /backups/** rwk,

    # enable storing backups anywhere in caller user home directory
    /@{HOME}/** rwk,
    }
    
    # enable storing backups only in /backups directory
    # /backups/** rwk,

    # enable storing backups anywhere in caller user home directory
    /@{HOME}/** rwk,
    }

Move the updated file:

.. sourcecode:: bash

    $ sudo mv usr.sbin.xtrabackup /etc/apparmor.d/
    
Install the profile with the following command:

    .. sourcecode:: bash

        $ sudo apparmor_parser -r -T -W /etc/apparmor.d/usr.sbin.xtrabackup 
    
Run the backup as usual. 

No additional AppArmor-related actions are required to restore a backup. 
