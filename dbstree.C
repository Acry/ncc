/******************************************************************************
	dbstree.C 

	Dynamic Binary Search Tree used throughout the entire project.

	A dbsNode applied as a parent class to a specific class will provide
	transparent storage in a dbstree. As a result no duplicates may be
	stored and locating stored data is performed in less than 32 steps.

	Check dbstree.tex for technical information on this tree.

*****************************************************************************/

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "dbstree.h"

//***************************************************************
// A tree of case sensitive strings -- char *Name
//***************************************************************

char *dbsNodeStr::Query;

int dbsNodeStr::compare (dbsNodeStr *n)
{
	return strcmp (Name, n->Name);
}

int dbsNodeStr::compare ()
{
	return strcmp (Name, Query);
}

dbsNodeStr::dbsNodeStr ()
{
	Name = Query;
}

dbsNodeStr::~dbsNodeStr ()
{ }
