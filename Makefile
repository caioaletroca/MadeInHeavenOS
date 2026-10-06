ARCH_FAMILY ?= x86
ARCH ?= x86_64
PLATFORM ?= pc
HOST ?= x86_64-mihos

BUILD_ID := $(HOST)/$(ARCH)-$(PLATFORM)
KERNEL := kernel/build/$(BUILD_ID)/kernel
OBJDUMP ?= $(HOST)-objdump

.PHONY: all headers libc kernel user grub install clean purge dump print-config

all: grub

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
	rm -rf isodir sysroot mihos.iso

purge:
	$(MAKE) -C libc purge
	$(MAKE) -C kernel purge
	$(MAKE) -C user purge
	$(MAKE) -C grub purge
	rm -rf isodir sysroot mihos.iso

dump: $(KERNEL)
	$(OBJDUMP) -d $(KERNEL) > mihos.txt

print-config:
	$(MAKE) -C kernel print-config
