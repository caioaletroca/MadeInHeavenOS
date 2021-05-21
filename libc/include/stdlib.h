#ifndef _STDLIB_H_
#define _STDLIB_H_ 1

#include <stddef.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

__attribute__((__noreturn__))
void abort(void);
void *malloc(size_t size);

__END_DECLS

#endif