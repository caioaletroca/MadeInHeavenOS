include ../common.mk

CPPFLAGS += -D__is_kernel

kernel := $(BINARY_DIR)/kernel

.PHONY: list

all: install

install: install-headers install-kernel

install-headers:
	mkdir -p $(INCLUDE_DIR)
	cp -R --preserve=timestamps include/. $(INCLUDE_DIR)

install-kernel: $(kernel)
	mkdir -p $(BOOT_DIR)
	cp $^ $(BOOT_DIR)

KERNEL_ARCH_OBJS=\
$(BINARY_DIR)/arch/$(ARCH)/boot.o \
$(BINARY_DIR)/arch/$(ARCH)/boot64.o \
$(BINARY_DIR)/arch/$(ARCH)/header.o \

KERNEL_OBJS=\
$(KERNEL_ARCH_OBJS) \
$(BINARY_DIR)/kmain.o

LIBS:=$(LIBS) -nostdlib -lk -lgcc

OBJS=\
$(BINARY_DIR)/arch/$(ARCH)/crtbegin.o \
$(BINARY_DIR)/arch/$(ARCH)/crtend.o \

LINK_LIST=\
$(LDFLAGS) \
$(BINARY_DIR)/arch/$(ARCH)/crti.o \
$(BINARY_DIR)/arch/$(ARCH)/crtbegin.o \
$(KERNEL_OBJS) \
$(LIBS) \
$(BINARY_DIR)/arch/$(ARCH)/crtend.o \
$(BINARY_DIR)/arch/$(ARCH)/crtn.o \

$(kernel): $(OBJS) $(objects) $(libk) $(ARCH_DIR)/linker.ld
	$(CC) -n -T $(ARCH_DIR)/linker.ld -o $@ $(CFLAGS) $(LDFLAGS) $(libk) $(LINK_LIST)

$(BINARY_DIR)/arch/$(ARCH)/crtbegin.o $(BINARY_DIR)/arch/$(ARCH)/crtend.o:
	OBJ=`$(CC) $(CFLAGS) $(LDFLAGS) -print-file-name=$(@F)` && cp "$$OBJ" $@

-include $(OBJS:.o=.d)

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(depends)),)
-include $(depends)
endif
endif

# DEFAULT_HOST!=../default-host.sh
# HOST?=DEFAULT_HOST
# HOSTARCH!=../target-triplet-to-arch.sh $(HOST)

# CFLAGS?=-O2 -g
# CPPFLAGS?=
# LDFLAGS?=
# LIBS?=

# DESTDIR?=
# PREFIX?=/usr/local
# EXEC_PREFIX?=$(PREFIX)
# BOOTDIR?=$(EXEC_PREFIX)/boot
# INCLUDEDIR?=$(PREFIX)/include

# CFLAGS:=$(CFLAGS) -ffreestanding -Wall -Wextra
# CPPFLAGS:=$(CPPFLAGS) -D__is_kernel -Iinclude
# LDFLAGS:=$(LDFLAGS)
# LIBS:=$(LIBS) -nostdlib -lk -lgcc

# ARCHDIR=arch/$(HOSTARCH)

# include $(ARCHDIR)/make.config

# CFLAGS:=$(CFLAGS) $(KERNEL_ARCH_CFLAGS)
# CPPFLAGS:=$(CPPFLAGS) $(KERNEL_ARCH_CPPFLAGS)
# LDFLAGS:=$(LDFLAGS) $(KERNEL_ARCH_LDFLAGS)
# LIBS:=$(LIBS) $(KERNEL_ARCH_LIBS)

# KERNEL_OBJS=\
# $(KERNEL_ARCH_OBJS) \
# kmain.o \

# OBJS=\
# $(ARCHDIR)/crti.o \
# $(ARCHDIR)/crtbegin.o \
# $(KERNEL_OBJS) \
# $(ARCHDIR)/crtend.o \
# $(ARCHDIR)/crtn.o \

# LINK_LIST=\
# $(LDFLAGS) \
# $(ARCHDIR)/crti.o \
# $(ARCHDIR)/crtbegin.o \
# $(KERNEL_OBJS) \
# $(LIBS) \
# $(ARCHDIR)/crtend.o \
# $(ARCHDIR)/crtn.o \

# .PHONY: all clean install install-headers install-kernel
# .SUFFIXES: .o .c .asm

# all: mihos.kernel

# mihos.kernel: $(OBJS) $(ARCHDIR)/linker.ld
# 	$(CC) -n -T $(ARCHDIR)/linker.ld -o $@ $(CFLAGS) $(LINK_LIST)

# $(ARCHDIR)/crtbegin.o $(ARCHDIR)/crtend.o:
# 	OBJ=`$(CC) $(CFLAGS) $(LDFLAGS) -print-file-name=$(@F)` && cp "$$OBJ" $@

# .c.o:
# 	$(CC) -MD -c $< -o $@ -std=gnu11 $(CFLAGS) $(CPPFLAGS)

# .asm.o:
# 	nasm -f elf64 $< -o $@

# clean:
# 	rm -f mihos.kernel
# 	rm -f $(OBJS) *.o */*.o */*/*.o
# 	rm -f $(OBJS:.o=.d) *.d */*.d */*/*.d

# install: install-headers install-kernel

# install-headers:
# 	mkdir -p $(DESTDIR)$(INCLUDEDIR)
# 	cp -R --preserve=timestamps driver/. $(DESTDIR)$(INCLUDEDIR)/driver
# 	cp -R --preserve=timestamps include/. $(DESTDIR)$(INCLUDEDIR)/.

# install-kernel: mihos.kernel
# 	mkdir -p $(DESTDIR)$(BOOTDIR)
# 	cp mihos.kernel $(DESTDIR)$(BOOTDIR)

# -include $(OBJS:.o=.d)