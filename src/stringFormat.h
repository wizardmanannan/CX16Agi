#ifndef _STRING_FORMAT_H_
#define _STRING_FORMAT_H_

#include <stddef.h>
#include "general.h"

/* Expand AGI format codes (%v variable, %0 object, %g global message,
** %w input word, %s string variable, %m current-logic message) into a
** caller-owned, banked destination. %v and %s use zero-based indices;
** %0, %g, %w, and %m use one-based indices. A backslash escapes the next
** character. Cyclic references and expansions deeper than eight active source
** strings are omitted. The destination is terminated when dstMax is nonzero.
** Uses resident workspace; call from foreground code, outside the display IRQ. */
void expandFormatCodes(const char* src, byte srcBank, char* dst, byte dstBank, size_t dstMax);

#endif
