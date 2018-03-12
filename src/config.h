#ifdef __GNUC__
#ifndef alloca
#define alloca __builtin_alloca
#endif
#else
#ifdef __FreeBSD__
#include <stdlib.h>
#else
#include <alloca.h>
#endif
#endif

#define NCC_VERSION "2.7"
#define GNU_VIOLATIONS
#define LABEL_VALUES
#define OUTPUT_EXT ".nccout"
#define NOGNU "/usr/include/nognu"
#define NCC_INFILE_KEY "ncc-key"
#define FAKE_VARIABLE_ARRAYS
#define NCC_ISOC99
#define PARSE_ARRAY_INITIALIZERS

// If you have linux with /dev/shm mounted
// try this one -- performance boost
//#define PREPROCESSOR_OUTPUT "/dev/shm/NCC.i"
#define PREPROCESSOR_OUTPUT "NCC.i"

