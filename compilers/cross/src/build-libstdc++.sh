#!/bin/bash
# libstdc++, GCC's C++ library, built on mlibc (roadmap D10e). Its configure
# links test programs to see what the C library offers, so mlibc goes into
# the sysroot GCC was configured with first: libc.a and crt1.o beside the
# headers. Then the GCC build tree build-gcc.sh left builds libstdc++, and
# both source trees go.
set -e

export PREFIX="/usr/local"
export VOLUME="/root/env"
export TARGET=x86_64-mihos
export PATH="$PREFIX/bin:$PATH"

cd $PREFIX/src

# As the top Makefile's mlibc target: the sysdeps take the system call
# numbers from the kernel's <mihos/syscall.h>
meson setup --cross-file mlibc-inputs/x86_64-mihos.cross --prefix=/usr \
    -Ddefault_library=static -Dcpp_args=-I$PREFIX/src/mlibc-inputs/uapi build-mlibc mlibc
ninja -C build-mlibc
DESTDIR=$VOLUME/sysroot ninja -C build-mlibc install

cd build-gcc
make -j 4 all-target-libstdc++-v3
make install-target-libstdc++-v3

cd $PREFIX/src
rm -rf build-gcc mihos-gcc build-mlibc mlibc mlibc-inputs
