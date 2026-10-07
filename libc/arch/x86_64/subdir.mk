# crt0.S is not part of libc.a: it is built and installed on its own as crt0.o
# (see libc/Makefile), since the linker only pulls archive members to resolve
# undefined symbols and nothing references _start.
