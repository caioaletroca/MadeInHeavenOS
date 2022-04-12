include ../common.mk

#######################################
# Folders and Paths
ISO_DIR := /root/env/isodir
GRUB_DIR := $(ISO_DIR)/boot/grub

GRUB := grub-mkrescue
GRUBFLAGS := /usr/lib/grub/i386-pc
GRUB_CFG := grub.cfg

all: iso

iso: $(BOOT_DIR)/kernel
	mkdir -p $(GRUB_DIR)
	cp $(GRUB_CFG) $(GRUB_DIR)
	cp $^ $(ISO_DIR)/boot/mihos.kernel
	$(GRUB) $(GRUBFLAGS) -o ../mihos.iso $(ISO_DIR)