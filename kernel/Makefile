include ../common.mk

###########################################
# Folders and Flags
ARCH_BINARY_DIR := $(BINARY_DIR)/arch/$(ARCH)
CPPFLAGS += -D__is_kernel

###########################################

kernel := $(BINARY_DIR)/kernel

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
$(libk) \
$(objects) \
$(ARCH_BINARY_DIR)/crtbegin.o \
$(LDFLAGS_EXTRA) \
$(ARCH_BINARY_DIR)/crtend.o \

$(kernel): $(GCC_GENERATED_OBJS) $(objects) $(libk) $(ARCH_BINARY_DIR)/linker.lds.i
	$(CC) -n -T $(ARCH_BINARY_DIR)/linker.lds.i -o $@ $(CFLAGS) $(LDFLAGS) $(LINK_LIST)

$(ARCH_BINARY_DIR)/linker.lds.i: $(ARCH_DIR)/linker.lds
	$(CC) -E -P -x c $(ARCH_DIR)/linker.lds -o $(ARCH_BINARY_DIR)/linker.lds.i

$(ARCH_BINARY_DIR)/crtbegin.o $(ARCH_BINARY_DIR)/crtend.o:
	OBJ=`$(CC) $(CFLAGS) $(LDFLAGS) -print-file-name=$(@F)` && cp "$$OBJ" $@

-include $(GCC_GENERATED_OBJS:.o=.d)

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(depends)),)
-include $(depends)
endif
endif