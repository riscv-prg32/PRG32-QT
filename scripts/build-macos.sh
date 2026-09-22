#!/bin/sh
set -eu
if [ -n "${QT_ROOT:-}" ]; then
  cmake -S . -B build-macos -G Ninja -DCMAKE_PREFIX_PATH="$QT_ROOT" -DPRG32QT_BUILD_APP=ON
elif [ -d "$HOME/Qt/6.8.3/macos" ]; then
  cmake -S . -B build-macos -G Ninja -DCMAKE_PREFIX_PATH="$HOME/Qt/6.8.3/macos" -DPRG32QT_BUILD_APP=ON
else
  # CI and package-manager installations normally expose Qt through CMAKE_PREFIX_PATH or the default search.
  cmake -S . -B build-macos -G Ninja -DPRG32QT_BUILD_APP=ON
fi
cmake --build build-macos
ctest --test-dir build-macos --output-on-failure
