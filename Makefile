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
CP 	= cp -n
 #set if DESTDIR is not defined
#DESTDIR        ?= /usr
DESTDIR		?= $(HOME)/.local
BINDIR 		= $(DESTDIR)/bin
MANDIR 		= $(DESTDIR)/share/man/man1
INCLUDEDIR	= $(DESTDIR)/include
NOGNU		= /usr/include/nognu

#ncc Objects
_NCCOBJECTS=dbstree.o inttree.o lex.o space.o cexpand.o cdb.o parser.o ccexpr.o preproc.o usage.o
#ncc Objects with path
NCCOBJECTS=$(patsubst %,$(BUILDDIR)/%,$(_NCCOBJECTS))

.PHONY: built_directories

all: built_directories ncc nccnav

built_directories: $(BUILDDIR)

$(BUILDDIR):
	$(MKDIR_P) $(BUILDDIR)

.PHONY: install_directories
install_directories: $(BINDIR) $(MANDIR) $(INCLUDEDIR)

$(BINDIR):
	$(MKDIR_P) $(BINDIR)

$(MANDIR):
	$(MKDIR_P) $(MANDIR)

$(INCLUDEDIR):
	$(MKDIR_P) $(INCLUDEDIR)

install: all install_directories install-nccnav
	$(CP) $(BUILDDIR)/ncc $(BINDIR)/ncc
	$(CP) $(srcdir)/ncc.1 $(MANDIR)
	mandb -f $(MANDIR)/ncc.1
	$(CP) scripts/nccstrip $(BINDIR)/nccstrip
	$(CP) scripts/nccgraph $(BINDIR)/nccgraph
	$(CP) scripts/nccgraph.1 $(MANDIR)/nccgraph.1
	mandb -f $(MANDIR)/nccgraph.1
	ln -sf $(BINDIR)/ncc $(BINDIR)/nccar
	ln -sf $(BINDIR)/ncc $(BINDIR)/nccld
	ln -sf $(BINDIR)/ncc $(BINDIR)/nccc++
	ln -sf $(BINDIR)/ncc $(BINDIR)/nccg++
	$(CP) doc/nognu $(INCLUDEDIR)

install-nccnav:
	$(CP) nccnav/nccnav $(BINDIR)/nccnav
	ln -fs $(BINDIR)/nccnav $(BINDIR)/nccnavi
	$(CP) nccnav/nccnav.1 $(MANDIR)/nccnav.1
	mandb -f $(MANDIR)/nccnav.1

.PHONY: uninstall
uninstall: uninstall-nccnav
	rm -f $(BINDIR)/ncc $(MANDIR)/ncc.1 $(INCLUDEDIR)/nognu
	rm -f $(BINDIR)/nccar $(BINDIR)/nccld $(BINDIR)/nccc++ $(BINDIR)/nccg++
	rm -f $(BINDIR)/nccstrip $(BINDIR)/nccgraph $(MANDIR)/nccgraph.1
	mandb

.PHONY: uninstall-nccnav
uninstall-nccnav:
	rm -f $(BINDIR)/nccnav
	rm -f $(BINDIR)/nccnavi
	rm -f $(MANDIR)/nccnav.1
	mandb

.PHONY: nccnav
nccnav: $(NCCNAVDIR)/nccnav.cpp
	@echo Compiling nccnav viewer.
	@cd $(NCCNAVDIR) && make

ncc: $(_NCCOBJECTS) $(srcdir)/main.cpp
	$(CXX) $(CXXFLAGS) $(srcdir)/main.cpp $(NCCOBJECTS) -o $(BUILDDIR)/ncc 

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

.PHONY: help
help :
	@echo ""
	@echo "make 		- all ncc and nccnav"
	@echo "make ncc	- builds ncc only"
	@echo "make nccnav	- builds nccnav only"
	@echo "make clean	- removes *.o files"
	@echo "make distclean	- removes anything built"
	@echo "make wc		- wordcount on sources"
