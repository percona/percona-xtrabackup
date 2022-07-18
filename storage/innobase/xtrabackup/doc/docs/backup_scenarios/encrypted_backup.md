# Encrypted Backup

*Percona XtraBackup* has implemented support for encrypted backups. It can be
used to encrypt/decrypt local or streaming backup with xbstream option
(streaming tar backups are not supported) in order to add another layer of
protection to the backups. Encryption is done with the `libgcrypt` library.

## Creating Encrypted Backups

To make an encrypted backup following options need to be specified (options
`xtrabackup --encrypt-key` and `xtrabackup --encrypt-key-file`
are mutually exclusive, i.e., just one of them needs to be provided):

* `--encrypt=ALGORITHM` - currently supported algorithms are: `AES128`, `AES192` and `AES256`

* `--encrypt-key=ENCRYPTION_KEY` - proper length encryption key to use. It is not recommended to use this option where there is uncontrolled access to the machine as the command line and thus the key can be viewed as part of the process info.

* `--encrypt-key-file=KEYFILE` - the name of a file where the raw key of the appropriate length can be read from. The file must be a simple binary (or text) file that contains exactly the key to be used.

Both `xtrabackup --encrypt-key` option  and
`xtrabackup --encrypt-key-file` option can be used to specify the
encryption key. Encryption key can be generated with command like:

```bash
$ openssl rand -base64 24
```

Example output of that command should look like this:

```default
GCHFLrDFVx6UAsRb88uLVbAVWbK+Yzfs
```

This value then can be used as the encryption key

### Using the `--encrypt-key` option

Example of the xtrabackup command using the `xtrabackup --encrypt-key`
should look like this:

```bash
$ xtrabackup --backup --target-dir=/data/backups --encrypt=AES256 \
--encrypt-key="GCHFLrDFVx6UAsRb88uLVbAVWbK+Yzfs"
```

### Using the `--encrypt-key-file` option

Example of the xtrabackup command using the
`xtrabackup --encrypt-key-file` should look like this:

```bash
$ xtrabackup --backup --target-dir=/data/backups/ --encrypt=AES256 \
--encrypt-key-file=/data/backups/keyfile
```

!!! note

    Depending on the text editor used for making the `KEYFILE`, text file in some cases can contain the CRLF and this will cause the key size to grow and thus making it invalid. Suggested way to do this would be to create the file with: `echo -n "GCHFLrDFVx6UAsRb88uLVbAVWbK+Yzfs" > /data/backups/keyfile`

## Optimizing the encryption process

Two options have been introduced with the encrypted backups that can be used to
speed up the encryption process. These are
`xtrabackup --encrypt-threads` and
`xtrabackup --encrypt-chunk-size`. By using the
`xtrabackup --encrypt-threads` option
multiple threads can be specified to be used for encryption in parallel. Option `xtrabackup --encrypt-chunk-size` can be used to specify the size (in
bytes) of the working encryption buffer for each encryption thread (default is
64K).

## Decrypting Encrypted Backups

*Percona XtraBackup* `xtrabackup --decrypt` option has been implemented
that can be used to decrypt the backups:

```bash
$ xtrabackup --decrypt=AES256 --encrypt-key="GCHFLrDFVx6UAsRb88uLVbAVWbK+Yzfs"\
--target-dir=/data/backups/
```

*Percona XtraBackup* doesn’t automatically remove the encrypted files. In order to clean up the backup directory users should remove the \*.xbcrypt
files. In *Percona XtraBackup* 2.4.6 `xtrabackup --remove-original`
option has been implemented that you can use to remove the encrypted files once they’ve been decrypted. To remove the files once they’re decrypted you should run:

```bash
$ xtrabackup --decrypt=AES256 --encrypt-key="GCHFLrDFVx6UAsRb88uLVbAVWbK+Yzfs"\
--target-dir=/data/backups/ --remove-original
```

!!! note

    `xtrabackup --parallel` can be used with `xtrabackup --decrypt` option to decrypt multiple files simultaneously.

When the files have been decrypted backup can be prepared.

## Preparing Encrypted Backups

After the backups have been decrypted, they can be prepared the same way as the standard full backups with the `xtrabackup --prepare` option:

```bash
$ xtrabackup --prepare --target-dir=/data/backups/
```

## Restoring Encrypted Backups

xtrabackup has a `xtrabackup --copy-back` option, which performs the
restoration of a backup to the server’s datadir:

```bash
$ xtrabackup --copy-back --target-dir=/data/backups/
```

It will copy all the data-related files back to the server’s `datadir`,
determined by the server’s `my.cnf` configuration file. You should check
the last line of the output for a success message:

```default
170214 12:37:01 completed OK!
```

## Other Reading


* [The Libgcrypt Reference Manual](http://www.gnupg.org/documentation/manuals/gcrypt/)
