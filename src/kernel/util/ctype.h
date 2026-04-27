// Tiny character classification helpers used by terminal-facing code. Keeping
// this separate from memory primitives avoids making byte-copy users depend on
// console parsing concerns.
#ifndef OS1_KERNEL_UTIL_CTYPE_H
#define OS1_KERNEL_UTIL_CTYPE_H

// Return true for printable ASCII bytes accepted by the current text console.
inline int isprint(char c)
{
	return c >= ' ';
}

#endif // OS1_KERNEL_UTIL_CTYPE_H