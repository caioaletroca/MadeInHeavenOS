include ../common.mk

###########################################
# Folders and Flags
ARCH_BINARY_DIR := $(BINARY_DIR)/arch/$(ARCH_FAMILY)/$(ARCH)
# Not the sysroot's headers (common.mk); libk.a is linked from its build folder
CPPFLAGS := -Iinclude $(FREESTANDING_CPPFLAGS) -D__is_kernel

# Header visibility
#   Every object:            include/ (generic + arch/platform contracts)
#                            arch/<family>/include (<asm/...>, the arch public interface)
#   arch/ and platform/ only: arch/<family>/common/include (<x86/...>, arch-private)
# Generic code that includes an arch-private header fails to compile.
ARCH_PUBLIC_INCLUDE := arch/$(ARCH_FAMILY)/include
ARCH_PRIVATE_INCLUDE := arch/$(ARCH_FAMILY)/common/include

CPPFLAGS += -I$(ARCH_PUBLIC_INCLUDE)

# The kernel/user contract (<mihos/...>) is used by the kernel under the same
# names user programs see, so the two can never drift apart
CPPFLAGS += -Iinclude/uapi

$(BINARY_DIR)/arch/%.o $(BINARY_DIR)/platform/%.o: CPPFLAGS += -I$(ARCH_PRIVATE_INCLUDE)

###########################################

kernel := $(BINARY_DIR)/kernel

.PHONY: all install install-headers install-kernel

all: install

install: install-headers install-kernel

# Only the uapi headers reach the sysroot: user programs and libc see the
# kernel/user contract (<mihos/...>), never kernel internals
install-headers:
	mkdir -p $(INCLUDE_DIR)
	cp -R --preserve=timestamps include/uapi/. $(INCLUDE_DIR)

install-kernel: $(kernel)
	mkdir -p $(BOOT_DIR)
	cp $^ $(BOOT_DIR)

# No crtbegin.o/crtend.o, as Linux: GCC's register their work in .init_array
# (D10e), which the kernel never runs, and are built for user space (32-bit
# addresses), not for a higher-half kernel. crti.S/crtn.S still make an empty _init.
LINK_LIST=\
$(objects) \
$(libk) \
$(LDFLAGS_EXTRA) \

$(kernel): $(objects) $(libk) $(ARCH_BINARY_DIR)/linker.lds.i
	mkdir -p $(@D)
	$(CC) -n -T $(ARCH_BINARY_DIR)/linker.lds.i -o $@ $(CFLAGS) $(LDFLAGS) $(LINK_LIST)

$(ARCH_BINARY_DIR)/linker.lds.i: $(ARCH_DIR)/linker.lds
	mkdir -p $(@D)
	$(CC) -E -P -x c $(CPPFLAGS) $(ARCH_DIR)/linker.lds -o $@


ifeq ($(filter clean purge,$(MAKECMDGOALS)),)
ifneq ($(strip $(depends)),)
-include $(depends)
endif
endif