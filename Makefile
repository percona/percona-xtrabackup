LIBS=	-lpthread
DEFS=	-DUNIV_LINUX -DMYSQL_SERVER

#x86 Linux
CFLAGS+=	-O2 -g -fmessage-length=0 -D_FORTIFY_SOURCE=2

#Mac OS 64 bit
#CFLAGS=	-O2 -arch x86_64 -g -fmessage-length=0 -D_FORTIFY_SOURCE=2

#LD=ld64

#MySQL 5.0
#INC=	-I. -I.. -I./../include -I./../../include
#INNODBOBJS=	../usr/libusr.a ../srv/libsrv.a ../dict/libdict.a ../que/libque.a\
		../srv/libsrv.a ../ibuf/libibuf.a ../row/librow.a ../pars/libpars.a\
		../btr/libbtr.a ../trx/libtrx.a ../read/libread.a ../usr/libusr.a\
		../buf/libbuf.a ../ibuf/libibuf.a ../eval/libeval.a ../log/liblog.a\
		../fsp/libfsp.a ../fut/libfut.a ../fil/libfil.a ../lock/liblock.a\
		../mtr/libmtr.a ../page/libpage.a ../rem/librem.a ../thr/libthr.a\
		../sync/libsync.a ../data/libdata.a ../mach/libmach.a ../ha/libha.a\
		../dyn/libdyn.a ../mem/libmem.a ../sync/libsync.a ../ut/libut.a\
		../os/libos.a ../ut/libut.a
#MYSQLOBJS=	../../mysys/libmysys.a ../../strings/libmystrings.a

#MySQL 5.1
#INC=	-I. -I.. -I./../include -I./../../include -I./../../../include
#INNODBOBJS=	../ha_innodb_la-btr0btr.o ../ha_innodb_la-btr0cur.o ../ha_innodb_la-btr0pcur.o ../ha_innodb_la-btr0sea.o ../ha_innodb_la-buf0buf.o ../ha_innodb_la-buf0flu.o ../ha_innodb_la-buf0lru.o ../ha_innodb_la-buf0rea.o ../ha_innodb_la-data0data.o ../ha_innodb_la-data0type.o ../ha_innodb_la-dict0boot.o ../ha_innodb_la-dict0crea.o ../ha_innodb_la-dict0dict.o ../ha_innodb_la-dict0load.o ../ha_innodb_la-dict0mem.o ../ha_innodb_la-dyn0dyn.o ../ha_innodb_la-eval0eval.o ../ha_innodb_la-eval0proc.o ../ha_innodb_la-fil0fil.o ../ha_innodb_la-fsp0fsp.o ../ha_innodb_la-fut0fut.o ../ha_innodb_la-fut0lst.o ../ha_innodb_la-ha0ha.o ../ha_innodb_la-hash0hash.o ../ha_innodb_la-ibuf0ibuf.o ../ha_innodb_la-lock0iter.o ../ha_innodb_la-lock0lock.o ../ha_innodb_la-log0log.o ../ha_innodb_la-log0recv.o ../ha_innodb_la-mach0data.o ../ha_innodb_la-mem0mem.o ../ha_innodb_la-mem0pool.o ../ha_innodb_la-mtr0log.o ../ha_innodb_la-mtr0mtr.o ../ha_innodb_la-os0file.o ../ha_innodb_la-os0proc.o ../ha_innodb_la-os0sync.o ../ha_innodb_la-os0thread.o ../ha_innodb_la-page0cur.o ../ha_innodb_la-page0page.o ../ha_innodb_la-lexyy.o ../ha_innodb_la-pars0grm.o ../ha_innodb_la-pars0opt.o ../ha_innodb_la-pars0pars.o ../ha_innodb_la-pars0sym.o ../ha_innodb_la-que0que.o ../ha_innodb_la-read0read.o ../ha_innodb_la-rem0cmp.o ../ha_innodb_la-rem0rec.o ../ha_innodb_la-row0ins.o ../ha_innodb_la-row0mysql.o ../ha_innodb_la-row0purge.o ../ha_innodb_la-row0row.o ../ha_innodb_la-row0sel.o ../ha_innodb_la-row0uins.o ../ha_innodb_la-row0umod.o ../ha_innodb_la-row0undo.o ../ha_innodb_la-row0upd.o ../ha_innodb_la-row0vers.o ../ha_innodb_la-srv0que.o ../ha_innodb_la-srv0srv.o ../ha_innodb_la-srv0start.o ../ha_innodb_la-sync0arr.o ../ha_innodb_la-sync0rw.o ../ha_innodb_la-sync0sync.o ../ha_innodb_la-thr0loc.o ../ha_innodb_la-trx0purge.o ../ha_innodb_la-trx0rec.o ../ha_innodb_la-trx0roll.o ../ha_innodb_la-trx0rseg.o ../ha_innodb_la-trx0sys.o ../ha_innodb_la-trx0trx.o ../ha_innodb_la-trx0undo.o ../ha_innodb_la-usr0sess.o ../ha_innodb_la-ut0byte.o ../ha_innodb_la-ut0dbg.o ../ha_innodb_la-ut0list.o ../ha_innodb_la-ut0mem.o ../ha_innodb_la-ut0rnd.o ../ha_innodb_la-ut0ut.o ../ha_innodb_la-ut0vec.o ../ha_innodb_la-ut0wqueue.o
#INNODBOBJS=	../libinnobase_a-btr0btr.o ../libinnobase_a-btr0cur.o ../libinnobase_a-btr0pcur.o ../libinnobase_a-btr0sea.o ../libinnobase_a-buf0buf.o ../libinnobase_a-buf0flu.o ../libinnobase_a-buf0lru.o ../libinnobase_a-buf0rea.o ../libinnobase_a-data0data.o ../libinnobase_a-data0type.o ../libinnobase_a-dict0boot.o ../libinnobase_a-dict0crea.o ../libinnobase_a-dict0dict.o ../libinnobase_a-dict0load.o ../libinnobase_a-dict0mem.o ../libinnobase_a-dyn0dyn.o ../libinnobase_a-eval0eval.o ../libinnobase_a-eval0proc.o ../libinnobase_a-fil0fil.o ../libinnobase_a-fsp0fsp.o ../libinnobase_a-fut0fut.o ../libinnobase_a-fut0lst.o ../libinnobase_a-ha0ha.o ../libinnobase_a-hash0hash.o ../libinnobase_a-ibuf0ibuf.o ../libinnobase_a-lock0iter.o ../libinnobase_a-lock0lock.o ../libinnobase_a-log0log.o ../libinnobase_a-log0recv.o ../libinnobase_a-mach0data.o ../libinnobase_a-mem0mem.o ../libinnobase_a-mem0pool.o ../libinnobase_a-mtr0log.o ../libinnobase_a-mtr0mtr.o ../libinnobase_a-os0file.o ../libinnobase_a-os0proc.o ../libinnobase_a-os0sync.o ../libinnobase_a-os0thread.o ../libinnobase_a-page0cur.o ../libinnobase_a-page0page.o ../libinnobase_a-lexyy.o ../libinnobase_a-pars0grm.o ../libinnobase_a-pars0opt.o ../libinnobase_a-pars0pars.o ../libinnobase_a-pars0sym.o ../libinnobase_a-que0que.o ../libinnobase_a-read0read.o ../libinnobase_a-rem0cmp.o ../libinnobase_a-rem0rec.o ../libinnobase_a-row0ins.o ../libinnobase_a-row0mysql.o ../libinnobase_a-row0purge.o ../libinnobase_a-row0row.o ../libinnobase_a-row0sel.o ../libinnobase_a-row0uins.o ../libinnobase_a-row0umod.o ../libinnobase_a-row0undo.o ../libinnobase_a-row0upd.o ../libinnobase_a-row0vers.o ../libinnobase_a-srv0que.o ../libinnobase_a-srv0srv.o ../libinnobase_a-srv0start.o ../libinnobase_a-sync0arr.o ../libinnobase_a-sync0rw.o ../libinnobase_a-sync0sync.o ../libinnobase_a-thr0loc.o ../libinnobase_a-trx0purge.o ../libinnobase_a-trx0rec.o ../libinnobase_a-trx0roll.o ../libinnobase_a-trx0rseg.o ../libinnobase_a-trx0sys.o ../libinnobase_a-trx0trx.o ../libinnobase_a-trx0undo.o ../libinnobase_a-usr0sess.o ../libinnobase_a-ut0byte.o ../libinnobase_a-ut0dbg.o ../libinnobase_a-ut0list.o ../libinnobase_a-ut0mem.o ../libinnobase_a-ut0rnd.o ../libinnobase_a-ut0ut.o ../libinnobase_a-ut0vec.o ../libinnobase_a-ut0wqueue.o
#MYSQLOBJS=	../../../mysys/libmysys.a ../../../strings/libmystrings.a

#MySQL Plugin
INC=	-I. -I.. -I./../include -I./../../include -I./../../../include
INNODBOBJS=	../ha_innodb_la-btr0btr.o ../ha_innodb_la-btr0cur.o ../ha_innodb_la-btr0pcur.o ../ha_innodb_la-btr0sea.o ../ha_innodb_la-buf0buddy.o ../ha_innodb_la-buf0buf.o ../ha_innodb_la-buf0flu.o ../ha_innodb_la-buf0lru.o ../ha_innodb_la-buf0rea.o ../ha_innodb_la-data0data.o ../ha_innodb_la-data0type.o ../ha_innodb_la-dict0boot.o ../ha_innodb_la-dict0crea.o ../ha_innodb_la-dict0dict.o ../ha_innodb_la-dict0load.o ../ha_innodb_la-dict0mem.o ../ha_innodb_la-dyn0dyn.o ../ha_innodb_la-eval0eval.o ../ha_innodb_la-eval0proc.o ../ha_innodb_la-fil0fil.o ../ha_innodb_la-fsp0fsp.o ../ha_innodb_la-fut0fut.o ../ha_innodb_la-fut0lst.o ../ha_innodb_la-ha0ha.o ../ha_innodb_la-ha0storage.o ../ha_innodb_la-hash0hash.o ../ha_innodb_la-ibuf0ibuf.o ../ha_innodb_la-lock0iter.o ../ha_innodb_la-lock0lock.o ../ha_innodb_la-log0log.o ../ha_innodb_la-log0recv.o ../ha_innodb_la-mach0data.o ../ha_innodb_la-mem0mem.o ../ha_innodb_la-mem0pool.o ../ha_innodb_la-mtr0log.o ../ha_innodb_la-mtr0mtr.o ../ha_innodb_la-os0file.o ../ha_innodb_la-os0proc.o ../ha_innodb_la-os0sync.o ../ha_innodb_la-os0thread.o ../ha_innodb_la-page0cur.o ../ha_innodb_la-page0page.o ../ha_innodb_la-page0zip.o ../ha_innodb_la-lexyy.o ../ha_innodb_la-pars0grm.o ../ha_innodb_la-pars0opt.o ../ha_innodb_la-pars0pars.o ../ha_innodb_la-pars0sym.o ../ha_innodb_la-que0que.o ../ha_innodb_la-read0read.o ../ha_innodb_la-rem0cmp.o ../ha_innodb_la-rem0rec.o ../ha_innodb_la-row0ext.o ../ha_innodb_la-row0ins.o ../ha_innodb_la-row0merge.o ../ha_innodb_la-row0mysql.o ../ha_innodb_la-row0purge.o ../ha_innodb_la-row0row.o ../ha_innodb_la-row0sel.o ../ha_innodb_la-row0uins.o ../ha_innodb_la-row0umod.o ../ha_innodb_la-row0undo.o ../ha_innodb_la-row0upd.o ../ha_innodb_la-row0vers.o ../ha_innodb_la-srv0que.o ../ha_innodb_la-srv0srv.o ../ha_innodb_la-srv0start.o ../ha_innodb_la-sync0arr.o ../ha_innodb_la-sync0rw.o ../ha_innodb_la-sync0sync.o ../ha_innodb_la-thr0loc.o ../ha_innodb_la-trx0purge.o ../ha_innodb_la-trx0rec.o ../ha_innodb_la-trx0roll.o ../ha_innodb_la-trx0rseg.o ../ha_innodb_la-trx0sys.o ../ha_innodb_la-trx0trx.o ../ha_innodb_la-trx0undo.o ../ha_innodb_la-usr0sess.o ../ha_innodb_la-ut0byte.o ../ha_innodb_la-ut0dbg.o ../ha_innodb_la-ut0list.o ../ha_innodb_la-ut0mem.o ../ha_innodb_la-ut0rnd.o ../ha_innodb_la-ut0ut.o ../ha_innodb_la-ut0vec.o ../ha_innodb_la-ut0wqueue.o
INNODBOBJS=	../libinnobase_a-btr0btr.o ../libinnobase_a-btr0cur.o ../libinnobase_a-btr0pcur.o ../libinnobase_a-btr0sea.o ../libinnobase_a-buf0buddy.o ../libinnobase_a-buf0buf.o ../libinnobase_a-buf0flu.o ../libinnobase_a-buf0lru.o ../libinnobase_a-buf0rea.o ../libinnobase_a-data0data.o ../libinnobase_a-data0type.o ../libinnobase_a-dict0boot.o ../libinnobase_a-dict0crea.o ../libinnobase_a-dict0dict.o ../libinnobase_a-dict0load.o ../libinnobase_a-dict0mem.o ../libinnobase_a-dyn0dyn.o ../libinnobase_a-eval0eval.o ../libinnobase_a-eval0proc.o ../libinnobase_a-fil0fil.o ../libinnobase_a-fsp0fsp.o ../libinnobase_a-fut0fut.o ../libinnobase_a-fut0lst.o ../libinnobase_a-ha0ha.o ../libinnobase_a-ha0storage.o ../libinnobase_a-hash0hash.o ../libinnobase_a-ibuf0ibuf.o ../libinnobase_a-lock0iter.o ../libinnobase_a-lock0lock.o ../libinnobase_a-log0log.o ../libinnobase_a-log0recv.o ../libinnobase_a-mach0data.o ../libinnobase_a-mem0mem.o ../libinnobase_a-mem0pool.o ../libinnobase_a-mtr0log.o ../libinnobase_a-mtr0mtr.o ../libinnobase_a-os0file.o ../libinnobase_a-os0proc.o ../libinnobase_a-os0sync.o ../libinnobase_a-os0thread.o ../libinnobase_a-page0cur.o ../libinnobase_a-page0page.o ../libinnobase_a-page0zip.o ../libinnobase_a-lexyy.o ../libinnobase_a-pars0grm.o ../libinnobase_a-pars0opt.o ../libinnobase_a-pars0pars.o ../libinnobase_a-pars0sym.o ../libinnobase_a-que0que.o ../libinnobase_a-read0read.o ../libinnobase_a-rem0cmp.o ../libinnobase_a-rem0rec.o ../libinnobase_a-row0ext.o ../libinnobase_a-row0ins.o ../libinnobase_a-row0merge.o ../libinnobase_a-row0mysql.o ../libinnobase_a-row0purge.o ../libinnobase_a-row0row.o ../libinnobase_a-row0sel.o ../libinnobase_a-row0uins.o ../libinnobase_a-row0umod.o ../libinnobase_a-row0undo.o ../libinnobase_a-row0upd.o ../libinnobase_a-row0vers.o ../libinnobase_a-srv0que.o ../libinnobase_a-srv0srv.o ../libinnobase_a-srv0start.o ../libinnobase_a-sync0arr.o ../libinnobase_a-sync0rw.o ../libinnobase_a-sync0sync.o ../libinnobase_a-thr0loc.o ../libinnobase_a-trx0purge.o ../libinnobase_a-trx0rec.o ../libinnobase_a-trx0roll.o ../libinnobase_a-trx0rseg.o ../libinnobase_a-trx0sys.o ../libinnobase_a-trx0trx.o ../libinnobase_a-trx0undo.o ../libinnobase_a-usr0sess.o ../libinnobase_a-ut0byte.o ../libinnobase_a-ut0dbg.o ../libinnobase_a-ut0list.o ../libinnobase_a-ut0mem.o ../libinnobase_a-ut0rnd.o ../libinnobase_a-ut0ut.o ../libinnobase_a-ut0vec.o ../libinnobase_a-ut0wqueue.o
MYSQLOBJS=	../../../mysys/libmysys.a ../../../strings/libmystrings.a ../../../zlib/.libs/libzlt.a




.SUFFIXES: .o .c

.c.o:
	$(CC) $(CFLAGS) $(INC) $(DEFS) -c $*.c

all: xtrabackup

xtrabackup : xtrabackup.o $(INNODBOBJS) $(MYSQLOBJS)
	$(CC)  $(CFLAGS)  xtrabackup.o $(INNODBOBJS) $(MYSQLOBJS) $(LIBS) -o xtrabackup

clean:
	rm -f *.o xtrabackup

install:
	cp innobackupex-1.5.1 /usr/bin
	cp xtrabackup /usr/bin
