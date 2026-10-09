# Toolchain flags required by the x86_64 target.
# Included by common.mk; every ARCH needs a config/$(ARCH).mk like this one.
# Kernel and libk.a only: user code (libc.a, user/) uses USER_CFLAGS.

# -mcmodel=kernel       Kernel linked in the top 2 GiB (KERNEL_VIRTUAL_ADDRESS)
# -mno-red-zone         Interrupts push onto the current stack; no red zone below RSP
# -mgeneral-regs-only   No SSE/x87/MMX: a context switch saves only the general
#                       registers, so kernel code must never touch the vector
#                       registers (they hold the interrupted program's state)
ARCH_CFLAGS := -mcmodel=kernel -mno-red-zone -mno-ms-bitfields -mgeneral-regs-only
