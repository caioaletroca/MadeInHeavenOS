# Lua (https://www.lua.org), the first port (roadmap D10f): the interpreter
# `lua` and the compiler `luac`, installed into the sysroot's bin/ (so into
# the initrd and the installed system). Run by the top Makefile's `ports`
# target, which passes OUTPUT_DIR and HOST.
#
# Never vendored: the release tarball is downloaded once into build/downloads/
# and checked against its pinned sha256 (lua.org/ftp lists it) before every
# unpack, so a changed or corrupt file fails here instead of building. A new
# version is the three lines below.
#
# Lua is ISO C and needs no patch: its own src/Makefile takes the compiler
# from the command line. The `posix` target adds LUA_USE_POSIX (isatty, popen,
# mkstemp, fseeko); `linux` would add dlopen, useless in a static program.

LUA_VERSION := 5.5.1
LUA_SHA256 := 1c4b4068d67061f2a2231ad2b5422e77acea1487ea9890f6320af614f4373dce
LUA_URL := https://www.lua.org/ftp/lua-$(LUA_VERSION).tar.gz

HOST ?= x86_64-mihos
OUTPUT_DIR ?= $(abspath ../../build/x86_64-pc)
SYSROOT_DIR := $(OUTPUT_DIR)/sysroot
DOWNLOADS_DIR := $(abspath $(OUTPUT_DIR)/../downloads)
TARBALL := $(DOWNLOADS_DIR)/lua-$(LUA_VERSION).tar.gz
SOURCE_DIR := $(OUTPUT_DIR)/objects/ports/lua-$(LUA_VERSION)
LIBC := $(SYSROOT_DIR)/usr/lib/libc.a

# Linked as user/Makefile links our programs (see the flags explained there):
# static on mlibc from the sysroot. The archiver without `u`: binutils makes
# deterministic archives and warns that it ignores it.
LUA_MAKE_FLAGS := CC=$(HOST)-gcc AR="$(HOST)-ar rc" RANLIB=$(HOST)-ranlib \
    MYCFLAGS="--sysroot=$(SYSROOT_DIR) -fno-pie -fno-stack-protector" \
    MYLDFLAGS="--sysroot=$(SYSROOT_DIR) -static -no-pie -Wl,-z,max-page-size=4096 \
               -Wl,--build-id=none -Wl,-z,noexecstack -Wl,-z,separate-code"

PROGRAMS := lua luac
INSTALLED := $(addprefix $(SYSROOT_DIR)/bin/,$(PROGRAMS))

.PHONY: all clean

all: $(INSTALLED)

# Downloaded under a temporary name, so an interrupted download never looks finished
$(TARBALL):
	mkdir -p $(@D)
	wget -q -O $@.part $(LUA_URL)
	mv $@.part $@

# A fresh tree for every unpack (a new version, or this Makefile changed)
$(SOURCE_DIR)/.unpacked: $(TARBALL) Makefile
	echo "$(LUA_SHA256)  $<" | sha256sum -c --quiet
	rm -rf $(SOURCE_DIR)
	mkdir -p $(SOURCE_DIR)
	tar -xzf $< -C $(SOURCE_DIR) --strip-components=1
	touch $@

# Lua's Makefile does not know mlibc: it would keep a program linked against an
# old libc.a, or objects built on old headers. So it starts clean whenever
# mlibc changed; a full build of Lua takes seconds.
$(SOURCE_DIR)/src/lua: $(SOURCE_DIR)/.unpacked $(LIBC)
	$(MAKE) -C $(SOURCE_DIR)/src clean
	$(MAKE) -C $(SOURCE_DIR)/src posix $(LUA_MAKE_FLAGS)

$(SOURCE_DIR)/src/luac: $(SOURCE_DIR)/src/lua ;

# Without the debug info, as user/Makefile installs (mlibc's is large)
$(SYSROOT_DIR)/bin/%: $(SOURCE_DIR)/src/%
	mkdir -p $(@D)
	$(HOST)-strip --strip-debug -o $@ $<

# The download stays: `make purge` at the top removes it
clean:
	rm -rf $(SOURCE_DIR) $(INSTALLED)
