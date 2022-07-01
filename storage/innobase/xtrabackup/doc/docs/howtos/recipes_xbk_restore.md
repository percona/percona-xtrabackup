# Restoring the Backup

Because `xtrabackup` doesn’t copy *MyISAM* files, `.frm` files, and the rest of the database, you might need to back those up separately. To restore the InnoDB data, simply do something like the following after preparing:

```default
cd /data/backups/mysql/
rsync -rvt --exclude 'xtrabackup_checkpoints' --exclude 'xtrabackup_logfile' \
   ./ /var/lib/mysql
chown -R mysql:mysql /var/lib/mysql/
```
