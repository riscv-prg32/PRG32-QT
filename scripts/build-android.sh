#!/bin/sh
set -eu
QT="${QT_ROOT:-$HOME/Qt/6.8.3/android_arm64_v8a}"
"$QT/bin/qt-cmake" -S . -B build-android -G Ninja -DQT_ANDROID_PACKAGE_SOURCE_DIR="$PWD/platform/android" -DPRG32QT_BUILD_APP=ON
cmake --build build-android
