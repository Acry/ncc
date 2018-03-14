CC=ncc
NCCFLAGS= -ncgcc -ncld -ncfabs
all: temp fptr

temp: temp.c
	$(CC) $(NCCFLAGS) $< -o $@

fptr: fptr.c
	$(CC) $(NCCFLAGS) $< -o $@
