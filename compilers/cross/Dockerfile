# gcc 16.2.0 / binutils 2.47 x86_64-mihos cross-compiler container, with
# libstdc++ on mlibc. Built from the repository's root (docker-build.bat):
# Dockerfile.dockerignore lets in only what is copied below.

FROM debian:latest

# Install cross-compiler prerequisites; meson and ninja build mlibc, here
# for libstdc++ and in the OS build
RUN set -x \
	&& apt-get update \
	&& apt-get upgrade -y \
	&& apt-get install -y wget gcc libgmp-dev libmpfr-dev libisl-dev \
		libmpc-dev texinfo bison flex make bzip2 patch \
		build-essential git-all meson ninja-build

# Pull binutils and gcc source code
RUN set -x \
	&& mkdir -p /usr/local/src \
	&& cd /usr/local/src \
	&& git clone --depth 1 --branch mihos-2.47 https://github.com/MiH-Operating-System/binutils.git mihos-binutils \
	&& git clone --depth 1 --branch mihos-16 https://github.com/MiH-Operating-System/gcc.git mihos-gcc \
	&& chown -R root:root mihos-binutils \
	&& chown -R root:root mihos-gcc \
	&& chmod -R o-w,g+w,g+x mihos-binutils \
	&& chmod -R o-w,g+w,g+x mihos-gcc

# The sysroot's headers (make toolchain-headers): GCC and libgcc build against them
COPY compilers/cross/sysroot /root/env/sysroot

# Build and install binutils
COPY --chmod=755 compilers/cross/src/build-binutils.sh /usr/local/src/build-binutils.sh
RUN sed -i 's/\r$//' /usr/local/src/build-binutils.sh \
	&& /usr/local/src/build-binutils.sh

# Build and install the cross-compiler and libgcc
COPY --chmod=755 compilers/cross/src/build-gcc.sh /usr/local/src/build-gcc.sh
RUN sed -i 's/\r$//' /usr/local/src/build-gcc.sh \
	&& /usr/local/src/build-gcc.sh

# mlibc, then libstdc++ against it. After GCC, so a change in mlibc rebuilds
# only this part
COPY mlibc /usr/local/src/mlibc
COPY kernel/include/uapi /usr/local/src/mlibc-inputs/uapi
COPY config/x86_64-mihos.cross /usr/local/src/mlibc-inputs/x86_64-mihos.cross
COPY --chmod=755 compilers/cross/src/build-libstdc++.sh /usr/local/src/build-libstdc++.sh
RUN sed -i 's/\r$//' /usr/local/src/build-libstdc++.sh \
	&& /usr/local/src/build-libstdc++.sh

CMD ["/bin/bash"]

# Install tools for grub
RUN apt-get install -y xorriso grub-pc-bin grub-common

# mke2fs (-d) builds the test disk image (disk.img) from disk/
RUN apt-get update && apt-get install -y e2fsprogs

# Mount source code volume
VOLUME /root/env
WORKDIR /root/env
