include ../common.mk

###########################################
# Programs: one directory each under a category. The category decides the
# install path in the sysroot. include/ holds headers shared by programs (test.h).

categories := bin sbin tests
install_dir_bin := $(SYSROOT_DIR)/bin
install_dir_sbin := $(SYSROOT_DIR)/sbin
install_dir_tests := $(USR_DIR)/tests

# e.g. tests/hello (a missing category directory just matches nothing)
programs := $(patsubst %/,%,$(foreach category,$(categories),$(wildcard $(category)/*/)))

# Waiting for libstdc++ (roadmap D10e): its operator new and C++ runtime came
# from our libc's cxx/, which is no longer built. Removed from the sysroot
# below, so an old build's copy does not linger in the image.
waiting := tests/cxx
programs := $(filter-out $(waiting),$(programs))

program_sources = $(wildcard $(SOURCE_DIR)/$1/*.c $(SOURCE_DIR)/$1/*.cpp $(SOURCE_DIR)/$1/*.S)
program_objects = $(call src_to_bin_dir,$(addsuffix .o,$(basename $(call program_sources,$1))))
program_elf = $(BINARY_DIR)/$1.elf

# tests/hello → <sysroot>/usr/tests/hello: the build tree keeps .elf, the
# installed program is a plain file, as it appears in the initrd
program_installed = $(install_dir_$(firstword $(subst /, ,$1)))/$(notdir $1)

###########################################
# Libraries: one directory each under lib/, every .c/.cpp/.S inside archived
# into lib<name>.a. Every program links all of them, before libc: the linker
# takes only the members a program uses, so a library costs nothing to a
# program that never calls it. The group lets one library use another,
# whatever their order. lib/<name>/<name>.h is included as <name/name.h>.

libraries := $(patsubst lib/%/,%,$(wildcard lib/*/))
library_archive = $(BINARY_DIR)/lib/lib$1.a
library_archives := $(foreach library,$(libraries),$(call library_archive,$(library)))

define library_rule
$(call library_archive,$1): $(call program_objects,lib/$1)
	mkdir -p $$(@D)
	rm -f $$@
	$$(AR) rcs $$@ $$^
endef

$(foreach library,$(libraries),$(eval $(call library_rule,$(library))))

###########################################
# Linking against mlibc (roadmap D10d), installed in the sysroot by the top
# Makefile's mlibc target, with the kernel's <mihos/...> headers beside its
# own. GCC's specs bring the startup files (mlibc's crt1.o, which holds
# _start, then crti.o crtbegin.o ... crtend.o crtn.o: crtbegin.o defines
# __dso_handle, which mlibc's C++ code registers destructors with), -lc and
# libgcc. Linked with the toolchain's default script: mlibc's static startup
# finds the program headers through __ehdr_start, which needs the ELF header
# in a loaded segment.
# -z noexecstack: some of mlibc's objects lack the .note.GNU-stack section,
# and without it the linker asks for an executable stack. -z separate-code:
# the default script for x86_64-mihos otherwise puts .rodata in the code's
# segment, which makes it executable (Linux targets turn it on by default).

comma := ,
CPPFLAGS := -Iinclude -Ilib --sysroot=$(SYSROOT_DIR)
CFLAGS := -std=gnu11 -O2 -g -Wall -Wextra -fno-pie -fno-stack-protector

# $1: the program, $2: where it is installed
define program_rule
$(call program_elf,$1): $(LIB_DIR)/crt1.o $(LIB_DIR)/libc.a $(call program_objects,$1) $(library_archives)
	mkdir -p $$(@D)
	$$(CC) -o $$@ --sysroot=$(SYSROOT_DIR) -static -no-pie \
	    -Wl,-z,max-page-size=4096 -Wl,--build-id=none -Wl,-z,noexecstack -Wl,-z,separate-code \
	    $(call program_objects,$1) \
	    $(if $(library_archives),-Wl$(comma)--start-group $(library_archives) -Wl$(comma)--end-group)

# Without the debug info (mlibc's is ~20 MB per program, too big for the disk
# images); the .elf in the build tree keeps it
$2: $(call program_elf,$1)
	mkdir -p $$(@D)
	$(HOST)-strip --strip-debug -o $$@ $$<
endef

$(foreach program,$(programs),$(eval $(call program_rule,$(program),$(call program_installed,$(program)))))

# mlibc's first programs (roadmap D9): mlibc/<name>/, installed among the tests
# as mlibc_<name> so /usr/tests/run runs them too
mlibc_programs := $(patsubst %/,%,$(wildcard mlibc/*/))
$(foreach program,$(mlibc_programs),$(eval $(call program_rule,$(program),$(install_dir_tests)/mlibc_$(notdir $(program)))))

installed := $(foreach program,$(programs),$(call program_installed,$(program))) \
             $(foreach program,$(mlibc_programs),$(install_dir_tests)/mlibc_$(notdir $(program)))
removed := $(foreach program,$(waiting),$(call program_installed,$(program)))
# The libraries' objects too, for their .d files: a changed header rebuilds them
user_objects := $(foreach program,$(programs) $(mlibc_programs) $(addprefix lib/,$(libraries)),$(call program_objects,$(program)))

###########################################

.PHONY: all install

all: install

install: $(installed)
	rm -f $(removed)

ifeq ($(filter clean purge,$(MAKECMDGOALS)),)
-include $(user_objects:.o=.d)
endif
