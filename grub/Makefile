include ../common.mk

ISO_DIR ?= $(PROJECT_ROOT)/isodir
GRUB_DIR := $(ISO_DIR)/boot/grub
ISO := $(PROJECT_ROOT)/mihos.iso

GRUB ?= grub-mkrescue
GRUBFLAGS ?= /usr/lib/grub/i386-pc
GRUB_CFG := grub.cfg

# User programs loaded by GRUB as Multiboot2 modules (see grub.cfg)
MODULES := $(BIN_DIR)/hello.elf

.PHONY: all iso

all: iso

iso: $(ISO)

$(ISO): $(BOOT_DIR)/kernel $(MODULES) $(GRUB_CFG)
	mkdir -p $(GRUB_DIR)
	cp $(GRUB_CFG) $(GRUB_DIR)
	cp $(BOOT_DIR)/kernel $(ISO_DIR)/boot/mihos.kernel
	cp $(MODULES) $(ISO_DIR)/boot/
	$(GRUB) $(GRUBFLAGS) -o $@ $(ISO_DIR)
