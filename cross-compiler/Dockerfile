# gcc 11.2.0 x86_64-mihos cross-compiler container

FROM debian:stretch

# ARG BINUTILS_VERSION=2.37
# ARG GCC_VERSION=11.2.0

# Install cross-compiler prerequisites
RUN set -x \
	&& apt-get update \
	&& apt-get upgrade -y \
	&& apt-get install -y wget gcc libgmp3-dev libmpfr-dev libisl-dev \
		libcloog-isl-dev libmpc-dev texinfo bison flex make bzip2 patch \
		build-essential git-all

# Pull binutils and gcc source code
RUN set -x \
	&& mkdir -p /usr/local/src \
	&& cd /usr/local/src \
	&& git clone https://github.com/caioaletroca/mihos-binutils.git \
	&& git clone https://github.com/caioaletroca/mihos-gcc.git \
	&& chown -R root:root mihos-binutils \
	&& chown -R root:root mihos-gcc \
	&& chmod -R o-w,g+w,g+x mihos-binutils \
	&& chmod -R o-w,g+w,g+x mihos-gcc

# Copy compile scripts
COPY src /usr/local/src/

# Copy Sysroot
COPY sysroot /root/env/sysroot

# Build and install binutils and the cross-compiler
RUN set -x \
	&& cd /usr/local/src \
	&& ./build-binutils.sh \
	&& ./build-gcc.sh

CMD ["/bin/bash"]

# # Install tools for grub
RUN apt-get install -y nasm
RUN apt-get install -y xorriso
RUN apt-get install -y grub-pc-bin
RUN apt-get install -y grub-common

# Mount source code volume
VOLUME /root/env
WORKDIR /root/env
