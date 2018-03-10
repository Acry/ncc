/*
	Very useful program to remove root path dir.
	Root path dir is bad because filenames are very long
	and you only get two columns of display in nccnav.

	Sample Usage:
find . -name \*.nccout | xargs cat | pathremover /mnt/sources/hacks/linux-2.40.2/ > Code.map

	the path argument must end in '/' to do it right.
*/
#include <stdio.h>
#include <string.h>

int main (int argc, char **argv)
{
	char *cwd, line [10240];
	int cwdl;
	FILE *in;

	if (argc != 2) return 1;

	cwdl = strlen (cwd = argv [1]);
	in = stdin;

	while (fgets (line, 10240, in))
		if (line [0] == 'P' && !strncmp (line + 3, cwd, cwdl))
		{
			fputs ("P: ", stdout);
			fputs (line + 3 + cwdl, stdout);
		} else fputs (line, stdout);

	return 0;
}
