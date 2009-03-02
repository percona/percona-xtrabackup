LIBS=	-lpthread -lrt
DEFS=	-DUNIV_LINUX -DMYSQL_SERVER

#x86_32
CFLAGS=	-O2 -g -fmessage-length=0 -D_FORTIFY_SOURCE=2

#5.0
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

.SUFFIXES: .o .c

.c.o:
	$(CC) $(CFLAGS) $(INC) $(DEFS) -c $*.c

all: xtrabackup

xtrabackup : xtrabackup.o $(INNODBOBJS) $(MYSQLOBJS)
	$(CC) xtrabackup.o $(INNODBOBJS) $(MYSQLOBJS) $(LIBS) -o xtrabackup

clean:
	rm -f *.o xtrabackup

