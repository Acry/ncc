#include "config.h"
#include "norm.h"
#include "mem_pool.h"
#define MSPEC 20
#define BITFIELD_Q 64

#define COLS "\033[01;37m"
#define COLE "\033[0m"

#define PRINTF(...) fprintf (report_stream, __VA_ARGS__)

extern FILE *report_stream;

//
// the types we'll be using
//
typedef int NormPtr;
typedef int RegionPtr, ObjPtr, typeID, ArglPtr, Symbol, *Vspec;
typedef int exprID;


//
// preprocessing
//
extern void preproc (int, char**);
extern void ncc_keys ();


//
// program options
//
extern bool usage_only, include_values, include_strings, multiple, abs_paths,
	    report_structs, halt_on_error, pseudo_code, no_error;
extern char *sourcefile, *preprocfile, *cwd;


//
// inform
//
extern char* StrDup (char*);
extern void debug (const char*, NormPtr, int);
extern void prcode (NormPtr, int);
extern void prcode (NormPtr, int, Symbol[]);
extern void prcode (NormPtr, int, Symbol);
extern void printtype (int, int*);
extern void printtype (typeID);
extern int syntax_error (NormPtr, char* = NULL);
extern int syntax_error (char*, char*);
extern int syntax_error (NormPtr, char*, char*);
extern void half_error (char*, char* = NULL);
extern void warning (char*, char = 0);
extern char *expand (int);
extern int cline_of (NormPtr);
extern int cfile_of (NormPtr);
extern char *in_file (NormPtr);
extern void report_error ();
class EXPR_ERROR {public: EXPR_ERROR () {}};


//
// lex & normalized C source
//
struct token
{
	int at_line;
	unsigned int type;
	char *p;
	int len;
};
extern token CTok;

struct cfile_i
{
	int	indx;
	char	*file;
};

struct clines_i
{
	int	ftok, line;
};

extern int*		CODE;
extern int		C_Ntok;
extern char**		C_Syms;
extern int		C_Nsyms;
extern char**		C_Strings;
extern int		C_Nstrings;
extern cfile_i*		C_Files;
extern int		C_Nfiles;
extern clines_i*	C_Lines;
extern int		C_Nlines;

extern double*		C_Floats;
extern signed char*	C_Chars;
extern short int*	C_Shortints;
extern long int*	C_Ints;
extern unsigned long*	C_Unsigned;

extern struct __builtins__ {
	int bt__builtin_alloca;
	int bt__builtin_return_address;
	int bt__FUNCTION__;
	int bt__func__;
	int bt__PRETTY_FUNCTION__;
} ccbuiltins;

extern void enter_token ();
extern void enter_file_indicator (char*);
extern void prepare ();
extern void make_norm ();
extern int getint (int);
extern void yynorm (char*, int);

extern Symbol intern_sym;

//
// utilities
//
extern void	intcpycat (int*, const int*, const int*);
extern int*	intdup (int*);
extern int	intcmp (int*, int*);
extern void	intncpy (int*, int*, int);
extern inline	void intcpy (int *d, const int *s)
		 { while ((*d++ = *s++) != -1); }
extern inline	int intlen (const int *i)
		 { int l=0; while (*i++ != -1) l++; return l; }

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

//
// the compilation
//
extern void parse_C ();


//
// CDB interface
//
enum VARSPC {
	EXTERN, STATIC, DEFAULT
};

typedef bool Ok;

extern typeID VoidType, SIntType;

enum BASETYPE {
	S_CHAR = -20, U_CHAR, S_SINT, U_SINT, S_INT, U_INT,
	S_LINT, U_LINT, S_LONG, U_LONG, FLOAT, DOUBLE, VOID,
	_BTLIMIT
};
#define INTEGRAL(x) (x >= S_CHAR && x <= U_LONG)
#define TYPEDEF_BASE 50000

struct type {
	int base;
	Vspec spec;
};
#define ISFUNCTION(t) ((t).spec [0] == '(')
#define T_BASETYPE(t) ((t).base < _BTLIMIT)
#define T_BASESTRUCT(t) (t > 0 && t < TYPEDEF_BASE)

#define ARGLIST_OPEN -2
#define SPECIAL_ELLIPSIS -3

#define INCODE (!INGLOBAL && !INSTRUCT)
extern bool INGLOBAL, INSTRUCT, infuncs;
extern ArglPtr NoArgSpec;
extern void init_cdb ();
extern typeID		gettype (type&);
extern typeID		gettype (int, int*);
extern ArglPtr		make_arglist (typeID*);
extern typeID*		ret_arglist (ArglPtr);
extern void		opentype (typeID, type&);
extern int		base_of (typeID);
extern int*		spec_of (typeID);
extern int		esizeof_objptr (ObjPtr);
extern int		sizeof_typeID (typeID);
extern int		sizeof_type (int, Vspec);
extern int		ptr_increment (int, Vspec);
extern Ok		introduce_obj (Symbol, typeID, VARSPC);
extern Ok		introduce_tdef (Symbol, typeID);
extern ObjPtr		lookup_typedef (Symbol);
extern Ok		is_typedef (Symbol);
extern Ok		introduce_enumconst (Symbol, int);
extern Ok		introduce_enumtag (Symbol, bool=false);
extern Ok		valid_enumtag (Symbol);
extern RegionPtr	introduce_anon_struct (bool);
extern RegionPtr	introduce_named_struct (Symbol, bool);
extern RegionPtr	use_struct_tag (Symbol, bool);
extern RegionPtr	fwd_struct_tag (Symbol, bool);
extern Ok		function_definition (Symbol, NormPtr, NormPtr, NormPtr, NormPtr);
extern Ok		function_no (int, NormPtr*, NormPtr*);
extern void		open_compound ();
extern void		close_region ();
extern Symbol		struct_by_name (RegionPtr);
extern void		functions_of_file ();
extern bool		have_function (Symbol);
extern bool		rename_struct (typeID, Symbol);
extern void		struct_location (typeID, NormPtr, NormPtr);
extern void		structs_of_file ();
extern void		spill_anonymous (RegionPtr);

//
// CDB lookups
//
struct lookup_object {
	bool enumconst;
	int ec;
	ObjPtr base;
	RegionPtr FRAME;
	int displacement;
	int spec [50];
	lookup_object (Symbol, bool=true);
};

struct lookup_function {
	bool fptr;
	ObjPtr base;
	RegionPtr FRAME;
	bool ARGFUNC;
	int displacement;
	int spec [50];
	bool found;
	lookup_function (Symbol, bool=true);
};

struct lookup_member {
	ObjPtr base;
	int spec [50];
	int displacement;
	lookup_member (Symbol, RegionPtr);
};


//
// cc-expressions
//
enum COPS {
	VALUE, FVALUE, SVALUE, UVALUE, AVALUE,
	SYMBOL,
	FCALL, ARRAY, MEMB,
	PPPOST, MMPOST,
	PPPRE, MMPRE, LNEG, OCPL, PTRIND, ADDROF, UPLUS, UMINUS, CAST, SIZEOF,
	MUL, DIV, REM, ADD, SUB, SHL, SHR,
	BEQ, BNEQ, CGR, CGRE, CLE, CLEE,
	BAND, BOR, BXOR,
	IAND, IOR,
	GCOND, COND, // assignments taken from norm.h defines
	COMMA, ARGCOMMA,
	COMPOUND_RESULT
};

struct subexpr
{
	int action;
	union {
		int using_result;
		long int value;
		unsigned long int uvalue;
		double fvalue;
		Symbol symbol;
		exprID e;
	} voici;
	exprID e;
	union {
		typeID cast;
		Symbol member;
		exprID eelse;
		typeID result_type;
	} voila;
};

struct exprtree
{
	subexpr *ee;
	int ne;
	exprID first;
};

extern exprtree	CExpr;
extern int	last_result;
extern subexpr	*&ee;
extern int	&NeTop;
extern NormPtr	ExpressionPtr;
extern typeID	typeof_expression ();
extern int	cc_int_expression ();

extern struct lrt {
	int base;
	int spec [MSPEC];
} last_result_type;

//
// expand initializer
//

class dcle
{
	struct {
		int p;
		bool marked;
		int c, max;
		int s;
	} nests [30];
	int ni;
	Symbol *dclstr;
	NormPtr p;
	void openarray ();
	void openstruct ();
	NormPtr skipbracket (NormPtr);
	bool opennest ();
	bool closenest ();
	Symbol pexpr [30];
   public:
	dcle (Symbol);
	bool open_bracket ();
	bool tofield ();
	bool comma ();
	bool tostruct (RegionPtr);
	bool designator (Symbol[]);
	bool close_bracket ();
	Symbol* mk_current ();
	void printexpr ();
	~dcle ();
};

//
// different behaviour of the compiler
//
extern class ncci
{
	public:
	virtual void cc_expression () = 0;
	virtual void new_function (Symbol) = 0;
	virtual void inline_assembly (NormPtr, int) = 0;
	virtual void finir () { }
} *ncc;

extern void set_compilation ();
extern void set_usage_report ();
