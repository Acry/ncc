SHELL = /bin/sh

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
srcdir		= src
MKDIR_P 	= mkdir -p
BUILDDIR 	= objdir
NCCNAVDIR	= nccnav
#$(srcdir)/

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
	cp $(BUILDDIR)/ncc $(BINDIR)/ncc
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

ncc: directories $(_NCCOBJECTS) $(srcdir)/main.cpp
	$(CXX) $(CXXFLAGS) $(srcdir)/main.cpp $(NCCOBJECTS) -o $(BUILDDIR)/ncc 

# $(OBJDIR)/%.o: %.c
# $(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

cexpand.o: $(srcdir)/cexpand.cpp
	$(CXX) -c $(CXXFLAGS) $(srcdir)/cexpand.cpp
	@mv $@ $(BUILDDIR)

parser.o: $(srcdir)/parser.cpp
	$(CXX) -c $(CXXFLAGS) $(srcdir)/parser.cpp
	@mv $@ $(BUILDDIR)

inttree.o: $(srcdir)/inttree.[cpp,h]
	$(CXX) -c $(CXXFLAGS) $(srcdir)/inttree.cpp
	@mv $@ $(BUILDDIR)

dbstree.o: $(srcdir)/dbstree.[cpp,h]
	$(CXX) -c $(CXXFLAGS) $(srcdir)/dbstree.cpp
	@mv $@ $(BUILDDIR)

lex.o: $(srcdir)/lex.cpp
	$(CXX) -c $(CXXFLAGS) $(srcdir)/lex.cpp
	@mv $@ $(BUILDDIR)

cdb.o: $(srcdir)/cdb.cpp
	$(CXX) -c $(CXXFLAGS) $(srcdir)/cdb.cpp
	@mv $@ $(BUILDDIR)

space.o: $(srcdir)/space.cpp
	$(CXX) -c $(CXXFLAGS) $(srcdir)/space.cpp
	@mv $@ $(BUILDDIR)

usage.o: $(srcdir)/usage.cpp
	$(CXX) -c $(CXXFLAGS) $(srcdir)/usage.cpp
	@mv $@ $(BUILDDIR)

ccexpr.o: $(srcdir)/ccexpr.cpp
	$(CXX) -c $(CXXFLAGS) $(srcdir)/ccexpr.cpp
	@mv $@ $(BUILDDIR)

preproc.o: $(srcdir)/preproc.cpp
	$(CXX) -c $(CXXFLAGS) $(srcdir)/preproc.cpp
	@mv $@ $(BUILDDIR)

.PHONY: wc
wc:
	wc $(srcdir)/*.[cpp,h] nccnav/*.cpp | sort -n

.PHONY: clean
clean:
	- rm -f $(NCCOBJECTS)

.PHONY: distclean

distclean:
	- rm -f $(BUILDDIR)/*
	- rmdir $(BUILDDIR)
	@cd nccnav && make clean
