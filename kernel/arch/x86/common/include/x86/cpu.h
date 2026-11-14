#ifndef _ARCH_X86_CPU_H_
#define _ARCH_X86_CPU_H_

#include <stdint.h>

// RFLAGS bits (Intel SDM Vol. 1, 3.4.3)
#define RFLAGS_CF (1ULL << 0)
#define RFLAGS_RESERVED_1 (1ULL << 1) // Always reads as 1
#define RFLAGS_IF (1ULL << 9)         // Interrupt enable
#define RFLAGS_DF (1ULL << 10)

// Initial RFLAGS for a new kernel thread: interrupts enabled
#define RFLAGS_KERNEL_THREAD (RFLAGS_RESERVED_1 | RFLAGS_IF)

// Initial RFLAGS for a user thread: interrupts enabled, IOPL 0 (no cli/hlt/in/out)
#define RFLAGS_USER_THREAD (RFLAGS_RESERVED_1 | RFLAGS_IF)

// SysV x86-64 ABI: RSP must be 16-byte aligned before a `call`
#define ABI_STACK_ALIGNMENT 16

// Model-specific registers (Intel SDM Vol. 4)
#define MSR_EFER 0xC0000080          // Extended feature enable
#define EFER_LONG_MODE (1ULL << 8)   // LME, set in boot.S
#define EFER_NO_EXECUTE (1ULL << 11) // NXE: page entries' bit 63 means no-execute

// CPUID leaf 0x80000001 (boot.S already required it, for long mode): EDX feature bits
#define CPUID_EXTENDED_FEATURES 0x80000001
#define CPUID_EXTENDED_EDX_NO_EXECUTE (1u << 20)

/**
 * Ask the CPU about itself: the leaf picks the question, the four registers hold the answer.
 */
static inline void cpuid(uint32_t leaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
    __asm__ __volatile__("cpuid" : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx) : "a"(leaf), "c"(0));
}

/**
 * Read a model-specific register: 64 bits, delivered split in EDX:EAX.
 */
static inline uint64_t msr_read(uint32_t msr)
{
    uint32_t low, high;
    __asm__ __volatile__("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

/**
 * Write a model-specific register, from EDX:EAX.
 */
static inline void msr_write(uint32_t msr, uint64_t value)
{
    __asm__ __volatile__("wrmsr" ::"c"(msr), "a"((uint32_t)value), "d"((uint32_t)(value >> 32)) : "memory");
}

#endif // _ARCH_X86_CPU_H_