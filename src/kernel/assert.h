#ifndef _assert_h_
#define _assert_h_

#include "debug.h"

#ifndef NDEBUG
#define assert(x)		\
	if (!(x)){ \
		debug(__FILE__)(":")(__LINE__); \
		debug(" assertion failed: ")(#x)(); \
		do{ \
		asm volatile("cli"); \
		asm volatile("hlt"); \
		}while(true); \
	}
#else
#define assert(x)		// do nothing
#endif

// static_assert(x) will generate a compile-time error if 'x' is false.
#define static_assert(x)	switch (x) case 0: case (x):

#endif // _assert_h_
