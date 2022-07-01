# Making a Compressed Backup

In order to make a compressed backup you’ll need to use `innobackupex --compress`

```default
$ innobackupex --compress /data/backup
```

If you want to speed up the compression you can use the parallel compression,
which can be enabled with `innobackupex --compress-threads`
option. Following example will use four threads for compression:

```default
$ innobackupex --compress --compress-threads=4 /data/backup
```

Output should look like this

```default
...
[01] Compressing ./imdb/comp_cast_type.ibd to /data/backup/2013-08-01_11-24-04/./imdb/comp_cast_type.ibd.qp
[01]        ...done
[01] Compressing ./imdb/aka_name.ibd to /data/backup/2013-08-01_11-24-04/./imdb/aka_name.ibd.qp
[01]        ...done
...
130801 11:50:24  innobackupex: completed OK
```

## Preparing the backup

Before you can prepare the backup you’ll need to uncompress all the files with
[qpress](http://www.quicklz.com/) (which is available from [Percona Software
repositories](http://www.percona.com/doc/percona-xtrabackup/2.1/installation.html#using-percona-software-repositories)).
You can use following one-liner to uncompress all the files:

```default
$ for bf in `find . -iname "*\.qp"`; do qpress -d $bf $(dirname $bf) && rm $bf; done
```

In *Percona XtraBackup* 2.1.4 new `innobackupex --decompress` option has
been implemented that can be used to decompress the backup:

```default
$ innobackupex --decompress /data/backup/2013-08-01_11-24-04/
```

!!! note

    In order to successfully use the `innobackupex --decompress` option,qpress binary needs to installed and within the path. `innobackupex --parallel` can be used with `innobackupex --decompress` option to decompress multiple files simultaneously.

When the files are uncompressed you can prepare the backup with
innobackupex –apply-log:

```default
$ innobackupex --apply-log /data/backup/2013-08-01_11-24-04/
```

You should check for a confirmation message:

```default
130802 02:51:02  innobackupex: completed OK!
```

Now the files in /data/backups/2013-08-01_11-24-04 is ready to be used
by the server.

!!! note

    *Percona XtraBackup* doesn’t automatically remove the compressed files. In order to clean up the backup directory users should remove the `\*.qp` files.

## Restoring the backup

Once the backup has been prepared you can use the `innobackupex --copy-back` to restore the backup.

```default
$ innobackupex --copy-back /data/backups/2013-08-01_11-24-04/
```

This will copy the prepared data back to its original location as defined by the
`datadir` in your `my.cnf`.

After the confirmation message:

```default
130802 02:58:44  innobackupex: completed OK!
```

you should check the file permissions after copying the data back. You may need
to adjust them with something like:

```default
$ chown -R mysql:mysql /var/lib/mysql
```

Now the `datadir` contains the restored data. You are ready to start the
server.
