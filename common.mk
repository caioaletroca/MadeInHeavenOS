ARCH_FAMILY ?= x86
ARCH ?= x86_64
PLATFORM ?= pc
HOST ?= x86_64-mihos

# Per-arch toolchain flags. Resolved relative to this file, so it works
# from every subproject (libc/, kernel/, grub/). Must stay before any include.
ARCH_CONFIG := $(dir $(lastword $(MAKEFILE_LIST)))config/$(ARCH).mk
ifeq ($(wildcard $(ARCH_CONFIG)),)
$(error Unsupported ARCH '$(ARCH)': missing $(ARCH_CONFIG))
endif
include $(ARCH_CONFIG)

# Everything built goes under build/<target>/, one folder per target:
#   images/   what runs: mihos.iso, disk.img, part.img
#   sysroot/  headers, libraries and installed programs
#   objects/  one folder per component: kernel/, libc/, user/, grub/
#   staging/  trees that become images: iso/, initrd/, disk/, root/
#   test/     run-tests.sh's log and disk copies
# The component is the directory this Makefile runs in (kernel, libc, ...).
TARGET := $(ARCH)-$(PLATFORM)
PROJECT_ROOT ?= $(abspath ..)
OUTPUT_DIR := $(PROJECT_ROOT)/build/$(TARGET)
COMPONENT := $(notdir $(CURDIR))
BINARY_DIR := $(OUTPUT_DIR)/objects/$(COMPONENT)
IMAGES_DIR := $(OUTPUT_DIR)/images
STAGING_DIR := $(OUTPUT_DIR)/staging

libc := $(OUTPUT_DIR)/objects/libc/libc.a
libk := $(OUTPUT_DIR)/objects/libc/libk.a

#####################################################################
# Common Programs and Flags
CC := $(HOST)-gcc
CXX := $(HOST)-g++
AR := $(HOST)-ar
OBJDUMP ?= $(HOST)-objdump
RM := rm -rf

CFLAGS := -O2 -g -std=gnu11 -Wall -Wextra $(ARCH_CFLAGS)
# CFLAGS		+= -ffreestanding -mno-red-zone -Iinclude  -Wpacked -Wpadded
# CFLAGS		+= -Wall -Werror -Wextra -Wparentheses -Wmissing-declarations -Wunreachable-code -Wunused 
# CFLAGS		+= -Wmissing-field-initializers -Wmissing-prototypes -Wpointer-arith -Wswitch-enum
# CFLAGS		+= -Wredundant-decls -Wshadow -Wstrict-prototypes -Wswitch-default -Wuninitialized
# Code that runs in ring 3 (user programs, libc.a): not the kernel's ARCH_CFLAGS.
# Linked at USER_BASE, so the default small code model; the red zone is fine
# (int $0x80 switches to the kernel stack). No unwinder or stack protector yet.
USER_COMMON_FLAGS := -O2 -g -Wall -Wextra -ffreestanding -fno-pie \
                     -fno-stack-protector -fno-asynchronous-unwind-tables
USER_CFLAGS := -std=gnu11 $(USER_COMMON_FLAGS)
# Freestanding C++ until mlibc and libstdc++ (roadmap phase D): no exceptions
# or RTTI (no unwinder, no typeinfo library), no thread-safe local statics
# (no __cxa_guard_*: one thread per process). The runtime is in libc/cxx/.
USER_CXXFLAGS := -std=gnu++20 $(USER_COMMON_FLAGS) -fno-exceptions -fno-rtti \
                 -fno-threadsafe-statics
CPPFLAGS = -Iinclude --sysroot=$(SYSROOT_DIR) -isystem $(INCLUDE_DIR)
LDFLAGS = -fno-PIC --sysroot=$(SYSROOT_DIR) -L$(LIB_DIR)
LDFLAGS_EXTRA := -nostdlib -lk -lgcc

#####################################################################
# Folders and paths
SOURCE_DIR := .
ARCH_FAMILY_DIR := $(SOURCE_DIR)/arch/$(ARCH_FAMILY)
ARCH_COMMON_DIR := $(ARCH_FAMILY_DIR)/common
ARCH_DIR := $(ARCH_FAMILY_DIR)/$(ARCH)
PLATFORM_DIR := $(SOURCE_DIR)/platform/$(PLATFORM)
SYSROOT_DIR ?= $(OUTPUT_DIR)/sysroot
USR_DIR := $(SYSROOT_DIR)/usr
INCLUDE_DIR := $(USR_DIR)/include
BOOT_DIR := $(USR_DIR)/boot
BIN_DIR := $(USR_DIR)/bin
LIB_DIR := $(USR_DIR)/lib

#####################################################################
# Common Macro Functions
src_to_bin_dir = $(patsubst $(SOURCE_DIR)%,$(BINARY_DIR)%,$1)

# subdir.mk lists `local_sources` and subdirectories in `dirs`. libc also uses
# `hosted_local_sources`: built into libc.a only (needs syscalls or user flags).
define include_dir
dirs :=
local_sources :=
hosted_local_sources :=
include $1/subdir.mk
sources += $$(if $$(local_sources),$$(addprefix $1/,$$(local_sources)))
hosted_sources += $$(if $$(hosted_local_sources),$$(addprefix $1/,$$(hosted_local_sources)))
$$(foreach dir,$$(dirs),$$(eval $$(call include_dir,$1/$$(dir))))
endef

sources :=
hosted_sources :=
objects = $(call src_to_bin_dir,$(addsuffix .o,$(basename $(sources))))
depends = $(patsubst %.o,%.d,$(objects))

#####################################################################
# Makefile template declarations
$(eval $(call include_dir,$(SOURCE_DIR)))

.PHONY: all clean purge print-config
.SUFFIXES: .o .c .S .cpp

all:

print-config:
	@echo ARCH_FAMILY=$(ARCH_FAMILY)
	@echo ARCH=$(ARCH)
	@echo PLATFORM=$(PLATFORM)
	@echo HOST=$(HOST)
	@echo TARGET=$(TARGET)
	@echo OUTPUT_DIR=$(OUTPUT_DIR)
	@echo BINARY_DIR=$(BINARY_DIR)
	@echo SYSROOT_DIR=$(SYSROOT_DIR)

# clean: this component's objects for this target; purge: for every target
clean:
	$(RM) $(BINARY_DIR)

purge:
	$(RM) $(PROJECT_ROOT)/build/*/objects/$(COMPONENT)

$(BINARY_DIR)/%.o: $(SOURCE_DIR)/%.c
	mkdir -p $(@D)
	$(CC) -MMD -MP -c $< -o $@ $(CPPFLAGS) $(CFLAGS)

$(BINARY_DIR)/%.o: $(SOURCE_DIR)/%.S
	mkdir -p $(@D)
	$(CC) -MMD -MP -c $< -o $@ $(CPPFLAGS) $(CFLAGS)

$(BINARY_DIR)/%.o: $(SOURCE_DIR)/%.cpp
	mkdir -p $(@D)
	$(CXX) -MMD -MP -c $< -o $@ $(CPPFLAGS) $(CXXFLAGS)