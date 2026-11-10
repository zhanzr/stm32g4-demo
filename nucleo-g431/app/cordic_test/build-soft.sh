#!/usr/bin/env bash
# Build the SOFT-float ABI cordic_test variant (cordic_test_soft): the whole
# image is compiled with -mfloat-abi=soft, simulating a no-FPU device.
# Separate build dir from the hard-float build (the toolchain file is cached).
# Run with:  bash build-soft.sh
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

if [ -d /mingw64/bin ] && ! command -v cmake >/dev/null 2>&1; then
    export PATH="/mingw64/bin:/usr/bin:$PATH"
fi

mkdir -p build-soft
cd build-soft
cmake -G Ninja "$@" ../soft
ninja