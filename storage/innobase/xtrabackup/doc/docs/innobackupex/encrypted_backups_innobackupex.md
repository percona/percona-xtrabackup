# Encrypted Backups

*Percona XtraBackup* has implemented support for encrypted backups. It can be
used to encrypt/decrypt local or streaming backup with xbstream option
(streaming tar backups are not supported) in order to add another layer of
protection to the backups. Encryption is done with the `libgcrypt` library.

## Creating Encrypted Backups

To make an encrypted backup following options need to be specified (options
`innobackupex --encrypt-key` and `innobackupex --encrypt-key-file` are mutually exclusive, i.e. just one of them needs to be provided):

* innobackupex –encrypt

* innobackupex –encrypt-key

* innobackupex –encrypt-key-file

Both `innobackupex --encrypt-key` option and `innobackupex --encrypt-key-file` option can be used to specify the encryption key. Encryption
key can be generated with a command like:

```default
$ openssl rand -base64 24
```

Example output of that command should look like this:

```default
GCHFLrDFVx6UAsRb88uLVbAVWbK+Yzfs
```

This value then can be used as the encryption key

### Using `innobackupex --encrypt-key`

Example of the innobackupex command using the innobackupex
–encrypt-key should look like this

```default
$ innobackupex --encrypt=AES256 --encrypt-key="GCHFLrDFVx6UAsRb88uLVbAVWbK+Yzfs" /data/backups
```

### Using `innobackupex --encrypt-key-file`

Example of the innobackupex command using the `innobackupex --encrypt-key-file` should look like this:

```default
$ innobackupex --encrypt=AES256 --encrypt-key-file=/data/backups/keyfile /data/backups
```

!!! note

    Depending on the text editor used for making the `KEYFILE`, text file in some cases can contain the CRLF and this will cause the key size to grow and thus making it invalid. Suggested way to do this would be to create the file with: `echo -n "GCHFLrDFVx6UAsRb88uLVbAVWbK+Yzfs" > /data/backups/keyfile`

Both of these examples will create a timestamped directory in `/data/backups` containing the encrypted backup.

!!! note

    You can use the innobackupex –no-timestamp option to override this behavior and the backup will be created in the given directory.

## Optimizing the encryption process

Two new options have been introduced with the encrypted backups that can be used
to speed up the encryption process. These are `innobackupex --encrypt-threads` and `innobackupex --encrypt-chunk-size`. By using the
`innobackupex --encrypt-threads` option multiple threads can be
specified to be used for encryption in parallel. Option `innobackupex --encrypt-chunk-size` can be used to specify the size (in bytes) of the working
encryption buffer for each encryption thread (default is 64K).

## Decrypting Encrypted Backups

Backups can be decrypted with [The xbcrypt binary](../xbcrypt/xbcrypt.md#xbcrypt). The following one-liner can be
used to encrypt the whole folder:

```default
$ for i in `find . -iname "*\.xbcrypt"`; do xbcrypt -d --encrypt-key-file=/root/secret_key --encrypt-algo=AES256 < $i > $(dirname $i)/$(basename $i .xbcrypt) && rm $i; done
```

*Percona XtraBackup* `innobackupex --decrypt` option has been
implemented that can be used to decrypt the backups:

```default
$ innobackupex --decrypt=AES256 --encrypt-key="GCHFLrDFVx6UAsRb88uLVbAVWbK+Yzfs" /data/backups/2015-03-18_08-31-35/
```

*Percona XtraBackup* doesn’t automatically remove the encrypted files. In order
to clean up the backup directory users should remove the `\*.xbcrypt`
files.

!!! note

    `innobackupex --parallel` can be used with `innobackupex --decrypt` option to decrypt multiple files simultaneously.

When the files have been decrypted backup can be prepared.

## Preparing Encrypted Backups

After the backups have been decrypted, they can be prepared the same way as the
standard full backups with the `innobackupex --apply-log` option:

```default
$ innobackupex --apply-log /data/backups/2015-03-18_08-31-35/
```

!!! note

    *Percona XtraBackup* doesn’t automatically remove the encrypted files. In order to clean up the backup directory users should remove the `\*.xbcrypt` files.

## Restoring Encrypted Backups

innobackupex has a `innobackupex --copy-back` option, which performs the restoration of a backup to the server’s `datadir`

```default
$ innobackupex --copy-back /path/to/BACKUP-DIR
```

It will copy all the data-related files back to the server’s `datadir`,
determined by the server’s `my.cnf` configuration file. You should check
the last line of the output for a success message:

```default
innobackupex: Finished copying back files.
150318 11:08:13  innobackupex: completed OK!
```

## Other Reading

* [The Libgcrypt Reference Manual](http://www.gnupg.org/documentation/manuals/gcrypt/)
