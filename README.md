# AppImage RISC-V 64

## The Tool

Use upstream [AppImage/appimagetool](https://github.com/AppImage/appimagetool) or modified [cyano-linux/appimagetool-riscv64](https://github.com/cyano-linux/appimagetool-riscv64).

The upstream appimagetool fails to detect riscv64 architecture. Since we have custom build of the runtime, this does not matter -- just set `ARCH=x86_64` when invoking appimagetool.

Official builds of appimagetool are distributed in AppImage format for portability. However, we are not meant to distribute the riscv64 build of appimagetool, so we simply build it with cmake on the same host where we build our app.

For example, on Ubuntu 20.04 riscv64:
```bash
apt install --no-install-recommends -y \
    build-essential cmake git pkg-config \
    libgpgme-dev libgcrypt20-dev libglib2.0-dev libcurl4-openssl-dev \
    desktop-file-utils file squashfs-tools

cd /path/to/appimagetool
cmake . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
cmake --install build
```

## The Runtime

We build the runtime statically targetting `riscv64-linux-musl`. The build environment is based on Ubuntu 20.04 riscv64 becuase Alpine Linux riscv64 is not stable yet.

Note: always use a container. Mixing glibc and musl may kill your system.

1. Prepare build environment
   ```bash
   podman build -t appimage-riscv64-runtime-builder runtime-builder
   ```
1. Build the runtime
   ```bash
   podman run -it --rm -v $PWD:/appimage-riscv64 -e GIT_COMMIT=$(git submodule status type2-runtime | awk '{ print substr($1, 0, 8) }') appimage-riscv64-runtime-builder /appimage-riscv64/build-runtime.sh
   ```

## Sample: busybox.AppImage

1. ```bash
   cp out/runtime-riscv64 sample-builder/runtime-riscv64
   ```
1. Prepare build environment
   ```bash
   podman build -t appimage-riscv64-sample-builder sample-builder
   ```
1. Build the sample
   ```bash
   podman run -it --rm -v $PWD:/appimage-riscv64 appimage-riscv64-sample-builder /appimage-riscv64/build-sample.sh
   ```
