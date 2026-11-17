#!/bin/bash
# Stop at the first failing step: without it a failed libgcc build left an image with no libgcc
set -e

export PREFIX="/usr/local"
export VOLUME="/root/env"
export TARGET=x86_64-mihos
export PATH="$PREFIX/bin:$PATH"

cd $PREFIX/src
mkdir build-gcc
cd build-gcc
../mihos-gcc/configure --target=$TARGET --prefix="$PREFIX" --with-sysroot="$VOLUME/sysroot" --disable-nls --disable-gcov --enable-languages=c,c++
make -j 4 all-gcc
make -j 4 all-target-libgcc CFLAGS_FOR_TARGET='-g -O2 -mcmodel=kernel -mno-red-zone'
make install-gcc
make install-target-libgcc

# The build tree stays: build-libstdc++.sh builds libstdc++ in it once mlibc
# is in the sysroot, then deletes it
