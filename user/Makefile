include ../common.mk

###########################################
# Userland flags: USER_CFLAGS from common.mk, shared with libc.a.
# -nostdlib: the toolchain's own startup files and libraries are not used;
# crt0.o and libc.a come from our sysroot and are named explicitly.
CFLAGS := $(USER_CFLAGS)
LDFLAGS := --sysroot=$(SYSROOT_DIR) -L$(LIB_DIR) -nostdlib -static -no-pie \
           -Wl,-z,max-page-size=4096 -Wl,--build-id=none -T user.ld

###########################################
# Programs: one directory each, every .c/.S inside is linked between crt0.o
# (first: holds _start) and libc (after: resolves what the program uses)

programs := hello

crt0 := $(LIB_DIR)/crt0.o
libs := -lc -lgcc
program_sources = $(wildcard $(SOURCE_DIR)/$1/*.c $(SOURCE_DIR)/$1/*.S)
program_objects = $(call src_to_bin_dir,$(addsuffix .o,$(basename $(call program_sources,$1))))
program_elf = $(BINARY_DIR)/$1.elf

define program_rule
$(call program_elf,$1): $(crt0) $(LIB_DIR)/libc.a $(call program_objects,$1) user.ld
	mkdir -p $$(@D)
	$$(CC) -o $$@ $$(CFLAGS) $$(LDFLAGS) $(crt0) $(call program_objects,$1) $(libs)
endef

$(foreach program,$(programs),$(eval $(call program_rule,$(program))))

elves := $(foreach program,$(programs),$(call program_elf,$(program)))
user_objects := $(foreach program,$(programs),$(call program_objects,$(program)))

###########################################

.PHONY: all install

all: install

install: $(elves)
	mkdir -p $(BIN_DIR)
	cp $^ $(BIN_DIR)

ifeq ($(filter clean purge,$(MAKECMDGOALS)),)
-include $(user_objects:.o=.d)
endif
