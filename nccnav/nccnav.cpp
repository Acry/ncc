// sxanth@ceid.upatras.gr
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <fcntl.h>

#include "dbstree.h"
#include "inttree.h"

// If you don't want an external viewer,
// comment out the definitions of SOURCE_VIEWER
#define SOURCE_VIEWER_INDENT "indent -st -kr | less"
#define SOURCE_VIEWER "less"
#define COLOR_VIEWER "vi -R"
#define OUTFILE1 "nccnav.outs"

static char *svr;
static char CWD [1000];

// "uti.mod"
//
// utilities
//

static char *strDup (char *s)
{
	return strcpy (new char [strlen (s) + 1], s);
}

#define SHUT_UP_GCC(x) (void)x;

static void progress ()
{
static	char pchars [] = "-\\|/", out [] = { '\r', '[', 0, ']' };
static	int pi, ti;
	if (!(ti++ & 127)) {
		out [2] = pchars [pi = (pi + 1)%(sizeof pchars - 1)];
		write (fileno (stdout), out, sizeof out);
	}
}

static void aprogress ()
{
static	int ti;
	char out [] = { '\r', '[', 0, ']' };
	out [2] = '0' + ti++;
	write (fileno (stdout), out, sizeof out);
}

class foreach_intNode
{
   protected:
virtual	void A (intNode*) = 0;
	void inorder (intNode*);
   public:
};

void foreach_intNode::inorder (intNode *i)
{
	if (i->less) inorder (i->less);
	A (i);
	if (i->more) inorder (i->more);
}


class load_file
{
	int fd;
   public:
	load_file (char*);
	int success;
	char *data;
	int len;
	~load_file ();
};

#define ZC_OK 0
#define ZC_NA 1
#define ZC_AC 2
#define ZC_FF 3

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
	success = read (fd, data, len) == len ? ZC_OK : ZC_FF;
}

load_file::~load_file ()
{
	if (data) free (data);
	if (fd != -1) close (fd);
}

#endif
// "st1.mod"
//
// Symbol tree
//

class symNode : public dbsNodeStr
{
   public:
	symNode *less, *more;
	unsigned int i;
	symNode ();
	~symNode ();
};

static dbsTree<symNode> symTree;
static unsigned int nsym = 1;

symNode::symNode () : dbsNodeStr ()
{
	symTree.addself (this);
	i = nsym++;
}

symNode::~symNode ()
{
	if (less) delete less;
	if (more) delete more;
}

unsigned int enter_symbol (char *sy)
{
	DBS_STRQUERY = sy;
	symNode *s = (symNode*) symTree.dbsFind ();
	if (!s) {
		DBS_STRQUERY = strDup (DBS_STRQUERY);
		s = new symNode;
		progress ();
	}
	return s->i;
}
//
// Hyper nodes
//

class hyperNode : public intNode
{
   public:
	void settle ();
	intTree itree;
	unsigned int *rel;
	hyperNode ();
	void relates (unsigned int);
	void rsettle ();
};

static intTree hyperTree;

hyperNode::hyperNode () : intNode (&hyperTree)
{ }

#define SOURCE_ID	0
#define STRUCT_ID	1
#define CALLER_ID	2
#define GLOBAL_ID	3
#define CALL_ID		4
#define MEMBER_ID	5
#define NFIELDS		6
#define RECURS_BOOST	7	/* color */

#define BASE		(1000*1000)
#define SOURCE_BASE	(SOURCE_ID * BASE)
#define CALLER_BASE	(CALLER_ID * BASE)
#define GLOBAL_BASE	(GLOBAL_ID * BASE)
#define CALL_BASE	(CALL_ID * BASE)
#define STRUCT_BASE	(STRUCT_ID * BASE)
#define MEMBER_BASE	(MEMBER_ID * BASE)
#define FIELD_BASE	(NFIELDS * BASE)
#define RECURS_BASE	(RECURS_BOOST * BASE)

static unsigned int symbol_of (unsigned int i)
{
	return i % BASE;
}

static unsigned int base_of (unsigned int i)
{
	return i / BASE;
}

unsigned int counts [NFIELDS], dupes;

void hyperNode::relates (unsigned int y)
{
	if (!itree.intFind (y)) new intNode (&itree);
	else if (base_of (y) != STRUCT_ID && base_of (Key) != STRUCT_ID)
		dupes++;
}

static void relate (int x, int y)
{
	hyperNode *h = (hyperNode*) hyperTree.intFind (x);

	if (!h) {
		h = new hyperNode;
		counts [base_of (x)]++;
	}
	h->relates (y);
}

static int nlinks = 0;

static void relative (int x, int y)
{
	++nlinks;
	relate (base_of (x) == CALLER_ID ? x - CALLER_BASE + CALL_BASE : x, y);
	relate (base_of (y) == CALLER_ID ? y - CALLER_BASE + CALL_BASE : y, x);
}
// "repl.mod"
//
// macro replacements
//

static struct repl_s {
	unsigned int r, f;
} * replarr;

static unsigned int nreplarr, areplarr;

static void stor_replacement (unsigned int r, unsigned int f)
{
	if (nreplarr == areplarr)
		replarr = (repl_s*) realloc (replarr, sizeof (repl_s) * (areplarr += 64));
	replarr [nreplarr].r = r;
	replarr [nreplarr++].f = f;
}

static void repl_line (char *l)
{
	char *d = strchr (l, ' ');

	if (d) {
		*d = 0;
		stor_replacement (enter_symbol (l) + CALL_BASE, enter_symbol (d + 1) + CALL_BASE);
	}
}

void do_replacements ()
{
	unsigned int i, j, k, nu = 0, *un = (unsigned int*) alloca (sizeof * un * nreplarr);
	hyperNode **hns = (hyperNode**) alloca (sizeof * hns * nreplarr);

	for (i = 0; i < nreplarr; i++) {
		for (j = 0; j < nu; j++)
			if (un [j] == replarr [i].r) break;
		if (j < nu) continue;
		un [nu++] = replarr [i].r;
		hyperNode *h = hns [j] = (hyperNode*) hyperTree.intFind (replarr [i].r);
		if (!h) continue;
		h->settle ();
		for (j = 0; j < (unsigned int) h->itree.cnt; j++) {
			hyperNode *h2 = (hyperNode*) hyperTree.intFind (h->rel [j] - CALLER_BASE + CALL_BASE);
			intNode *n = h2->itree.intFind (replarr [i].r);
			if (n) {
				n->intRemove (&h2->itree);
				delete n;
			}
		}
		h->intRemove (&hyperTree);
	}

	for (i = 0; i < nreplarr; i++) {
		for (j = 0; j < nu; j++)
			if (un [j] == replarr [i].f) break;
		if (j < nu) {
			unsigned int l, m;
			k = replarr [i].f;
			for (j = 0; un [j] != replarr [i].r; j++);
			for (l = 0; l < nreplarr; l++)
				if (replarr [l].r == k) {
				 	for (m = 0; m < nu; m++)
						if (un [m] == replarr [l].f) break;
					if (m < nu) continue;
					if (hns [j]) for (m = 0; (int) m < hns [j]->itree.cnt; m++)
						relative (hns [j]->rel [m], replarr [l].f);
				}
			continue;
		}
		for (j = 0; un [j] != replarr [i].r; j++);
		if (hns [j]) for (k = 0; (int) k < hns [j]->itree.cnt; k++)
			relative (hns [j]->rel [k], replarr [i].f);
	}
}
// "ln.mod"
//
// Line numbers
//

struct file_chunk {
	unsigned int start, end;
} NoChunk = { ~0, ~0 };

class lnnode : public intNode
{
   public:
	file_chunk line;
	lnnode (unsigned int, unsigned int);
};

static intTree lnTree;

lnnode::lnnode (unsigned int l, unsigned int e) : intNode (&lnTree)
{
	line.start = l;
	line.end = e;
}

static void enter_line (unsigned int f, unsigned int l, unsigned int e)
{
	if (!lnTree.intFind (f)) new lnnode (l, e);
}

file_chunk line_of (unsigned int f)
{
	lnnode *l = (lnnode*) lnTree.intFind (f);
	return (l) ? l->line : NoChunk;
}
// "agr.mod"
//
// Make structure -> members link
//

void struct_fp_link (char *s)
{
	char *dot, ss [100];

	if (!(dot = strchr (s, '.'))) return;
	*dot = 0; strcpy (ss, s+1); *dot = '.';
	relative (CALL_BASE + enter_symbol (s),
		  STRUCT_BASE + enter_symbol (ss));
}

void struct_link (char *s)
{
	char *dot, ss [100];

	if (!(dot = strchr (s, '.'))) return;
	*dot = 0; strcpy (ss, s); *dot = '.';
	relative (MEMBER_BASE + enter_symbol (s),
		  STRUCT_BASE + enter_symbol (ss));
}
//
// Structure line numbers
//

void struct_loc (char *s)
{
	char *p;

	if (!(p = strchr (s, ' ')))
		return;
	*p++ = 0;
	enter_line (enter_symbol (s) + STRUCT_BASE, atoi (p), atoi (strchr (p, ' ')));
}
//
// Symbol Table
//

static struct st_entry {
	char *name;
	int weight;
} *symbol_table;

static void totable (symNode *s)
{
static	int order = 0;
	symbol_table [s->i].name = s->Name;
	symbol_table [s->i].weight = order++;
}

void make_symbol_table ()
{
	symbol_table = new st_entry [nsym];
	symTree.foreach (totable);
	delete symTree.root;
}

static int weight (unsigned int i)
{
	return symbol_table [symbol_of (i)].weight;
}

static char *symbol_name (unsigned int i)
{
	return symbol_table [symbol_of (i)].name;
}
// "hns.mod"
//
// Settle hypernodes
//

static unsigned int *tmp_table;

class enter_intnodes : public foreach_intNode
{
	void A (intNode *i) {
		*tmp_table++ = i->Key;
	}
   public:
	enter_intnodes (intTree *I) { if (I->root) inorder (I->root); }
};

void hyperNode::settle ()
{
	if (!itree.root)
		return;
	tmp_table = rel = new unsigned int [itree.cnt];
	enter_intnodes E (&itree);
	SHUT_UP_GCC (E)
	delete itree.root;
}

class hyper_settle : public foreach_intNode
{
   protected:
	void A (intNode *i) {
		((hyperNode*)i)->settle ();
	}
   public:
	hyper_settle () { inorder (hyperTree.root); }
};

void settle_hyperNodes ()
{
	hyper_settle H;
	SHUT_UP_GCC (H)
}
//
// Sort symbol indexes
//

static void swap (unsigned int v[], int i, int j)
{
	unsigned int swp = v [i];
	v [i] = v [j];
	v [j] = swp;
}

void qsort (unsigned int v[], int left, int right)
{
	int i, last, wleft;

	if (left >= right)
		return;
	swap (v, left, (left+right)/2);
	wleft = weight (v [left]);
	last = left;
	for (i = left+1; i <= right; i++)
		if (weight (v [i]) < wleft)
			swap (v, i, ++last);
	swap (v, last, left);
	qsort (v, left, last-1);
	qsort (v, last+1, right);
}

void sort_fields (unsigned int v [], int n)
{
	int i, j, k;
	struct {
		unsigned int *v;
		int n;
	} field [NFIELDS];

	for (i = 0; i < NFIELDS; i++) {
		field [i].v = (unsigned int*) alloca (n * sizeof (int));
		field [i].n = 0;
	}

	for (i = 0; i < n; i++)
		field [base_of (v [i])].v [field [base_of (v [i])].n++] = v [i];

	for (j = i = 0; i < NFIELDS; i++) {
		if (field [i].n > 1)
			qsort (field [i].v, 0, field [i].n - 1);
		for (k = 0; k < field [i].n; k++)
			v [j++] = field [i].v [k];
	}
}
// "ia.mod"
//
// Staring tables
//

class hyper_gets : public foreach_intNode
{
	void A (intNode*);
   public:
	unsigned int nfiles, nfuncs;
	hyper_gets ();
};

unsigned int *Files, *funcs, *entries, nentries, *globals, nglobals;

static bool has_callers (hyperNode *h)
{
	int i;
	for (i = 0; i < h->itree.cnt; i++)
		if (base_of (h->rel [i]) == CALLER_ID
		// recursive callers of self, do not count
		&& symbol_of (h->rel [i]) != symbol_of (h->Key))
			return true;
	return false;
}

void hyper_gets::A (intNode *i)
{
	hyperNode *h = (hyperNode*) i;

	switch (base_of (h->Key)) {
		case SOURCE_ID: Files [nfiles++] = h->Key; break;
		case GLOBAL_ID: globals [nglobals++] = h->Key; break;
		case CALL_ID: funcs [nfuncs++] = h->Key;
			if (!has_callers (h)) nentries++; break;
	}
}

hyper_gets::hyper_gets ()
{
	nglobals = nfiles = nfuncs = nentries = 0;
	inorder (hyperTree.root);
	counts [CALL_ID] = nfuncs;
}

void starting_tables ()
{
	unsigned int i, j;
	Files = new unsigned int [counts [SOURCE_ID]];
	funcs = new unsigned int [counts [CALL_ID]];
	globals = new unsigned int [counts [GLOBAL_ID]];
	hyper_gets H;
	SHUT_UP_GCC (H);
	entries = new unsigned int [nentries];
	for (i = j = 0; i < counts [CALL_ID]; i++)
		if (!has_callers ((hyperNode*) hyperTree.intFind (funcs [i])))
			entries [j++] = funcs [i];
	qsort (Files, 0, counts [SOURCE_ID] - 1);
	qsort (funcs, 0, counts [CALL_ID] - 1);
	qsort (globals, 0, counts [GLOBAL_ID] - 1);
	qsort (entries, 0, nentries - 1);
}
// "pf.mod"
//
// Parse File
//

void linenumber (char *s)
{
	char *f;
	if (!(f = strchr (s, ' ')))
		return;
	*f++ = 0;
	enter_line (enter_symbol (s) + CALL_BASE, atoi (f), atoi (strchr (f, ' ')));
}

#define FMT_CALLER	'D'
#define FMT_CALL	'F'
#define FMT_SOURCE	'P'
#define FMT_GLOBAL	'g'
#define FMT_GLOBALw	'G'
#define FMT_STRUCT	's'
#define FMT_STRUCTw	'S'
#define FMT_LINE	'L'
#define FMT_SLINE	'Y'
#define FMT_REPL	'R'

static bool wrt;

unsigned int parse_line (char *l)
{
	int base;
	char tmp [1000];

	wrt = false;

	switch (l [0]) {
		case FMT_LINE:	 base = CALL_BASE;
				 linenumber (l+3);	break;
		case FMT_CALLER: base = CALLER_BASE;	break;
		case FMT_CALL:   base = CALL_BASE;
				 if (l[3] == '*')
				 struct_fp_link (l+3);	break;
		case FMT_SOURCE: base = SOURCE_BASE;
				 if (!strncmp (CWD, l + 3, strlen (CWD)))
				 strcpy (l + 3, strcpy (tmp, l + 3 + strlen (CWD)));
							break;
		case FMT_GLOBALw: wrt = true;
		case FMT_GLOBAL: base = GLOBAL_BASE;	break;
		case FMT_STRUCTw: wrt = true;
		case FMT_STRUCT: base = MEMBER_BASE;
				 struct_link (l+3);	break;
		case FMT_SLINE:  base = STRUCT_BASE;
				 struct_loc (l + 3);	break;
		case FMT_REPL:	 repl_line (l + 3);	return ~0;
		default: return ~0;
	}

	return base + enter_symbol (l + 3);
}

#define	MLINE 512
void parse_file (FILE *f)
{
static	int dfunc = 0;
	int h;
	char line [MLINE];


	while (fgets (line, MLINE, f)) {
		line [strlen (line) - 1] = 0;
		if ((h = parse_line (line)) == ~0) continue;
		if (base_of (h) == CALLER_ID || base_of (h) == SOURCE_ID)
			dfunc = h;
		else relative (wrt ? dfunc - CALLER_BASE + CALL_BASE : dfunc, h);
	}
}

void read_file (char *p)
{
	FILE *f = fopen (p, "r");

	if (!f) {
		printf ("No such file [%s]\n", p);
		exit (1);
	}
	parse_file (f);
	fclose (f);
	if (!hyperTree.root) {
		printf ("File [%s] empty\n", p);
		exit (0);
	}
	aprogress ();
	make_symbol_table ();
	aprogress ();
	do_replacements ();
	aprogress ();
	settle_hyperNodes ();
	aprogress ();
	starting_tables ();
}
//
// Get Relations of Link
//

struct linker {
	unsigned int tn, *t;
	linker (unsigned int, bool = true);
};

linker::linker (unsigned int i, bool dosort)
{
	hyperNode *h = (hyperNode*) hyperTree.intFind (i);
	if (dosort) sort_fields (t = h->rel, tn = h->itree.cnt);
	else t = h->rel, tn = h->itree.cnt;
}
// "recur.mod"
//
// Recursion detection
//

class recursion_detector
{
	unsigned int caller;
	bool check_in (unsigned int);
   public:
	recursion_detector (unsigned int, unsigned int);
	unsigned int *visited;
	bool indeed;
	~recursion_detector ();
};

bool recursion_detector::check_in (unsigned int c)
{
	hyperNode *h = (hyperNode*) hyperTree.intFind (c);
	unsigned int cs = symbol_of (c);
	unsigned int *t = h->rel;
	unsigned int tn = h->itree.cnt;
	unsigned int i;

	for (i = 0; i < tn; i++)
		if (base_of (t [i]) == CALL_ID) {
			unsigned int s = symbol_of (t [i]);
			if (s == caller) {
				visited [caller] = cs;
				return true;
			} else if (!visited [s]) {
				visited [s] = cs;
				if (check_in (t [i])) return true;
			}
		}
	return false;
}

recursion_detector::recursion_detector (unsigned int ca, unsigned ch)
{
	visited = (unsigned int*) calloc (nsym, sizeof (int));
	caller = symbol_of (ca);
	visited [symbol_of (ch)] = caller;

	indeed = check_in (ch);
}

recursion_detector::~recursion_detector ()
{
	free (visited);
}

bool is_recursive (unsigned int caller, unsigned int child)
{
	recursion_detector RD (caller, child);
	return RD.indeed;
}
//
// make recursion path
//

struct recursion_path
{
	unsigned int *path;
	unsigned int n;
	recursion_path (unsigned int, unsigned int);
	~recursion_path ();
};

recursion_path::recursion_path (unsigned int caller, unsigned int child)
{
	recursion_detector RD (caller, child);

	if (RD.indeed) {
		unsigned int cs = symbol_of (caller);
		unsigned int i, cnt;

		for (cnt = 2, i = RD.visited [cs]; i != cs; i = RD.visited [i])
			++cnt;
		n = cnt;
		path = (unsigned int*) malloc (sizeof (int) * cnt);
		path [n-1] = cs + CALL_BASE;
		for (cnt = 1, i = RD.visited [cs]; i != cs; i = RD.visited [i])
			path [n-1-cnt++] = i + CALL_BASE;
		path [n-1-cnt++] = i + CALLER_BASE;
		n = cnt;
	} else path = 0;
}

recursion_path::~recursion_path ()
{
	if (path) free (path);
}
// "grf.mod"
//
// Graphics
//

unsigned int scr_x, scr_y;

enum APP_COLOR {
	BLACK, HIGHLIGHT, C_UP1, C_UP2, C_UP3, NORMAL, C_DOWN1,
	C_DOWN2, C_DOWN3, C_DOWN4, OTHER
};

enum ecol {
	WHITE, BLUE, RED, GREEN, YELLOW, CYAN, MAGENTA
};

int Gcolor (APP_COLOR a)
{
	switch (a) {
	case HIGHLIGHT:	return COLOR_PAIR(WHITE)|A_BOLD;
	case C_UP1:	return COLOR_PAIR (BLUE);
	case C_UP2:	return COLOR_PAIR (BLUE)|A_BOLD;
	case C_UP3:	return COLOR_PAIR (RED);
	case NORMAL:	return COLOR_PAIR(WHITE);
	case C_DOWN1:	return COLOR_PAIR(YELLOW)|A_DIM;
	case C_DOWN2:	return COLOR_PAIR(WHITE)|A_DIM;
	case C_DOWN3:	return COLOR_PAIR (CYAN);
	case C_DOWN4:	return COLOR_PAIR(WHITE)|A_DIM;
	default:
	case OTHER:	return COLOR_PAIR(MAGENTA);
	}
}

void printstr (int x, int y, char *s, APP_COLOR a)
{
	attrset (Gcolor (a));
	move (y, x);
	/* unfortunatelly, there's a bug in ncurses and
	   addnstr (s, -1); counts tabs as 1 character
	   with the result that long lines go past the end
	   of the scr_x and appear on the next line
	 */
	unsigned int i;
	char *p;
	for (i = x, p = s; i < scr_x && *p; p++)
		if (*p == '\t') i += 8;
		else i++;
	if (!*p) printw ("%s", s);
	else {
		char tmp [256];
		strncpy (tmp, s, p - s);
		tmp [p - s] = 0;
		printw ("%s", tmp);
	}
}

void cls ()
{
	clear ();
}

int inkey ()
{
	return getch ();
}

// Init modes

void init_ncurses ()
{
	initscr ();
	//leaveok (stdscr, true);
	start_color ();
	init_pair (BLUE, COLOR_BLUE, COLOR_BLACK);
	init_pair (RED, COLOR_RED, COLOR_BLACK);
	init_pair (GREEN, COLOR_GREEN, COLOR_BLACK);
	init_pair (YELLOW, COLOR_YELLOW, COLOR_BLACK);
	init_pair (CYAN, COLOR_CYAN, COLOR_BLACK);
	init_pair (MAGENTA, COLOR_MAGENTA, COLOR_BLACK);
	crmode ();
	keypad (stdscr, TRUE);
	noecho ();
	scr_x = COLS;
	scr_y = LINES-1;
}

void init_graphics ()
{
	init_ncurses ();
}

void end_graphics ()
{
	flash ();
	endwin ();
}
//
// Text entry display
//

static bool has_calls (unsigned int i)
{
	hyperNode *h = (hyperNode*) hyperTree.intFind (i);
	unsigned int *a = h->rel, j = h->itree.cnt;

	for (i = 0; i < j; i++)
		if (base_of (a [i]) == CALL_ID) return true;
	return false;
}

static bool has_file (unsigned int i)
{
	hyperNode *h = (hyperNode*) hyperTree.intFind (i);
	unsigned int *a = h->rel, j = h->itree.cnt;

	for (i = 0; i < j; i++)
		if (base_of (a [i]) == SOURCE_ID) return true;
	return false;
}

APP_COLOR colors (unsigned int v)
{
	switch (base_of (v)) {
	case SOURCE_ID: return C_UP1;
	case CALLER_ID: return C_UP2;
	case MEMBER_ID: return C_DOWN4;
	case GLOBAL_ID: return C_UP3;
	case NFIELDS:	return OTHER;
	case CALL_ID:   return !has_file (v) ? C_DOWN3 :
			       has_calls (v) ? C_DOWN1 : C_DOWN2;
	case RECURS_BOOST: return C_UP3;
	}
	return OTHER;
}

void printent (int x, int y, unsigned int e, unsigned int m, bool hi)
{
	char trunc [180], *dot;
	char *s = symbol_name (e);

	if (strlen (s) > m)
		if (strchr (s, '/'))
			s = strcpy (trunc, strrchr (s, '/'));
		else
			s = strcpy (trunc, s + strlen (s) - m);
	printstr (x, y, s, hi ? HIGHLIGHT : colors (e));
	if ((dot = strchr (s, '.')))
		printstr (x + (dot - s), y, ".", NORMAL);
}
// "vscr.mod"
//
// a screenful
//

class nvscr {
   public:
static	int jump_node;
	int ii;
	nvscr *next, *prev;
virtual	void vdraw () = 0;
} *nvtop;

int nvscr::jump_node;

// "tdm.mod"
//
// Map with ncurses
//

struct coords {
	unsigned int x, y;
};

class TDmap : public nvscr {
	unsigned int epl, eps, e_spc;
	unsigned int *vmap, e_tot, e_cur, e_top, e_topp;
	coords locate (unsigned int);
	void add_to_list ();
   public:
	TDmap (unsigned int*, unsigned int, unsigned int, unsigned int=0);
	void move_arrow (int);
	void move_jump (unsigned int);
	void vdraw ()	{ draw_map (); }
	void draw_map ();
	unsigned int enter ();
	unsigned int cursor ();
virtual	~TDmap ();
};

void TDmap::add_to_list ()
{
	if ((prev = nvtop)) {
		prev->next = this;
		ii = prev->ii + 1;
	} else ii = 0;
	next = 0;
	nvtop = this;
	jump_node = ii;
}

TDmap::~TDmap ()
{
	if ((nvtop = prev))
		nvtop->next = 0;
}

unsigned int TDmap::enter ()
{
	if (base_of (vmap [e_cur]) != RECURS_BOOST)
		return vmap [e_cur];
	return symbol_of (vmap [e_cur]) + CALL_BASE;
}

unsigned int TDmap::cursor ()
{
	return e_cur;
}

void TDmap::move_arrow (int a)
{
	unsigned int dir = 0, mov;
	switch (a) {
		case KEY_LEFT:	mov = 1, dir = 1; break;
		case KEY_RIGHT:	mov = 1; break;
		case KEY_UP:	mov = epl, dir = 1; break;
		case KEY_DOWN:	mov = epl; break;
		case KEY_PPAGE:	mov = eps, dir = 1; break;
		case KEY_NPAGE: mov = eps; break;
		default: 	return;
	}

	if (!dir) {
		if (e_cur+mov < e_tot) e_cur += mov;
		else if (mov == eps)
			while (e_cur + epl < e_tot) e_cur += epl;
		if (e_cur >= e_top + eps) e_top += eps;
	} else {
		e_cur = (e_cur >= mov) ? e_cur - mov : e_cur % epl;
		if (e_cur < e_top) e_top = (e_top > eps) ? e_top - eps : 0;
	}
}

void TDmap::move_jump (unsigned int i)
{
	e_cur = i;
	e_top = (i/eps) * eps;
}

coords TDmap::locate (unsigned int i)
{
	coords r;
	r.x = e_spc * (i % epl);
	r.y = (i - e_top) / epl;
	return r;
}

void TDmap::draw_map ()
{
	unsigned int i;
	coords xy;

	if (e_topp != e_top) {
		e_topp = e_top;
		cls ();
	}
	for (i = e_top; i < e_top+eps && i < e_tot; i++) {
		xy = locate (i);
		printent (xy.x, xy.y, vmap [i], e_spc-1, false);
	}
	xy = locate (e_cur);
	printent (xy.x, xy.y, vmap [e_cur], e_spc-1, true);
}

unsigned int fieldlen (unsigned int *v, unsigned int n)
{
	unsigned int i, ml;

	for (ml = i = 0; i < n; i++)
		if (strlen (symbol_name (v [i])) > ml)
			ml = strlen (symbol_name (v [i]));

	return (ml > scr_x/2) ? scr_x/2 : (ml < 15) ? 15 : ml+1;
}

TDmap::TDmap (unsigned int *v, unsigned int n, unsigned int c, unsigned int ml)
{
	if (!ml) ml = fieldlen (v, n);
	e_spc = scr_x/(scr_x/ml);
	vmap = v;
	e_topp = e_tot = n;
	epl = scr_x / e_spc;
	eps = scr_y * epl;
	move_jump (e_cur = c);
	add_to_list ();
}
// "tv.mod"
//
// text Viewer
//

#ifndef SOURCE_VIEWER

class txtviewer
{
	char **line;
	unsigned int nlines, maxline, topline, tab;
	void draw ();
	void scrolll (int);
	void main ();
   public:
	txtviewer (char*, char*, unsigned int);
};

void txtviewer::draw ()
{
	unsigned int i;
	cls ();
	for (i = topline; i < nlines && i < topline+scr_y; i++) {
		if (strlen (line [i]) <= tab) continue;
		printstr (0, i-topline, line [i]+tab, NORMAL);
	}
}

void txtviewer::scrolll (int key)
{
	unsigned int tlp = topline, tbp = tab;
	switch (key) {
	case KEY_UP:
		if (topline > 0) topline--;
		break;
	case KEY_DOWN:
		if (topline + scr_y < nlines) topline++;
		break;
	case KEY_LEFT:
		if (tab) tab--;
		break;
	case KEY_RIGHT:
		if (maxline - tab > scr_x/2) tab++;
		break;
	case KEY_PPAGE:
		if (topline) topline = (topline > scr_y) ? topline - scr_y : 0;
		break;
	case KEY_NPAGE:
		if (topline + scr_y + 1 < nlines)
			topline += scr_y;
		break;
	default:
		return;
	}
	if (tlp != topline || tbp != tab) draw ();
}

void txtviewer::main ()
{
	int key;
	while ((key = inkey ()) != '\n' && key != ' ')
		scrolll (key);
}

txtviewer::txtviewer (char *title, char *txt, unsigned int l)
{
	nlines = l+1;
	line = (char**) alloca (nlines * sizeof (char*));
	maxline = l = 0;
	line [l++] = title;
	do {
		line [l] = txt;
		if ((txt = strchr (txt, '\n')))
			*txt++ = 0;
		if (strlen (line [l]) > maxline)
			maxline = strlen (line [l]);
	} while (++l < nlines);
	topline = tab = 0;

	draw ();
	main ();
}

#else		/* SOURCE_VIEWER exists */

void source_viewer_fatal ()
{
	endwin ();
	printf ("ALERT! Failed executing [%s] to display the text.\n"
		" Please recompile nccnav with a different SOURCE_VIEWER value.\n", svr);
	exit (1);
}

void txtviewer (char *title, char *txt, unsigned int l, bool colored)
{
	def_prog_mode();		/* Save the tty modes		  */
	endwin();			/* End curses mode temporarily	  */
	if (colored) {
		unsigned int h = 0, *ip = (unsigned int*) title;
		char *tmpc, cmd [100];
		char tmpname [100];

		int i;
		for (i = 0; i < strlen (title) / 4; i++)
			h ^= *ip++;
		sprintf (tmpname, "/tmp/nccnav-tmp%x.c", h);

		FILE *f = fopen (tmpc = tmpname, "w");
		fputs (title, f); fputc ('\n', f);
		fputs (txt, f);
		fclose (f);
		sprintf (cmd, COLOR_VIEWER" %s", tmpc);
		system (cmd);
		unlink (tmpc);
	} else {
		FILE *p = popen (svr, "w");
		if (!p) source_viewer_fatal ();
		fputs (title, p); fputc ('\n', p);
		fputs (txt, p);
		fflush (p);
		pclose (p);
	}
	reset_prog_mode();		/* Return to the previous tty mode*/
					/* stored by def_prog_mode() 	  */
	refresh();			/* Do refresh() to restore the	  */
					/* Screen contents		  */
}

#endif
// "vf.mod"
//
// Extract text from file
//

char *text_from (char *f, int l1, int l2)
{
	load_file L (f);

	if (L.success != ZC_OK) return NULL;

	int i, cl;
	char *r;

	for (i = 0, cl = 1; i <= L.len && cl < l1; i++)
		if (L.data [i] == '\n') cl++;
	if (cl < l1) return NULL;
	l1 = i;
	for (;i < L.len && cl <= l2; i++)
		if (L.data [i] == '\n') cl++;
	if (cl < l2) return NULL;
	l2 = i;

	for (i = l1; i && !isspace (L.data [i]); --i);
	l1 = 0;
	for (; i; i--)
		if (L.data [i] == '/' && L.data [i - 1] == '*')
			for (i -= 2; i && (L.data [i] != '/' || L.data [i + 1] != '*'); --i);
		else if (isspace (L.data [i])) continue;
		else {
			cl = i;
			while (i && L.data [i] != '\n') --i;
			for (i += !!i; isspace (L.data [i]); ++i);
			if (L.data [i] != '/' || L.data [i + 1] != '/') {
				for (l1 = cl + 1; isspace (L.data [l1]); l1++);
				break;
			}
		}

	r = new char [1 + l2 - l1];
	memcpy (r, L.data + l1, l2 - l1);
	r [l2 - l1] = 0;
	return r;
}

void func_text (unsigned int f, bool color=false)
{
	char *txt, *title, *fn;
	file_chunk F = line_of (f);

	if (F.start == ~0U) return;
	linker L (f);
	fn = symbol_name (L.t [0]);
	txt = text_from (fn, F.start, F.end);
	if (txt) {
		title = (char*) alloca (strlen (fn) + 20);
		sprintf (title, "# %s %u", fn, F.start);
		txtviewer (title, txt, F.end - F.start + 1, color);
		delete [] txt;
	}
}
// "file"
//
// view entire file
//

void file_text (unsigned int i, bool color)
{
#ifdef SOURCE_VIEWER
	def_prog_mode();		/* Save the tty modes		  */
	endwin();			/* End curses mode temporarily	  */
	if (color) {
		char tmp [1024];
		strcat (strcpy (tmp, COLOR_VIEWER" "), symbol_name (i));
		system (tmp);
	} else {
		load_file L (symbol_name (i));

		if (L.success != ZC_OK) return;

		FILE *p = popen (svr, "w");
		if (!p) source_viewer_fatal ();
		fwrite (L.data, 1, L.len, p);
		fflush (p);
		pclose (p);
	}
	reset_prog_mode();		/* Return to the previous tty mode*/
					/* stored by def_prog_mode() 	  */
	refresh();			/* Do refresh() to restore the	  */
					/* Screen contents		  */
#endif
}
// "pd.mod"
//
// pop ups
//

void pastmode ();
void enter_othermode (unsigned int);
class FOO {};

class popup : public nvscr {
	popup *outer;
	unsigned int *ent, nent;
	WINDOW *w;
	unsigned int top, cur, mx, my, dh, dw;
	void add_to_list ();
	void prefresh ();
	void draw ();
	void main ();
   public:
	popup (popup*, unsigned int, unsigned int, unsigned int);
	void vdraw ();
virtual	~popup ();
};

void popup::add_to_list ()
{
	next = 0;
	prev = (outer) ? outer->prev : nvtop;
	prev->next = nvtop = this;
	jump_node = ii = prev->ii + 1;
}

void popup::prefresh ()
{
	if (outer) outer->prefresh ();
	else {
		touchwin (stdscr);
		wnoutrefresh (stdscr);
	}
	touchwin (w);
	wnoutrefresh (w);
}

#define CLEARBOX \
	wclear (w);\
	box (w, 0, 0);

void popup::draw ()
{
	unsigned int i;
	for (i = 0; i < dh; i++) {
		wattrset (w, Gcolor (i ? colors (ent [top + i]) : C_UP3));
		mvwprintw (w, i, 1, "%s", symbol_name (ent [top + i]));
	}
	wmove (w, cur - top, 1);
	wattrset (w, Gcolor (HIGHLIGHT));
	wprintw (w, "%s", symbol_name (ent [cur]));
	touchwin (w);
	wrefresh (w);
}

void popup::vdraw ()
{
	prefresh ();
	doupdate ();
}

void popup::main ()
{
	draw ();
	prefresh ();
	for (; ii <= jump_node; draw()) switch (inkey ()) {
		case KEY_DOWN: if (cur < nent - 1)
				if (++cur == top + dh) {
					top++;
					CLEARBOX;
				}
				break;
		case KEY_UP: if (cur > 0)
				if (--cur < top) {
					top--;
					CLEARBOX
				}
				break;
		case KEY_PPAGE: cur = top = 0;
				CLEARBOX
				break;
		case KEY_NPAGE: if (cur < nent - 1) {
				if ((cur += 6) >= nent) cur = nent - 1;
				if (cur >= top + dh) {
					top = cur - dh;
					CLEARBOX
				} }
				break;
		case KEY_BACKSPACE: throw FOO ();
		case KEY_LEFT:
		case 'q': return;
		case KEY_RIGHT: if (cur == 0) break;
		case '\n': if (!cur) return;
			{ popup (this, ent [cur], mx + dw - 4, my + cur - top); }
			vdraw ();
			break;
		case 'v':
			func_text (ent [cur], true);
			break;
		case ' ':
			func_text (ent [cur]);
			cls ();
			vdraw ();
			break;
		case '2': enter_othermode (ent [cur]);
			cls ();
			vdraw ();
			break;
		case '<':
			pastmode ();
			cls ();
			vdraw ();
	}
}

popup::popup (popup *o, unsigned int f, unsigned int x, unsigned int y)
{
	linker L (f);
	unsigned int i, j;

	for (i = j = 0; i < L.tn; i++)
		if (base_of (L.t [i]) == CALL_ID) j++;

	ent = 0;
	if (!j) return;

	ent = (unsigned int*) malloc (sizeof (unsigned int) * (nent = j + 1));
	ent [0] = f;
	for (i = 0, j = 1; i < L.tn; i++)
		if (base_of (L.t [i]) == CALL_ID)
			ent [j++] = L.t [i];

	if (nent >= scr_y) {
		dh = scr_y - 1;
		my = 0;
	} else {
		dh = nent;
		my = (y + dh >= scr_y) ? 0 : y;
	}

	for (i = j = 0; i < nent; i++)
		if ((f = strlen (symbol_name (ent [i]))) > j) j = f;
	dw = j + 2;
	mx = (x + dw >= scr_x) ? 0 : x;
	top = cur = 0;

	outer = o;
	w = newwin (dh + 1, dw, my, mx);
	CLEARBOX;
	add_to_list ();
	main ();
}

popup::~popup ()
{
	if (ent) {
		(nvtop = prev)->next = 0;
		free (ent);
		delwin (w);
	}
}
// "mod.mod"
//
// modes
//

void pastmode ()
{
	char m [20];
	nvscr *c = nvtop->prev;
	for (;;) {
		cls ();
		sprintf (m, "%i/%i", c->ii, nvtop->ii);
		printstr (0, scr_y, m, HIGHLIGHT);
		c->vdraw ();
		switch (inkey ()) {
			case '<': if (c->prev) c = c->prev; break;
			case '>': if (c->next) c = c->next; break;
			case '\n': c->jump_node = c->ii; return;
			default: nvtop->vdraw (); return;
		}
	}
}

void othermode (unsigned int);

void recursmode (recursion_path &RP)
{
	int i;
	TDmap M (RP.path, RP.n, 0, scr_x);

	M.draw_map ();
	for (; M.ii <= M.jump_node; M.draw_map ()) {
		printstr (0, scr_y, "Recursion unroll", C_UP2);
		if ((i = inkey ()) == 'q')
			break;
		if (i == '\n') {
			if (M.cursor () == 0) break;
			othermode (M.enter ());
			cls ();
		} else M.move_arrow (i);
	}
}

void color_recursives (unsigned int caller, unsigned int *pa, int npa)
{
	int i;
	for (i = 0; i < npa; i++)
		if (base_of (pa [i]) == CALL_ID && is_recursive (caller, pa [i]))
			pa [i] = symbol_of (pa [i]) + RECURS_BASE;
}

void othermode (unsigned int e)
{
static	bool rdetect = true;

	int key;
	unsigned int i, j, c, *jarr;

	e = base_of (e) == CALLER_ID ? symbol_of (e) + CALL_BASE : e;
	unsigned int tbase = base_of (e);
	linker L (e);

	jarr = (unsigned int*) alloca ((L.tn + 1) * sizeof(int));
	i = j = 0;
	while (i < L.tn && base_of (L.t [i]) < CALL_ID)
		jarr [j++] = L.t [i++];
	jarr [c = j++] = e = symbol_of (e) + FIELD_BASE;
	while (i < L.tn)
		jarr [j++] = L.t [i++];

	if (rdetect && tbase == CALL_ID)
		color_recursives (e, jarr + c + 1, i - c);

	TDmap M (jarr, j, c);

	M.draw_map ();
	for (;M.ii <= M.jump_node; M.draw_map ()) {
		if (tbase == CALL_ID && c >= 2 && base_of (jarr [1]) == SOURCE_ID) {
			printstr (0, scr_y, "* FUNCTION IS       :MULTIPLE DEFINITIONS *", HIGHLIGHT);
			printstr (13, scr_y,             " br0ken", C_UP2);
		}
		switch (key = inkey ()) {
		case ' ':
		case 'v':
			j = base_of (M.enter ());
			if (j == CALL_ID) func_text (M.enter (), key == 'v');
			else if (j == CALLER_ID)
				func_text (symbol_of (M.enter ()) + CALL_BASE, key == 'v');
			else if (j == NFIELDS && tbase == CALL_ID)
				func_text (symbol_of (M.enter ()) + CALL_BASE, key == 'v');
			else if ((j == SOURCE_ID||(j == NFIELDS&&tbase == SOURCE_ID)))
				file_text (symbol_of (M.enter ()), key == 'v');
			else if (j == STRUCT_ID || (j == NFIELDS && tbase == STRUCT_ID))
				func_text (symbol_of (M.enter ()) + STRUCT_BASE, key == 'v');
			else break;
			if (key != '1')
				cls ();
			break;
		case '\n':
			if (M.enter () == e) return;
			othermode (M.enter ());
			cls ();
			break;
		case KEY_BACKSPACE:
			cls ();
			throw FOO();
		case 'q':
			cls ();
			return;
		case 'r':
			if (base_of (M.enter ()) == CALL_ID) {
				recursion_path RP (e, M.enter ());
				if (RP.path) recursmode (RP);
			}
			cls ();
			break;
		case 'R':
			if ((rdetect =! rdetect)) {
				printstr (0, scr_y, "Recursion Detector ENABLED.", OTHER);
				color_recursives (e, jarr + c + 1, i - c);
			} else {
				printstr (0, scr_y, "Recursion Detector DISABLED", C_UP3);
				for (j = c; j < i+1; j++)
					if (base_of (jarr [j]) == RECURS_BOOST)
						jarr [j] -= RECURS_BASE - CALL_BASE;
			}
			break;
		case 'm':
			j = base_of (M.enter ());
			if (j == CALL_ID || j == CALLER_ID || (j == NFIELDS && tbase == CALL_ID))
				try {
					j = j == CALL_ID ? M.enter () : symbol_of (M.enter ()) + CALL_BASE;
					cls ();
					popup P (0, j, 0, 0);
				} catch (FOO) { }
			cls ();
			break;
		case '<':
			pastmode ();
			cls ();
			break;
		case 'C':
			// end_graphics ();
			def_prog_mode();		/* Save the tty modes		  */
			endwin();			/* End curses mode temporarily	  */
			system ("bash");
			reset_prog_mode();		/* Return to the previous tty mode*/
							/* stored by def_prog_mode() 	  */
			refresh();			/* Do refresh() to restore the	  */
							/* Screen contents		  */
			// init_graphics ();
			break;
		default:
			M.move_arrow (key);
		}
	}
}

void enter_othermode (unsigned int e)
{
	try {
		othermode (e);
	} catch (FOO) { }
}

void globmode ()
{
	unsigned int *t, tn;
	t = globals, tn = counts [GLOBAL_ID];

	int key;
	TDmap M (t, tn, 0);

	M.draw_map ();
	for (;;M.draw_map ()) switch (key = inkey ()) {
	case '\n':
		enter_othermode (M.enter ());
		cls ();
		break;
	case KEY_BACKSPACE:
	case 'q':
		return;
	default:
		if (key < 256 && isalpha (key)) {
			unsigned int j = M.cursor ();
			if (*symbol_name (t [j]) < key)
				while (j < tn
				&& *symbol_name (t [j]) < key)
					j++;
			else
				while (j > 0
				&& *symbol_name (t [j]) > key)
					j--;
			M.move_jump (j);
		} else M.move_arrow (key);
	}
}

void funcmode (bool all)
{
	unsigned int *t, tn;
	if (all) t = funcs, tn = counts [CALL_ID];
	else t = entries, tn = nentries;

	if (!tn) {
		flash ();
		return;
	}

	int key;
	TDmap M (t, tn, 0);

	M.draw_map ();
	for (;;M.draw_map ()) switch (key = inkey ()) {
	case '\n':
		enter_othermode (M.enter ());
		cls ();
		break;
	case KEY_BACKSPACE:
	case 'q':
		return;
	case 'v':
		func_text (M.enter (), true);
		cls ();
		break;
	case ' ':
		func_text (M.enter ());
		cls ();
		break;
	default:
		if (key < 256 && isalpha (key)) {
			unsigned int j = M.cursor ();
			if (*symbol_name (t [j]) < key)
				while (j < tn
				&& *symbol_name (t [j]) < key)
					j++;
			else
				while (j > 0
				&& *symbol_name (t [j]) > key)
					j--;
			M.move_jump (j);
		} else M.move_arrow (key);
	}
}

void initialmode ()
{
	int key;
	TDmap M (Files, counts [SOURCE_ID], 0);

	M.draw_map ();
	for (;;M.draw_map ()) switch (key = inkey ()) {
		case '\n':
			enter_othermode (M.enter ());
			cls ();
			break;
		case KEY_BACKSPACE:
		case 'q':
			return;
		case 'E':
			funcmode (false);
			cls ();
			break;
		case 'O':
			funcmode (true);
			cls ();
			break;
		case 'g':
		case 'G':
			globmode ();
			cls ();
			break;
		default:
			M.move_arrow (key);
	}
}
// "main"
//

int main (int argc, char **argv)
{
	getcwd (CWD, sizeof CWD-1);
	if (CWD [strlen (CWD) - 1] != '/')
		strcat (CWD, "/");
#ifdef	SOURCE_VIEWER
	svr = (char*) ((!strcmp (argv [0], "nccnav")) ? SOURCE_VIEWER : SOURCE_VIEWER_INDENT);
#endif
	char *MapFile = (argc > 1) ? argv [1] : (char*)"Code.map";
	read_file (MapFile);
	signal (SIGPIPE, SIG_IGN);
	init_graphics ();
	initialmode ();
	end_graphics ();
	printf ("%i\tFilez \n%i\tfunctionz \n%i\tglobal variables \n"
		"%i\tstructs\n%i\tmembers \n%i\tTotal links\n",
		counts [SOURCE_ID], counts [CALL_ID], counts [GLOBAL_ID],
		counts [STRUCT_ID], counts [MEMBER_ID], nlinks - dupes/2);
}
