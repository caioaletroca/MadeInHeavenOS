# Toolchain flags required by the x86_64 target.
# Included by common.mk; every ARCH needs a config/$(ARCH).mk like this one.

# -mcmodel=kernel  Kernel linked in the top 2 GiB (KERNEL_VIRTUAL_ADDRESS)
# -mno-red-zone    Interrupts push onto the current stack; no red zone below RSP
ARCH_CFLAGS := -mcmodel=kernel -mno-red-zone -mno-ms-bitfields
