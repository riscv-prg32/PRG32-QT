#!/bin/sh
set -eu
BUILD_DIR="${PRG32QT_BUILD_DIR:-build-macos}"
# A deployed bundle can retain frameworks from an older Qt kit. Remove the generated app and clear cached
# Qt package locations before configuring so the executable and every bundled plugin use the selected kit.
cmake -E remove_directory "$BUILD_DIR/PRG32.app"
if [ -n "${QT_ROOT:-}" ]; then
  cmake -S . -B "$BUILD_DIR" -G Ninja -U 'Qt6*' -DCMAKE_PREFIX_PATH="$QT_ROOT" -DPRG32QT_BUILD_APP=ON
elif [ -d "$HOME/Qt/6.8.3/macos" ]; then
  cmake -S . -B "$BUILD_DIR" -G Ninja -U 'Qt6*' -DCMAKE_PREFIX_PATH="$HOME/Qt/6.8.3/macos" -DPRG32QT_BUILD_APP=ON
else
  # CI and package-manager installations normally expose Qt through CMAKE_PREFIX_PATH or the default search.
  cmake -S . -B "$BUILD_DIR" -G Ninja -U 'Qt6*' -DPRG32QT_BUILD_APP=ON
fi
cmake --build "$BUILD_DIR"
ctest --test-dir "$BUILD_DIR" --output-on-failure
