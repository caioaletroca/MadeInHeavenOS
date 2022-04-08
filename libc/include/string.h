#ifndef _STRING_H_
#define _STRING_H_ 1

#include <stddef.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

void *memcpy(void *dest, const void * src, size_t n);
void *memmove(void *str1, const void *str2, size_t n);
void *memset(void *str, int c, size_t n);
char *strcpy(char *dest, const char* src);
size_t strlen(const char*);

__END_DECLS

#endif