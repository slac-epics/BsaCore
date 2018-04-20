#ifndef BSA_DEBUGGING_H
#define BSA_DEBUGGING_H

#include <stdio.h>

// if undefined then there is no debugging at all;
// if defined then messages can be controlled at run-time
#define BSA_CORE_DEBUG 0

#ifdef BSA_CORE_DEBUG
extern "C" int   bsaCoreDebug;
extern "C" FILE *bsaCoreDebugFile;
#endif

#define BSA_CORE_DEBUG_THREADS  (1<<0)
#define BSA_CORE_DEBUG_CHANNEL  (1<<1)
#define BSA_CORE_DEBUG_PATTERN  (1<<2)
#define BSA_CORE_DEBUG_CORE     (1<<3)

#ifdef BSA_CORE_DEBUG
#define BSA_CORE_DBG(fac, msg...) \
	do { \
		if ( (bsaCoreDebug & fac) ) { \
			fprintf(bsaCoreDebugFile, msg); \
		} \
	} while (0)
#else
#define BSA_CORE_DBG(fac, msg...) \
	do {} while (0)
#endif

#endif
