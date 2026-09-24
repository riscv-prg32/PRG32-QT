#!/bin/sh
set -eu
QT="${QT_ROOT:-$HOME/Qt/6.8.3/tvos}"
QT_HOST="${QT_HOST_ROOT:-$(dirname "$QT")/macos}"
DESTINATION="${TVOS_DESTINATION:-generic/platform=tvOS}"
case "$DESTINATION" in
  *Simulator*) BUILD_DIR=build-tvos-sim; SDK=appletvsimulator; ARCHS="${TVOS_ARCHS:-$(uname -m)}" ;;
  *) BUILD_DIR=build-tvos-device; SDK=appletvos; ARCHS="${TVOS_ARCHS:-arm64}" ;;
esac
"$QT/bin/qt-cmake" -S . -B "$BUILD_DIR" -G Xcode -DCMAKE_SYSTEM_NAME=tvOS -DCMAKE_OSX_SYSROOT="$SDK" -DCMAKE_OSX_ARCHITECTURES="$ARCHS" -DQT_HOST_PATH="$QT_HOST" -DPRG32QT_TV_MODE=ON -DPRG32QT_BUILD_APP=ON -DPRG32QT_BUILD_TESTS=OFF
if [ -n "${TVOS_TEAM_ID:-}" ]; then
  xcodebuild -project "$BUILD_DIR/PRG32_QT.xcodeproj" -scheme PRG32_QT -configuration Release -destination "$DESTINATION" -allowProvisioningUpdates DEVELOPMENT_TEAM="$TVOS_TEAM_ID" CODE_SIGN_STYLE=Automatic build
else
  if [ "$SDK" = appletvsimulator ]; then
    xcodebuild -project "$BUILD_DIR/PRG32_QT.xcodeproj" -scheme PRG32_QT -configuration Release -destination "$DESTINATION" CODE_SIGNING_ALLOWED=NO SDKROOT=appletvsimulator LIBRARY_SEARCH_PATHS= build
  else
    xcodebuild -project "$BUILD_DIR/PRG32_QT.xcodeproj" -scheme PRG32_QT -configuration Release -destination "$DESTINATION" CODE_SIGNING_ALLOWED=NO build
  fi
fi
