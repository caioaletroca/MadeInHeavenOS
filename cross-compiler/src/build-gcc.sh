#!/bin/bash

export PREFIX="/usr/local"
export VOLUME="/root/env"
export TARGET=x86_64-mihos
export PATH="$PREFIX/bin:$PATH"

cd $PREFIX/src
mkdir build-gcc
cd build-gcc
../mihos-gcc/configure --target=$TARGET --prefix="$PREFIX" --with-sysroot="$VOLUME/sysroot" --disable-nls --enable-languages=c,c++
make -j 4 all-gcc
make -j 4 all-target-libgcc
make install-gcc
make install-target-libgcc

cd $PREFIX/src
# rm -rf build-gcc.sh build-gcc mihos-gcc
