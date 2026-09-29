ARCH_FAMILY ?= x86
ARCH ?= x86_64
PLATFORM ?= pc
HOST ?= x86_64-mihos

BUILD_ID := $(HOST)/$(ARCH)-$(PLATFORM)
BINARY_DIR := build/$(BUILD_ID)

libc := ../libc/$(BINARY_DIR)/libc.a
libk := ../libc/$(BINARY_DIR)/libk.a

#####################################################################
# Common Programs and Flags
CC := $(HOST)-gcc
AR := $(HOST)-ar
OBJDUMP ?= $(HOST)-objdump
RM := rm -rf

CFLAGS := -O2 -g -std=gnu11 -mcmodel=kernel -mno-red-zone -mno-ms-bitfields -Wall -Wextra
# CFLAGS		+= -ffreestanding -mno-red-zone -Iinclude  -Wpacked -Wpadded
# CFLAGS		+= -Wall -Werror -Wextra -Wparentheses -Wmissing-declarations -Wunreachable-code -Wunused 
# CFLAGS		+= -Wmissing-field-initializers -Wmissing-prototypes -Wpointer-arith -Wswitch-enum
# CFLAGS		+= -Wredundant-decls -Wshadow -Wstrict-prototypes -Wswitch-default -Wuninitialized
CPPFLAGS = -Iinclude -Iarch/$(ARCH_FAMILY)/common -Iarch/$(ARCH_FAMILY)/$(ARCH) -Iplatform/$(PLATFORM) --sysroot=$(SYSROOT_DIR) -isystem $(INCLUDE_DIR)
LDFLAGS = -fno-PIC --sysroot=$(SYSROOT_DIR) -L$(LIB_DIR)
LDFLAGS_EXTRA := -nostdlib -lk -lgcc

#####################################################################
# Folders and paths
SOURCE_DIR := .
PROJECT_ROOT ?= $(abspath ..)
ARCH_FAMILY_DIR := $(SOURCE_DIR)/arch/$(ARCH_FAMILY)
ARCH_COMMON_DIR := $(ARCH_FAMILY_DIR)/common
ARCH_DIR := $(ARCH_FAMILY_DIR)/$(ARCH)
PLATFORM_DIR := $(SOURCE_DIR)/platform/$(PLATFORM)
SYSROOT_DIR ?= $(PROJECT_ROOT)/sysroot
USR_DIR := $(SYSROOT_DIR)/usr
INCLUDE_DIR := $(USR_DIR)/include
BOOT_DIR := $(USR_DIR)/boot
LIB_DIR := $(USR_DIR)/lib

#####################################################################
# Common Macro Functions
src_to_bin_dir = $(patsubst $(SOURCE_DIR)%,$(BINARY_DIR)%,$1)

define include_dir
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

.PHONY: all clean purge print-config
.SUFFIXES: .o .c .S

all:

print-config:
	@echo ARCH_FAMILY=$(ARCH_FAMILY)
	@echo ARCH=$(ARCH)
	@echo PLATFORM=$(PLATFORM)
	@echo HOST=$(HOST)
	@echo BINARY_DIR=$(BINARY_DIR)
	@echo SYSROOT_DIR=$(SYSROOT_DIR)

clean:
	$(RM) $(BINARY_DIR)

purge:
	$(RM) build

$(BINARY_DIR)/%.o: $(SOURCE_DIR)/%.c
	mkdir -p $(@D)
	$(CC) -MMD -MP -c $< -o $@ $(CPPFLAGS) $(CFLAGS)

$(BINARY_DIR)/%.o: $(SOURCE_DIR)/%.S
	mkdir -p $(@D)
	$(CC) -MMD -MP -c $< -o $@ $(CPPFLAGS) $(CFLAGS)