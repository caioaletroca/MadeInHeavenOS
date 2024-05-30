#ifndef _ADDRESSES_H_
#define _ADDRESSES_H_

#include <stdint.h>
#include <stddef.h>
#include <memory.h>

typedef uintptr_t physaddr_t;
typedef void *virtaddr_t;
typedef void *kernaddr_t;

/**
 * Convert a physical address to a kernel one
 * 
 * @param address	Physical address
 * @return			Kernel address
*/
static inline kernaddr_t phys_to_kern(physaddr_t address) {
	if (address == (physaddr_t)NULL)
		return NULL;
	return (kernaddr_t)(address + KERNEL_VIRTUAL_ADDRESS);
}

#endif