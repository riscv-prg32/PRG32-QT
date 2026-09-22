#!/bin/sh
set -eu
QT="${QT_ROOT:-$HOME/Qt/6.8.3/ios}"
QT_HOST="${QT_HOST_ROOT:-$(dirname "$QT")/macos}"
DESTINATION="${IOS_DESTINATION:-generic/platform=iOS}"
case "$DESTINATION" in
  *Simulator*) BUILD_DIR=build-ios-sim; SDK=iphonesimulator; ARCHS="${IOS_ARCHS:-x86_64}" ;;
  *) BUILD_DIR=build-ios-device; SDK=iphoneos; ARCHS="${IOS_ARCHS:-arm64}" ;;
esac
"$QT/bin/qt-cmake" -S . -B "$BUILD_DIR" -G Xcode -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT="$SDK" -DCMAKE_OSX_ARCHITECTURES="$ARCHS" -DQT_HOST_PATH="$QT_HOST" -DPRG32QT_BUILD_APP=ON -DPRG32QT_BUILD_TESTS=OFF
if [ -n "${IOS_TEAM_ID:-}" ]; then
  xcodebuild -project "$BUILD_DIR/PRG32_QT.xcodeproj" -scheme PRG32_QT -configuration Release -destination "$DESTINATION" -allowProvisioningUpdates DEVELOPMENT_TEAM="$IOS_TEAM_ID" CODE_SIGN_STYLE=Automatic build
else
  xcodebuild -project "$BUILD_DIR/PRG32_QT.xcodeproj" -scheme PRG32_QT -configuration Release -destination "$DESTINATION" CODE_SIGNING_ALLOWED=NO build
fi
