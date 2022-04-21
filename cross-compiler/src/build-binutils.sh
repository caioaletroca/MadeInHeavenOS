#!/bin/bash

export PREFIX="/usr/local"
export VOLUME="/root/env"
export TARGET=x86_64-mihos
export PATH="$PREFIX/bin:$PATH"

cd $PREFIX/src
mkdir build-binutils
cd build-binutils
../mihos-binutils/configure --target=$TARGET --prefix="$PREFIX" --with-sysroot="$VOLUME/sysroot" --disable-nls --disable-werror
make -j 4
make install

cd $PREFIX/src
rm -rf build-binutils.sh build-binutils mihos-binutils
