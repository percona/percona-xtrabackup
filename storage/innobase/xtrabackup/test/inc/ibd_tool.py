#!/usr/bin/env python3
"""InnoDB on-disk page helpers for the --check-tables (PXB-3804/3807) tests.

The accessors below deliberately mirror the InnoDB C API (same names, same
offsets) so a reader who knows the engine can follow them at a glance:

    fil_page_get_type(page) == FIL_PAGE_INDEX
    btr_page_get_level(page)
    btr_page_get_index_id(page)
    rec_get_next_offs(page, rec_offset)

A "page" here is just the bytes object for one page; mach_read_from_N(buf, off)
is int.from_bytes(buf[off:off+N], 'big'), exactly like InnoDB's big-endian
mach_read_from_N(ptr). Offsets/constants come from
storage/innobase/include/{fil0types.h,page0types.h,fsp0types.h,rem0rec.h}.

It is driven from inc/common.sh, but is also a usable standalone forensics
tool from the terminal. The input is always the FILE (or directory) first,
then a required command -- so you can keep one file on the line and just edit
the trailing command/args:

    # quick look at a tablespace (page size, page count, page-0 header)
    python3 ibd_tool.py t1.ibd summary

    # dump one page's FIL + index header fields
    python3 ibd_tool.py t1.ibd header 3

    # list every page (type; level/index-id/n_recs for INDEX pages)
    python3 ibd_tool.py t1.ibd scan

    # raw field access (big-endian); value may be 0x-hex on write
    python3 ibd_tool.py t1.ibd page-size
    python3 ibd_tool.py t1.ibd read  0 54 4           # FSP_SPACE_FLAGS
    python3 ibd_tool.py t1.ibd write 5 24 0x0000 2    # corrupt FIL_PAGE_TYPE

    # B-tree navigation used by the tests
    python3 ibd_tool.py t1.ibd find-index-page 0          # first leaf
    python3 ibd_tool.py t1.ibd clustered-pages            # "MAXLEVEL L0 L1 L2"
    python3 ibd_tool.py t1.ibd leftmost-node-ptr          # "ROOT OFF"
    python3 ibd_tool.py t1.ibd first-user-rec-origin 3

Commands that need a page size accept an optional trailing override; omit it
(or pass an empty string) to read it from the FSP header on page 0. An unknown
command is rejected. Run with -h/--help (or no arguments) to print the usage.
See main()/USAGE below for the full list.
"""

import os
import sys

# --- fil0types.h : file page header (the "FIL header", first 38 bytes) ------
FIL_PAGE_SPACE_OR_CHKSUM = 0   # page checksum (4 bytes)
FIL_PAGE_OFFSET = 4        # page number of this page (4 bytes)
FIL_PAGE_PREV = 8          # previous page in the index (4 bytes)
FIL_PAGE_NEXT = 12         # next page in the index (4 bytes)
FIL_PAGE_LSN = 16          # LSN of the page's latest log record (8 bytes)
FIL_PAGE_TYPE = 24         # page type (2 bytes)
FIL_PAGE_FILE_FLUSH_LSN = 26  # flush LSN (page 0) / key version (8 bytes)
FIL_PAGE_SPACE_ID = 34     # space id (4 bytes)
FIL_PAGE_DATA = 38         # start of the data / index header on the page
FIL_NULL = 0xFFFFFFFF       # "no page" sentinel for FIL_PAGE_PREV / FIL_PAGE_NEXT
# FIL trailer: last 8 bytes of the page = FIL_PAGE_END_LSN_OLD_CHKSUM
#   [old-style checksum (4)] [low 32 bits of FIL_PAGE_LSN (4)]
FIL_PAGE_END_LSN_OLD_CHKSUM_LEN = 8
FIL_PAGE_INDEX = 0x45BF    # B-tree node (clustered/secondary index)
FIL_PAGE_RTREE = 0x45BE    # R-tree (spatial) index node
FIL_PAGE_SDI = 0x45BD      # serialized-dictionary-information index node

# FIL_PAGE_TYPE values (fil0fil.h), for human-readable dumps.
PAGE_TYPE_NAMES = {
    0: "ALLOCATED", 1: "UNUSED", 2: "UNDO_LOG", 3: "INODE",
    4: "IBUF_FREE_LIST", 5: "IBUF_BITMAP", 6: "SYS", 7: "TRX_SYS",
    8: "FSP_HDR", 9: "XDES", 10: "BLOB", 11: "ZBLOB", 12: "ZBLOB2",
    13: "UNKNOWN", 14: "COMPRESSED", 15: "ENCRYPTED",
    16: "COMPRESSED_AND_ENCRYPTED", 17: "ENCRYPTED_RTREE",
    18: "SDI_BLOB", 19: "SDI_ZBLOB",
    22: "LOB_INDEX", 23: "LOB_DATA", 24: "LOB_FIRST",
    25: "ZLOB_FIRST", 26: "ZLOB_DATA", 27: "ZLOB_INDEX",
    28: "ZLOB_FRAG", 29: "ZLOB_FRAG_ENTRY",
    0x45BD: "SDI", 0x45BE: "RTREE", 0x45BF: "INDEX",
}

# --- page0types.h : index page header (starts at PAGE_HEADER) ----------------
PAGE_HEADER = FIL_PAGE_DATA
PAGE_N_DIR_SLOTS = 0       # number of page-directory slots (2 bytes)
PAGE_HEAP_TOP = 2          # offset of the heap top (2 bytes)
PAGE_N_HEAP = 4            # records in the heap; bit15 = compact format
PAGE_FREE = 6              # offset of the free record list (2 bytes)
PAGE_GARBAGE = 8           # bytes in deleted records (2 bytes)
PAGE_LAST_INSERT = 10      # offset of the last inserted record (2 bytes)
PAGE_DIRECTION = 12        # last insert direction (2 bytes)
PAGE_N_DIRECTION = 14      # consecutive inserts in the same direction (2 bytes)
PAGE_N_RECS = 16           # number of user records (2 bytes)
PAGE_MAX_TRX_ID = 18       # max trx id (8 bytes; secondary index / ibuf)
PAGE_LEVEL = 26            # B-tree level, 0 == leaf (2 bytes)
PAGE_INDEX_ID = 28         # index id this page belongs to (8 bytes)
PAGE_BTR_SEG_LEAF = 36     # leaf file-segment header (root page only)
PAGE_BTR_SEG_TOP = 46      # non-leaf file-segment header (root page only)
PAGE_NEW_INFIMUM = 99      # infimum record origin on a COMPACT page

# PAGE_DIRECTION values (page0types.h).
PAGE_DIRECTION_NAMES = {1: "LEFT", 2: "RIGHT", 3: "SAME_REC",
                        4: "SAME_PAGE", 5: "NO_DIRECTION"}

# --- fsp0types.h : a file-segment header is 10 bytes -------------------------
FSEG_HDR_SPACE = 0         # tablespace id (4 bytes)
FSEG_HDR_PAGE_NO = 4       # inode page number (4 bytes)
FSEG_HDR_OFFSET = 8        # inode offset (2 bytes)
FSEG_HEADER_SIZE = 10

# --- rem0rec.h ---------------------------------------------------------------
REC_NEXT = 2               # the 2-byte "next" field sits at rec_origin - REC_NEXT

# --- fsp0types.h : the FSP header on page 0 (page type FSP_HDR) -------------
FIL_PAGE_TYPE_FSP_HDR = 8  # FIL_PAGE_TYPE value of page 0
FSP_HEADER_OFFSET = FIL_PAGE_DATA
FSP_SPACE_ID = 0           # space id (4 bytes) -> absolute offset 38
FSP_SIZE = 8               # current size of the tablespace in pages (4 bytes)
FSP_FREE_LIMIT = 12        # first page not yet initialized (4 bytes)
FSP_SPACE_FLAGS = 16       # FSP_SPACE_FLAGS (4 bytes) -> absolute offset 54
FSP_FRAG_N_USED = 20       # pages used in the FSP_FREE_FRAG list (4 bytes)
FSP_SEG_ID = 72            # next unused segment id (8 bytes)

# Byte offset of the encryption info (MAGIC/key/iv) within page 0, keyed by the
# PHYSICAL page size. Mirrors innodb_page_header.sh; it is FSP_HEADER + the XDES
# array, which grows with the page size.
ENCRYPTION_INFO_OFFSET = {
    1024: 790, 2048: 1430, 4096: 2710, 8192: 5270,
    16384: 10390, 32768: 20630, 65536: 41110,
}
ENCRYPTION_MAGIC_LEN = 3   # "lCA"/"lCB"/"lCC" version magic
ENCRYPTION_KEY_LEN = 32
ENCRYPTION_SERVER_UUID_LEN = 36


# ---------------------------------------------------------------------------
# mach_read_from_N(buf, off): big-endian field reads, like the InnoDB C macros.
# ---------------------------------------------------------------------------
def mach_read_from_1(b, off):
    return b[off]


def mach_read_from_2(b, off):
    return int.from_bytes(b[off:off + 2], "big")


def mach_read_from_4(b, off):
    return int.from_bytes(b[off:off + 4], "big")


def mach_read_from_8(b, off):
    return int.from_bytes(b[off:off + 8], "big")


def mach_read_from_n(b, off, n):
    return int.from_bytes(b[off:off + n], "big")


# ---------------------------------------------------------------------------
# Page-header accessors, named after their InnoDB counterparts. Each takes a
# whole-page bytes object ("page").
# ---------------------------------------------------------------------------
def fil_page_get_type(page):
    return mach_read_from_2(page, FIL_PAGE_TYPE)


def fil_page_type_is_index(page_type):
    """True for the B-tree index page types -- clustered/secondary INDEX, RTREE
    (spatial) and SDI -- which all share the index page header layout. Matches
    InnoDB's fil_page_type_is_index()."""
    return page_type in (FIL_PAGE_INDEX, FIL_PAGE_RTREE, FIL_PAGE_SDI)


def fil_page_index_page_check(page):
    return fil_page_type_is_index(fil_page_get_type(page))


def page_get_page_no(page):
    return mach_read_from_4(page, FIL_PAGE_OFFSET)


def fil_page_get_prev(page):
    return mach_read_from_4(page, FIL_PAGE_PREV)


def fil_page_get_next(page):
    return mach_read_from_4(page, FIL_PAGE_NEXT)


def fil_page_get_lsn(page):
    return mach_read_from_8(page, FIL_PAGE_LSN)


def page_header_get_field(page, field):
    return mach_read_from_2(page, PAGE_HEADER + field)


def page_header_get_offs(page, field):     # PAGE_HEAP_TOP / PAGE_FREE ...
    return page_header_get_field(page, field)


def btr_page_get_level(page):
    return page_header_get_field(page, PAGE_LEVEL)


def btr_page_get_index_id(page):
    return mach_read_from_8(page, PAGE_HEADER + PAGE_INDEX_ID)


def page_get_n_recs(page):
    return page_header_get_field(page, PAGE_N_RECS)


def page_dir_get_n_slots(page):
    return page_header_get_field(page, PAGE_N_DIR_SLOTS)


def rec_get_next_offs(page, rec_offset):
    """Offset of the record after the one at rec_offset (COMPACT page).

    Like InnoDB: the 2-byte value at rec_offset - REC_NEXT is a page-relative
    delta that wraps within the page (page sizes divide 65536, so a plain
    modulo reproduces the signed-16 wrap). 0 means "no next record".
    """
    field = mach_read_from_2(page, rec_offset - REC_NEXT)
    return 0 if field == 0 else (rec_offset + field) % len(page)


def fsp_flags_get_page_size(flags):
    """Logical page size from FSP_SPACE_FLAGS (PAGE_SSIZE field)."""
    ssize = (flags >> 6) & 0xF      # after POST_ANTELOPE(1)+ZIP_SSIZE(4)+ATOMIC_BLOBS(1)
    return 16384 if ssize == 0 else (512 << ssize)


# ---------------------------------------------------------------------------
# File-level operations used by the tests.
# ---------------------------------------------------------------------------
def get_page_size(path):
    with open(path, "rb") as f:
        head = f.read(FSP_HEADER_OFFSET + FSP_SPACE_FLAGS + 4)
    return fsp_flags_get_page_size(
        mach_read_from_4(head, FSP_HEADER_OFFSET + FSP_SPACE_FLAGS))


def read_page(path, page_no, page_size):
    with open(path, "rb") as f:
        f.seek(page_no * page_size)
        return f.read(page_size)


def mach_read_field(path, page_no, offset, n, page_size):
    """Read an n-byte big-endian field at page_no:offset (mirrors mach_read_from_n)."""
    return mach_read_from_n(read_page(path, page_no, page_size), offset, n)


def mach_write_field(path, page_no, offset, value, n, page_size):
    """Write value as an n-byte big-endian field at page_no:offset (mirrors mach_write_to_n)."""
    with open(path, "r+b") as f:
        f.seek(page_no * page_size + offset)
        f.write(value.to_bytes(n, "big"))


# --- ut_crc32 / page checksum (checksum.cc) --------------------------------
# InnoDB's ut_crc32 is CRC-32C (Castagnoli), NOT zlib's CRC-32: reflected,
# poly 0x1EDC6F41 (reflected 0x82F63B78), init/xorout 0xFFFFFFFF.
# Verified: ut_crc32(b"123456789") == 0xE3069283.
_CRC32C_POLY = 0x82F63B78
_CRC32C_TABLE = []
for _n in range(256):
    _c = _n
    for _ in range(8):
        _c = (_c >> 1) ^ _CRC32C_POLY if (_c & 1) else (_c >> 1)
    _CRC32C_TABLE.append(_c)


def ut_crc32(data):
    """CRC-32C over a bytes-like object (mirrors InnoDB's ut_crc32)."""
    crc = 0xFFFFFFFF
    for b in data:
        crc = (crc >> 8) ^ _CRC32C_TABLE[(crc ^ b) & 0xFF]
    return crc ^ 0xFFFFFFFF


def buf_calc_page_crc32(page, page_size):
    """InnoDB CRC32 page checksum (checksum.cc buf_calc_page_crc32):
    c1 over [FIL_PAGE_OFFSET, FIL_PAGE_FILE_FLUSH_LSN), c2 over
    [FIL_PAGE_DATA, page_size - 8); result is c1 ^ c2."""
    c1 = ut_crc32(page[FIL_PAGE_OFFSET:FIL_PAGE_FILE_FLUSH_LSN])
    c2 = ut_crc32(page[FIL_PAGE_DATA:page_size - FIL_PAGE_END_LSN_OLD_CHKSUM_LEN])
    return (c1 ^ c2) & 0xFFFFFFFF


def update_page_checksum(path, page_no, page_size):
    """Recompute and store the CRC32 page checksum so a corrupted page still
    passes checksum validation (mirrors the colleague's update_checksum). The
    CRC32 algorithm stores the value in FIL_PAGE_SPACE_OR_CHKSUM (offset 0) and
    in the first 4 bytes of the FIL trailer (page_size - 8); the last 4 trailer
    bytes (low 32 bits of the LSN) are left unchanged."""
    page = bytearray(read_page(path, page_no, page_size))
    chk = buf_calc_page_crc32(page, page_size)
    page[FIL_PAGE_SPACE_OR_CHKSUM:FIL_PAGE_SPACE_OR_CHKSUM + 4] = chk.to_bytes(4, "big")
    trailer = page_size - FIL_PAGE_END_LSN_OLD_CHKSUM_LEN
    page[trailer:trailer + 4] = chk.to_bytes(4, "big")
    with open(path, "r+b") as f:
        f.seek(page_no * page_size)
        f.write(page)
    return chk


def fill_page_bytes(path, page_no, start, end, value, page_size):
    """Set page bytes [start, end) to value -- simulates a partial/torn page
    write that overwrites a run of bytes. Unlike a single mach_write field, this
    fills an arbitrary-length range with one repeated byte. Returns the count."""
    if not 0 <= start < end <= page_size:
        raise ValueError("range out of page bounds")
    with open(path, "r+b") as f:
        f.seek(page_no * page_size + start)
        f.write(bytes([value & 0xFF]) * (end - start))
    return end - start


def iter_index_pages(path, page_size):
    """Scan the file page by page, yielding (page_no, level, index_id) for every
    user FIL_PAGE_INDEX page (the clustered/secondary B-trees). Deliberately
    excludes SDI/RTREE so the test navigation helpers (find_index_page, etc.)
    target the user index, not the SDI tree."""
    n_pages = os.path.getsize(path) // page_size
    with open(path, "rb") as f:
        for page_no in range(n_pages):
            page = f.read(page_size)
            if fil_page_get_type(page) == FIL_PAGE_INDEX:
                yield (page_no,
                       btr_page_get_level(page),
                       btr_page_get_index_id(page))


def find_index_page(path, page_size, want_level):
    for page_no, level, _ in iter_index_pages(path, page_size):
        if level == want_level:
            return page_no
    return None


def find_clustered_pages_by_level(path, page_size):
    """(max_level, {level: page_no}) for the clustered index -- the index whose
    root has the deepest level."""
    pages = list(iter_index_pages(path, page_size))
    max_level = max(level for _, level, _ in pages)
    clustered_id = next(idx for _, level, idx in pages if level == max_level)
    at_level = {}
    for page_no, level, idx in pages:
        if idx == clustered_id:
            at_level.setdefault(level, page_no)
    return max_level, at_level


def find_leftmost_node_ptr(path, page_size):
    """(root_page_no, child_page_no_field_offset) for the leftmost node pointer
    on the clustered-index root, or None if the tree is single-level."""
    pages = list(iter_index_pages(path, page_size))
    max_level = max((level for _, level, _ in pages), default=-1)
    if max_level < 1:
        return None
    root = next(page_no for page_no, level, _ in pages if level == max_level)
    first = rec_get_next_offs(read_page(path, root, page_size), PAGE_NEW_INFIMUM)
    # clustered node pointer record = [ PK (4 bytes) ][ child page no (4 bytes) ]
    return root, first + 4


def find_first_user_rec_origin(path, page_no, page_size):
    """In-page offset of the first user record (infimum -> next), COMPACT page."""
    return rec_get_next_offs(read_page(path, page_no, page_size), PAGE_NEW_INFIMUM)


def find_leftmost_leaf(path, page_size):
    """(leaf_page_no, right_sibling_page_no) for the leftmost clustered leaf --
    the FIL_PAGE_INDEX page at level 0 whose FIL_PAGE_PREV is FIL_NULL -- and the
    page its FIL_PAGE_NEXT points to (FIL_NULL if it is the only leaf). Returns
    None if no leaf is found. The B-tree descent lands on this leaf first, so
    its right sibling is parsed (right_rec) before being validated -- the path
    the sibling-corruption tests exercise."""
    for page_no, level, _id in iter_index_pages(path, page_size):
        if level == 0:
            page = read_page(path, page_no, page_size)
            if fil_page_get_prev(page) == FIL_NULL:
                return page_no, fil_page_get_next(page)
    return None


# ---------------------------------------------------------------------------
# Human-readable views (terminal use).
# ---------------------------------------------------------------------------
def _page_type_name(t):
    return PAGE_TYPE_NAMES.get(t, "unknown")


# Table columns: field(26) bytes(11) : value(dec)(14) value(hex)(14) decoded
def _field_line(name, off, length, value, note=""):
    """A table row for a field at [off, off+length): name, byte range, decimal
    value, hex value, and any decoded note. An int value fills both the dec and
    hex columns; a str value (e.g. a composed FSEG header) goes in the dec
    column with no hex."""
    rng = "(%d-%d)" % (off, off + length)
    if value is None:                     # composite field: decode goes in note
        dec, hexv = "n/a", "n/a"
    elif isinstance(value, int):
        dec, hexv = str(value), "0x%x" % value
    else:
        dec, hexv = str(value), "n/a"
    return ("  %-26s %-11s : %-14s %-14s %s"
            % (name, rng, dec, hexv, note)).rstrip()


def _sub_line(name, value):
    """A decoded sub-field row (no byte range), aligned in the dec column."""
    return ("    %-24s %-11s : %-14s" % (name, "", value)).rstrip()


def _table(title, rows):
    """Wrap field rows as a titled table with a column header and rule."""
    head = ("  %-26s %-11s : %-14s %-14s %s"
            % ("field", "bytes", "value(dec)", "value(hex)", "decoded")).rstrip()
    return ["", title, head, "  " + "-" * 74] + rows


def _fil_header_rows(page):
    t = fil_page_get_type(page)
    return [
        _field_line("FIL_PAGE_SPACE_OR_CHKSUM", 0, 4,
                    mach_read_from_4(page, FIL_PAGE_SPACE_OR_CHKSUM)),
        _field_line("FIL_PAGE_OFFSET", FIL_PAGE_OFFSET, 4,
                    page_get_page_no(page), "page no"),
        _field_line("FIL_PAGE_PREV", FIL_PAGE_PREV, 4, fil_page_get_prev(page)),
        _field_line("FIL_PAGE_NEXT", FIL_PAGE_NEXT, 4, fil_page_get_next(page)),
        _field_line("FIL_PAGE_LSN", FIL_PAGE_LSN, 8, fil_page_get_lsn(page)),
        _field_line("FIL_PAGE_TYPE", FIL_PAGE_TYPE, 2, t, _page_type_name(t)),
        _field_line("FIL_PAGE_FILE_FLUSH_LSN", FIL_PAGE_FILE_FLUSH_LSN, 8,
                    mach_read_from_8(page, FIL_PAGE_FILE_FLUSH_LSN)),
        _field_line("FIL_PAGE_SPACE_ID", FIL_PAGE_SPACE_ID, 4,
                    mach_read_from_4(page, FIL_PAGE_SPACE_ID))]


def _index_header_rows(page):
    H = PAGE_HEADER
    n_heap = page_header_get_field(page, PAGE_N_HEAP)
    direction = page_header_get_field(page, PAGE_DIRECTION)
    return [
        _field_line("PAGE_N_DIR_SLOTS", H + PAGE_N_DIR_SLOTS, 2,
                    page_dir_get_n_slots(page)),
        _field_line("PAGE_HEAP_TOP", H + PAGE_HEAP_TOP, 2,
                    page_header_get_offs(page, PAGE_HEAP_TOP)),
        _field_line("PAGE_N_HEAP", H + PAGE_N_HEAP, 2, n_heap,
                    "%d heap recs (incl. infimum/supremum + deleted), %s" % (
                        n_heap & 0x7FFF,
                        "COMPACT" if n_heap & 0x8000 else "REDUNDANT")),
        _field_line("PAGE_FREE", H + PAGE_FREE, 2,
                    page_header_get_field(page, PAGE_FREE)),
        _field_line("PAGE_GARBAGE", H + PAGE_GARBAGE, 2,
                    page_header_get_field(page, PAGE_GARBAGE)),
        _field_line("PAGE_LAST_INSERT", H + PAGE_LAST_INSERT, 2,
                    page_header_get_field(page, PAGE_LAST_INSERT)),
        _field_line("PAGE_DIRECTION", H + PAGE_DIRECTION, 2, direction,
                    PAGE_DIRECTION_NAMES.get(direction, "unknown")),
        _field_line("PAGE_N_DIRECTION", H + PAGE_N_DIRECTION, 2,
                    page_header_get_field(page, PAGE_N_DIRECTION)),
        _field_line("PAGE_N_RECS", H + PAGE_N_RECS, 2, page_get_n_recs(page),
                    "live user records"),
        _field_line("PAGE_MAX_TRX_ID", H + PAGE_MAX_TRX_ID, 8,
                    mach_read_from_8(page, H + PAGE_MAX_TRX_ID)),
        _field_line("PAGE_LEVEL", H + PAGE_LEVEL, 2, btr_page_get_level(page)),
        _field_line("PAGE_INDEX_ID", H + PAGE_INDEX_ID, 8,
                    btr_page_get_index_id(page)),
        _field_line("PAGE_BTR_SEG_LEAF", H + PAGE_BTR_SEG_LEAF, FSEG_HEADER_SIZE,
                    None, _fseg_header(page, H + PAGE_BTR_SEG_LEAF)),
        _field_line("PAGE_BTR_SEG_TOP", H + PAGE_BTR_SEG_TOP, FSEG_HEADER_SIZE,
                    None, _fseg_header(page, H + PAGE_BTR_SEG_TOP)),
        "  (PAGE_BTR_SEG_* are meaningful only on the index root page)"]


def dump_page_header(path, page_no, page_size):
    """Dump a page as separate tables: the FIL header always; the FSP header for
    page 0; the index header for INDEX pages."""
    page = read_page(path, page_no, page_size)
    t = fil_page_get_type(page)
    out = ["page %d of %s (page_size=%d)" % (page_no, path, page_size)]
    out += _table("FIL header", _fil_header_rows(page))
    if t == FIL_PAGE_TYPE_FSP_HDR:
        out += _table("FSP header", _fsp_header_rows(page))
    if fil_page_index_page_check(page):
        out += _table("index header", _index_header_rows(page))
    return "\n".join(out)


def _fseg_header(page, off):
    """Decode a 10-byte file-segment header (space / inode page / inode offset)."""
    return "space=%d inode_page=%d inode_off=%d" % (
        mach_read_from_4(page, off + FSEG_HDR_SPACE),
        mach_read_from_4(page, off + FSEG_HDR_PAGE_NO),
        mach_read_from_2(page, off + FSEG_HDR_OFFSET))


def _fsp_header_rows(page):
    """The FSP header rows (only on page 0): size/free-limit/flags (with each
    decoded flag on its own row)/seg id."""
    F = FSP_HEADER_OFFSET
    flags = mach_read_from_4(page, F + FSP_SPACE_FLAGS)
    d = decode_fsp_flags(flags)
    rows = [_field_line("FSP_SPACE_ID", F + FSP_SPACE_ID, 4,
                        mach_read_from_4(page, F + FSP_SPACE_ID)),
            _field_line("FSP_SIZE", F + FSP_SIZE, 4,
                        mach_read_from_4(page, F + FSP_SIZE), "pages"),
            _field_line("FSP_FREE_LIMIT", F + FSP_FREE_LIMIT, 4,
                        mach_read_from_4(page, F + FSP_FREE_LIMIT)),
            _field_line("FSP_SPACE_FLAGS", F + FSP_SPACE_FLAGS, 4, flags)]
    for k in ("POST_ANTELOPE", "ZIP_SSIZE", "ATOMIC_BLOBS", "PAGE_SSIZE",
              "DATA_DIR", "SHARED", "TEMPORARY", "ENCRYPTION", "SDI",
              "PHYSICAL_PAGE_SIZE", "LOGICAL_PAGE_SIZE"):
        rows.append(_sub_line(k, d[k]))
    rows += [_field_line("FSP_FRAG_N_USED", F + FSP_FRAG_N_USED, 4,
                         mach_read_from_4(page, F + FSP_FRAG_N_USED)),
             _field_line("FSP_SEG_ID", F + FSP_SEG_ID, 8,
                         mach_read_from_8(page, F + FSP_SEG_ID))]
    return rows


def dump_trailer(path, page_no, page_size):
    """Dump the 8-byte FIL trailer (FIL_PAGE_END_LSN_OLD_CHKSUM) of a page:
    a 4-byte old-style checksum followed by the low 32 bits of FIL_PAGE_LSN
    (which should match the header LSN)."""
    page = read_page(path, page_no, page_size)
    size = len(page)
    chksum = mach_read_from_4(page, size - 8)
    lsn_low = mach_read_from_4(page, size - 4)
    hdr_lsn_low = fil_page_get_lsn(page) & 0xFFFFFFFF
    note = ("matches FIL_PAGE_LSN low32" if lsn_low == hdr_lsn_low
            else "MISMATCH (FIL_PAGE_LSN low32 = %d)" % hdr_lsn_low)
    return "\n".join([
        "page %d FIL trailer (FIL_PAGE_END_LSN_OLD_CHKSUM, last 8 bytes):" % page_no,
        _field_line("old-style checksum", size - 8, 4, chksum),
        _field_line("low32(FIL_PAGE_LSN)", size - 4, 4, lsn_low, note)])


def scan_pages(path, page_size):
    """One line per page: number, type, and (for INDEX pages) level/index-id/n_recs."""
    n_pages = os.path.getsize(path) // page_size
    rows = []
    with open(path, "rb") as f:
        for page_no in range(n_pages):
            page = f.read(page_size)
            t = fil_page_get_type(page)
            row = "%6d  %-26s" % (page_no, "%d (%s)" % (t, _page_type_name(t)))
            if fil_page_index_page_check(page):
                row += "  level=%d index_id=%d n_recs=%d" % (
                    btr_page_get_level(page), btr_page_get_index_id(page),
                    page_get_n_recs(page))
            rows.append(row)
    return "\n".join(rows)


def summary(path):
    """Page size, page count, and the page-0 header -- the bare-filename view."""
    psz = get_page_size(path)
    n_pages = os.path.getsize(path) // psz
    return "%s: page_size=%d, pages=%d\n%s" % (
        path, psz, n_pages, dump_page_header(path, 0, psz))


def read_space_id(path):
    """Space id from FSP_SPACE_ID on page 0 (FSP header)."""
    with open(path, "rb") as f:
        head = f.read(FSP_HEADER_OFFSET + FSP_SPACE_ID + 4)
    return mach_read_from_4(head, FSP_HEADER_OFFSET + FSP_SPACE_ID)


def decode_fsp_flags(flags):
    """Decode FSP_SPACE_FLAGS into its named bit-fields (fsp0types.h)."""
    f = flags
    post_antelope = f & 1;            f >>= 1
    zip_ssize = f & 0xF;              f >>= 4
    atomic_blobs = f & 1;             f >>= 1
    page_ssize = f & 0xF;             f >>= 4
    data_dir = f & 1;                 f >>= 1
    shared = f & 1;                   f >>= 1
    temporary = f & 1;                f >>= 1
    encryption = f & 1;               f >>= 1
    sdi = f & 1
    logical = 16384 if page_ssize == 0 else (512 << page_ssize)
    physical = (512 << zip_ssize) if zip_ssize else logical
    return {
        "POST_ANTELOPE": post_antelope, "ZIP_SSIZE": zip_ssize,
        "ATOMIC_BLOBS": atomic_blobs, "PAGE_SSIZE": page_ssize,
        "DATA_DIR": data_dir, "SHARED": shared, "TEMPORARY": temporary,
        "ENCRYPTION": encryption, "SDI": sdi,
        "PHYSICAL_PAGE_SIZE": physical, "LOGICAL_PAGE_SIZE": logical,
        "COMPRESSED": zip_ssize != 0,
    }


def dump_flags(path):
    """Human-readable FSP_SPACE_FLAGS breakdown for page 0 (like decode_flags)."""
    with open(path, "rb") as f:
        head = f.read(FSP_HEADER_OFFSET + FSP_SPACE_FLAGS + 4)
    flags = mach_read_from_4(head, FSP_HEADER_OFFSET + FSP_SPACE_FLAGS)
    d = decode_fsp_flags(flags)
    lines = ["FSP_SPACE_FLAGS of %s: 0x%X (%d)" % (path, flags, flags)]
    for k in ("POST_ANTELOPE", "ZIP_SSIZE", "ATOMIC_BLOBS", "PAGE_SSIZE",
              "DATA_DIR", "SHARED", "TEMPORARY", "ENCRYPTION", "SDI",
              "COMPRESSED", "PHYSICAL_PAGE_SIZE", "LOGICAL_PAGE_SIZE"):
        lines.append("  %-18s: %s" % (k, d[k]))
    return "\n".join(lines)


def dump_encryption(path):
    """Dump the page-0 encryption info (MAGIC, master key id, server uuid, key,
    iv) for an encrypted tablespace -- the decode_encryption use case."""
    with open(path, "rb") as f:
        head = f.read(FSP_HEADER_OFFSET + FSP_SPACE_FLAGS + 4)
        flags = mach_read_from_4(head, FSP_HEADER_OFFSET + FSP_SPACE_FLAGS)
        d = decode_fsp_flags(flags)
        if not d["ENCRYPTION"]:
            return "%s: tablespace is not encrypted (ENCRYPTION flag = 0)" % path
        phys = d["PHYSICAL_PAGE_SIZE"]
        off = ENCRYPTION_INFO_OFFSET.get(phys)
        if off is None:
            return "%s: cannot locate encryption info for physical page size %d" % (path, phys)
        f.seek(off)
        blob = f.read(ENCRYPTION_MAGIC_LEN + 4 + ENCRYPTION_SERVER_UUID_LEN +
                      2 * ENCRYPTION_KEY_LEN)
    p = 0
    magic = blob[p:p + ENCRYPTION_MAGIC_LEN].decode("latin1");  p += ENCRYPTION_MAGIC_LEN
    master_key_id = int.from_bytes(blob[p:p + 4], "big");       p += 4
    uuid = blob[p:p + ENCRYPTION_SERVER_UUID_LEN].rstrip(b"\0").decode("latin1")
    p += ENCRYPTION_SERVER_UUID_LEN
    key = blob[p:p + ENCRYPTION_KEY_LEN].hex();                 p += ENCRYPTION_KEY_LEN
    iv = blob[p:p + ENCRYPTION_KEY_LEN].hex()
    return ("%s: encryption info @ offset %d\n"
            "  MAGIC         : %s\n"
            "  MASTER_KEY_ID : %d\n"
            "  SERVER_UUID   : %s\n"
            "  KEY (hex)     : %s\n"
            "  IV  (hex)     : %s" %
            (path, off, magic, master_key_id, uuid, key, iv))


def dump_space_ids(directory):
    """List the space id of every tablespace file under a directory (dump_space_ids)."""
    import fnmatch
    patterns = ("*.ibd", "undo_[0-9][0-9][0-9]", "ibdata*",
                "*ibtmp*", "*.ibu", "*.new")
    found = []
    for root, _dirs, files in os.walk(directory):
        for name in files:
            if any(fnmatch.fnmatch(name, pat) for pat in patterns):
                path = os.path.join(root, name)
                try:
                    found.append((read_space_id(path), path))
                except (OSError, ValueError):
                    pass
    return "\n".join("%d  %s" % (sid, path) for sid, path in sorted(found))


# ---------------------------------------------------------------------------
# CLI dispatch. The data-only subcommands (read/write/find-*) are called from
# inc/common.sh. The input is always FILE first, then the command; commands
# that take a page size accept an optional trailing override (empty => read it
# from the FSP header on page 0).
# ---------------------------------------------------------------------------
USAGE = """ibd_tool.py -- read / scan / edit InnoDB tablespace pages.

The input is always the file (or directory) first, then a (required) command:
  python3 ibd_tool.py <file> <command> [args]

Commands:
  summary                         page size, page count, page-0 (FIL+FSP) header
  header <page_no>                dump a page's full FIL + index header (with byte ranges)
  trailer <page_no>               dump the 8-byte FIL trailer (checksum + low32 LSN)
  scan                            list every page (type; level/index-id for INDEX)
  page-size                       logical page size (from FSP_SPACE_FLAGS)
  flags                           decode FSP_SPACE_FLAGS (encryption/SDI/zip/...)
  encryption                      dump page-0 encryption info (magic/key/iv)
  space-id                        space id (FSP_SPACE_ID on page 0)
  read  <page> <off> <nbytes> [psz]          big-endian unsigned field
  write <page> <off> <value> <nbytes> [psz]  write a field (value may be 0x-hex)
  fill  <page> <start> <end> <value> [psz]   set page bytes [start,end) to value
  update-checksum <page> [psz]               recompute+store the CRC32 page checksum
  find-index-page <level> [psz]
  clustered-pages [psz]           -> "MAXLEVEL L0 L1 L2"
  leftmost-node-ptr [psz]         -> "ROOT OFF"
  leftmost-leaf [psz]             -> "LEAF NEXT" (leftmost level-0 page + its sibling)
  first-user-rec-origin <page> [psz]

List the space id of every tablespace file under a directory:
  python3 ibd_tool.py <datadir> space-ids

Examples:
  python3 ibd_tool.py t1.ibd summary
  python3 ibd_tool.py t1.ibd header 3
  python3 ibd_tool.py t1.ibd scan
  python3 ibd_tool.py t1.ibd flags
  python3 ibd_tool.py t1.ibd read  0 54 4           # FSP_SPACE_FLAGS
  python3 ibd_tool.py t1.ibd write 5 24 0x0000 2    # corrupt FIL_PAGE_TYPE
"""

_SUBCOMMANDS = {
    "summary", "page-size", "read", "write", "header", "trailer", "scan",
    "flags", "encryption", "space-id", "space-ids",
    "find-index-page", "clustered-pages", "leftmost-node-ptr", "leftmost-leaf",
    "first-user-rec-origin", "fill", "update-checksum",
}


def _opt(rest, i):
    """Optional positional arg: rest[i] if present, else "" (so [psz] can be omitted)."""
    return rest[i] if i < len(rest) else ""


def _int(s):
    """Parse an integer argument, accepting decimal or 0x/0o/0b-prefixed input
    (so '54' and '0x36' are equivalent). Raises ValueError on junk, which the
    caller turns into that command's usage message."""
    return int(s, 0)


def _psz(path, arg):
    return int(arg) if arg else get_page_size(path)


# Per-command argument syntax, for a focused error when a required arg is
# missing (rather than dumping the whole usage).
COMMAND_USAGE = {
    "header":                "<file> header <page_no>",
    "trailer":               "<file> trailer <page_no>",
    "read":                  "<file> read <page> <off> <nbytes> [psz]",
    "write":                 "<file> write <page> <off> <value> <nbytes> [psz]",
    "find-index-page":       "<file> find-index-page <level> [psz]",
    "first-user-rec-origin": "<file> first-user-rec-origin <page_no> [psz]",
    "fill":                  "<file> fill <page> <start> <end> <value> [psz]",
    "update-checksum":       "<file> update-checksum <page> [psz]",
}


def _require(cmd, rest, n):
    """Exit with a command-specific usage line if fewer than n args were given."""
    if len(rest) < n:
        sys.exit("ibd_tool.py: %s: missing argument(s)\n"
                 "  usage: python3 ibd_tool.py %s" % (cmd, COMMAND_USAGE[cmd]))


def main(argv):
    # No args or an explicit help request: print usage.
    if len(argv) < 2 or argv[1] in ("-h", "--help", "help"):
        print(USAGE)
        return

    # The input is ALWAYS the file (or directory) first, then the command:
    #     ibd_tool.py <file> <command> [args]
    path = argv[1]
    if not os.path.exists(path):
        print(USAGE)
        sys.exit("ibd_tool.py: no such file or directory: %r" % path)

    # A command is always required (no implicit default).
    if len(argv) == 2:
        print(USAGE)
        sys.exit("ibd_tool.py: missing command after %r "
                 "(e.g. summary, header, scan, flags, space-ids; -h for all)" % path)

    cmd = argv[2]
    rest = argv[3:]
    if cmd not in _SUBCOMMANDS:
        print(USAGE)
        sys.exit("ibd_tool.py: invalid command %r (run 'ibd_tool.py -h' for usage)" % cmd)

    try:
        _dispatch(cmd, path, rest)
    except ValueError:
        sys.exit("ibd_tool.py: %s: invalid argument\n"
                 "  usage: python3 ibd_tool.py %s"
                 % (cmd, COMMAND_USAGE.get(cmd, "<file> " + cmd)))


def _dispatch(cmd, path, rest):
    if cmd == "summary":
        print(summary(path))

    elif cmd == "page-size":
        print(get_page_size(path))

    elif cmd == "header":
        _require("header", rest, 1)
        print(dump_page_header(path, _int(rest[0]), _psz(path, _opt(rest, 1))))

    elif cmd == "trailer":
        _require("trailer", rest, 1)
        print(dump_trailer(path, _int(rest[0]), _psz(path, _opt(rest, 1))))

    elif cmd == "scan":
        print(scan_pages(path, _psz(path, _opt(rest, 0))))

    elif cmd == "flags":
        print(dump_flags(path))

    elif cmd == "encryption":
        print(dump_encryption(path))

    elif cmd == "space-id":
        print(read_space_id(path))

    elif cmd == "space-ids":
        print(dump_space_ids(path))      # here "path" is a directory

    elif cmd == "read":
        _require("read", rest, 3)
        page_no, offset, n = _int(rest[0]), _int(rest[1]), _int(rest[2])
        if n < 1:
            sys.exit("ibd_tool.py: read: <nbytes> must be >= 1")
        print(mach_read_field(path, page_no, offset, n, _psz(path, _opt(rest, 3))))

    elif cmd == "write":
        _require("write", rest, 4)
        page_no, offset = _int(rest[0]), _int(rest[1])
        value, n = _int(rest[2]), _int(rest[3])      # decimal or 0x-hex
        if n < 1:
            sys.exit("ibd_tool.py: write: <nbytes> must be >= 1")
        if not 0 <= value < (1 << (8 * n)):
            sys.exit("ibd_tool.py: write: value %#x does not fit in %d byte(s) "
                     "(allowed 0..%#x)" % (value, n, (1 << (8 * n)) - 1))
        mach_write_field(path, page_no, offset, value, n, _psz(path, _opt(rest, 4)))

    elif cmd == "find-index-page":
        _require("find-index-page", rest, 1)
        page_no = find_index_page(path, _psz(path, _opt(rest, 1)), _int(rest[0]))
        if page_no is not None:
            print(page_no)

    elif cmd == "clustered-pages":
        max_level, at = find_clustered_pages_by_level(path, _psz(path, _opt(rest, 0)))
        print("%d %s %s %s" % (max_level, at.get(0, "NONE"),
                               at.get(1, "NONE"), at.get(2, "NONE")))

    elif cmd == "leftmost-node-ptr":
        res = find_leftmost_node_ptr(path, _psz(path, _opt(rest, 0)))
        print("ERR not enough levels" if res is None else "%d %d" % res)

    elif cmd == "leftmost-leaf":
        res = find_leftmost_leaf(path, _psz(path, _opt(rest, 0)))
        print("NONE NONE" if res is None else "%d %d" % res)

    elif cmd == "first-user-rec-origin":
        _require("first-user-rec-origin", rest, 1)
        print(find_first_user_rec_origin(path, _int(rest[0]), _psz(path, _opt(rest, 1))))

    elif cmd == "fill":
        _require("fill", rest, 4)
        page_no, start, end = _int(rest[0]), _int(rest[1]), _int(rest[2])
        value = _int(rest[3])
        n = fill_page_bytes(path, page_no, start, end, value,
                            _psz(path, _opt(rest, 4)))
        print(n)

    elif cmd == "update-checksum":
        _require("update-checksum", rest, 1)
        chk = update_page_checksum(path, _int(rest[0]), _psz(path, _opt(rest, 1)))
        print("0x%08x" % chk)


if __name__ == "__main__":
    main(sys.argv)
