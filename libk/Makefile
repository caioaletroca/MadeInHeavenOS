include ../common.mk

###########################################
# libk.a, the freestanding C library linked into the kernel: kernel flags
# (ARCH_CFLAGS, -mcmodel=kernel), -D__is_libk, from the local_sources of each
# subdir.mk. Never installed: the kernel links it from here and includes
# include/ directly (common.mk's FREESTANDING_CPPFLAGS).
#
# User programs are on mlibc since roadmap D10d. Our libc's user side
# (hosted_local_sources, crt0.S, cxx/) is no longer built; its sources stay
# in the tree for now.

libk_dir := $(BINARY_DIR)/libk
libk_objects := $(patsubst $(SOURCE_DIR)/%,$(libk_dir)/%,$(addsuffix .o,$(basename $(sources))))
libk_archive := $(BINARY_DIR)/libk.a

# libk.a sees only its own headers, never the sysroot (common.mk)
LIBK_CPPFLAGS := -Iinclude $(FREESTANDING_CPPFLAGS)

###########################################

.PHONY: all

all: $(libk_archive)

$(libk_dir)/%.o: $(SOURCE_DIR)/%.c
	mkdir -p $(@D)
	$(CC) -MD -MP -c $< -o $@ $(LIBK_CPPFLAGS) -D__is_libk $(CFLAGS)

$(libk_dir)/%.o: $(SOURCE_DIR)/%.S
	mkdir -p $(@D)
	$(CC) -MD -MP -c $< -o $@ $(LIBK_CPPFLAGS) -D__is_libk $(CFLAGS)

$(libk_archive): $(libk_objects)
	mkdir -p $(@D)
	rm -f $@
	$(AR) rcs $@ $^

ifeq ($(filter clean purge,$(MAKECMDGOALS)),)
-include $(libk_objects:.o=.d)
endif
