/******************************************************************************
	Interger Key Binary Tree.
	O(1)
******************************************************************************/

class intNode;

class intTree
{
friend	class intNode;
static	unsigned int Query;
static	intNode **FoundSlot;
   public:
	intNode *root;
	int cnt;
	intTree		();
	intNode*	intFind		(unsigned int);
};

class intNode
{
friend	class intTree;
friend	class foreach_intNode;
friend	void enter_intNode (intNode*);
	void addself	(intTree*);
   protected:
	intNode *less, *more;
   public:
	unsigned int Key;
	intNode (intTree*);
	void intRemove	(intTree*);
	~intNode ();
};
