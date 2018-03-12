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
#include <sys/wait.h>
#include <signal.h>
#include "global.h"

bool		usage_only, multiple, include_strings, quiet = false;
bool		abs_paths = false, report_structs = true, pseudo_code = false;
bool		halt_on_error = false, no_error = false;
char		*sourcefile, *preprocfile = PREPROCESSOR_OUTPUT, *cwd;
static char	**ncc_key;

static bool issource (char *f)
{
	char *e = f + strlen (f) - 1;
	if (*(e-1) == '.')
		return *e == 'c' || *e == 'h' || *e == 'C' || *e == 'i';
	return *(e-3) == '.' && (*(e-2) == 'c' || *(e-2) == 'C');
}

static bool isobj (char *f)
{
	int i = strlen (f);
	return (f [i-1] == 'o' || f [i-1] == 'a') && f [i-2] == '.';
}

static char *objfile (char *f)
{
	char *r = strdup (f);
	char *p = rindex (r, '.');
	if (p) strcpy (p, ".o");
	return r;
}

FILE *report_stream;

static void openout (char *f, char *mode = "w")
{
	if (!f) return;
	char *c = (char*) alloca (strlen (f) + sizeof OUTPUT_EXT);
	report_stream = fopen (strcat (strcpy (c, f), OUTPUT_EXT), mode);
	fprintf (stderr, "ncc: output file is ["COLS"%s"COLE"]\n", c);
}

static void stripout (char *f)
{
	if (report_stream != stdout) {
		fclose (report_stream);
		char tmp [1024];
		sprintf (tmp, "nccstrip2.py %s"OUTPUT_EXT" %s"OUTPUT_EXT, f, f);
		system (tmp);
	}
	report_stream = stdout;
}

static void CATFILE (char *data, int len, FILE *out)
{
	/* writing out a huge mmaped file with fwrite takes
	   a lot of time for some reason.  */
#define	MAXWRITE 16 * 1024

	if (len <= MAXWRITE) {
		fwrite (data, 1, len, out);
		return;
	}

	CATFILE (data, len / 2, out);
	CATFILE (data + len / 2, len - len / 2, out);
}

#define NCCOBJ "# nccobj "

static void LINK (char **obj, int objn)
{
	int i;
	for (i = 0; i < objn; i++) {
		char *ncobj = (char*) alloca (strlen (obj [i]) + 10 + sizeof OUTPUT_EXT);
		strcat (strcpy (ncobj, obj [i]), OUTPUT_EXT);
		load_file L (ncobj);
		if (L.success == ZC_OK) {
			fprintf (stderr, "ncc: Linking object file ["COLS"%s"COLE"] (%i bytes)\n",
				 ncobj, L.len);
			PRINTF (NCCOBJ"%s\n", ncobj);
			CATFILE (L.data, L.len, report_stream);
		}
	}
}

static void RUN (char *outfile, char **argv)
{
	int i;
	if (!quiet) {
		fprintf (stderr, "Running: ");
		for (i = 0; argv [i]; i++)
			fprintf (stderr, "%s ", argv [i]);
		fprintf (stderr, "\n");
	}

	int pid = fork ();
	if (pid == 0) {
		if (outfile) {
			if (!freopen (outfile, "w", stdout))
				exit (127);
		}
		execvp (argv [0], argv);
		exit (127);
	}
	int status;
	waitpid (pid, &status, 0);
	if (WEXITSTATUS (status) != 0) {
		fprintf (stderr, "[%s] Program failed..\n", argv[0]);
		exit (1);
	}
}

/*************************************************************
#
# binutils emulation mode.
# happens to work in some cases...
#
*************************************************************/

static char *ext (char *f)
{
	char *f2 = (char*) alloca (strlen (f) + 10 + sizeof OUTPUT_EXT);
	return StrDup (strcat (strcpy (f2, f), OUTPUT_EXT));
}

static bool startswith (char *s, char *h)
{
	return strncmp (s, h, strlen (h)) == 0;
}

static void nccar_x (int argc, char **argv)
{
	/* ar x archive [members]
	 * extract into current directory. At the moment we extract all members.
	 */
	argc--, ++argv;
	FILE *F = fopen (ext (argv [0]), "r"), *OUTF = 0;
	argc--, ++argv;

	int i;
	char tmp [1024], n [500];
	if (!F) return;
	while (fgets (tmp, sizeof tmp, F)) {
		if (startswith (tmp, NCCOBJ)) {
			strcpy (n, tmp + sizeof NCCOBJ - 1);
			n [strlen (n) - 1] = 0;
			if (OUTF) fclose (OUTF);
			OUTF = fopen (n, "w");
			fprintf (stderr, "ncc: extract ["COLS"%s"COLE"]\n", n);
		}
		if (OUTF)
			fputs (tmp, OUTF);
	}
	if (OUTF) fclose (OUTF);
}

static void nccar (int argc, char **argv)
{
	/*
	 * we take the simple usage:
	 *   ar [max-four-options] archive-file.[ao] obj1.o obj2.o ...
	 */
	int i, j;

	if (strchr (argv [1], 'x'))
		return nccar_x (argc-1, argv+1);

	if (!strchr (argv [1], 'r')) {
		for (i = 1; i < 5 && i < argc; i++)
			if (isobj (argv [i])) {
				openout (argv [i], "a");
				LINK (&argv [i + 1], argc - i - 1);
				stripout (argv [i]);
				return;
			}
		fprintf (stderr, "nccar: Nothing to do\n");
	} else {
		for (i = 1; i < 5 && i < argc; i++)
			if (isobj (argv [i]))
				break;

#define TMPARCH "tmp.nccarchive"
		char *archive = ext (argv [i]);
		link (archive, TMPARCH);
		unlink (archive);
		FILE *oldarch = fopen (TMPARCH, "r");
		if (!oldarch) {
			openout (argv [i], "a");
			LINK (&argv [i + 1], argc - i - 1);
			stripout (argv [i]);
			return;
		}

		/* replacing members in the archive */
		char *outfile = argv [i];
		openout (outfile, "w");
		bool skipping = false;
		char l [512], n [512];
		while (fgets (l, 500, oldarch)) {
			if (startswith (l, NCCOBJ)) {
				strcpy (n, l + sizeof NCCOBJ - 1);
				n [strlen (n) - sizeof OUTPUT_EXT] = 0;
				skipping = false;
				for (j = i; j < argc; j++)
					if (!strcmp (argv [j], n)) {
						LINK (&argv [j], 1);
						argv [j] = " ";
						skipping = true;
						break;
					}
			}
			if (!skipping)
				fputs (l, report_stream);
		}
		fclose (oldarch);
		unlink (TMPARCH);
		for (j = i + 1; j < argc; j++)
			if (strcmp (argv [j], " "))
				LINK (&argv [j], 1);
		stripout (outfile);
	}
}

static void nccld (int argc, char **argv)
{
	/* 
	 * 'c++'/'g++' is sometimes used as a linker
	 * we only do the job if there are object files
	 */
	int i, objfileno = 0;
	char *ofile = 0, **objfiles = (char**) alloca (argc * sizeof *objfiles);
	for (i = 1; i < argc; i++)
		if (argv [i][0] == '-') {
			if (argv [i][1] == 'o' && argv [i][2] == 0)
				ofile = argv [++i];
		} else if (isobj (argv [i]))
			objfiles [objfileno++] = argv [i];
	if (ofile && objfileno) {
		openout (ofile);
		LINK (objfiles, objfileno);
		stripout (ofile);
	} else fprintf (stderr, "%s: Nothing to do\n", argv [0]);
}

static void emubinutils (int argc, char **argv)
{
	if (0);
#define PROGNAME(x, y) else if (!strcmp (argv [0] + strlen (argv [0])  + 1 - sizeof x, x)) {\
	argv [0] = x;\
	RUN (NULL, argv);\
	ncc ## y (argc, argv);\
	}
#define SPROGNAME(x) PROGNAME(#x,x)
	SPROGNAME(ar)
	SPROGNAME(ld)
	PROGNAME("c++", ld)
	PROGNAME("g++", ld)
	else return;
	fflush (stdout);
	exit (0);
}

const char help [] =
"ncc "NCC_VERSION"  -  The new/next generation C compiler\n"
"The user is the only one responsible for any damages\n"
"Written by Stelios Xanthakis\n"
"Homepage: http://students.ceid.upatras.gr/~sxanth/ncc/\n"
"\n"
"Options starting with '-nc' are ncc options, while the rest gcc:\n"
"	-ncgcc : also run gcc compiler (produces useful object file)\n"
"	-ncgcc=PROG : use PROG instead of \"gcc\"\n"
"	-nccpp=PROG : use PROG for preprocessing\n"
" Files:\n"
"	-ncld : emulate object file output: write the output to <objectfile>"OUTPUT_EXT"\n"
"	-ncoo : write the output to sourcefile.c"OUTPUT_EXT"\n"
"	-ncspp : keep sourcefile.i preprocessor output instead of "
				PREPROCESSOR_OUTPUT"\n"
" Output:\n"
"	-nccc : compile and produce virtual bytecode\n"
"	-ncmv : display multiple uses of functions and variables\n"
"	-ncpc : like -ncmv but also produce pseudo-code data\n"
"	-ncnrs : do not report structure declaration locations (lots of data)\n"
"	-ncerr : halt ncc on the first expression error detected\n"
" Switches:\n"
"	-nc00 : do not include constant values (faster/less memory)\n"
" Extras:\n"
"	-nckey : scan source file for additional output (see doc)\n"
" Filenames:\n"
"	-ncfabs : report absolute pathnames in the output\n"
"	-nc- : ignored option\n"
"\nncc can also be called as 'nccar', 'nccld', 'nccc++', 'nccg++'\n"
"In these cases it will invoke the programs 'ar', 'ld', 'c++' and 'g++'\n"
"and then attempt to collect and link object files with the extension "OUTPUT_EXT"\n"
;

void preproc (int argc, char**argv)
{
	int i;

	report_stream = stdout;

	for (i = 0; i < argc; i++)
		if (!strcmp (argv [i], "-ncquiet")) {
			quiet = true;
			for (--argc; i < argc; i++)
				argv [i] = argv [i + 1];
			argv [i] = 0;
		}


	if (1 && !quiet) {
		fprintf (stderr, "Invoked: ");
		for (i = 0; i < argc; i++) fprintf (stderr, "%s ", argv [i]);
		fprintf (stderr, "\n");
	}

	/* emulate ar, ld, ... */
	emubinutils (argc, argv);

	bool spp = false, dontdoit = false, mkobj = false, ncclinker = false;
	bool rungcc = false;
	char *keys [10];
	char **gccopt, **cppopt, **nccopt, **files, **objfiles, **nofileopt;
	char *ofile = 0;
	int gccno, cppno, nccno, filesno, objfileno, nofileno, keyno;

	cppopt = (char**) alloca ((8 + argc) * sizeof (char*));
	nccopt = (char**) alloca ((3 + argc) * sizeof (char*));
	gccopt = (char**) alloca ((3 + argc) * sizeof (char*));
	files = (char**) alloca (argc * sizeof (char*));
	objfiles = (char**) alloca (argc * sizeof (char*));
	nofileopt = (char**) alloca ((3 + argc) * sizeof (char*));

	cppopt [0] = gccopt [0] = "gcc";
	cppopt [1] = "-E";
	cppopt [2] = "-D__NCC__";
	cppopt [3] = "-imacros";
	cppopt [4] = NOGNU;
	nofileopt [0] = "ncc";
	files [0] = NULL;
	cppno = 5;
	gccno = 1;
	keyno = filesno = nccno = objfileno = 0;
	nofileno = 1;
	for (i = 1; i < argc; i++)
		if (argv [i][0] == '-' && argv [i][1] == 'n'
		&& argv [i][2] == 'c') {
			nccopt [nccno++] =
			 (nofileopt [nofileno++] = argv[i]) + 3;
			if (!strcmp (argv [i], "-ncld"))
				ncclinker = true;
			else if (!strcmp (argv [i], "-ncgcc"))
				rungcc = true;
		}
		else {
			gccopt [gccno++] = argv [i];
			if (issource (argv [i]))
				cppopt [cppno++] = files [filesno++] = argv [i];
			else {
				nofileopt [nofileno++] = argv [i];
				if (isobj (argv [i]))
					objfiles [objfileno++] = argv [i];
				if (argv [i][0] == '-')
				if (argv [i][1] == 'O' || argv [i][1] == 'm') goto addit;
				else if (argv [i][1] == 'D' || argv [i][1] == 'I')
					if (argv [i][2] == 0) goto separate;
					else addit: cppopt [cppno++] = argv [i];
				else if (argv [i][1] == 'i') {
				separate:
					cppopt [cppno++] = argv [i++];
					gccopt [gccno++] = cppopt [cppno++] =
							   argv [i];
				} else if (argv [i][1] == 'E')
					dontdoit = true;
			}
			if (!strcmp (argv [i], "-o"))
				gccopt [gccno++] = ofile = argv [++i];
			else if (!strcmp (argv [i], "-c")) mkobj = 1;
		}
	if (!mkobj && ncclinker)
		nofileopt [nofileno++] = "-c";

	nccopt [nccno] = gccopt [gccno] = cppopt [cppno] =
	nofileopt [nofileno + 1] = NULL;

	if (!ofile)
		ofile = mkobj ? filesno != 1 ? 0 : objfile (files [0]) : (char*) "a.out";

	if (filesno > 1) {
		if (rungcc)
			RUN (NULL, gccopt);
		fprintf (stderr, "Multiple files. Forking\n");
		if (ofile)
			for (i = 0; i < nofileno; i++)
				if (!strcmp (nofileopt [i], "-o"))
					nofileopt [i] = "-c";
		if (rungcc)
			for (i = 0; i < nofileno; i++)
				if (!strcmp (nofileopt [i], "-ncgcc"))
					nofileopt [i] = "-nc-";
		for (i = 0; i < filesno; i++) {
			nofileopt [nofileno] = files [i];
			RUN (NULL, nofileopt);
			if (ncclinker)
				objfiles [objfileno++] = objfile (files [i]);
		}
		if (ncclinker) {
			if (ofile) openout (ofile);
			LINK (objfiles, objfileno);
			if (ofile) stripout (ofile);
		}
		exit (0);
	}

	include_values = usage_only = true;
	multiple = false;
	for (i = 0; i < nccno; i++)
		if (0);
#define		NCCOPT(x) else if (!strcmp (nccopt [i], x))
#define		NCCPOPT(x) else if (!strncmp (nccopt [i], x, sizeof x - 1))
		NCCPOPT("gcc=") {
			gccopt [0] = nccopt [i] + 4;
			RUN (NULL, gccopt);
		}
		NCCOPT ("-");
		NCCOPT ("gcc")	RUN (NULL, gccopt);
		NCCOPT ("cc")	usage_only = false;
		NCCOPT ("err")	halt_on_error = true;
		NCCOPT ("noerr")no_error = true;
		NCCOPT ("mv")	multiple = true;
		NCCOPT ("pc")	multiple = pseudo_code = true;
		NCCOPT ("oo")	openout (files [0]);
		NCCOPT ("ld")   openout (ofile);
		NCCOPT ("00")	include_values = false;
		NCCOPT ("spp")	spp = true;
		NCCOPT ("fabs")	abs_paths = true;
		NCCOPT ("nrs")	report_structs = false;
		NCCPOPT ("cpp=") cppopt [0] = nccopt [i] + 4;
		NCCPOPT ("key")  keys [keyno++] = nccopt [i] + 3;
		NCCOPT ("quiet");
		else {
			fputs (help, stderr);
			exit (strcmp (nccopt [i], "help") != 0);
		}

	if (!(sourcefile = files [0])) {
		if (objfileno) {
			LINK (objfiles, objfileno);
			if (ofile)
				stripout (ofile);
		} else fprintf (stderr, "ncc: No C source file\n");
		exit (0);
	}
	if (dontdoit) {
		fprintf (stderr, "ncc: '-E'. Won't do it...\n");
		exit (0);
	}
	if (spp) {
		preprocfile = StrDup (sourcefile);
		preprocfile [strlen (preprocfile) - 1] = 'i';
	}
	if (keyno) {
		ncc_key = new char* [keyno + 1];
		for (i = 0; i < keyno; i++) ncc_key [i] = keys [i];
		ncc_key [i] = NULL;
	}
	if (usage_only) set_usage_report ();
	else set_compilation ();

	include_strings = include_values || ncc_key;

	if (objfileno)
		LINK (objfiles, objfileno);

	RUN (preprocfile, cppopt);
}

void ncc_keys ()
{
#define KSZ (sizeof NCC_INFILE_KEY - 1)
	int i, j;
	if (ncc_key)
	for (i = 0; i < C_Nstrings; i++)
	if (!strncmp (C_Strings [i], NCC_INFILE_KEY, KSZ)) {
		for (j = 0; ncc_key [j]; j++)
			if (!strncmp (C_Strings [i] + KSZ, ncc_key [j],
				      strlen (ncc_key [j]))) break;
		if (ncc_key [j])
			PRINTF ("%s", C_Strings [i] + KSZ +
				strlen (ncc_key [j]));
	}
}
