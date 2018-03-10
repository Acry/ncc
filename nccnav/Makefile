#C-Compiler
CC 		= gcc

CFLAGS 		=
CPPFLAGS 	=

CXX 		= g++
CXXFLAGS 	= -ansi

OBJDIR = ../objdir

$(OBJDIR)/nccnav: nccnav.cpp $(OBJDIR)/dbstree.o $(OBJDIR)/inttree.o
	$(CXX) $(CXXFLAGS) nccnav.cpp -I.. -lncurses -o nccnav $(OBJDIR)/dbstree.o $(OBJDIR)/inttree.o

$(OBJDIR)/dbstree.o: ../dbstree.[Ch]
	$(CXX) $(CXXFLAGS) ../dbstree.cpp -c
	@mv dbstree.o $(OBJDIR)

$(OBJDIR)/inttree.o: ../inttree.[Ch]
	$(CXX) $(CXXFLAGS) ../inttree.cpp -c
	@mv inttree.o $(OBJDIR)

clean:
	rm -f *.o nccnav
