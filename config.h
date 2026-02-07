/**
 * Miscellaneous Configuration Constants 
 */
#ifndef SPLINTER_CONFIG_H
#define SPLINTER_CONFIG_H

#include "build.h"

#define SPLINTER_VERSION "0.9.1"

// so tools can be used without many / any arguments
#define DEFAULT_BUS "splinter_debug"
#define DEFAULT_KEY "__debug"
#define DEFAULT_SLOTS 1024
#define DEFAULT_VAL_MAXLEN 4096

// un-comment to enable NUMA
//#ifndef SPLINTER_NUMA_AFFINITY
//#define SPLINTER_NUMA_AFFINITY
//#endif

// do we have the valgrind development headers?
// comment out (undefine) if not.
//#define HAVE_VALGRIND_H 1

// more can go here as needed.

#endif // SPLINTER_CONFIG_H
