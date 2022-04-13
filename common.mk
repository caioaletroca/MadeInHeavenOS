ARCH := x86_64

libc := ../libc/build/$(ARCH)/libc.a
libk := ../libc/build/$(ARCH)/libk.a

#####################################################################
# Common Programs and Flags
DEFAULT_HOST := x86_64-elf

HOST := ${DEFAULT_HOST}

CC := ${HOST}-gcc
AR := ${HOST}-ar
AS := nasm
RM := rm -rf

CFLAGS := -O2 -g -ffreestanding -Wall -Wextra
CPPFLAGS := -Iinclude
ASFLAGS := -f elf64
LDFLAGS := 
LDFLAGS_EXTRA := -nostdlib -lk -lgcc

#####################################################################
# Folders and paths
BINARY_DIR := build/$(ARCH)
SOURCE_DIR := .
ARCH_DIR := $(SOURCE_DIR)/arch/${ARCH}
SYSROOT_DIR := /root/env/sysroot
USR_DIR := $(SYSROOT_DIR)/usr
INCLUDE_DIR := $(USR_DIR)/include
BOOT_DIR := $(USR_DIR)/boot
LIB_DIR := $(USR_DIR)/lib

#####################################################################
# Adjustments
CFLAGS += --sysroot=${SYSROOT_DIR} -isystem=/usr/include

#####################################################################
# Common Macro Functions
src_to_bin_dir = $(patsubst $(SOURCE_DIR)%,$(BINARY_DIR)%,$1)

define include_dir
$(shell mkdir -p $(call src_to_bin_dir,$1))
dirs :=
local_sources :=
include $1/subdir.mk
sources += $$(if $$(local_sources),$$(addprefix $1/,$$(local_sources)))
$$(foreach dir,$$(dirs),$$(eval $$(call include_dir,$1/$$(dir))))
endef

sources :=
objects = $(call src_to_bin_dir,$(addsuffix .o,$(basename $(sources))))
depends = $(patsubst %.o,%.d,$(objects))

#####################################################################
# Makefile template declarations
$(eval $(call include_dir,$(SOURCE_DIR)))

.PHONY: all clean purge
.SUFFIXES: .o .c .asm

all:

clean:
	$(RM) $(BINARY_DIR)

purge:
	$(RM) build

$(BINARY_DIR)/%.o: $(SOURCE_DIR)/%.c
	$(CC) -MD -c $< -o $@ -std=gnu11 $(CFLAGS) $(CPPFLAGS)

$(BINARY_DIR)/%.o: $(SOURCE_DIR)/%.asm
	$(AS) $(ASFLAGS) $< -o $@

%.o: %.c
	$(CC) -MD -c $< -o $@ -std=gnu11 $(CFLAGS) $(CPPFLAGS)

$(BINARY_DIR)/%: $(SOURCE_DIR)/%.c
	@echo $^

# $(ARCHDIR)/crtbegin.o $(ARCHDIR)/crtend.o:
# 	OBJ=`$(CC) $(CFLAGS) $(LDFLAGS) -print-file-name=$(@F)` && cp "$$OBJ" $@