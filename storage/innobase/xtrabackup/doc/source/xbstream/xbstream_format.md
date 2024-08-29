# xbstream file format
This page defines the file format layout of xbstream chunks

Chunks
------
An xbstream file is composed by two or more chunks. We always have one or
more chunks of either a payload or sparse map chunk followed by a last chunk of
type EOF to inform this is the End Of File and no more chunks for this file
will be sent.

The structure of a chunk is:

| Magic Number | Flag   | Type   | Path Length | Path              | [Sparse Map Size] | Payload size | Payload offset | Checksum | [ Sparse Map ]            | Payload            |
| ------------ | ------ |--------| ----------- | ----------------- | ----------------- | ------------ | -------------- | ---------| ------------------------- | ------------------ |
| 8 bytes      | 1 byte | 1 byte | 4 bytes     | Path Length Bytes | 4 bytes           | 8 bytes      | 8 bytes        | 4 bytes  | 8 bytes * Sparse Map Size | Payload Size Bytes |


`Magic Number`

8 Bytes - Value: **XBSTCK01**

`Flag`

1 Byte - Chunk flags. Currently Only used for the lowest bit. If bit is set a chunk of type **Unknown (\0)** can be ignored.

`Type`

1 Byte - Type of chunk. It can be 4 type:
 * **XB_CHUNK_TYPE_UNKNOWN = '\0'** - This is a unknown type of chunk.
 * **XB_CHUNK_TYPE_PAYLOAD = 'P'** - This is payload chunk. It contains plain data that can be written directly to disk. Payload chunks do not have the optional [Sparse Map Size] nor [Sparse Map].
 * **XB_CHUNK_TYPE_SPARSE = 'S'** - This is a Sparse map chunk. Those chunks have data and a mapping where data should be skipped.
 * **XB_CHUNK_TYPE_EOF = 'E'** - This is an End Of File chunk. Those chunks indicate that all the data for this particular file have been received and the file can be closed.

`Path Length`

4 Bytes - Indicates the length of file path.

`Path`

Variable Bytes - Stores the path of the file in which payload and/or sparse map should be written to

`Sparse Map Size`

4 bytes - Indicates the size of Sparse Map. Only present on **XB_CHUNK_TYPE_SPARSE**.

`Payload size`

8 bytes - Indicates the size of payload field.

`Payload offset`

8 bytes - Indicates the offset in which payload needs to be applied on the file.

`Checksum`

4 bytes - a CRC32 ISO 3309 calculation of sparse map and payload.

`Sparse map`

Variable bytes - mapping of offset and size where wholes should be inserted on files.

`Payload`

Variable bytes - Payload of `Payload size` to be written at `Payload offset` on `Path` file.


Sparse Map
------
Sparse consists in a mapping of skip and len, and work aligned with Payload. We should iterate through all the sparse map and:

1. Advance file position by `Skip` bytes
2. Write `Len` bytes from Payload
3. Advance payload by `Len`

In some systems, such as XFS, is advised to punch a hole via fallocate.

Examples
------
Here are a few examples of manually reading xbstream file.
### XB_CHUNK_TYPE_SPARSE

```
# Magic
$ xxd -b -l8 -c8  sparse.xbs
00000000: 01011000 01000010 01010011 01010100 01000011 01001011 00110000 00110001  XBSTCK01

# Flags
$ xxd -b -l1 -s 8  sparse.xbs
00000008: 00000000

# Type
$ xxd -b -l1 -s 9  sparse.xbs
00000009: 01010011                                               S

# Path length - Result 11 bytes
$ xxd -b -l4 -s 10  sparse.xbs
0000000a: 00001011 00000000 00000000 00000000                    ....

# Path
$ xxd -b -l11 -s 14  sparse.xbs
0000000e: 01110100 01100101 01110011 01110100 00101111 01110100  test/t
00000014: 00110001 00101110 01101001 01100010 01100100           1.ibd

# Sparse Map Size - Result 5
$ xxd -b -l4 -s 25  sparse.xbs
00000019: 00000101 00000000 00000000 00000000                    ....

# Payload size - Result 50991 bytes
$ xxd -b -l8 -s 29  sparse.xbs
0000001d: 00101111 11000111 00000000 00000000 00000000 00000000  /.....
00000023: 00000000 00000000                                      ..

# Payload offset - Result 0
$ xxd -b -l8 -s 37  sparse.xbs
00000025: 00000000 00000000 00000000 00000000 00000000 00000000  ......
0000002b: 00000000 00000000                                      ..

# Checksum
$ xxd -b -l4 -s 45  sparse.xbs
0000002d: 10010111 11101111 11011111 10010010                    ....

# Sparse Map
## 1 (skip 0, len 16474)
$ xxd -b -l8 -s 49  sparse.xbs
00000031: 00000000 00000000 00000000 00000000 01011010 01000000  ....Z@
00000037: 00000000 00000000                                      ..

## 2 (skip 16294, len 152)
$ xxd -b -l8 -s 57  sparse.xbs
00000039: 10100110 00111111 00000000 00000000 10011000 00000000  .?....
0000003f: 00000000 00000000                                      ..

## 3 (skip 16232, len 1450)
$ xxd -b -l8 -s 65 sparse.xbs
00000041: 01101000 00111111 00000000 00000000 10101010 00000101  h?....
00000047: 00000000 00000000

## 4 (skip 14934, len 147)
$ xxd -b -l8 -s 73 sparse.xbs
00000049: 01010110 00111010 00000000 00000000 10010011 00000000  V:....
0000004f: 00000000 00000000

## 5 (skip 16237, len 32768)
$ xxd -b -l8 -s 81 sparse.xbs
00000051: 01101101 00111111 00000000 00000000 00000000 10000000  m?....
00000057: 00000000 00000000

# Payload
xxd -b -l50991 -s 89 sparse.xbs
```

### XB_CHUNK_TYPE_PAYLOAD

```
# Magic
$ xxd -b -l8 -c8  file.xbs
00000000: 01011000 01000010 01010011 01010100 01000011 01001011 00110000 00110001  XBSTCK01

# Flags
$ xxd -b -l1 -s8  file.xbs
00000008: 00000000                                               .

# Type
$ xxd -b -l1 -s9  file.xbs
00000009: 01010000                                               P

# Path length - Result 7 bytes
$ xxd -b -l4 -s10  file.xbs
0000000a: 00000111 00000000 00000000 00000000                    ....

# Path
$ xxd -b -l7 -s14  file.xbs
0000000e: 01101001 01100010 01100100 01100001 01110100 01100001  ibdata
00000014: 00110001                                               1

# Payload size - Result 10485760 bytes
$ xxd -b -l8 -s21  file.xbs
00000015: 00000000 00000000 10100000 00000000 00000000 00000000  ......
0000001b: 00000000 00000000                                      ..

# Payload offset - Result 0
$ xxd -b -l8 -s29  file.xbs
0000001d: 00000000 00000000 00000000 00000000 00000000 00000000  ......
00000023: 00000000 00000000                                      ..

# Checksum
$ xxd -b -l4 -s37  file.xbs
00000025: 10000011 01000110 11010110 00100000                    .F.

# Payload
$ xxd -b -l10485760 -s41  file.xbs
```

### XB_CHUNK_TYPE_EOF

```
# Magic
$ xxd -b -l8 -s 12582994  file.xbs
00c00052: 01011000 01000010 01010011 01010100 01000011 01001011  XBSTCK
00c00058: 00110000 00110001                                      01

# Flags
$ xxd -b -l1 -s 12583002  file.xbs
00c0005a: 00000000                                               .

# Type
$ xxd -b -l1 -s 12583003  file.xbs
00c0005b: 01000101                                               E

# Path length - Result 7 bytes
$ xxd -b -l4 -s 12583004  file.xbs
00c0005c: 00000111 00000000 00000000 00000000                    ....

# Path
$ xxd -b -l7 -s 12583008  file.xbs
00c00060: 01101001 01100010 01100100 01100001 01110100 01100001  ibdata
00c00066: 00110001                                               1
```

