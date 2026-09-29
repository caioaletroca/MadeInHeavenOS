include ../common.mk

###########################################
# Folders and Flags
ARCH_BINARY_DIR := $(BINARY_DIR)/arch/$(ARCH_FAMILY)/$(ARCH)
CPPFLAGS += -D__is_kernel

###########################################

kernel := $(BINARY_DIR)/kernel

.PHONY: all install install-headers install-kernel

all: install

install: install-headers install-kernel

install-headers:
	mkdir -p $(INCLUDE_DIR)
	cp -R --preserve=timestamps include/. $(INCLUDE_DIR)

install-kernel: $(kernel)
	mkdir -p $(BOOT_DIR)
	cp $^ $(BOOT_DIR)

GCC_GENERATED_OBJS=\
$(ARCH_BINARY_DIR)/crtbegin.o \
$(ARCH_BINARY_DIR)/crtend.o \

LINK_LIST=\
$(ARCH_BINARY_DIR)/crtbegin.o \
$(objects) \
$(libk) \
$(LDFLAGS_EXTRA) \
$(ARCH_BINARY_DIR)/crtend.o \

$(kernel): $(GCC_GENERATED_OBJS) $(objects) $(libk) $(ARCH_BINARY_DIR)/linker.lds.i
	mkdir -p $(@D)
	$(CC) -n -T $(ARCH_BINARY_DIR)/linker.lds.i -o $@ $(CFLAGS) $(LDFLAGS) $(LINK_LIST)

$(ARCH_BINARY_DIR)/linker.lds.i: $(ARCH_DIR)/linker.lds
	mkdir -p $(@D)
	$(CC) -E -P -x c $(CPPFLAGS) $(ARCH_DIR)/linker.lds -o $@

$(ARCH_BINARY_DIR)/crtbegin.o $(ARCH_BINARY_DIR)/crtend.o:
	mkdir -p $(@D)
	OBJ=`$(CC) $(CFLAGS) $(LDFLAGS) -print-file-name=$(@F)` && cp "$$OBJ" $@

-include $(GCC_GENERATED_OBJS:.o=.d)

ifeq ($(filter clean purge,$(MAKECMDGOALS)),)
ifneq ($(strip $(depends)),)
-include $(depends)
endif
endif