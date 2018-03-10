
CC = g++
LCFLAGS = -Wall -g
CFLAGS = $(LCFLAGS) -c -g

OBJDIR = ../objdir

$(OBJDIR)/nccnav: nccnav.C $(OBJDIR)/dbstree.o $(OBJDIR)/inttree.o
	$(CC) -g nccnav.C -I.. -lncurses -o nccnav $(OBJDIR)/dbstree.o $(OBJDIR)/inttree.o

$(OBJDIR)/dbstree.o: ../dbstree.[Ch]
	$(CC) $(CFLAGS) ../dbstree.C -c
	@mv dbstree.o $(OBJDIR)

$(OBJDIR)/inttree.o: ../inttree.[Ch]
	$(CC) $(CFLAGS) ../inttree.C -c
	@mv inttree.o $(OBJDIR)

clean:
	rm -f *.o nccnav
