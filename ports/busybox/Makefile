# BusyBox (https://busybox.net): the commands (roadmap D10g). One static
# program holding every applet chosen in applets.config, installed as
# /bin/busybox and run as "busybox <applet> ...". Run by the top Makefile's
# `ports` target, which passes OUTPUT_DIR and HOST.
#
# Never vendored, like ports/lua/: the release tarball is downloaded once into
# build/downloads/ and checked against its pinned sha256 (busybox.net publishes
# it next to the tarball) before every unpack. A new version is the three lines
# below, and mihos.patch checked against it.
#
# mihos.patch: three changes, all for headers and calls that are mlibc's
# "linux" option, which our mlibc port leaves off (<mntent.h>, <sys/statfs.h>,
# <malloc.h>, sched_getaffinity). Busybox already builds without them on the
# BSDs; the patch adds __mihos__ to those cases.

BUSYBOX_VERSION := 1.38.0
BUSYBOX_SHA256 := 34f9ea6ff8636f2c9241153b9114eefa9e65674a45318ae1ef95bb5f31c53bb2
BUSYBOX_URL := https://busybox.net/downloads/busybox-$(BUSYBOX_VERSION).tar.bz2

HOST ?= x86_64-mihos
OUTPUT_DIR ?= $(abspath ../../build/x86_64-pc)
SYSROOT_DIR := $(OUTPUT_DIR)/sysroot
DOWNLOADS_DIR := $(abspath $(OUTPUT_DIR)/../downloads)
TARBALL := $(DOWNLOADS_DIR)/busybox-$(BUSYBOX_VERSION).tar.bz2
SOURCE_DIR := $(OUTPUT_DIR)/objects/ports/busybox-$(BUSYBOX_VERSION)
LIBC := $(SYSROOT_DIR)/usr/lib/libc.a
INSTALLED := $(SYSROOT_DIR)/bin/busybox

# Linked as user/Makefile links our programs (see the flags explained there):
# static on mlibc from the sysroot. Busybox's own CONFIG_STATIC adds -static.
define BUILD_CONFIG
CONFIG_STATIC=y
CONFIG_CROSS_COMPILER_PREFIX="$(HOST)-"
CONFIG_SYSROOT="$(SYSROOT_DIR)"
CONFIG_EXTRA_CFLAGS="-fno-pie -fno-stack-protector"
CONFIG_EXTRA_LDFLAGS="-no-pie -Wl,-z,max-page-size=4096 -Wl,--build-id=none -Wl,-z,noexecstack -Wl,-z,separate-code"
endef
export BUILD_CONFIG

.PHONY: all clean

all: $(INSTALLED)

# Downloaded under a temporary name, so an interrupted download never looks finished
$(TARBALL):
	mkdir -p $(@D)
	wget -q -O $@.part $(BUSYBOX_URL)
	mv $@.part $@

# A fresh, patched tree for every unpack (a new version, the patch or this
# Makefile changed)
$(SOURCE_DIR)/.unpacked: $(TARBALL) mihos.patch Makefile
	echo "$(BUSYBOX_SHA256)  $<" | sha256sum -c --quiet
	rm -rf $(SOURCE_DIR)
	mkdir -p $(SOURCE_DIR)
	tar -xjf $< -C $(SOURCE_DIR) --strip-components=1
	patch -d $(SOURCE_DIR) -p1 --quiet < mihos.patch
	touch $@

# The configuration: everything off, then applets.config and the build
# settings in place of allnoconfig's lines for the same options, then every
# option they bring in at its default. Kconfig's KCONFIG_ALLCONFIG would be
# the usual way, but busybox's copy keeps only the string options from it.
$(SOURCE_DIR)/.config: $(SOURCE_DIR)/.unpacked applets.config
	$(MAKE) -C $(SOURCE_DIR) allnoconfig > /dev/null
	echo "$$BUILD_CONFIG" > $(SOURCE_DIR)/.config.wanted
	grep '^CONFIG_' applets.config >> $(SOURCE_DIR)/.config.wanted
	sed -n 's/^\(CONFIG_[A-Z0-9_]*\)=.*/\1[= ]/p' $(SOURCE_DIR)/.config.wanted > $(SOURCE_DIR)/.config.names
	grep -v -f $(SOURCE_DIR)/.config.names $(SOURCE_DIR)/.config > $(SOURCE_DIR)/.config.base
	cat $(SOURCE_DIR)/.config.base $(SOURCE_DIR)/.config.wanted > $@
	yes "" | $(MAKE) -C $(SOURCE_DIR) oldconfig > /dev/null
	# A wanted line kconfig dropped (a typo, or an option whose dependency is off) fails here
	@missing=$$(grep -vxF -f $@ $(SOURCE_DIR)/.config.wanted); \
	if [ -n "$$missing" ]; then echo "busybox: not in .config:"; echo "$$missing"; exit 1; fi

# Busybox's Makefile does not know mlibc: like Lua, it starts clean whenever
# libc.a changed, so busybox is never linked against an old one
$(SOURCE_DIR)/busybox: $(SOURCE_DIR)/.config $(LIBC)
	$(MAKE) -C $(SOURCE_DIR) clean > /dev/null
	$(MAKE) -C $(SOURCE_DIR)

# Busybox strips its own binary (busybox_unstripped keeps the symbols)
$(INSTALLED): $(SOURCE_DIR)/busybox
	mkdir -p $(@D)
	cp $< $@

# The download stays: `make purge` at the top removes it
clean:
	rm -rf $(SOURCE_DIR) $(INSTALLED)
