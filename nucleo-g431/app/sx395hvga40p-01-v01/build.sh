#!/usr/bin/env bash
# Build the nucleo-g431 sx395hvga40p-01-v01 project (ILI9488 320x480 SPI
# LCD, soft bit-bang + HW SPI2 with DMA) with CMake + Ninja (Pico-style).
# Run with:  bash build.sh    (or ./build.sh on Linux)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

if [ -d /mingw64/bin ] && ! command -v cmake >/dev/null 2>&1; then
    export PATH="/mingw64/bin:/usr/bin:$PATH"
fi

mkdir -p build
cd build
cmake -G Ninja "$@" ..
ninja
