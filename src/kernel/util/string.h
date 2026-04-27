/*
 * Generic string-handling functions as defined in the C standard.
 *
 * Copyright (C) 1997 Massachusetts Institute of Technology
 * See section "MIT License" in the file LICENSES for licensing terms.
 *
 * Derived from the MIT Exokernel and JOS.
 * Adapted for PIOS by Bryan Ford at Yale University.
 */

#ifndef _string_h_
#define _string_h_

#include "stddef.h"
#include "stdint.h"

int	strlen(const char *s);
// Copy a nul-terminated string; destination must be large enough.
char *	strcpy(char *dst, const char *src);
// Copy at most `size` bytes and nul-pad when source is shorter.
char *	strncpy(char *dst, const char *src, size_t size);
// Copy with BSD-style truncation reporting.
size_t	strlcpy(char *dst, const char *src, size_t size);
// Compare two nul-terminated strings.
int	strcmp(const char *s1, const char *s2);
// Compare up to `size` bytes of two strings.
int	strncmp(const char *s1, const char *s2, size_t size);
// Find the first matching byte in a string.
char *	strchr(const char *s, char c);

// Fill memory with a byte value.
void *	memset(void *dst, int c, size_t len);
// Copy non-overlapping memory.
void *	memcpy(void *dst, const void *src, size_t len);
// Copy memory that may overlap.
void *	memmove(void *dst, const void *src, size_t len);
// Compare byte ranges.
int	memcmp(const void *s1, const void *s2, size_t len);
// Find a byte in a memory range.
void *	memchr(const void *str, int c, size_t len);

// Parse an integer from a string.
long	strtol(const char *s, char **endptr, int base);

// Return a static error string for a libc errno value.
char *	strerror(int err);

#endif// _string_h_
