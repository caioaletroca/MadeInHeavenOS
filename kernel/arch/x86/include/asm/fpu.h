#ifndef _ASM_X86_FPU_H_
#define _ASM_X86_FPU_H_

#include <stdint.h>

#define FPU_CONTROL_DEFAULT 0x037F // x87 at reset: every exception masked, round to nearest
#define FPU_MXCSR_DEFAULT 0x1F80   // SSE at reset: every exception masked, round to nearest
#define FPU_MXCSR_VALID 0xFFBF     // bits every SSE CPU accepts; fxrstor faults on others (DAZ, bit 6, left out)

/**
 * A thread's floating-point and vector registers, in the layout fxsave writes.
 * fxsave needs 16-byte alignment.
 */
typedef struct
{
    uint16_t control; // x87 control word
    uint16_t status;  // x87 status word
    uint8_t tag;      // which x87 registers hold a value (0: none)
    uint8_t reserved;
    uint16_t opcode;      // last x87 instruction
    uint64_t instruction; // its address
    uint64_t data;        // its operand's address
    uint32_t mxcsr;       // SSE rounding and masked exceptions
    uint32_t mxcsr_mask;  // written by fxsave, ignored by fxrstor
    uint8_t x87[8][16];   // st0-st7
    uint8_t xmm[16][16];  // xmm0-xmm15
    uint8_t unused[96];
} __attribute__((aligned(16))) fpu_state_t;

_Static_assert(sizeof(fpu_state_t) == 512, "fxsave writes 512 bytes");

/**
 * @brief Initialize an FPU state to the default values.
 *
 * @param state The FPU state to initialize.
 */
static inline void fpu_init(fpu_state_t *state)
{
    *state = (fpu_state_t){.control = FPU_CONTROL_DEFAULT, .mxcsr = FPU_MXCSR_DEFAULT};
}

/**
 * @brief Save the current FPU state into the provided structure.
 *
 * @param state The FPU state structure to save into.
 */
static inline void fpu_save(fpu_state_t *state)
{
    __asm__ volatile("fxsave64 %0" : "=m"(*state));
}

/**
 * @brief Restore the FPU state from the provided structure.
 *
 * @param state The FPU state structure to restore from.
 */
static inline void fpu_restore(fpu_state_t *state)
{
    __asm__ volatile("fxrstor64 %0" : : "m"(*state));
}

#endif /* _ASM_X86_FPU_H_ */