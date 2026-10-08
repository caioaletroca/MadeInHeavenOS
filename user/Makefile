include ../common.mk

###########################################
# Userland flags: USER_CFLAGS from common.mk, shared with libc.a.
# -nostdlib: the toolchain's own startup files and libraries are not used;
# crt0.o and libc.a come from our sysroot and are named explicitly.
# include/ (via CPPFLAGS) holds headers shared by programs, e.g. test.h.
CFLAGS := $(USER_CFLAGS)
LDFLAGS := --sysroot=$(SYSROOT_DIR) -L$(LIB_DIR) -nostdlib -static -no-pie \
           -Wl,-z,max-page-size=4096 -Wl,--build-id=none -T user.ld

###########################################
# Programs: one directory each under a category. Every .c/.S inside is linked
# between crt0.o (first: holds _start) and libc (after: resolves what the
# program uses). The category decides the install path in the sysroot.

categories := bin sbin tests
install_dir_bin := $(SYSROOT_DIR)/bin
install_dir_sbin := $(SYSROOT_DIR)/sbin
install_dir_tests := $(USR_DIR)/tests

# e.g. tests/hello (a missing category directory just matches nothing)
programs := $(patsubst %/,%,$(foreach category,$(categories),$(wildcard $(category)/*/)))

crt0 := $(LIB_DIR)/crt0.o
libs := -lc -lgcc
program_sources = $(wildcard $(SOURCE_DIR)/$1/*.c $(SOURCE_DIR)/$1/*.S)
program_objects = $(call src_to_bin_dir,$(addsuffix .o,$(basename $(call program_sources,$1))))
program_elf = $(BINARY_DIR)/$1.elf
# tests/hello → <sysroot>/usr/tests/hello.elf (.elf until the initramfs, phase B)
program_installed = $(install_dir_$(firstword $(subst /, ,$1)))/$(notdir $1).elf

define program_rule
$(call program_elf,$1): $(crt0) $(LIB_DIR)/libc.a $(call program_objects,$1) user.ld
	mkdir -p $$(@D)
	$$(CC) -o $$@ $$(CFLAGS) $$(LDFLAGS) $(crt0) $(call program_objects,$1) $(libs)

$(call program_installed,$1): $(call program_elf,$1)
	mkdir -p $$(@D)
	cp $$< $$@
endef

$(foreach program,$(programs),$(eval $(call program_rule,$(program))))

installed := $(foreach program,$(programs),$(call program_installed,$(program)))
user_objects := $(foreach program,$(programs),$(call program_objects,$(program)))

###########################################

.PHONY: all install

all: install

install: $(installed)

ifeq ($(filter clean purge,$(MAKECMDGOALS)),)
-include $(user_objects:.o=.d)
endif
