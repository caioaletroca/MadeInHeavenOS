include ../common.mk

library := $(BINARY_DIR)/libk.a $(BINARY_DIR)/libc.a

all: install

install: install-headers install-libs

$(library): $(objects)
	${AR} rcs $@ $?

install-libs: $(library)
	mkdir -p $(LIB_DIR)
	cp $^ $(LIB_DIR)

install-headers:
	mkdir -p $(INCLUDE_DIR)
	cp -R --preserve=timestamps include/. $(INCLUDE_DIR)

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(depends)),)
-include $(depends)
endif
endif