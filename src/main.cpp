#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>

#include "global.h"


extern bool quiet;
class ncci * ncc;

//
int*		CODE;
int		C_Ntok;
char**		C_Syms;
int		C_Nsyms;
char**		C_Strings;
int		C_Nstrings;
cfile_i*	C_Files;
int		C_Nfiles;
clines_i*	C_Lines;
int		C_Nlines;

double*		C_Floats;
signed char*	C_Chars;
short int*	C_Shortints;
long int*	C_Ints;
unsigned long*	C_Unsigned;
// *********** --------- ***********


struct __builtins__ ccbuiltins;

int syntax_error (int i, char *c)
{
	if (c) fprintf (stderr, "ncc-error :  %s \n", c);
	debug ("syntax error:", i - 25, 50);
	exit (1);
}

int syntax_error (char *a1, char *a2)
{
	fprintf (stderr, "ncc-error: %s %s\n", a1, a2);
	exit (1);
}

int syntax_error (int i, char *p, char *t)
{
	fprintf (stderr, "ncc-error %s --> [%s]\n", p, t);
	debug ("syntax error:", i - 25, 50);
	exit (1);
}

static	int nhe = 0;
void half_error (char *m1, char *m2)
{
#define STDDBG stderr
	if (m2) fprintf (STDDBG, "%s %s\n", m1, m2);
	else fprintf (STDDBG, "%s\n", m1);
	debug ("expression ncc-error:", ExpressionPtr - 15, 30);
	if (halt_on_error) { *(int*)0=0; }
	if (no_error)
		report_error ();
	else if (nhe++ > 20)
		syntax_error ("Maximum number of errors", "aborted");
	throw EXPR_ERROR ();
}

void warning (char *m1, char m2)
{
	if (m2) fprintf (stderr, "warning:%s %c\n", m1, m2);
	else fprintf (stderr, "warning:%s\n", m1);
}

void yylex_open (char *file)
{
	fprintf (stderr, "Opening preprocessed file %s\n", file);
	load_file C (file);
	if (C.success != ZC_OK) {
		fprintf (stderr, "Problems opening %s\n", file);
		exit (1);
	}
	yynorm (C.data, C.len);
}

extern void showdb ();

static void set_cwd ()
{
	char tmp [512];
	if (!getcwd (tmp, 512)) strcpy (tmp, "/TOO_BIG_PATH");
	cwd = StrDup (strcat (tmp, "/"));
}

int main (int argc, char **argv)
{
	set_cwd ();

	// parse options and invoke preprocessor
	preproc (argc, argv);

	// initialize lexical analyser
	prepare ();

	// lexical analysis
	yylex_open (preprocfile);

	// unlink NCC.i if not -ncspp
	if (preprocfile == PREPROCESSOR_OUTPUT)
		unlink (preprocfile);

	// sum up into the big normalized array of tokens
	make_norm ();

	// strings are in the symbol table, so look for ncc keys
	ncc_keys ();

	// initialize syntactical analyser database
	init_cdb ();

	// syntactical analysis
	parse_C ();

	// object file useless ?
	if (nhe && !no_error) syntax_error ("Compilation errors", "in expressions");

	// final stuff
	ncc->finir ();

	// file-as-function calling functions defined in this file
	if (usage_only) functions_of_file ();

	// report structure declaration locations
	if (usage_only && report_structs) structs_of_file ();

	// print out what we learned from all this
	showdb ();
	fprintf (stderr,
		quiet ? "%i Tokens, %i symbols, %i expressions\n":
		 "%i Tokens\n%i symbols\n%i expressions\n",
		 C_Ntok, C_Nsyms, last_result);

}

char *StrDup (char *c)
{
	return strcpy (new char [strlen (c) + 1], c);
}

char *StrDup (char *c, int i)
{
	char *d = new char [i + 1];
	d [i] = 0;
	return strncpy (d, c, i);
}

#ifdef EFENCE
// ###### Electric Fence ######
// These activate efence on our C++ allocations.
// #############################################

void *operator new (size_t s)
{
	return malloc (s);
}

void operator delete (void *p)
{
	free (p);
}

void *operator new[] (size_t s)
{
	return malloc (s);
}

void operator delete[] (void *p)
{
	free (p);
}

#endif

#ifdef _POSIX_MAPPED_FILES

load_file::load_file (char *f)
{
	data = NULL;
	success = ZC_NA;
	fd = -1;

	struct stat statbuf;
	if (stat (f, &statbuf) == -1) return;
	len = statbuf.st_size;
	if (len == -1 || (fd = open (f, O_RDONLY)) == -1) return;

	success = ((data = (char*) mmap (0, len, PROT_READ, MAP_PRIVATE, fd, 0))
		 != MAP_FAILED)	? ZC_OK : ZC_FF;
}

load_file::~load_file ()
{
	if (data && data != MAP_FAILED) munmap (data, len);
	if (fd != -1) close (fd);
}

#else

load_file::load_file (char *f)
{
	data = NULL;
	success = ZC_NA;
	fd = -1;

	struct stat statbuf;
	if (stat (f, &statbuf) == -1) return;
	len = statbuf.st_size;
	if (len == -1 || (fd = open (f, O_RDONLY)) == -1) return;

	data = (char*) malloc (len);
	success = (len = read (fd, data, len)) != -1 ? ZC_OK : ZC_FF;
}

load_file::~load_file ()
{
	if (data) free (data);
	if (fd != -1) close (fd);
}

#endif
//***********************************************************************
//		definitions
//***********************************************************************

void intcpycat (int *d, const int *s1, const int *s2)
{
	while ((*d++ = *s1++) != -1);
	d -= 1;
	while ((*d++ = *s2++) != -1);
}

int *intdup (int *i)
{
	int *r = new int [intlen (i) + 1];
	intcpy (r, i);
	return r;
}

int intcmp (int *x, int *y)
{
	while (*x == *y && *x != -1) x++, y++;
	return (*x < *y) ? -1 : (*x == *y) ? 0 : 1;
}

void intncpy (int *d, int *s, int l)
{
	while (l--) *d++ = *s++;
}
