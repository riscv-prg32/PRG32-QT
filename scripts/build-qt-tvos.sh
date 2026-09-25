#!/bin/sh
set -eu

QT_VERSION="${QT_VERSION:-6.8.3}"
QT_ARCHIVE="qt-everywhere-src-${QT_VERSION}.tar.xz"
QT_URL="https://download.qt.io/archive/qt/6.8/${QT_VERSION}/single/${QT_ARCHIVE}"
QT_SHA256="${QT_SHA256:-cdd3a69967208276bb01af7ace7dba0ba53e679f886a4cbe624225c60fb73f2c}"
WORK_DIR="${QT_TVOS_WORK_DIR:-${TMPDIR:-/tmp}/prg32-qt-tvos-$QT_VERSION}"
QT_HOST="${QT_HOST_ROOT:-$HOME/Qt/${QT_VERSION}/macos}"
QT_PREFIX="${QT_TVOS_PREFIX:-$WORK_DIR/install}"
SOURCE_DIR="$WORK_DIR/qt-everywhere-src-${QT_VERSION}"
BUILD_DIR="$WORK_DIR/build"
ARCHIVE_PATH="$WORK_DIR/$QT_ARCHIVE"

if [ -x "$QT_PREFIX/bin/qt-cmake" ]; then
  printf '%s\n' "$QT_PREFIX"
  exit 0
fi

case "$(uname -m)" in
  arm64) QT_TVOS_ARCH=arm64 ;;
  x86_64) QT_TVOS_ARCH=x86_64 ;;
  *) echo "Unsupported macOS architecture: $(uname -m)" >&2; exit 1 ;;
esac

test -x "$QT_HOST/bin/qt-cmake"
mkdir -p "$WORK_DIR"
if [ ! -f "$ARCHIVE_PATH" ]; then
  curl --fail --location --retry 3 --output "$ARCHIVE_PATH" "$QT_URL"
fi
echo "$QT_SHA256  $ARCHIVE_PATH" | shasum -a 256 -c -

if [ ! -d "$SOURCE_DIR" ]; then
  tar -xf "$ARCHIVE_PATH" -C "$WORK_DIR"
  patch -d "$SOURCE_DIR" -p1 < "$PWD/patches/qt-6.8.3-tvos.patch"
fi

if [ ! -x "$QT_HOST/bin/qsb" ]; then
  "$QT_HOST/bin/qt-cmake" -S "$SOURCE_DIR/qtshadertools" -B "$WORK_DIR/host-shadertools" -G Ninja -DCMAKE_BUILD_TYPE=Release
  cmake --build "$WORK_DIR/host-shadertools" --parallel
  cmake --install "$WORK_DIR/host-shadertools"
fi

cmake -S "$SOURCE_DIR" -B "$BUILD_DIR" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$QT_PREFIX" \
  -DCMAKE_SYSTEM_NAME=tvOS \
  -DCMAKE_OSX_SYSROOT=appletvsimulator \
  -DCMAKE_OSX_ARCHITECTURES="$QT_TVOS_ARCH" \
  -DQT_HOST_PATH="$QT_HOST" \
  -DQT_QMAKE_TARGET_MKSPEC=unsupported/macx-tvos-clang \
  -DQT_BUILD_SUBMODULES="qtbase;qtdeclarative;qtshadertools;qtmultimedia;qtwebsockets" \
  -DBUILD_qtquick3d=OFF \
  -DBUILD_qtquicktimeline=OFF \
  -DQT_BUILD_EXAMPLES=OFF \
  -DQT_BUILD_TESTS=OFF \
  -DQT_FEATURE_permissions=OFF \
  -DQT_FEATURE_testlib=OFF \
  -DQT_FEATURE_clipboard=OFF \
  -DQT_FEATURE_metal=ON \
  -DQT_NO_APPLE_SDK_MAX_VERSION_CHECK=ON \
  -DCMAKE_IGNORE_PREFIX_PATH=/opt/homebrew
cmake --build "$BUILD_DIR" --parallel
cmake --install "$BUILD_DIR"
printf '%s\n' "$QT_PREFIX"
