#ifndef _ASM_X86_MEMORY_H_
#define _ASM_X86_MEMORY_H_

#define PAGE_SIZE 0x1000
#define KERNEL_VIRTUAL_ADDRESS 0xFFFFFFFF80000000

// Unused kernel virtual range, reserved for self-tests
#define KERNEL_SELFTEST_VIRTUAL_BASE 0xFFFF900000000000ULL

// TODO: Make this better, maybe this is not enough
#define KERNEL_HEAP_SIZE 0x1000000

#endif