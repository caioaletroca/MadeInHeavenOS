local_sources := \
kprintf.c \
kmain.c

dirs += arch/$(ARCH_FAMILY)/common arch/$(ARCH_FAMILY)/$(ARCH) platform/$(PLATFORM) driver mm sys sched fs selftest
