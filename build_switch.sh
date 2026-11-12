#!/bin/bash
set -e

if [ ! -d "/opt/devkitpro" ]; then
    echo "Running build inside devkitpro/devkita64 container..."
    docker run --rm -v "$PWD:/work" -w /work devkitpro/devkita64:latest ./build_switch.sh "$@"
    exit $?
fi

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
