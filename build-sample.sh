#!/bin/bash

set -xe

mkdir -p /appimage-riscv64/out

mkdir /build && cd /build
mkdir busybox.AppDir
cp /bin/busybox busybox.AppDir
ln -s busybox busybox.AppDir/AppRun
cp /appimage-riscv64/sample-resources/busybox.{desktop,png} busybox.AppDir

appimagetool --runtime-file /usr/local/lib/appimagetool/runtime busybox.AppDir /appimage-riscv64/out/busybox-riscv64.AppImage
