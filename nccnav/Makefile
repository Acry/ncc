SHELL = /bin/sh

#C-Compiler
CC 		= gcc
CFLAGS 		=
CPPFLAGS 	=
#C++-Compiler and flags
CXX 		= g++
CXXFLAGS 	= -ansi -Wno-write-strings
#FIXME: -ansi is ancient - rewrite to compile and silent warnings

#BUILD VARS
srcdir		= ../src
MKDIR_P 	= mkdir -p
BUILDDIR 	= ../objdir

.PHONY: directories
all: directories nccnav

directories: $(BUILDDIR)

$(BUILDDIR):
	$(MKDIR_P) $(BUILDDIR)

nccnav: nccnav.cpp $(BUILDDIR)/dbstree.o $(BUILDDIR)/inttree.o
	$(CXX) $(CXXFLAGS) nccnav.cpp -I.. -lncurses -o nccnav $(BUILDDIR)/dbstree.o $(BUILDDIR)/inttree.o

$(BUILDDIR)/dbstree.o: $(srcdir)/dbstree.[cpp,h]
	$(CXX) $(CXXFLAGS) $(srcdir)/dbstree.cpp -c
	@mv dbstree.o $(BUILDDIR)

$(BUILDDIR)/inttree.o: $(srcdir)/inttree.[cpp,h]
	$(CXX) $(CXXFLAGS) $(srcdir)/inttree.cpp -c
	@mv inttree.o $(BUILDDIR)

.PHONY: clean
clean:
	rm -f nccnav
