include ../common.mk

ISO_DIR ?= $(STAGING_DIR)/iso
GRUB_DIR := $(ISO_DIR)/boot/grub
ISO := $(IMAGES_DIR)/mihos.iso

GRUB ?= grub-mkrescue
GRUBFLAGS ?= /usr/lib/grub/i386-pc
GRUB_CFG := grub.cfg

# The initramfs: every program user/ installed, packed into one ustar archive
# that GRUB loads as the "initrd" module (see grub.cfg); the kernel finds files
# in it by path (kernel/fs/initrd.c). Headers and libraries in the sysroot are
# for building, not running, so only the program directories go in. Leftover
# .elf files from older builds are left out. Evaluated when this Makefile
# starts, after user/ has installed.
PROGRAM_DIRS := bin sbin usr/tests
PROGRAMS := $(filter-out %.elf,$(foreach dir,$(PROGRAM_DIRS),$(wildcard $(SYSROOT_DIR)/$(dir)/*)))

# Empty directories other file systems are mounted on (fs/devfs.c mounts /dev,
# the first ext2 disk goes on /mnt): a mount point must exist before anything
# can be mounted on it
MOUNT_POINTS := dev mnt

INITRD_STAGING := $(STAGING_DIR)/initrd
INITRD := $(BINARY_DIR)/initrd.tar
staging_path = $(INITRD_STAGING)$(patsubst $(SYSROOT_DIR)%,%,$1)

.PHONY: all iso

all: iso

iso: $(ISO)

# ustar: the format the kernel reads. Owner 0 and names sorted, so the archive
# only changes when a program (or this layout) does. --sort=name orders what is
# inside each directory, not the operands: those are sorted here.
$(INITRD): $(PROGRAMS) Makefile
	rm -rf $(INITRD_STAGING)
	$(foreach program,$(PROGRAMS),mkdir -p $(dir $(call staging_path,$(program))) && cp $(program) $(call staging_path,$(program));)
	$(foreach dir,$(MOUNT_POINTS),mkdir -m 0755 -p $(INITRD_STAGING)/$(dir);)
	mkdir -p $(@D)
	tar --format=ustar --owner=0 --group=0 --numeric-owner --sort=name \
	    -cf $@ -C $(INITRD_STAGING) $(sort $(PROGRAM_DIRS) $(MOUNT_POINTS))

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
