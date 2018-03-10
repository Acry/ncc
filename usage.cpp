/*****************************************************************************
$	C-flow and data usage analysis.
$
$	Stripped-down version of ccexpr. expressions are compiled but
$	without producing bytecode assembly. just inform about
$	function calls, use of global variables and use of members
$	of structures
*****************************************************************************/
#include <stdio.h>
#include <assert.h>

#include "global.h"
#include "inttree.h"

bool infuncs = false;

static intTree printed;
static intTree printed_function;

//***********************************************************
// Output Formats & Text
//***********************************************************

static char *txt_func =		"\nD: %s()\n";
static char *txt_fcall =	"F: %s()\n";
static char *txt_virt =		"F: (*virtual)()\n";
static char *txt_error =	"F: NCC:syntax_error()\n";
static char *txt_gvar_r =	"g: %s\n";
static char *txt_evar_r =	"g: %s\n";
static char *txt_gvar_ra =	"g: %s[]\n";
static char *txt_evar_ra =	"g: %s[]\n";
static char *txt_gvar =		"G: %s\n";
static char *txt_gvar_a =	"G: %s[]\n";
static char *txt_evar =		"G: %s\n";
static char *txt_evar_a =	"G: %s[]\n";
static char *txt_memb_r =	"s: %s.%s\n";
static char *txt_memb =		"S: %s.%s\n";
static char *txt_memb_ra =	"s: %s.%s[]\n";
static char *txt_memb_a =	"S: %s.%s[]\n";
static char *txt_fpcall =	"F: *%s()\n";
static char *txt_fcallback =	"F: *%s.%s()\n";
static char *farg_named_call =	"F: %s/%s()\n";
static char *farg_call_redirect = "\nD: %s/%s()\nF: *%s()__farg%i()\n";
static char *txt_argval =	"R: *%s()__farg%i() %s()\n";
static char *txt_argvalf =	"R: *%s()__farg%i() *%s()\n";
static char *txt_argvalfl =	"R: *%s()__farg%i() %s/%s()\n";
static char *txt_argvalargval =	"R: *%s()__farg%i() *%s()__farg%i()\n";
static char *txt_membargval =	"R: *%s()__farg%i() *%s.%s()\n";

//********************************************

char *structname (Symbol s)
{
	return s == -1 ? (char*) "{anonymous}" : C_Syms [SYMBOLID (s)];
}

void report (Symbol s, int frame, bool wrt, bool arr)
{
	if (INGLOBAL || !infuncs) return;
	if (!multiple) {
		if (printed.intFind ((wrt ? 1000000 : 0) + (arr ? 2000000 : 0) +
		    frame * 100000 + s))
			return;
		else new intNode (&printed);
	}

	if (frame == -1 ) PRINTF (wrt ? (arr ? txt_evar_a : txt_evar) :
				 arr ? txt_evar_ra : txt_evar_r, C_Syms [SYMBOLID (s)]);
	else if (frame == 0 ) PRINTF (wrt ? (arr ? txt_gvar_a : txt_gvar) :
				 arr ? txt_gvar_ra : txt_gvar_r, C_Syms [SYMBOLID (s)]);
	else PRINTF (wrt ? (arr ? txt_memb_a : txt_memb) : arr ? txt_memb_ra : txt_memb_r, 
				 structname (struct_by_name (frame)), C_Syms [SYMBOLID (s)]);
	if (arr) report (s, frame, false, false);
}

static Symbol current_function;

void newfunction (Symbol s)
{
	current_function = s;
	PRINTF (txt_func, C_Syms [SYMBOLID (s)]);
	if (printed.root) {
		delete printed.root;
		printed.root = NULL;
	}
	if (printed_function.root) {
		delete printed_function.root;
		printed_function.root = NULL;
	}
}

static bool local_object (Symbol s)
{
	lookup_object L (s);
	return L.FRAME > 0;
}

void report_call (Symbol s, bool pointer)
{
        Symbol func = pointer && local_object (s) ? current_function : 0;
	if (!multiple)
		if (printed_function.intFind (s + func)) return;
		else new intNode (&printed_function);
	if (func)
		PRINTF (farg_named_call, C_Syms [SYMBOLID (func)], C_Syms [SYMBOLID (s)]);
	else
		PRINTF (pointer ? txt_fpcall : txt_fcall, C_Syms [SYMBOLID (s)]);
}

void report_callback (Symbol rec, Symbol s)
{
	PRINTF (txt_fcallback, structname (rec), C_Syms [SYMBOLID (s)]);
}

struct argnameassign { Symbol s1, s2; int argi; };
static earray<argnameassign> arg_names;

static void stor_named_conv (Symbol f, Symbol a, int n)
{
	int i = arg_names.alloc ();
	arg_names.x [i].s1 = SYMBOLID (f);
	arg_names.x [i].s2 = SYMBOLID (a);
	arg_names.x [i].argi = n;
}

void report_named_convert ()
{
	int i;
	if (arg_names.nr) PRINTF ("\n");
	for (i = 0; i < arg_names.nr; i++)
		PRINTF (farg_call_redirect, C_Syms [arg_names.x [i].s1], C_Syms [arg_names.x [i].s2],
			C_Syms [arg_names.x [i].s1], arg_names.x [i].argi);
}

void report_fargcall (int a, Symbol s)
{
	stor_named_conv (current_function, s, a);
	PRINTF (farg_named_call, C_Syms [SYMBOLID (current_function)], C_Syms [SYMBOLID (s)]);
}

struct fptrassign { Symbol s1, s2, m1, m2, fn, fn1; bool sptr, a1, a2; };
static earray<fptrassign> fptr_assignments;

// some help with the arguments
// 's1' and 's2' are the names of the functions assigned.
//   For example 'funcptr=foo', s1=funcptr and s2=foo
// 'm1' and 'm2' are useful for the case one of the two is a member
//   For example 'obj->funcptr=foo' or 'obj->funcptr=x.f'
// if 'fn' is present then the second call is converted to 'fn/s2()'
// if 'fn1' is present then the first call is of 'f1/s1()' form
//  This is all too hairy but this function is the result of hacking
//  and extending ncc itself.
//
void functionptr (Symbol s1, Symbol s2, bool pointer, Symbol m1, Symbol m2, Symbol fn, Symbol fn1, bool a1, bool a2)
{
	int i;
	for (i = 0; i < fptr_assignments.nr; i++) {
		fptrassign fp = fptr_assignments.x [i];
		if (fp.s1 == s1 && fp.s2 == s2
		&& fp.fn == fn && fp.fn1 == fn1
		&& fp.a1 == a1 && fp.a2 == a2
		&& fp.m1 == m1 && fp.m2 == m2) return;
	}

	i = fptr_assignments.alloc ();
	fptr_assignments.x [i].s1 = s1;
	fptr_assignments.x [i].s2 = s2;
	fptr_assignments.x [i].m1 = m1;
	fptr_assignments.x [i].m2 = m2;
	fptr_assignments.x [i].sptr = pointer;
	fptr_assignments.x [i].fn = fn;
	fptr_assignments.x [i].a1 = a1;
	fptr_assignments.x [i].a2 = a2;
	fptr_assignments.x [i].fn1 = fn1;
}

void report_fptrs ()
{
	int i;
	for (i = 0; i < fptr_assignments.nr; i++) {
		fptrassign fp = fptr_assignments.x [i];

		PRINTF ("\nD: ");
		if (fp.m1 == -1 && fp.fn1 != -1)
			PRINTF ("%s/", C_Syms [SYMBOLID (fp.fn1)]);
		else if (fp.m1 != -1)
			PRINTF ("*%s.", structname (fp.s1));
		else PRINTF ("*");
		PRINTF ("%s", C_Syms [fp.m1 == -1 ? SYMBOLID (fp.s1) : SYMBOLID (fp.m1)]);
		if (fp.a1) PRINTF ("[]");
		PRINTF ("()\n");
		
		PRINTF ("F: ");
		if (fp.m2 == -1 && fp.fn != -1)
			PRINTF ("%s/", C_Syms [SYMBOLID (fp.fn)]);
		else if (fp.m2 == -1 && fp.fn == -1 && fp.sptr)
			PRINTF ("*");
		else if (fp.m2 != -1)
			PRINTF ("*%s.", structname (fp.s2));
		PRINTF ("%s", C_Syms [fp.m2 == -1 ? SYMBOLID (fp.s2) : SYMBOLID (fp.m2)]);
		if (fp.a2) PRINTF ("[]");
		PRINTF ("()\n");
	}
}

void report_virtual ()
{
	PRINTF (txt_virt);
}

void report_error ()
{
	PRINTF (txt_error);
}

struct fargsave { Symbol f, f2, s, lf; int ia, ia2; bool ptr; };
static earray<fargsave> farg_saves;

void report_fargval (Symbol f, int ia, Symbol f2, int ia2, bool ptr = false)
{
	int i = farg_saves.alloc ();
	farg_saves.x [i].f = f;
	farg_saves.x [i].f2 = f2;
	farg_saves.x [i].ia = ia;
	farg_saves.x [i].ia2 = ia2;
	farg_saves.x [i].ptr = ptr;
	farg_saves.x [i].lf = farg_saves.x [i].s = -1;
}

void report_fargval_memb (Symbol f, int ia, Symbol s, Symbol m)
{
	int i = farg_saves.alloc ();
	farg_saves.x [i].f = f;
	farg_saves.x [i].f2 = m;
	farg_saves.x [i].ia = ia;
	farg_saves.x [i].s = s;
	farg_saves.x [i].lf = -1;
}

void report_fargval_locf (Symbol f, int ia, Symbol f2, Symbol l)
{
	int i = farg_saves.alloc ();
	farg_saves.x [i].f = f;
	farg_saves.x [i].f2 = f2;
	farg_saves.x [i].ia = ia;
	farg_saves.x [i].lf = l;
	farg_saves.x [i].s = -1;
}

void report_argument_calls ()
{
	int i;
	if (farg_saves.nr) PRINTF ("\n");
	for (i = 0; i < farg_saves.nr; i++)
		if (farg_saves.x [i].s != -1)
		PRINTF (txt_membargval, C_Syms [SYMBOLID (farg_saves.x [i].f)],
			farg_saves.x [i].ia, C_Syms [SYMBOLID (farg_saves.x [i].s)],
			C_Syms [SYMBOLID (farg_saves.x [i].f2)]);
		else if (farg_saves.x [i].lf != -1)
		PRINTF (txt_argvalfl, C_Syms [SYMBOLID (farg_saves.x [i].f)],
			farg_saves.x [i].ia, C_Syms [SYMBOLID (farg_saves.x [i].lf)],
			C_Syms [SYMBOLID (farg_saves.x [i].f2)]);
		else if (farg_saves.x [i].ia2 == -1)
		PRINTF (farg_saves.x [i].ptr ? txt_argvalf : txt_argval,
			C_Syms [SYMBOLID (farg_saves.x [i].f)],
			farg_saves.x [i].ia,  C_Syms [SYMBOLID (farg_saves.x [i].f2)]);
		else
		PRINTF (txt_argvalargval, C_Syms [SYMBOLID (farg_saves.x [i].f)],
			farg_saves.x [i].ia,  C_Syms [SYMBOLID (farg_saves.x [i].f2)],
			farg_saves.x [i].ia2);
}

class ccsub_small
{
inline	void fconv ();
inline	void iconv ();
inline	void settype (int);
inline	void lvaluate ();
inline	void copytype (ccsub_small&);
inline	void degrade (ccsub_small&);
inline	void arithmetic_convert (ccsub_small&, ccsub_small&);
inline	bool arithmetic ();
inline	bool voidp ();
inline	bool structure ();
inline	void assign_convert (ccsub_small&);
	bool op1return;
static	ccsub_small op1;
inline	void cc_binwconv (ccsub_small&, ccsub_small&);
inline	void cc_addptr (ccsub_small&, ccsub_small&);
	bool lv, modify, array;
	void cc_fcall (exprID);
	void cc_prepostfix (exprID);
inline	void cc_terminal (exprID);
inline	void cc_dot (exprID);
inline	void cc_array (exprID);
inline	void cc_star (exprID);
inline	void cc_addrof (exprID);
inline	void cc_ecast (exprID);
inline	void cc_usign (exprID);
inline	void cc_nbool (exprID);
inline	void cc_compl (exprID);
inline	void cc_add (exprID, bool = false);
	void cc_sub (exprID, bool = false);
	void cc_muldiv (exprID, bool = false);
	void cc_bintg (exprID, bool = false);
inline	void cc_cmp (exprID);
	void cc_bool (exprID);
	void cc_conditional (exprID, exprID = -2);
	void cc_assign (exprID);
	void cc_oassign (exprID);
inline	void cc_compound (exprID);
   public:
inline	ccsub_small (exprID, bool = false, bool = false);
	ccsub_small () {}
	ccsub_small (typeID, char*);

	int base, spec [MSPEC];
};

ccsub_small ccsub_small::op1;

void ccsub_small::cc_terminal (exprID ei)
{
	subexpr e = ee [ei];
	lookup_object ll (e.voici.symbol);
	if (ll.enumconst) {
		settype (S_INT);
		return;
	}
	base = ll.base;
	intcpy (spec, ll.spec);
	if (ll.FRAME <= 0)
		report (e.voici.symbol, ll.FRAME, modify, array);
	lvaluate ();
}

void ccsub_small::cc_addrof (exprID ei)
{
	ccsub_small o (ee [ei].voici.e);

	base = o.base;
	if (o.lv || o.structure ()) {
		spec [0] = '*';
		intcpy (&spec [1], o.spec);
	} else if (o.spec [0] != -1)
		intcpy (spec, o.spec);
	else half_error ("&address_of not addressable");
}

void ccsub_small::cc_star (exprID e)
{
	ccsub_small o (ee [e].voici.e);
	degrade (o);
	lvaluate ();
}

void ccsub_small::cc_array (exprID ei)
{
	ccsub_small o1 (ee [ei].voici.e, modify, true), o2 (ee [ei].e);
	cc_addptr (o1, o2);
	degrade (*this);
	lvaluate ();
}

void ccsub_small::cc_dot (exprID ei)
{
	subexpr e = ee [ei];
	ccsub_small o (e.voici.e, modify);
	lookup_member lm (e.voila.member, o.base);
	base = lm.base;
	intcpy (spec, lm.spec);
	report (e.voila.member, o.base, modify, array);
	lvaluate ();
}

void ccsub_small::cc_ecast (exprID ei)
{
	subexpr e = ee [ei];
	ccsub_small o (e.voici.e), pseudo (e.voila.cast, "");
	o.assign_convert (pseudo);
	copytype (o);
	*this = o;
}

void ccsub_small::cc_usign (exprID ei)
{
	subexpr e = ee [ei];
	ccsub_small o (e.voici.e);
	copytype (o);
}

void ccsub_small::cc_nbool (exprID ei)
{
	ccsub_small o (ee [ei].voici.e);
	(void) o;
	settype (S_INT);
}

void ccsub_small::cc_compl (exprID ei)
{
	ccsub_small o (ee [ei].voici.e);
	(void) o;
	settype (S_INT);
}

void ccsub_small::cc_bintg (exprID ei, bool m1)
{
	subexpr e = ee [ei];
	ccsub_small o1 (e.voici.e, m1), o2 (e.e);
	(void) o2;
	if (op1return) op1 = o1;
	settype (S_INT);
}

void ccsub_small::cc_muldiv (exprID ei, bool m1)
{
	subexpr e = ee [ei];
	ccsub_small o1 (e.voici.e, m1), o2 (e.e);
	if (op1return) op1 = o1;
	cc_binwconv (o1, o2);
}

void ccsub_small::cc_prepostfix (exprID ei)
{
	subexpr e = ee [ei];
	ccsub_small o (e.voici.e, true);
	copytype (o);
}

void ccsub_small::cc_add (exprID ei, bool m1)
{
	subexpr e = ee [ei];
	ccsub_small o1 (e.voici.e, m1), o2 (e.e);

	if (op1return) op1 = o1;
	if (o1.arithmetic () && o2.arithmetic ())
		cc_binwconv (o1, o2);
	else    cc_addptr (o1, o2);
}

void ccsub_small::cc_sub (exprID ei, bool m1)
{
	subexpr e = ee [ei];
	ccsub_small o1 (e.voici.e, m1), o2 (e.e);

	if (op1return) op1 = o1;

	if (o1.arithmetic () && o2.arithmetic ()) {
		cc_binwconv (o1, o2);
		return;
	}

	if (!o1.arithmetic () && !o2.arithmetic ()) settype (S_INT);
	else copytype (o1);
}

void ccsub_small::cc_cmp (exprID ei)
{
	subexpr e = ee [ei];
	ccsub_small o1 (e.voici.e), o2 (e.e);

	if (o1.arithmetic () && o1.arithmetic ())
		arithmetic_convert (o1, o2);
	settype (S_INT);
}

void ccsub_small::cc_bool (exprID ei)
{
	subexpr e = ee [ei];
	ccsub_small o1 (e.voici.e);
	ccsub_small o2 (e.e);
	(void) o1;
	(void) o2;
	settype (S_INT);
}

void ccsub_small::cc_conditional (exprID ei, exprID args)
{
	subexpr e = ee [ei];
	if (args != -2) {
		//   we have the:
		// (i ? a : b) ()
		//   case. Convert it to:
		// (i ? a () : b ())
		ee [NeTop] = ee [e.e];
		ee [e.e].action = FCALL;
		ee [e.e].voici.e = NeTop++;
		ee [e.e].e = args;
		ee [NeTop] = ee [e.voila.eelse];
		ee [e.voila.eelse].action = FCALL;
		ee [e.voila.eelse].voici.e = NeTop++;
		ee [e.voila.eelse].e = args;
	}
	ccsub_small o (e.voici.e);
	ccsub_small o1 (e.e);
	ccsub_small o2 (e.voila.eelse);
	ccsub_small *po = e.e==-1 ? &o : &o1;
	(void) o1;
	(void) o2;
	(void) o;
	if (po->arithmetic () || po->voidp ()) copytype (o2);
	else copytype (*po);
}

static void callbacks (exprID e1, exprID e2)
{
	Symbol s1, s2, m1 = -1, m2 = -1, fn = -1, fn1 = -1;
	bool ptrs = false;
	int a1 = 0, a2 = 0;

	again1: if (ee [e1].action == SYMBOL) {
		--a1;
		s1 = ee [e1].voici.symbol;
		if (local_object (s1))
			fn1 = current_function;
	} else if (ee [e1].action == MEMB) {
		ccsub_small os (ee [e1].voici.e);
		--a1;
		s1 = struct_by_name (os.base);
		m1 = ee [e1].voila.member;
	} else if (ee [e1].action == ARRAY || ee [e1].action == PTRIND) {
		e1 = ee [e1].voici.e;
		a1 = 2;
		goto again1;
	} else return;

	bool casted = false;
	again: if (ee [e2].action == SYMBOL) {
		--a2;
		s2 = ee [e2].voici.symbol;
		if ((ptrs = !have_function (s2))) {
			lookup_function lf (s2, false);
			if (!lf.found) return;
			if (lf.ARGFUNC) {
				fn = current_function;
//				fn = s2;
//				s2 = current_function;
				stor_named_conv (s2, fn, lf.displacement);
			} else if (lf.fptr && lf.FRAME > 0) {
//				fn = s2;
//				s2 = current_function;
				fn = current_function;
			}
		}
	} else if (ee [e2].action == MEMB) {
		--a2;
		ccsub_small os (ee [e2].voici.e);
		s2 = struct_by_name (os.base);
		m2 = ee [e2].voila.member;
	} else if (casted = ee [e2].action == CAST) {
		e2 = ee [e2].voici.e;
		goto again;
	} else if (ee [e2].action == ARRAY || ee [e2].action == PTRIND) {
		e2 = ee [e2].voici.e;
		a2 = 2;
		goto again;
	} else if (ee [e2].action == COND) {
		callbacks (e1, ee [e2].e == -1 ? ee [e2].voici.e : ee [e2].e);
		callbacks (e1, ee [e2].voila.eelse);
		return;
	} else return;

	functionptr (s1, s2, ptrs, m1, m2, fn, fn1, a1 == 1, a2 == 1);
}

void ccsub_small::cc_assign (exprID ei)
{
	subexpr e = ee [ei];
	ccsub_small o1 (e.voici.e, true), o2 (e.e);

	if (o1.spec[0] == '*' && o1.spec [1] == '(')
		callbacks (e.voici.e, e.e);

	(void) o2.lv;
	copytype (o1);
}

void ccsub_small::cc_oassign (exprID ei)
{
	op1return = true;
	switch (ee [ei].action) {
		case ASSIGNA:	cc_add (ei, true); break;
		case ASSIGNS:	cc_sub (ei, true); break;
		case ASSIGNM:
		case ASSIGND:	cc_muldiv (ei, true); break;
		case ASSIGNBA: case ASSIGNBX: case ASSIGNBO:
		case ASSIGNRS: case ASSIGNLS:
		case ASSIGNR:	cc_bintg (ei, true); break;
	}
	copytype (op1);
}

static void fargs (exprID ei, Symbol fs, int i)
{
	again: if (ee [ei].action == SYMBOL) {
		Symbol t = ee [ei].voici.symbol;
		if (have_function (t))
			report_fargval (fs, i, t, -1);
		else {
			lookup_function lf (t, false);
			if (!lf.found) return;
			if (lf.ARGFUNC)
				report_fargval (fs, i, current_function, lf.displacement);
			else if (!local_object (t))
				report_fargval (fs, i, t, -1, true);
			else
				report_fargval_locf (fs, i, t, current_function);
		}
	} else if (ee [ei].action == MEMB) {
		ccsub_small oo (ee [ei].voici.e);
		report_fargval_memb (fs, i, struct_by_name (oo.base),
				     ee [ei].voila.member);
	} else if (ee [ei].action == CAST) {
		ei = ee [ei].voici.e;
		goto again;
	}
}

void ccsub_small::cc_fcall (exprID ei)
{
	int i = 2;
	subexpr e = ee [ei];
	subexpr fe = ee [e.voici.e];
	Symbol fs = -1;

	if (fe.action == SYMBOL) {
		lookup_function lf (fe.voici.symbol);
		fs = fe.voici.symbol;
		if (!lf.ARGFUNC)
			report_call (fe.voici.symbol, lf.fptr);
		else
			report_fargcall (lf.displacement, fe.voici.symbol);
		base = lf.base;
		intcpy (spec, lf.spec + 2);
	} else if (fe.action == COND) {
		cc_conditional (e.voici.e, e.e);
		return;
	} else {
		ccsub_small fn (e.voici.e);

		if (fn.spec [0] != '(') {
			if (fn.spec [0] == '*' && fn.spec [1] == '(')
				i = 3;
			else half_error ("not a function");
		}
		base = fn.base;
		intcpy (spec, fn.spec + i);

		if (fe.action == MEMB) {
			ccsub_small os (fe.voici.e);
			report_callback (struct_by_name (os.base), fe.voila.member);
		} else if (fe.action == PTRIND) {
			if (ee [fe.voici.e].action == MEMB) {
				subexpr effi = ee [fe.voici.e];
				ccsub_small os (effi.voici.e);
				report_callback (struct_by_name (os.base), effi.voila.member);
			} else if (ee [fe.voici.e].action == SYMBOL) {
				lookup_function lf (ee [fe.voici.e].voici.symbol, false);
				if (!lf.ARGFUNC)
					report_call (fs = ee [fe.voici.e].voici.symbol, true);
				else
					report_fargcall (lf.displacement, ee [fe.voici.e].voici.symbol);
			}
		} else report_virtual ();
	}

	if ((ei = e.e) != -1) {
		int i;
		bool fsok = fs != -1 && fs != current_function;

		for (i = 0; ee [ei].action == ARGCOMMA; ei = ee [ei].e, i++) {
			exprID eii = ee [ei].voici.e;
			ccsub_small o (eii);
			o.lv = o.lv;
			if (fsok && o.spec [0] == '*' && o.spec [1] == '(')
				fargs (eii, fs, i);
		}
		ccsub_small o (ei);
		o.lv = o.lv;
		if (fsok && o.spec [0] == '*' && o.spec [1] == '(')
			fargs (ei, fs, i);
	}
}

void ccsub_small::cc_compound (exprID ei)
{
	base = base_of (ee [ei].voila.result_type);
	intcpy (spec, spec_of (ee [ei].voila.result_type));
}

////////////////////////////////////////////////////////////////////////////

void ccsub_small::cc_binwconv (ccsub_small &o1, ccsub_small &o2)
{
	arithmetic_convert (o1, o2);
	settype (o1.base);
}

void ccsub_small::cc_addptr (ccsub_small &o1, ccsub_small &o2)
{
	bool b2 = o2.arithmetic ();
	if (b2) {
		o2.lv = false;
		copytype (o1);
	} else {
		o1.lv = false;
		copytype (o2);
	}
}

void ccsub_small::copytype (ccsub_small &o)
{
	base = o.base;
	intcpy (spec, o.spec);
}

void ccsub_small::degrade (ccsub_small &o)
{
	base = o.base;
	if (o.spec [0] == -1) half_error ("Not a pointer");
	intcpy (spec, o.spec + (o.spec [0] == '(' ? 0 : o.spec [0] == '*' ? 1 : 2));
}

bool ccsub_small::structure ()
{
	return spec [0] == -1 && base >= 0;
}

bool ccsub_small::arithmetic ()
{
	return spec [0] == -1 && base < VOID || spec [0] == ':';
}

bool ccsub_small::voidp ()
{
	return spec [0] == '*' && base == VOID;
}

void ccsub_small::lvaluate ()
{
	lv = !(spec [0] =='[' || spec [0] ==-1 && base >=0 || spec [0] =='(');
}

void ccsub_small::settype (int b)
{
	base = b;
	spec [0] = -1;
}

void ccsub_small::assign_convert (ccsub_small &o)
{
	if (o.arithmetic ())
		if (o.base != FLOAT) iconv ();
		else fconv ();
	base = o.base;
	intcpy (spec, o.spec);
}

void ccsub_small::arithmetic_convert (ccsub_small &o1, ccsub_small &o2)
{
	if (o1.base == FLOAT || o2.base == FLOAT) {
		if (o1.base != o2.base)
			if (o1.base == FLOAT) o2.fconv ();
			else o1.fconv ();
	}
}

void ccsub_small::fconv ()
{
	settype (FLOAT);
}

void ccsub_small::iconv ()
{
	settype (S_INT);
}

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

ccsub_small::ccsub_small (exprID ei, bool mod, bool arr)
{
	modify = mod;
	array = arr;
	if (ei == -1) return;
advance:
	subexpr e = ee [ei];

	lv = false;
	op1return = false;
	switch (e.action) {
		case VALUE:
		case UVALUE:	settype (S_INT);	break;
		case FVALUE:	settype (FLOAT);	break;
		case SVALUE:	base = S_CHAR; spec [0] = '*'; spec [1] = -1;
				break;
		case AVALUE:	base = VOID; spec [0] = '*'; spec [1] = -1;
				break;
		case SYMBOL:	cc_terminal (ei);	break;
		case FCALL:	cc_fcall (ei);		break;
		case MEMB:	cc_dot (ei);		break;
		case ARRAY:	cc_array (ei);		break;
		case ADDROF:	cc_addrof (ei);		break;
		case PTRIND:	cc_star (ei);		break;
		case MMPOST: case PPPOST:
		case PPPRE:
		case MMPRE:	cc_prepostfix (ei);	break;
		case CAST:	cc_ecast (ei);		break;
		case LNEG:	cc_nbool (ei);		break;
		case OCPL:	cc_compl (ei);		break;
		case UPLUS:
		case UMINUS:	cc_usign (ei);		break;
		case SIZEOF:	settype (S_INT);	break;
		case MUL:
		case DIV:	cc_muldiv (ei);		break;
		case ADD:	cc_add (ei);		break;
		case SUB:	cc_sub (ei);		break;
		case SHR: case SHL: case BOR: case BAND: case BXOR:
		case REM:	cc_bintg (ei);	break;
		case IAND:
		case IOR:	cc_bool (ei);		break;
		case BNEQ: case CGR: case CGRE: case CLE: case CLEE:
		case BEQ:	cc_cmp (ei);	break;
		case COND:	cc_conditional (ei);	break;
		case COMPOUND_RESULT:	cc_compound (ei); break;
		case COMMA: {
			ccsub_small o (e.voici.e);
			ei = e.e;
			(void) o;
			goto advance;
		}
		default:
			if (e.action == '=') cc_assign (ei);
			else cc_oassign (ei);
	}
}


ccsub_small::ccsub_small (typeID t, char*)
{
	base = base_of (t);
	intcpy (spec, spec_of (t));
}

//
//
//
//

class ncci_usage : public ncci
{
	public:
	void cc_expression ();
	void new_function (Symbol);
	void inline_assembly (NormPtr, int);
	void finir ();
};

void ncci_usage::cc_expression ()
{
	try {
		if (CExpr.first != -1)  {
			ccsub_small CC (CExpr.first);
#ifdef GNU_VIOLATIONS
			last_result_type.base = CC.base;
			intcpy (last_result_type.spec, CC.spec);
#endif
		}
	} catch (EXPR_ERROR) { }
	last_result++;
}

void ncci_usage::new_function (Symbol s)
{
	newfunction (s);
}

void ncci_usage::inline_assembly (NormPtr p, int n)
{
// hoping that inline assembly won't use global variables,
// nor call functions, it is ignored (for now at least)
}

void ncci_usage::finir ()
{
	report_fptrs ();
	report_named_convert ();
	report_argument_calls ();
}

void set_usage_report ()
{
	ncc = new ncci_usage;
}
