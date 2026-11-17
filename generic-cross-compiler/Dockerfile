FROM randomdude/gcc-cross-x86_64-elf

RUN apt-get update
RUN apt-get upgrade -y

RUN apt-get install -y nasm
RUN apt-get install -y xorriso
RUN apt-get install -y grub-pc-bin
RUN apt-get install -y grub-common
RUN apt-get install -y e2fsprogs

# meson and ninja install mlibc's headers (make toolchain-headers): stage 1's input
RUN apt-get install -y meson ninja-build

VOLUME /root/env
WORKDIR /root/env