#C-Compiler
CC 		= gcc

CFLAGS 		=
CPPFLAGS 	=

CXX 		= g++
CXXFLAGS 	= 

DESTDIR ?= /usr
LCFLAGS = -g -O2
BINDIR = ${DESTDIR}/bin
MANDIR = ${DESTDIR}/share/man
INCLUDEDIR = ${DESTDIR}/include
NOGNU = /usr/include/nognu

#CFLAGS = $(LCFLAGS) -c


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

nccnav/nccnav: nccnav/nccnav.cpp
	@echo Compiling nccnav viewer.
	@cd nccnav && make

objdir/ncc: objdir/dbstree.o objdir/inttree.o objdir/lex.o objdir/space.o objdir/cexpand.o objdir/cdb.o objdir/parser.o objdir/ccexpr.o objdir/preproc.o objdir/usage.o main.cpp
	$(CXX) $(CXXFLAGS) main.cpp objdir/*.o -o objdir/ncc 

objdir/cexpand.o: cexpand.cpp
	$(CXX) -c $(CXXFLAGS) cexpand.cpp
	@mv cexpand.o objdir/

objdir/parser.o: parser.cpp
	$(CXX) -c $(CXXFLAGS) parser.cpp
	@mv parser.o objdir/

objdir/inttree.o: inttree.[cpp,h]
	$(CXX) -c $(CXXFLAGS) inttree.cpp
	@mv inttree.o objdir/

objdir/dbstree.o: dbstree.[cpp,h]
	$(CXX) -c $(CXXFLAGS) dbstree.cpp
	@mv dbstree.o objdir/

objdir/lex.o: lex.cpp
	$(CXX) -c $(CXXFLAGS) lex.cpp
	@mv lex.o objdir/

objdir/cdb.o: cdb.cpp
	$(CXX) -c $(CXXFLAGS) cdb.cpp
	@mv cdb.o objdir/

objdir/space.o: space.cpp
	$(CXX) -c $(CXXFLAGS) space.cpp
	@mv space.o objdir/

objdir/usage.o: usage.cpp
	$(CXX) -c $(CXXFLAGS) usage.cpp
	@mv usage.o objdir/

objdir/ccexpr.o: ccexpr.cpp
	$(CXX) -c $(CXXFLAGS) ccexpr.cpp
	@mv ccexpr.o objdir/

objdir/preproc.o: preproc.cpp
	$(CXX) -c $(CXXFLAGS) preproc.cpp
	@mv preproc.o objdir/

wc:
	wc *.[cpp,h] nccnav/*.cpp | sort -n

clean:
	rm -f objdir/*.o

distclean:
	rm -f objdir/* objdir/ncc
	@cd nccnav && make clean
