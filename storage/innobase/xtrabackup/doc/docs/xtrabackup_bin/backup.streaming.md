# Streaming Backups

**Percona XtraBackup** supports streaming mode. Streaming mode sends a backup to `STDOUT` in the *xbstream* format instead of copying the files to the backup directory.

This method allows you to use other programs to filter the output of the backup,
providing greater flexibility for storage of the backup. For example,
compression is achieved by piping the output to a compression utility. One of
the benefits of streaming backups and using Unix pipes is that the backups can
be automatically encrypted.

To use the streaming feature, you must use the `--stream`,
providing the format of the stream (`xbstream` ) and where to store
the temporary files:

```
$ xtrabackup --stream=xbstream --target-dir=/tmp
```

*xtrabackup* uses *xbstream* to stream all of the data files to `STDOUT`, in a
special `xbstream` format. After it finishes streaming all of the data files
to `STDOUT`, it stops xtrabackup and streams the saved log file too.

When compression is enabled, *xtrabackup* compresses the output data, except for the meta and non-InnoDB files which are not compressed, using the specified
compression algorithm. The only currently supported algorithm is
`quicklz`. The resulting files have the `qpress` archive format, i.e. every
\*.qp file produced by xtrabackup is essentially a one-file qpress archive and
can be extracted and uncompressed by the [qpress file archiver](http://www.quicklz.com/) which is available from Percona Software
repositories.

Using *xbstream* as a stream option, backups can be copied and compressed in
parallel. This option can significantly improve the speed of the backup process. In case backups
were both compressed and encrypted, they must be decrypted before they are uncompressed.

| Task

 | Command

 |
| ----------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------- |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |
| Stream the backup into an archive named `backup.xbstream`

           | `xtrabackup --backup --stream=xbstream --target-dir=./ > backup.xbstream`

                                                                                                                                                                                                                                                                                                                                                                                                     |
| Stream the backup into a compressed archive named `backup.xbstream`

 | `xtrabackup --backup --stream=xbstream --compress --target-dir=./ > backup.xbstream`

                                                                                                                                                                                                                                                                                                                                                                                          |
| Encrypt the backup

                                                | $ xtrabackup –backup –stream=xbstream ./ > backup.xbstream gzip -  | openssl des3 -salt -k “password” > backup.xbstream.gz.des3

                                                                                                                                                                                                                                                                                                                                             |
| Unpack the backup to the current directory

                        | `xbstream -x <  backup.xbstream`

                                                                                                                                                                                                                                                                                                                                                                                                                                              |
| Send the backup compressed directly to another host and unpack it

 | `xtrabackup --backup --compress --stream=xbstream --target-dir=./ | ssh user@otherhost “xbstream -x”`

                                                                                                                                                                                                                                                                                                                                                                         |
| Send the backup to another server using `netcat`.

                   | On the destination host:

```
$ nc -l 9999 | cat - > /data/backups/backup.xbstream
```

On the source host:

```
$ xtrabackup --backup --stream=xbstream ./ | nc desthost 9999
```

                                                                                                                                                                                                                                                                                                          |
| Send the backup to another server using a one-liner:

              | $ ssh [user@desthost](mailto:user@desthost) “( nc -l 9999 > /data/backups/backup.xbstream & )” && xtrabackup –backup –stream=xbstream ./ |  nc desthost 9999

                                                                                                                                                                                                                                                                                                                                        |
| Throttle the throughput to 10MB/sec using the `pipe viewer` tool 

  | $ xtrabackup –backup –stream=xbstream ./ | pv -q -L10m ssh [user@desthost](mailto:user@desthost) “cat - > /data/backups/backup.xbstream”

                                                                                                                                                                                                                                                                                                                                                            |
| Checksumming the backup during the streaming:

                     | On the destination host:

```
$ nc -l 9999 | tee >(sha1sum > destination_checksum) > /data/backups/backup.xbstream
```

On the source host:

```
$ xtrabackup --backup --stream=xbstream ./ | tee >(sha1sum > source_checksum) | nc desthost 9999
```

Compare the checksums on the source host:

```
$ cat source_checksum
65e4f916a49c1f216e0887ce54cf59bf3934dbad  -
```

Compare the checksums on the destination host:

```
$ cat destination_checksum
65e4f916a49c1f216e0887ce54cf59bf3934dbad  -
```

 |
| Parallel compression with parallel copying backup

                 | `xtrabackup --backup --compress --compress-threads=8 --stream=xbstream --parallel=4 --target-dir=./ > backup.xbstream`

                                                                                                                                                                                                                                                                                                                                                        |
### Footnotes

Note that the streamed backup will need to be prepared before
restoration. Streaming mode does not prepare the backup.
