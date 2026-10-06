include ../common.mk

###########################################
# Userland flags
#   Not the kernel's: programs run at USER_BASE (4 MiB), so the default small
#   code model, no -mcmodel=kernel, and the red zone is fine (int $0x80
#   switches to the kernel stack). No libc yet: freestanding, linked by hand.
CFLAGS := -O2 -g -std=gnu11 -Wall -Wextra -ffreestanding -fno-pie \
          -fno-stack-protector -fno-asynchronous-unwind-tables
LDFLAGS := --sysroot=$(SYSROOT_DIR) -L$(LIB_DIR) -nostdlib -static -no-pie \
           -Wl,-z,max-page-size=4096 -Wl,--build-id=none -T user.ld

###########################################
# Programs: one directory each, every .c/.S inside is linked with start.S

programs := hello

crt := $(BINARY_DIR)/start.o
program_sources = $(wildcard $(SOURCE_DIR)/$1/*.c $(SOURCE_DIR)/$1/*.S)
program_objects = $(call src_to_bin_dir,$(addsuffix .o,$(basename $(call program_sources,$1))))
program_elf = $(BINARY_DIR)/$1.elf

define program_rule
$(call program_elf,$1): $(crt) $(call program_objects,$1) user.ld
	mkdir -p $$(@D)
	$$(CC) -o $$@ $$(CFLAGS) $$(LDFLAGS) $(crt) $(call program_objects,$1) -lgcc
endef

$(foreach program,$(programs),$(eval $(call program_rule,$(program))))

elves := $(foreach program,$(programs),$(call program_elf,$(program)))
user_objects := $(crt) $(foreach program,$(programs),$(call program_objects,$(program)))

###########################################

.PHONY: all install

all: install

install: $(elves)
	mkdir -p $(BIN_DIR)
	cp $^ $(BIN_DIR)

ifeq ($(filter clean purge,$(MAKECMDGOALS)),)
-include $(user_objects:.o=.d)
endif
