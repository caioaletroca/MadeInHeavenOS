#ifndef _X86_TSS_H_
#define _X86_TSS_H_

#include <stdint.h>

// IST slots (1-based, as written in an IDT gate's ist field; 0 = no stack switch)
#define TSS_IST_NMI 1
#define TSS_IST_DOUBLE_FAULT 2

/**
 * @brief 64-bit Task State Segment (Intel SDM Vol. 3, 8.7).
 *
 * Long mode does no hardware task switching; the TSS only provides the
 * stacks the CPU switches to on interrupts.
 */
typedef struct tss
{
    uint32_t reserved0;
    uint64_t rsp[3];
    uint64_t reserved1;
    uint64_t ist[7];
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed)) tss_t;

_Static_assert(sizeof(tss_t) == 104, "TSS must be 104 bytes");

/**
 * @brief Fill the TSS and its GDT descriptor, then load the task register.
 */
void tss_init(void);

/**
 * @brief Set the kernel stack the CPU switches to when a ring 3 thread is interrupted.
 *
 * @param top Highest address (exclusive) of the current thread's kernel stack.
 */
void tss_set_kernel_stack(uintptr_t top);

#endif // _X86_TSS_H_