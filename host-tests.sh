#!/bin/bash
set -e
cmake -S tests/host -B build-host-tests -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-host-tests 
ctest --test-dir build-host-tests --output-on-failure --no-tests=error 
