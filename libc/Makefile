include ../common.mk

###########################################
# Two libraries from one source tree
#   libk.a  freestanding subset linked into the kernel: kernel flags
#           (ARCH_CFLAGS, -mcmodel=kernel), -D__is_libk
#   libc.a  for user programs: USER_CFLAGS, -D__is_libc, plus the hosted
#           parts (syscalls, stdio over fds, ...)
# In subdir.mk, local_sources go into both, hosted_local_sources only into libc.a.
# Each library has its own object tree, since the same file is compiled twice.

libk_dir := $(BINARY_DIR)/libk
libc_dir := $(BINARY_DIR)/libc

to_objects = $(patsubst $(SOURCE_DIR)/%,$2/%,$(addsuffix .o,$(basename $1)))

libk_objects := $(call to_objects,$(sources),$(libk_dir))
libc_objects := $(call to_objects,$(sources) $(hosted_sources),$(libc_dir))

libk_archive := $(BINARY_DIR)/libk.a
libc_archive := $(BINARY_DIR)/libc.a

# Program entry, linked first by every user program (not an archive member)
crt0 := $(BINARY_DIR)/crt0.o

# libc's private headers: internal/ (syscall macros) and the arch's
# syscall_arch.h. Never installed: user code calls write(), not __syscall3.
LIBC_PRIVATE_CPPFLAGS := -Iinternal -Iarch/$(ARCH)

###########################################

.PHONY: all install install-headers install-libs

all: install

install: install-headers install-libs

$(libk_dir)/%.o: $(SOURCE_DIR)/%.c
	mkdir -p $(@D)
	$(CC) -MMD -MP -c $< -o $@ $(CPPFLAGS) -D__is_libk $(CFLAGS)

$(libk_dir)/%.o: $(SOURCE_DIR)/%.S
	mkdir -p $(@D)
	$(CC) -MMD -MP -c $< -o $@ $(CPPFLAGS) -D__is_libk $(CFLAGS)

$(libc_dir)/%.o: $(SOURCE_DIR)/%.c
	mkdir -p $(@D)
	$(CC) -MMD -MP -c $< -o $@ $(CPPFLAGS) $(LIBC_PRIVATE_CPPFLAGS) -D__is_libc $(USER_CFLAGS)

$(libc_dir)/%.o: $(SOURCE_DIR)/%.S
	mkdir -p $(@D)
	$(CC) -MMD -MP -c $< -o $@ $(CPPFLAGS) $(LIBC_PRIVATE_CPPFLAGS) -D__is_libc $(USER_CFLAGS)

$(crt0): $(SOURCE_DIR)/arch/$(ARCH)/crt0.S
	mkdir -p $(@D)
	$(CC) -MMD -MP -c $< -o $@ $(CPPFLAGS) $(LIBC_PRIVATE_CPPFLAGS) -D__is_libc $(USER_CFLAGS)

$(libk_archive): $(libk_objects)
	mkdir -p $(@D)
	rm -f $@
	$(AR) rcs $@ $^

$(libc_archive): $(libc_objects)
	mkdir -p $(@D)
	rm -f $@
	$(AR) rcs $@ $^

install-libs: $(libk_archive) $(libc_archive) $(crt0)
	mkdir -p $(LIB_DIR)
	cp $^ $(LIB_DIR)

install-headers:
	mkdir -p $(INCLUDE_DIR)
	cp -R --preserve=timestamps include/. $(INCLUDE_DIR)

ifeq ($(filter clean purge,$(MAKECMDGOALS)),)
-include $(libk_objects:.o=.d) $(libc_objects:.o=.d) $(crt0:.o=.d)
endif
