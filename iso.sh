#!/bin/sh
set -e
. ./build.sh

mkdir -p isodir
mkdir -p isodir/boot
mkdir -p isodir/boot/grub

cp grub/grub.cfg isodir/boot/grub
cp sysroot/usr/local/boot/mihos.kernel isodir/boot/mihos.kernel
grub-mkrescue /usr/lib/grub/i386-pc -o mihos.iso isodir