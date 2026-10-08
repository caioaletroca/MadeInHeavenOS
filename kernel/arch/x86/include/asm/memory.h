#ifndef _ASM_X86_MEMORY_H_
#define _ASM_X86_MEMORY_H_

#define PAGE_SIZE 0x1000
#define KERNEL_VIRTUAL_ADDRESS 0xFFFFFFFF80000000

// Unused kernel virtual range, reserved for self-tests
#define KERNEL_SELFTEST_VIRTUAL_BASE 0xFFFF900000000000ULL

// Physical range [0, KERNEL_DIRECT_MAP_SIZE) is mapped at KERNEL_VIRTUAL_ADDRESS
#define KERNEL_DIRECT_MAP_SIZE 0x40000000ULL

// User half of the address space: [USER_BASE, USER_TOP)
#define USER_BASE 0x0000000000400000ULL      // First user page (leaves 0 unmapped for NULL)
#define USER_STACK_TOP 0x00007FFFFFFFF000ULL // Initial user stack pointer (grows down)
#define USER_TOP 0x0000800000000000ULL       // End of the canonical lower half
#define USER_STACK_SIZE (16 * PAGE_SIZE)     // 16 pages for the user stack

#endif