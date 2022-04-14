#!/bin/bash

GCC_VERSION=$1

export PREFIX="/usr/local"
export TARGET=x86_64-mihos
export PATH="$PREFIX/bin:$PATH"

cd $PREFIX/src/gcc-${GCC_VERSION}/gcc
patch < config.gcc.patch

cd $PREFIX/src
mkdir build-gcc
cd build-gcc
../gcc-${GCC_VERSION}/configure --target=$TARGET --prefix="$PREFIX" --with-sysroot="$PREFIX/src/sysroot" --disable-nls --enable-languages=c,c++
make -j 4 all-gcc
make -j 4 all-target-libgcc
make install-gcc
make install-target-libgcc

cd $PREFIX/src
rm -rf build-gcc.sh build-gcc gcc-${GCC_VERSION}
