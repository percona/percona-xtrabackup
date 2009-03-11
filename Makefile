LIBS=	-lpthread
DEFS=	-DUNIV_LINUX -DMYSQL_SERVER

#x86 Linux
CFLAGS=	-O2 -g -fmessage-length=0 -D_FORTIFY_SOURCE=2

#Mac OS 64 bit
#CFLAGS=	-O2 -arch x86_64 -g -fmessage-length=0 -D_FORTIFY_SOURCE=2

#LD=ld64

#MySQL 5.0
INC=	-I. -I.. -I./../include -I./../../include
INNODBOBJS=	../usr/libusr.a ../srv/libsrv.a ../dict/libdict.a ../que/libque.a\
		../srv/libsrv.a ../ibuf/libibuf.a ../row/librow.a ../pars/libpars.a\
		../btr/libbtr.a ../trx/libtrx.a ../read/libread.a ../usr/libusr.a\
		../buf/libbuf.a ../ibuf/libibuf.a ../eval/libeval.a ../log/liblog.a\
		../fsp/libfsp.a ../fut/libfut.a ../fil/libfil.a ../lock/liblock.a\
		../mtr/libmtr.a ../page/libpage.a ../rem/librem.a ../thr/libthr.a\
		../sync/libsync.a ../data/libdata.a ../mach/libmach.a ../ha/libha.a\
		../dyn/libdyn.a ../mem/libmem.a ../sync/libsync.a ../ut/libut.a\
		../os/libos.a ../ut/libut.a
MYSQLOBJS=	../../mysys/libmysys.a ../../strings/libmystrings.a

#MySQL 5.1
#INC=	-I. -I.. -I./../include -I./../../include -I./../../../include
#INNODBOBJS=	../libinnobase_a-btr0btr.o ../libinnobase_a-btr0cur.o ../libinnobase_a-btr0pcur.o ../libinnobase_a-btr0sea.o ../libinnobase_a-buf0buf.o ../libinnobase_a-buf0flu.o ../libinnobase_a-buf0lru.o ../libinnobase_a-buf0rea.o ../libinnobase_a-data0data.o ../libinnobase_a-data0type.o ../libinnobase_a-dict0boot.o ../libinnobase_a-dict0crea.o ../libinnobase_a-dict0dict.o ../libinnobase_a-dict0load.o ../libinnobase_a-dict0mem.o ../libinnobase_a-dyn0dyn.o ../libinnobase_a-eval0eval.o ../libinnobase_a-eval0proc.o ../libinnobase_a-fil0fil.o ../libinnobase_a-fsp0fsp.o ../libinnobase_a-fut0fut.o ../libinnobase_a-fut0lst.o ../libinnobase_a-ha0ha.o ../libinnobase_a-hash0hash.o ../libinnobase_a-ibuf0ibuf.o ../libinnobase_a-lock0iter.o ../libinnobase_a-lock0lock.o ../libinnobase_a-log0log.o ../libinnobase_a-log0recv.o ../libinnobase_a-mach0data.o ../libinnobase_a-mem0mem.o ../libinnobase_a-mem0pool.o ../libinnobase_a-mtr0log.o ../libinnobase_a-mtr0mtr.o ../libinnobase_a-os0file.o ../libinnobase_a-os0proc.o ../libinnobase_a-os0sync.o ../libinnobase_a-os0thread.o ../libinnobase_a-page0cur.o ../libinnobase_a-page0page.o ../libinnobase_a-lexyy.o ../libinnobase_a-pars0grm.o ../libinnobase_a-pars0opt.o ../libinnobase_a-pars0pars.o ../libinnobase_a-pars0sym.o ../libinnobase_a-que0que.o ../libinnobase_a-read0read.o ../libinnobase_a-rem0cmp.o ../libinnobase_a-rem0rec.o ../libinnobase_a-row0ins.o ../libinnobase_a-row0mysql.o ../libinnobase_a-row0purge.o ../libinnobase_a-row0row.o ../libinnobase_a-row0sel.o ../libinnobase_a-row0uins.o ../libinnobase_a-row0umod.o ../libinnobase_a-row0undo.o ../libinnobase_a-row0upd.o ../libinnobase_a-row0vers.o ../libinnobase_a-srv0que.o ../libinnobase_a-srv0srv.o ../libinnobase_a-srv0start.o ../libinnobase_a-sync0arr.o ../libinnobase_a-sync0rw.o ../libinnobase_a-sync0sync.o ../libinnobase_a-thr0loc.o ../libinnobase_a-trx0purge.o ../libinnobase_a-trx0rec.o ../libinnobase_a-trx0roll.o ../libinnobase_a-trx0rseg.o ../libinnobase_a-trx0sys.o ../libinnobase_a-trx0trx.o ../libinnobase_a-trx0undo.o ../libinnobase_a-usr0sess.o ../libinnobase_a-ut0byte.o ../libinnobase_a-ut0dbg.o ../libinnobase_a-ut0list.o ../libinnobase_a-ut0mem.o ../libinnobase_a-ut0rnd.o ../libinnobase_a-ut0ut.o ../libinnobase_a-ut0vec.o ../libinnobase_a-ut0wqueue.o
#MYSQLOBJS=	../../../mysys/libmysys.a ../../../strings/libmystrings.a


.SUFFIXES: .o .c

.c.o:
	$(CC) $(CFLAGS) $(INC) $(DEFS) -c $*.c

all: xtrabackup

xtrabackup : xtrabackup.o $(INNODBOBJS) $(MYSQLOBJS)
	$(CC)  $(CFLAGS)  xtrabackup.o $(INNODBOBJS) $(MYSQLOBJS) $(LIBS) -o xtrabackup

clean:
	rm -f *.o xtrabackup

