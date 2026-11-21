include ../common.mk

ISO_DIR ?= $(STAGING_DIR)/iso
GRUB_DIR := $(ISO_DIR)/boot/grub
ISO := $(IMAGES_DIR)/mihos.iso

GRUB ?= grub-mkrescue
GRUBFLAGS ?= /usr/lib/grub/i386-pc
GRUB_CFG := grub.cfg

# The initramfs: every program and configuration file user/ installed, packed
# into one ustar archive that GRUB loads as the "initrd" module (see grub.cfg);
# the kernel finds files in it by path (kernel/fs/initrd.c). Headers and
# libraries in the sysroot are for building, not running, so only the
# directories of the running system go in. Leftover .elf files from older
# builds are left out. Evaluated when this Makefile starts, after user/ has
# installed.
ROOT_DIRS := bin etc sbin usr/tests
ROOT_FILES := $(filter-out %.elf,$(foreach dir,$(ROOT_DIRS),$(wildcard $(SYSROOT_DIR)/$(dir)/*)))

# Empty directories other file systems are mounted on (fs/devfs.c mounts /dev,
# the first ext2 disk goes on /mnt): a mount point must exist before anything
# can be mounted on it
MOUNT_POINTS := dev mnt

# BusyBox's applets by name: each name in the list ports/busybox installs
# becomes /bin/<name> → busybox (busybox runs the applet argv[0] names), unless
# a program of ours already has the name (/bin/sh stays ours). Absent until
# the port has been built.
APPLET_LIST := $(SYSROOT_DIR)/usr/share/busybox/applets

# The symlink fixtures of the ext2 test (user/tests/ext2), read-only on the
# initrd in the live boot and real ext2 symlinks on the disk root in the other:
# a relative link, an absolute one to a directory, a chain of two, a dangling
# one, a loop, and one whose target is past 59 characters (on ext2 a "slow"
# symlink, its target in a data block; shorter targets live in the inode)
LINK_FIXTURES = $(INITRD_STAGING)/usr/tests/links

INITRD_STAGING := $(STAGING_DIR)/initrd
INITRD := $(BINARY_DIR)/initrd.tar
staging_path = $(INITRD_STAGING)$(patsubst $(SYSROOT_DIR)%,%,$1)

.PHONY: all iso

all: iso

iso: $(ISO)

# ustar: the format the kernel reads. Owner 0 and names sorted, so the archive
# only changes when a program (or this layout) does. --sort=name orders what is
# inside each directory, not the operands: those are sorted here.
$(INITRD): $(ROOT_FILES) $(wildcard $(APPLET_LIST)) Makefile
	rm -rf $(INITRD_STAGING)
	$(foreach file,$(ROOT_FILES),mkdir -p $(dir $(call staging_path,$(file))) && cp $(file) $(call staging_path,$(file));)
	$(foreach dir,$(MOUNT_POINTS),mkdir -m 0755 -p $(INITRD_STAGING)/$(dir);)
	if [ -f $(APPLET_LIST) ]; then \
	    for path in $$(cat $(APPLET_LIST)); do \
	        name=$${path##*/}; \
	        [ -e $(INITRD_STAGING)/bin/$$name ] || ln -s busybox $(INITRD_STAGING)/bin/$$name; \
	    done; \
	fi
	mkdir -p $(LINK_FIXTURES)
	ln -s ../hello $(LINK_FIXTURES)/relative
	ln -s /usr/tests $(LINK_FIXTURES)/directory
	ln -s chain2 $(LINK_FIXTURES)/chain1
	ln -s relative $(LINK_FIXTURES)/chain2
	ln -s nowhere $(LINK_FIXTURES)/dangling
	ln -s loop $(LINK_FIXTURES)/loop
	ln -s $$(printf './%.0s' $$(seq 30))../hello $(LINK_FIXTURES)/long
	mkdir -p $(@D)
	tar --format=ustar --owner=0 --group=0 --numeric-owner --sort=name \
	    -cf $@ -C $(INITRD_STAGING) $(sort $(ROOT_DIRS) $(MOUNT_POINTS))

# The ISO holds the kernel, grub.cfg and the initrd: rebuilt from scratch, so
# files from older layouts (one module per program) never linger
$(ISO): $(BOOT_DIR)/kernel $(INITRD) $(GRUB_CFG)
	rm -rf $(ISO_DIR)
	mkdir -p $(GRUB_DIR)
	cp $(GRUB_CFG) $(GRUB_DIR)
	cp $(BOOT_DIR)/kernel $(ISO_DIR)/boot/mihos.kernel
	cp $(INITRD) $(ISO_DIR)/boot/initrd.tar
	mkdir -p $(@D)
	$(GRUB) $(GRUBFLAGS) -o $@ $(ISO_DIR)
