# Which libc the programs link against (roadmap D10, until the switch): ours by
# default, or `make LIBC=mlibc` for every program at once, the switch's test
# bed. mlibc's objects go to objects/user-mlibc/, so the two never mix.
LIBC ?= ours
ifeq ($(LIBC),mlibc)
COMPONENT := user-mlibc
else ifneq ($(LIBC),ours)
$(error LIBC is ours or mlibc, not '$(LIBC)')
endif

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

program_sources = $(wildcard $(SOURCE_DIR)/$1/*.c $(SOURCE_DIR)/$1/*.cpp $(SOURCE_DIR)/$1/*.S)
program_objects = $(call src_to_bin_dir,$(addsuffix .o,$(basename $(call program_sources,$1))))
program_elf = $(BINARY_DIR)/$1.elf

# tests/hello → <sysroot>/usr/tests/hello: the build tree keeps .elf, the
# installed program is a plain file, as it appears in the initrd
program_installed = $(install_dir_$(firstword $(subst /, ,$1)))/$(notdir $1)

# The installed programs come from one libc at a time: this file changes with
# LIBC, and every installed program depends on it, so switching reinstalls them
# all (the other libc's .elf files are older than what is installed)
libc_stamp := $(OUTPUT_DIR)/objects/user-libc
$(shell mkdir -p $(dir $(libc_stamp)) && { [ "$$(cat $(libc_stamp) 2>/dev/null)" = "$(LIBC)" ] || echo $(LIBC) > $(libc_stamp); })

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
# Linking against mlibc: every program with LIBC=mlibc, and mlibc/<name>/
# always (below). mlibc's sysroot is made by the top Makefile's mlibc target,
# with the kernel's <mihos/...> headers beside mlibc's. mlibc's crt1.o holds
# _start. Linked with the toolchain's default script, not user.ld: mlibc's
# static startup finds the program headers through __ehdr_start, which needs
# the ELF header in a loaded segment. -z noexecstack: some of mlibc's objects
# lack the .note.GNU-stack section, and without it the linker asks for an
# executable stack. -z separate-code: the default script for x86_64-mihos
# otherwise puts .rodata in the code's segment, which makes it executable
# (Linux targets turn it on by default; user.ld separates them itself).

MLIBC_SYSROOT := $(OUTPUT_DIR)/mlibc-sysroot
comma := ,
mlibc_lib_dir := $(MLIBC_SYSROOT)/usr/lib
# GCC's startup files around the program, in the usual order (crtbegin.o
# defines __dso_handle, which mlibc's C++ code registers destructors with)
mlibc_start_files := $(mlibc_lib_dir)/crt1.o $(shell $(CC) -print-file-name=crti.o) \
                     $(shell $(CC) -print-file-name=crtbegin.o)
mlibc_end_files := $(shell $(CC) -print-file-name=crtend.o) $(shell $(CC) -print-file-name=crtn.o)
# Hosted C: mlibc's headers, not our libc's (no -isystem of our sysroot)
mlibc_cppflags := -Iinclude -Ilib --sysroot=$(MLIBC_SYSROOT)
mlibc_cflags := -std=gnu11 -O2 -g -Wall -Wextra -fno-pie -fno-stack-protector

# $1: the program, $2: where it is installed, $3: the libraries' archives (built
# against mlibc: none for mlibc/<name>/ while the rest is on our libc)
define mlibc_program_rule
$(call program_elf,$1): $(mlibc_lib_dir)/crt1.o $(mlibc_lib_dir)/libc.a $(call program_objects,$1) $3
	mkdir -p $$(@D)
	$$(CC) -o $$@ --sysroot=$(MLIBC_SYSROOT) -L$(mlibc_lib_dir) -nostdlib -static -no-pie \
	    -Wl,-z,max-page-size=4096 -Wl,--build-id=none -Wl,-z,noexecstack -Wl,-z,separate-code \
	    $(mlibc_start_files) $(call program_objects,$1) \
	    $(if $3,-Wl$(comma)--start-group $3 -Wl$(comma)--end-group) -lc -lgcc $(mlibc_end_files)

# Without the debug info (mlibc's is ~20 MB per program, too big for the disk
# images); the .elf in the build tree keeps it
$2: $(call program_elf,$1) $(libc_stamp)
	mkdir -p $$(@D)
	$(HOST)-strip --strip-debug -o $$@ $$<
endef

###########################################
# The programs in bin/, sbin/ and tests/

ifeq ($(LIBC),ours)

# USER_CFLAGS from common.mk, shared with libc.a. -nostdlib: the toolchain's
# own startup files and libraries are not used; crt0.o (first: holds _start)
# and libc.a (after: resolves what the program uses) come from our sysroot.
CPPFLAGS += -Ilib
CFLAGS := $(USER_CFLAGS)
CXXFLAGS := $(USER_CXXFLAGS)
LDFLAGS := --sysroot=$(SYSROOT_DIR) -L$(LIB_DIR) -nostdlib -static -no-pie \
           -Wl,-z,max-page-size=4096 -Wl,--build-id=none -T user.ld

crt0 := $(LIB_DIR)/crt0.o
libs := -lc -lgcc

define program_rule
$(call program_elf,$1): $(crt0) $(LIB_DIR)/libc.a $(call program_objects,$1) $(library_archives) user.ld
	mkdir -p $$(@D)
	$$(CC) -o $$@ $$(CFLAGS) $$(LDFLAGS) $(crt0) $(call program_objects,$1) \
	    -Wl,--start-group $(library_archives) -Wl,--end-group $(libs)

$(call program_installed,$1): $(call program_elf,$1) $(libc_stamp)
	mkdir -p $$(@D)
	cp $$< $$@
endef

$(foreach program,$(programs),$(eval $(call program_rule,$(program))))

# Programs on mlibc (roadmap D9): mlibc/<name>/, installed among the tests as
# mlibc_<name> so /usr/tests/run runs them too
mlibc_programs := $(patsubst %/,%,$(wildcard mlibc/*/))
mlibc_objects := $(foreach program,$(mlibc_programs),$(call program_objects,$(program)))
$(mlibc_objects): CPPFLAGS := $(mlibc_cppflags)
$(mlibc_objects): CFLAGS := $(mlibc_cflags)

else

# Waiting for libstdc++ (D10): its operator new and C++ runtime come from our
# libc's cxx/ until then. Removed from the sysroot below, so the image never
# holds programs of both libcs.
mlibc_waiting := tests/cxx
programs := $(filter-out $(mlibc_waiting),$(programs))
mlibc_removed := $(foreach program,$(mlibc_waiting),$(call program_installed,$(program)))

CPPFLAGS := $(mlibc_cppflags)
CFLAGS := $(mlibc_cflags)

$(foreach program,$(programs),$(eval $(call mlibc_program_rule,$(program),$(call program_installed,$(program)),$(library_archives))))

mlibc_programs := $(patsubst %/,%,$(wildcard mlibc/*/))
mlibc_objects := $(foreach program,$(mlibc_programs),$(call program_objects,$(program)))
mlibc_archives := $(library_archives)

endif

$(foreach program,$(mlibc_programs),$(eval $(call mlibc_program_rule,$(program),$(install_dir_tests)/mlibc_$(notdir $(program)),$(mlibc_archives))))

installed := $(foreach program,$(programs),$(call program_installed,$(program))) \
             $(foreach program,$(mlibc_programs),$(install_dir_tests)/mlibc_$(notdir $(program)))
# The libraries' objects too, for their .d files: a changed header rebuilds them
user_objects := $(foreach program,$(programs) $(addprefix lib/,$(libraries)),$(call program_objects,$(program))) \
                $(mlibc_objects)

###########################################

.PHONY: all install

all: install

install: $(installed)
	$(if $(mlibc_removed),rm -f $(mlibc_removed))

ifeq ($(filter clean purge,$(MAKECMDGOALS)),)
-include $(user_objects:.o=.d)
endif
