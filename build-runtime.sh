#!/bin/bash

set -xe

mkdir -p /appimage-riscv64/out

CFLAGS="-std=gnu99 -s -Os -D_FILE_OFFSET_BITS=64 -DGIT_COMMIT=\"${GIT_COMMIT}\" -T data_sections.ld -ffunction-sections -fdata-sections -Wl,--gc-sections -static"
LIBS="-lsquashfuse -lsquashfuse_ll -lzstd"

cd /appimage-riscv64/type2-runtime/src/runtime
musl-gcc -o runtime-fuse3.o -c $CFLAGS runtime.c
musl-gcc $CFLAGS runtime-fuse3.o $LIBS -lfuse3 -o runtime-fuse3
strip runtime-fuse3
echo -ne 'AI\x02' | dd of=runtime-fuse3 bs=1 count=3 seek=8 conv=notrunc # magic bytes, always do AFTER strip
rm runtime-fuse3.o
mv runtime-fuse3 /appimage-riscv64/out/runtime-riscv64
