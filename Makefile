
# these are set by config
CC = g++
DESTDIR ?= /usr
LCFLAGS = -g -O2
BINDIR = ${DESTDIR}/bin
MANDIR = ${DESTDIR}/share/man
INCLUDEDIR = ${DESTDIR}/include
NOGNU = /usr/include/nognu

#

CFLAGS = $(LCFLAGS) -c


tout: objdir/ncc nccnav/nccnav
	@echo Salut.

install: tout
	cp objdir/ncc $(BINDIR)/ncc
	cp scripts/nccstrip2.py $(BINDIR)/nccstrip2.py
	ln -sf $(BINDIR)/ncc $(BINDIR)/nccar
	ln -sf $(BINDIR)/ncc $(BINDIR)/nccld
	ln -sf $(BINDIR)/ncc $(BINDIR)/nccc++
	ln -sf $(BINDIR)/ncc $(BINDIR)/nccg++
	cp nccnav/nccnav $(BINDIR)/nccnav
	ln -fs $(BINDIR)/nccnav $(BINDIR)/nccnavi
	cp ncc.1 $(MANDIR)/man1
	cp nccnav/nccnav.1 $(MANDIR)/man1
	cp doc/nognu $(INCLUDEDIR)

uninstall:
	rm -f $(BINDIR)/ncc $(BINDIR)/nccnav $(BINDIR)/nccnavi $(MANDIR)/man1/ncc.1 $(INCLUDEDIR)/nognu
	rm -f $(BINDIR)/nccar $(BINDIR)/nccld $(BINDIR)/nccc++ $(BINDIR)/nccg++

nccnav/nccnav: nccnav/nccnav.C
	@echo Compiling nccnav viewer.
	@cd nccnav && make

objdir/ncc: objdir/dbstree.o objdir/inttree.o objdir/lex.o objdir/space.o objdir/cexpand.o objdir/cdb.o objdir/parser.o objdir/ccexpr.o objdir/preproc.o objdir/usage.o main.C
	$(CC) $(LCFLAGS) main.C objdir/*.o -o objdir/ncc 

objdir/cexpand.o: cexpand.C
	$(CC) $(CFLAGS) cexpand.C
	@mv cexpand.o objdir/

objdir/parser.o: parser.C
	$(CC) $(CFLAGS) parser.C
	@mv parser.o objdir/

objdir/inttree.o: inttree.[Ch]
	$(CC) $(CFLAGS) inttree.C
	@mv inttree.o objdir/

objdir/dbstree.o: dbstree.[Ch]
	$(CC) $(CFLAGS) dbstree.C
	@mv dbstree.o objdir/

objdir/lex.o: lex.C
	$(CC) $(CFLAGS) lex.C
	@mv lex.o objdir/

objdir/cdb.o: cdb.C
	$(CC) $(CFLAGS) cdb.C
	@mv cdb.o objdir/

objdir/space.o: space.C
	$(CC) $(CFLAGS) space.C
	@mv space.o objdir/

objdir/usage.o: usage.C
	$(CC) $(CFLAGS) usage.C
	@mv usage.o objdir/

objdir/ccexpr.o: ccexpr.C
	$(CC) $(CFLAGS) ccexpr.C
	@mv ccexpr.o objdir/

objdir/preproc.o: preproc.C
	$(CC) $(CFLAGS) preproc.C
	@mv preproc.o objdir/

wc:
	wc *.[Ch] nccnav/*.C | sort -n

clean:
	rm -f objdir/*.o

distclean:
	rm -f objdir/* objdir/ncc
	@cd nccnav && make clean
