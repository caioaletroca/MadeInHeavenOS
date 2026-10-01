#ifndef _ARCH_X86_CPU_H_
#define _ARCH_X86_CPU_H_

// RFLAGS bits (Intel SDM Vol. 1, 3.4.3)
#define RFLAGS_CF (1ULL << 0)
#define RFLAGS_RESERVED_1 (1ULL << 1) // Always reads as 1
#define RFLAGS_IF (1ULL << 9)         // Interrupt enable
#define RFLAGS_DF (1ULL << 10)

// Initial RFLAGS for a new kernel thread: interrupts enabled
#define RFLAGS_KERNEL_THREAD (RFLAGS_RESERVED_1 | RFLAGS_IF)

// SysV x86-64 ABI: RSP must be 16-byte aligned before a `call`
#define ABI_STACK_ALIGNMENT 16

#endif // _ARCH_X86_CPU_H_