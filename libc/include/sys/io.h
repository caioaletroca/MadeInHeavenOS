#ifndef _SYS_IO_H_
#define _SYS_IO_H_ 1

#include <sys/cdefs.h>

__BEGIN_DECLS

static inline unsigned char inb(unsigned short port) {
    unsigned char v;
    __asm__ __volatile__ ("in %%dx %%al" : "=a" (v) : "d" (port));
    return v;
}

static inline void outb(unsigned char value, unsigned short port) {
    __asm__ __volatile__ ("out %%al, %%dx" : : "a" (value), "d" (port));
}


__END_DECLS

#endif