include ../common.mk

ISO_DIR ?= $(PROJECT_ROOT)/isodir
GRUB_DIR := $(ISO_DIR)/boot/grub
ISO := $(PROJECT_ROOT)/mihos.iso

GRUB ?= grub-mkrescue
GRUBFLAGS ?= /usr/lib/grub/i386-pc
GRUB_CFG := grub.cfg

# Every program user/ installed, loaded by GRUB as a Multiboot2 module (see
# grub.cfg). The ISO mirrors the sysroot under /boot: <sysroot>/usr/tests/hello.elf
# → /boot/usr/tests/hello.elf. Evaluated when this Makefile starts, after user/.
PROGRAM_DIRS := bin sbin usr/tests
MODULES := $(foreach dir,$(PROGRAM_DIRS),$(wildcard $(SYSROOT_DIR)/$(dir)/*.elf))
iso_path = $(ISO_DIR)/boot$(patsubst $(SYSROOT_DIR)%,%,$1)

.PHONY: all iso

all: iso

iso: $(ISO)

$(ISO): $(BOOT_DIR)/kernel $(MODULES) $(GRUB_CFG)
	mkdir -p $(GRUB_DIR)
	cp $(GRUB_CFG) $(GRUB_DIR)
	cp $(BOOT_DIR)/kernel $(ISO_DIR)/boot/mihos.kernel
	$(foreach module,$(MODULES),mkdir -p $(dir $(call iso_path,$(module))) && cp $(module) $(call iso_path,$(module));)
	$(GRUB) $(GRUBFLAGS) -o $@ $(ISO_DIR)
