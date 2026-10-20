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

.PHONY: all headers libc kernel user grub install clean purge dump print-config

all: grub $(DISK_IMAGE)

# The test disk: an ext2 file system (1 KiB blocks) made from disk/, attached
# by run-tests.sh and .bochsrc next to the boot CD. Rebuilt when a file changes
$(DISK_IMAGE): $(DISK_FILES) Makefile
	rm -f $@
	mke2fs -q -F -t ext2 -b 1024 -L mihos-test -E root_owner=0:0 -d disk $@ $(DISK_SIZE)

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
	rm -rf isodir sysroot mihos.iso $(DISK_IMAGE)

purge:
	$(MAKE) -C libc purge
	$(MAKE) -C kernel purge
	$(MAKE) -C user purge
	$(MAKE) -C grub purge
	rm -rf isodir sysroot mihos.iso $(DISK_IMAGE)

dump: $(KERNEL)
	$(OBJDUMP) -d $(KERNEL) > mihos.txt

print-config:
	$(MAKE) -C kernel print-config
