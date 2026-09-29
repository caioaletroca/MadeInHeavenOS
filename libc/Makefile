include ../common.mk

library := $(BINARY_DIR)/libk.a $(BINARY_DIR)/libc.a

.PHONY: all install install-headers install-libs

all: install

install: install-headers install-libs

$(library): $(objects)
	mkdir -p $(@D)
	rm -f $@
	$(AR) rcs $@ $^

install-libs: $(library)
	mkdir -p $(LIB_DIR)
	cp $^ $(LIB_DIR)

install-headers:
	mkdir -p $(INCLUDE_DIR)
	cp -R --preserve=timestamps include/. $(INCLUDE_DIR)

ifeq ($(filter clean purge,$(MAKECMDGOALS)),)
ifneq ($(strip $(depends)),)
-include $(depends)
endif
endif