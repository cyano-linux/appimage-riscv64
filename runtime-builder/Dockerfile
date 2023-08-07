FROM docker.io/riscv64/ubuntu:20.04

RUN export DEBIAN_FRONTEND=noninteractive && \
    apt update && \
    apt upgrade -y && \
    apt install -y --no-install-recommends \
        ca-certificates curl xz-utils \
        linux-libc-dev musl-dev \
        autoconf automake libtool make meson musl-tools pkg-config

ARG CC=musl-gcc
ARG CFLAGS="-ffunction-sections -fdata-sections -Os"
ARG PREFIX=/usr
ARG INCLUDEDIR=${PREFIX}/include/riscv64-linux-musl
ARG LIBDIR=${PREFIX}/lib/riscv64-linux-musl

ARG FUSE_VERSION=3.12.0
ARG ZSTD_VERSION=1.5.5
ARG SUQASHFUSE_VERSION=0.1.105

RUN export CC=${CC} CFLAGS="${CFLAGS}" PREFIX=${PREFIX} INCLUDEDIR=${INCLUDEDIR} LIBDIR=${LIBDIR} MAKEFLAGS=-j$(nproc) PKG_CONFIG_PATH=${LIBDIR}/pkgconfig && \
    ## fuse3
    mkdir /build && cd /build && \
    curl -L https://github.com/libfuse/libfuse/releases/download/fuse-${FUSE_VERSION}/fuse-${FUSE_VERSION}.tar.xz | tar -xJ && cd fuse-${FUSE_VERSION} && \
    meson setup --prefix ${PREFIX} --includedir ${INCLUDEDIR} --libdir ${LIBDIR} --buildtype minsize --default-library both --strip -D udevrulesdir=/lib/udev/rules.d -D utils=false -D examples=false -D tests=false build && cd build && \
    ninja && \
    ninja install && \
    ## zstd
    cd /build && \
    curl -L https://github.com/facebook/zstd/releases/download/v${ZSTD_VERSION}/zstd-${ZSTD_VERSION}.tar.gz | tar -xz && cd zstd-${ZSTD_VERSION} && \
    make -C lib HAVE_PTHREAD=1 && \
    make -C lib install && \
    ## squashfuse
    cd /build && \
    curl -L https://github.com/vasi/squashfuse/archive/refs/tags/${SUQASHFUSE_VERSION}.tar.gz | tar -xz && cd squashfuse-${SUQASHFUSE_VERSION} && \
    ./autogen.sh && \
    ./configure CFLAGS="${CFLAGS} -no-pie" LDFLAGS=-static --prefix=${PREFIX} --includedir=${INCLUDEDIR} --libdir=${LIBDIR} --disable-demo && \
    make && \
    make install && \
    install -c -m 644 ./*.h ${INCLUDEDIR}/squashfuse && \
    cd / && rm -rf /build
