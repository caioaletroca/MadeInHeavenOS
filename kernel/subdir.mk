local_sources := \
kprintf.c \
kmalloc.c \
kmain.c

dirs += arch/$(ARCH_FAMILY)/common arch/$(ARCH_FAMILY)/$(ARCH) platform/$(PLATFORM) cpu driver mm sys selftest
