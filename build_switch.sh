#!/bin/bash
set -e
export DEVKITPRO=/opt/devkitpro
export DEVKITA64=/opt/devkitpro/devkitA64
export PATH=$DEVKITA64/bin:$DEVKITPRO/tools/bin:$PATH

echo "=== GCC ==="
aarch64-none-elf-gcc --version | head -n 1

echo "=== CMAKE CONFIGURE ==="
cmake -S switch -B build-switch -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/Switch.cmake \
  -DCMAKE_BUILD_TYPE=Release

echo "=== BUILD ELF & NRO ==="
ninja -C build-switch -j4
