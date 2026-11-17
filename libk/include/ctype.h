#ifndef _CTYPE_H_
#define _CTYPE_H_

// Test for black character
#define isspace(c) ((int) ((c) == ' ' || (c) == '\t'))

// Test for a decimal digit
#define isdigit(c) ((int) ((c) >= '0' && (c) <= '9'))

#endif