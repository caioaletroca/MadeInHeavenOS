ARCH_FAMILY ?= x86
ARCH ?= x86_64
PLATFORM ?= pc
HOST ?= x86_64-mihos

BUILD_ID := $(HOST)/$(ARCH)-$(PLATFORM)
KERNEL := kernel/build/$(BUILD_ID)/kernel
OBJDUMP ?= $(HOST)-objdump

.PHONY: all libc kernel grub install clean purge dump print-config

all: grub

libc:
	$(MAKE) -C libc

kernel: libc
	$(MAKE) -C kernel

grub: kernel
	$(MAKE) -C grub

install: libc
	$(MAKE) -C kernel install

clean:
	$(MAKE) -C libc clean
	$(MAKE) -C kernel clean
	$(MAKE) -C grub clean
	rm -rf isodir sysroot mihos.iso

purge:
	$(MAKE) -C libc purge
	$(MAKE) -C kernel purge
	$(MAKE) -C grub purge
	rm -rf isodir sysroot mihos.iso

dump: $(KERNEL)
	$(OBJDUMP) -d $(KERNEL) > mihos.txt

print-config:
	$(MAKE) -C kernel print-config
