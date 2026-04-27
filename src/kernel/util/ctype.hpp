// Tiny character classification helpers used by terminal-facing code. Keeping
// this separate from memory primitives avoids making byte-copy users depend on
// console parsing concerns.
#pragma once

// Return true for printable ASCII bytes accepted by the current text console.
inline int isprint(char c)
{
	return c >= ' ';
}

