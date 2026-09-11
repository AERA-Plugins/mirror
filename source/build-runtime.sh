#!/bin/bash
set -euo pipefail
source_dir=$(cd -- "$(dirname -- "$0")" && pwd)
output=${1:-"$source_dir/../stage"}
sysroot=${AERA_MUSL_SYSROOT:-/tmp/aera-webkit-sysroot}
toolchain=${AERA_ANDROID_CLANG:-/home/koaan/Desktop/AERA_16.0/prebuilts/clang/host/linux-x86/clang-r547379/bin}
cc=${AERA_CC:-$toolchain/clang}
strip=${AERA_STRIP:-$toolchain/llvm-strip}
build=$(mktemp -d)
trap 'rm -rf "$build"' EXIT

"$cc" --target=aarch64-alpine-linux-musl --sysroot="$sysroot" \
  --gcc-toolchain="$sysroot/usr" -fuse-ld=lld \
  -std=c11 -Os -g0 -Wall -Wextra -Werror -ffunction-sections \
  -fdata-sections -static "$source_dir/main.c" -Wl,--gc-sections \
  -Wl,-z,relro,-z,now -o "$build/aera-plugin"
"$strip" --strip-unneeded "$build/aera-plugin"
rm -rf "$output"
mkdir -p "$output/usr/bin"
cp "$build/aera-plugin" "$output/usr/bin/aera-plugin"
echo "Staged AERA Mirror runtime: $(du -sh "$output" | cut -f1)"
