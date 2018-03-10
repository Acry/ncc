//
//  "analyse *this"
// This is a sample dummy program which demonstrates how can
// ncc study pointers to functions to report a nicely connected
// call graph.
//
typedef int (*initcall_t)(void);

int init_ext2_fs (void)
{}

static initcall_t __init_module = init_ext2_fs;

struct fs_callbacks {
	int (*open_file)();
	int (*close_file)();
	int (*read_bytes)();
	int (*write_bytes)();
	struct fs_callbacks *next;
} FileSystems [10];

int open_ext2 () {}
int close_ext2 () {}
int read_ext2 () {}
int write_ext2 () {}
int open_ffs () {}
int close_ffs () {}
int read_ffs () {}
int write_ffs () {}

struct fs_callbacks FS = {
	close_file: close_ext2,
	open_file: open_ext2,
	read_bytes: read_ext2,
	write_bytes: write_ext2
};

struct redirector {
	int (*foo)();
} R = { FS.read_bytes };

int main ()
{
	struct fs_callbacks *f = &FileSystems [0];
	f->open_file = open_ffs;
	f->close_file = close_ffs;
	f->read_bytes = read_ffs;
	f->write_bytes = write_ffs;
	application ();
}

void application ()
{
	void *p;

	FileSystems [2].open_file ();
	FileSystems [3].next->read_bytes ();	
	(FileSystems [2].next)->write_bytes ();
	((struct fs_callbacks*)p)->next->close_file ();
}
