ARCH_FAMILY ?= x86
ARCH ?= x86_64
PLATFORM ?= pc
HOST ?= x86_64-mihos

BUILD_ID := $(HOST)/$(ARCH)-$(PLATFORM)
KERNEL := kernel/build/$(BUILD_ID)/kernel
OBJDUMP ?= $(HOST)-objdump

DISK_IMAGE := disk.img
DISK_SIZE := 4M
DISK_FILES := $(shell find disk -type f)
DISK_STAGING := build/disk
PART_IMAGE := part.img
PART_ROOT_STAGING := build/root
INITRD := grub/build/$(BUILD_ID)/initrd.tar

.PHONY: all headers libc kernel user grub install clean purge dump print-config

all: grub $(DISK_IMAGE) $(PART_IMAGE)

# The test disk: an ext2 file system (1 KiB blocks), attached by run-tests.sh
# and .bochsrc next to the boot CD, rebuilt when a file changes. Made from
# disk/ plus files generated here, too big, sparse or long-named for git:
#   big.txt   300 KiB of 8-byte lines "%07d\n", each holding its own line
#             number (byte offset = 8 * line), so a block read from the wrong
#             place shows; 300 blocks: direct, single and double indirect
#   sparse    "start", a hole up to 16 KiB, "end": no blocks for the middle
#   aaa...a   a 255-character name, the longest ext2 allows
#   link      a symlink to hello.txt (the VFS has no symlinks yet: skipped)
#   grow/     files the tests grow (no truncate yet: other tests' files stay
#             as they are): small (5 bytes), empty, holes (sparse's layout),
#             long (big.txt's content, 300 blocks), user (the ext2 test's own)
$(DISK_IMAGE): $(DISK_FILES) Makefile
	rm -rf $@ $(DISK_STAGING)
	mkdir -p $(DISK_STAGING)
	cp -R disk/. $(DISK_STAGING)/
	seq -f '%07g' 0 38399 > $(DISK_STAGING)/big.txt
	printf 'start' > $(DISK_STAGING)/sparse
	truncate -s 16384 $(DISK_STAGING)/sparse
	printf 'end' >> $(DISK_STAGING)/sparse
	echo 'The longest name.' > "$(DISK_STAGING)/$$(printf 'a%.0s' $$(seq 255))"
	ln -s hello.txt $(DISK_STAGING)/link
	mkdir -p $(DISK_STAGING)/grow
	printf 'small' > $(DISK_STAGING)/grow/small
	touch $(DISK_STAGING)/grow/empty $(DISK_STAGING)/grow/user
	cp $(DISK_STAGING)/sparse $(DISK_STAGING)/grow/holes
	cp $(DISK_STAGING)/big.txt $(DISK_STAGING)/grow/long
	mke2fs -q -F -t ext2 -b 1024 -L mihos-test -E root_owner=0:0 -d $(DISK_STAGING) $@ $(DISK_SIZE)

# The partitioned disk, attached as the second disk next to disk.img: an MBR
# and the self-test's markers (the script lists its layout), and on partition
# 1 (sector 2048, 12 MiB) the installed system: an ext2 holding the initrd's
# tree, the root of the "from disk" boot. mke2fs -E offset writes the file
# system inside the partition and leaves the rest of the image as it is.
$(PART_IMAGE): disk-tools/partition-disk.sh $(INITRD) Makefile
	rm -rf $@ $(PART_ROOT_STAGING)
	mkdir -p $(PART_ROOT_STAGING)
	tar -xf $(INITRD) -C $(PART_ROOT_STAGING)
	sh disk-tools/partition-disk.sh $@
	mke2fs -q -F -t ext2 -b 1024 -L mihos-root -E offset=$$((2048 * 512)),root_owner=0:0 \
	    -d $(PART_ROOT_STAGING) $@ 12288

# Made by grub/ (the "grub" target, before this disk in "all")
$(INITRD):
	$(MAKE) grub

# The kernel/user contract (<mihos/...>) first: libc and user programs include it
headers:
	$(MAKE) -C kernel install-headers

libc: headers
	$(MAKE) -C libc

kernel: libc
	$(MAKE) -C kernel

# After kernel and libc: programs link against the sysroot
user: kernel
	$(MAKE) -C user

grub: kernel user
	$(MAKE) -C grub

install: libc
	$(MAKE) -C kernel install

clean:
	$(MAKE) -C libc clean
	$(MAKE) -C kernel clean
	$(MAKE) -C user clean
	$(MAKE) -C grub clean
	rm -rf isodir sysroot mihos.iso $(DISK_IMAGE) $(PART_IMAGE) build

purge:
	$(MAKE) -C libc purge
	$(MAKE) -C kernel purge
	$(MAKE) -C user purge
	$(MAKE) -C grub purge
	rm -rf isodir sysroot mihos.iso $(DISK_IMAGE) $(PART_IMAGE) build

dump: $(KERNEL)
	$(OBJDUMP) -d $(KERNEL) > mihos.txt

print-config:
	$(MAKE) -C kernel print-config
