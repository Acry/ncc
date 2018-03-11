#C-Compiler and flags
CC 		= gcc
CFLAGS 		=
CPPFLAGS 	=
#C++-Compiler and flags
CXX 		= g++
CXXFLAGS 	= -Wno-write-strings -Wno-literal-suffix
#LCFLAGS = -g -O2
#CFLAGS = $(LCFLAGS) -c

#BUILD VARS
MKDIR_P 	= mkdir -p
BUILDDIR 	= objdir
NCCNAVDIR	= nccnav
#INSTALL VARS
DESTDIR        ?= /usr
BINDIR 		= $(DESTDIR)/bin
MANDIR 		= $(DESTDIR)/share/man
INCLUDEDIR	= $(DESTDIR)/include
NOGNU		= /usr/include/nognu

#ncc Objects
_NCCOBJECTS=dbstree.o inttree.o lex.o space.o cexpand.o cdb.o parser.o ccexpr.o preproc.o usage.o
#ncc Objects with path
NCCOBJECTS=$(patsubst %,$(BUILDDIR)/%,$(_NCCOBJECTS))

.PHONY: directories
all: directories ncc nccnav

directories: $(BUILDDIR)

$(BUILDDIR):
	$(MKDIR_P) $(BUILDDIR)

install: all
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

.PHONY: nccnav
nccnav: $(NCCNAVDIR)/nccnav.cpp
	@echo Compiling nccnav viewer.
	@cd $(NCCNAVDIR) && make

ncc: directories $(_NCCOBJECTS) main.cpp
	$(CXX) $(CXXFLAGS) main.cpp $(NCCOBJECTS) -o $(BUILDDIR)/ncc 

# $(OBJDIR)/%.o: %.c
# $(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

cexpand.o: cexpand.cpp
	$(CXX) -c $(CXXFLAGS) cexpand.cpp
	@mv $@ $(BUILDDIR)

parser.o: parser.cpp
	$(CXX) -c $(CXXFLAGS) parser.cpp
	@mv $@ $(BUILDDIR)

inttree.o: inttree.[cpp,h]
	$(CXX) -c $(CXXFLAGS) inttree.cpp
	@mv $@ $(BUILDDIR)

dbstree.o: dbstree.[cpp,h]
	$(CXX) -c $(CXXFLAGS) dbstree.cpp
	@mv $@ $(BUILDDIR)

lex.o: lex.cpp
	$(CXX) -c $(CXXFLAGS) lex.cpp
	@mv $@ $(BUILDDIR)

cdb.o: cdb.cpp
	$(CXX) -c $(CXXFLAGS) cdb.cpp
	@mv $@ $(BUILDDIR)

space.o: space.cpp
	$(CXX) -c $(CXXFLAGS) space.cpp
	@mv $@ $(BUILDDIR)

usage.o: usage.cpp
	$(CXX) -c $(CXXFLAGS) usage.cpp
	@mv $@ $(BUILDDIR)

ccexpr.o: ccexpr.cpp
	$(CXX) -c $(CXXFLAGS) ccexpr.cpp
	@mv $@ $(BUILDDIR)

preproc.o: preproc.cpp
	$(CXX) -c $(CXXFLAGS) preproc.cpp
	@mv $@ $(BUILDDIR)

.PHONY: wc
wc:
	wc *.[cpp,h] nccnav/*.cpp | sort -n

.PHONY: clean
clean:
	- rm -f $(NCCOBJECTS)

.PHONY: distclean

distclean:
	- rm -f $(BUILDDIR)/*
	- rmdir $(BUILDDIR)
	@cd nccnav && make clean
