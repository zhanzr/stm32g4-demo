#!/usr/bin/env bash
# Build the nucleo-g431 ips395-st7796-captouch project (ST7796S 320x480 SPI + FT6336U touch
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
