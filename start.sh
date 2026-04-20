#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
BUILD_DIR="${BUILD_DIR:-$SCRIPT_DIR/build}"
TOOLCHAIN_FILE="${TOOLCHAIN_FILE:-$SCRIPT_DIR/cmake/toolchains/x86_64-elf.cmake}"

cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE"
cmake --build "$BUILD_DIR" --target run
