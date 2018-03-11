/*
 run ncc on this file, with : ncc farg.c
 
 to see how pointer-to-function function arguments are reported.

 The new thing is the new output directive 'R'
 
 R: A B
 
 Should be interpreted by nccnav that the callers of 'A' do not
 really call 'A' but 'B'. This is a "replacement" directive.
 
*/

int f1(){}
int f2(){}
int f3(){}
int f4(){}

struct CALLBACK {
	int (*fp) ();
} OBJ;

int test ()
{
	OBJ.fp ();
}

int foo (int (*f)())
{
	f ();
}

int bar (int (*a)(), int (*b)())
{
	OBJ.fp = a;
	foo (a);
	b ();
}

int main ()
{
	bar (f1, f2);
	bar (f3, f2);
	bar (f4, 0);
	// this is not implemented
	//bar (OBJ.fp, 0);
}

//
// sample qsort
//

int strcmp (char*, char*);

int qsort (void *v, int n, int (*comp)(void*, void*))
{
	qsort (v/2, n/2, comp);
	comp (v[0], v [1]);
}

void do_sort ()
{
	char v [100];
	qsort (v, 100, strcmp);
}
