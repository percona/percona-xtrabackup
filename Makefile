# Makefile to build XtraBackup for Percona Server and different versions of MySQL
#
# Syntax:
# make [5.1|xtradb|xtradb55]
#
# Default is xtradb - to build XtraBackup for Percona Server 5.1
# xtradb55 - Xtrabackup for Percona Server 5.5
# 5.1 - XtraBackup for MySQL versions 5.1.* with builtin InnoDB

LIBS = -lpthread
DEFS = -DUNIV_LINUX -DMYSQL_SERVER

CFLAGS += -O3 -g

TARGET=xtrabackup
PREFIX=/usr
BIN_DIR=$(PREFIX)/bin

default: xtradb

# XtraBackup for MySQL 5.1
5.1: INC = -I. -I.. -I./../include -I./../../include -I./../../../include
5.1: INNODBOBJS = ../libinnobase_a-btr0btr.o ../libinnobase_a-btr0cur.o ../libinnobase_a-btr0pcur.o \
	../libinnobase_a-btr0sea.o ../libinnobase_a-buf0buf.o ../libinnobase_a-buf0flu.o \
	../libinnobase_a-buf0lru.o ../libinnobase_a-buf0rea.o ../libinnobase_a-data0data.o \
	../libinnobase_a-data0type.o ../libinnobase_a-dict0boot.o ../libinnobase_a-dict0crea.o \
	../libinnobase_a-dict0dict.o ../libinnobase_a-dict0load.o ../libinnobase_a-dict0mem.o \
	../libinnobase_a-dyn0dyn.o ../libinnobase_a-eval0eval.o ../libinnobase_a-eval0proc.o \
	../libinnobase_a-fil0fil.o ../libinnobase_a-fsp0fsp.o ../libinnobase_a-fut0fut.o \
	../libinnobase_a-fut0lst.o ../libinnobase_a-ha0ha.o ../libinnobase_a-hash0hash.o \
	../libinnobase_a-ibuf0ibuf.o ../libinnobase_a-lock0iter.o ../libinnobase_a-lock0lock.o \
	../libinnobase_a-log0log.o ../libinnobase_a-log0recv.o ../libinnobase_a-mach0data.o \
	../libinnobase_a-mem0mem.o ../libinnobase_a-mem0pool.o ../libinnobase_a-mtr0log.o \
	../libinnobase_a-mtr0mtr.o ../libinnobase_a-os0file.o ../libinnobase_a-os0proc.o \
	../libinnobase_a-os0sync.o ../libinnobase_a-os0thread.o ../libinnobase_a-page0cur.o \
	../libinnobase_a-page0page.o ../libinnobase_a-lexyy.o ../libinnobase_a-pars0grm.o \
	../libinnobase_a-pars0opt.o ../libinnobase_a-pars0pars.o ../libinnobase_a-pars0sym.o \
	../libinnobase_a-que0que.o ../libinnobase_a-read0read.o ../libinnobase_a-rem0cmp.o \
	../libinnobase_a-rem0rec.o ../libinnobase_a-row0ins.o ../libinnobase_a-row0mysql.o \
	../libinnobase_a-row0purge.o ../libinnobase_a-row0row.o ../libinnobase_a-row0sel.o \
	../libinnobase_a-row0uins.o ../libinnobase_a-row0umod.o ../libinnobase_a-row0undo.o \
	../libinnobase_a-row0upd.o ../libinnobase_a-row0vers.o ../libinnobase_a-srv0que.o \
	../libinnobase_a-srv0srv.o ../libinnobase_a-srv0start.o ../libinnobase_a-sync0arr.o \
	../libinnobase_a-sync0rw.o ../libinnobase_a-sync0sync.o ../libinnobase_a-thr0loc.o \
	../libinnobase_a-trx0purge.o ../libinnobase_a-trx0rec.o ../libinnobase_a-trx0roll.o \
	../libinnobase_a-trx0rseg.o ../libinnobase_a-trx0sys.o ../libinnobase_a-trx0trx.o \
	../libinnobase_a-trx0undo.o ../libinnobase_a-usr0sess.o ../libinnobase_a-ut0byte.o \
	../libinnobase_a-ut0dbg.o ../libinnobase_a-ut0list.o ../libinnobase_a-ut0mem.o \
	../libinnobase_a-ut0rnd.o ../libinnobase_a-ut0ut.o ../libinnobase_a-ut0vec.o \
	../libinnobase_a-ut0wqueue.o
5.1: MYSQLOBJS= ../../../mysys/libmysys.a ../../../strings/libmystrings.a
5.1: TARGET := xtrabackup_51
5.1: $(TARGET)

# XtraBackup for XtraDB 
xtradb: INC=-I. -I.. -I./../include -I./../../include -I./../../../include
xtradb: INNODBOBJS = ../libinnobase_a-btr0btr.o ../libinnobase_a-btr0cur.o ../libinnobase_a-btr0pcur.o \
	../libinnobase_a-btr0sea.o ../libinnobase_a-buf0buddy.o ../libinnobase_a-buf0buf.o \
	../libinnobase_a-buf0flu.o ../libinnobase_a-buf0lru.o ../libinnobase_a-buf0rea.o \
	../libinnobase_a-data0data.o ../libinnobase_a-data0type.o ../libinnobase_a-dict0boot.o \
	../libinnobase_a-dict0crea.o ../libinnobase_a-dict0dict.o ../libinnobase_a-dict0load.o \
	../libinnobase_a-dict0mem.o ../libinnobase_a-dyn0dyn.o ../libinnobase_a-eval0eval.o \
	../libinnobase_a-eval0proc.o ../libinnobase_a-fil0fil.o ../libinnobase_a-fsp0fsp.o \
	../libinnobase_a-fut0fut.o ../libinnobase_a-fut0lst.o ../libinnobase_a-ha0ha.o \
	../libinnobase_a-ha0storage.o ../libinnobase_a-hash0hash.o ../libinnobase_a-ibuf0ibuf.o \
	../libinnobase_a-lock0iter.o ../libinnobase_a-lock0lock.o ../libinnobase_a-log0log.o \
	../libinnobase_a-log0recv.o ../libinnobase_a-mach0data.o ../libinnobase_a-mem0mem.o \
	../libinnobase_a-mem0pool.o ../libinnobase_a-mtr0log.o ../libinnobase_a-mtr0mtr.o \
	../libinnobase_a-os0file.o ../libinnobase_a-os0proc.o ../libinnobase_a-os0sync.o \
	../libinnobase_a-os0thread.o ../libinnobase_a-page0cur.o ../libinnobase_a-page0page.o \
	../libinnobase_a-page0zip.o ../libinnobase_a-lexyy.o ../libinnobase_a-pars0grm.o \
	../libinnobase_a-pars0opt.o ../libinnobase_a-pars0pars.o ../libinnobase_a-pars0sym.o \
	../libinnobase_a-que0que.o ../libinnobase_a-read0read.o ../libinnobase_a-rem0cmp.o \
	../libinnobase_a-rem0rec.o ../libinnobase_a-row0ext.o ../libinnobase_a-row0ins.o \
	../libinnobase_a-row0merge.o ../libinnobase_a-row0mysql.o ../libinnobase_a-row0purge.o \
	../libinnobase_a-row0row.o ../libinnobase_a-row0sel.o ../libinnobase_a-row0uins.o \
	../libinnobase_a-row0umod.o ../libinnobase_a-row0undo.o ../libinnobase_a-row0upd.o \
	../libinnobase_a-row0vers.o ../libinnobase_a-srv0que.o ../libinnobase_a-srv0srv.o \
	../libinnobase_a-srv0start.o ../libinnobase_a-sync0arr.o ../libinnobase_a-sync0rw.o \
	../libinnobase_a-sync0sync.o ../libinnobase_a-thr0loc.o ../libinnobase_a-trx0purge.o \
	../libinnobase_a-trx0rec.o ../libinnobase_a-trx0roll.o ../libinnobase_a-trx0rseg.o \
	../libinnobase_a-trx0sys.o ../libinnobase_a-trx0trx.o ../libinnobase_a-trx0undo.o \
	../libinnobase_a-usr0sess.o ../libinnobase_a-ut0byte.o ../libinnobase_a-ut0dbg.o \
	../libinnobase_a-ut0list.o ../libinnobase_a-ut0mem.o ../libinnobase_a-ut0rnd.o \
	../libinnobase_a-ut0ut.o ../libinnobase_a-ut0vec.o ../libinnobase_a-ut0wqueue.o \
	../libinnobase_a-ut0rbt.o
xtradb: MYSQLOBJS = ../../../mysys/libmysys.a ../../../strings/libmystrings.a ../../../zlib/.libs/libzlt.a
xtradb: DEFS += -DXTRADB_BASED 
xtradb: TARGET := xtrabackup 
xtradb: $(TARGET)

# XtraBackup for XtraDB 5.5
xtradb55: INC=-I. -I.. -I./../include -I./../../include -I./../../../include
xtradb55: INNODBOBJS = ../libinnobase.a
ifeq ($(shell uname -s),Linux)
xtradb55: LIBS += -laio
endif
xtradb55: MYSQLOBJS = ../../../mysys/libmysys.a ../../../strings/libstrings.a ../../../zlib/libzlib.a
# In CMake server builds it is important to build with exactly the same preprocessor flags
# as were used to build InnoDB
xtradb55: DEFS = $(shell grep C_DEFINES ../CMakeFiles/innobase.dir/flags.make | \
               sed -e 's/C_DEFINES = //')
xtradb55: DEFS += -DXTRADB_BASED -DXTRADB55
xtradb55: TARGET := xtrabackup_55
xtradb55: $(TARGET)


xtrabackup.o: xtrabackup.c
	$(CC) $(CFLAGS) $(INC) $(DEFS) -c $*.c

$(TARGET): xtrabackup.o $(INNODBOBJS) $(MYSQLOBJS)
	$(CC)  $(CFLAGS)  xtrabackup.o $(INNODBOBJS) $(MYSQLOBJS) $(LIBS) -o $(TARGET)

clean:
	rm -f *.o xtrabackup_* 
install:
	install -m 755 innobackupex-1.5.1 $(BIN_DIR)
	install -m 755 xtrabackup_*  $(BIN_DIR)
