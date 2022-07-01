# The xbcrypt binary

To support encryption and decryption of the backups, a new tool `xbcrypt` was
introduced to *Percona XtraBackup*.

This utility has been modeled after [The xbstream binary](../xbstream/xbstream.md#xbstream-binary) to perform
encryption and decryption outside of *Percona XtraBackup*. `xbcrypt` has
following command line options:

### -d, --decrypt
Decrypt data input to output.

### -i, --input=name
Optional input file. If not specified, input will be read from standard
input.

### -o, --output=name
Optional output file. If not specified, output will be written to standard
output.

### -a, --encrypt-algo=name
Encryption algorithm.

### -k, --encrypt-key=name
Encryption key.

### -f, --encrypt-key-file=name
File which contains encryption key.

### -s, --encrypt-chunk-size=#
Size of working buffer for encryption in bytes. The default value is 64K.

### --encrypt-threads=#
This option specifies the number of worker threads that will be used for
parallel encryption/decryption.

### -v, --verbose
Display verbose status output.
