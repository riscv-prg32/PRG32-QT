#!/bin/sh
set -eu
cmake -S . -B build-macos -G Ninja -DCMAKE_PREFIX_PATH="${QT_ROOT:-$HOME/Qt/6.8.3/macos}" -DPRG32QT_BUILD_APP=ON
cmake --build build-macos
ctest --test-dir build-macos --output-on-failure
