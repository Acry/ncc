/******************************************************************************

	The Integer Tree

	We want to store things whos comparison key is an integer value.

	Whether a node is less or more compared to another node will be
	just a matter of the n-th bit of the integer, where n the current
	depth. For example the Least Significant Bit will be used to
	specify less (0) or more (1) for the comparison with the root node.

	Each integer therefore carries its tree path (0 - go less, 1 - go more)

	For any possible order of the nodes the tree will NEVER exceed the
	depth of 33 (for 32-bit integers).

	Even though it could be unbalanced, its logarithmic unbalance.
	We will always be able to find something in less than 32 steps.
		O(32)=O(1) < O(log(n)) < O(n) < O(n^2) < O(\infty)

	Moverover, if the numbers are in a range 0..2^n the depth will
	always be <= n+1. For a database with 65536 ID-elements, finds will
	be a matter of 17 steps.

	In order to remove an element from the tree any node can replace
	the node to be removed as long as:
		- It is below it
		- It is a terminal node
	Because it has the same path and no other node's postition is
	changed in the tree.

******************************************************************************/
/**************************************************************************
 Copyright (C) 2000, 2001, 2002 Stelios Xanthakis
**************************************************************************/

#include <stdio.h>

#include "inttree.h"

unsigned int intTree::Query;
intNode **intTree::FoundSlot;

intTree::intTree ()
{
	cnt = 0;
	root = NULL;
	FoundSlot = NULL;
}

intNode::~intNode ()
{
	if (less) delete less;
	if (more) delete more;
}

intNode *intTree::intFind (unsigned int q)
{
	Query = q;

	intNode *n;

	if (!(n = root)) {
		FoundSlot = &root;
		return NULL;
	}

	FoundSlot = NULL;

	for (unsigned int bt = 1; bt; bt *= 2) {
		if (n->Key == q) return n;
		if (q & bt)
			if (n->less) n = n->less;
			else {
				FoundSlot = &n->less;
				return NULL;
			}
		else
			if (n->more) n = n->more;
			else {
				FoundSlot = &n->more;
				return NULL;
			}
	}

	fprintf (stderr, "intTree FUBAR. Segmentation Fault. sorry\n");
	return NULL;
}

intNode::intNode (intTree *i)
{
	if (i->FoundSlot) addself (i);

	less = more = NULL;
}

void intNode::addself (intTree *i)
{
	*i->FoundSlot = this;
	++i->cnt;
	i->FoundSlot = NULL;
	Key = i->Query;
}

void intNode::intRemove (intTree *i)
{
	unsigned int isroot, bt = 0;
	intNode *n = i->root;

	if (!(isroot = n == this))
		for (bt = 1; bt; bt *= 2)
			if (Key & bt)	// avoid braces like hell
				if (n->less != this) n = n->less;
				else break;
			else		// yes but why?
				if (n->more != this) n = n->more;
				else break;

	if (!less && !more)
		if (isroot) i->root = NULL;
		else
			if (Key & bt) n->less = NULL;
			else n->more = NULL;
	else {
		intNode *r = this, *rp = NULL;
		while (r->more || r->less) {
			rp = r;
			r = (r->more) ? r->more : r->less;
		}
		if (isroot) i->root = r;
		else
			if (Key & bt) n->less = r;
			else n->more = r;
		if (rp->more == r) rp->more = NULL;
		else rp->less = NULL;
		r->more = more;
		r->less = less;
	}

	i->cnt--;
	less = more = NULL;
}
