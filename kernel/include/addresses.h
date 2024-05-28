#ifndef _ADDRESSES_H_
#define _ADDRESSES_H_

#include <stdint.h>
#include <stddef.h>
#include <memory.h>

typedef uintptr_t physaddr_t;
typedef void *virtaddr_t;
typedef void *kernaddr_t;

static inline kernaddr_t phys_to_kern(physaddr_t p) {
	if (p == (physaddr_t)NULL)
		return NULL;
	return (kernaddr_t)(p + KERNEL_VIRTUAL_ADDRESS);
}

#endif