FROM docker.io/riscv64/ubuntu:20.04

COPY runtime-riscv64 /usr/local/lib/appimagetool/runtime

RUN export DEBIAN_FRONTEND=noninteractive && \
    apt update && \
    apt upgrade -y && \
    apt install --no-install-recommends -y \
        build-essential ca-certificates cmake git pkg-config \
        libgpgme-dev libgcrypt20-dev libglib2.0-dev libcurl4-openssl-dev \
        desktop-file-utils file squashfs-tools \
        busybox

RUN mkdir /build && cd /build && \
    git clone https://github.com/cyano-linux/appimagetool-riscv64 appimagetool && cd appimagetool && \
    cmake . -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build --parallel && \
    cmake --install build && \
    cd / && rm -rf /build
