#!/bin/sh
set -eu
QT="${QT_ROOT:-$HOME/Qt/6.8.3/android_arm64_v8a}"
QT_HOST="${QT_HOST_PATH:-$HOME/Qt/6.8.3/macos}"
ANDROID_SDK="${ANDROID_SDK_ROOT:-$HOME/Library/Android/sdk}"
ANDROID_NDK="${ANDROID_NDK_ROOT:-$ANDROID_SDK/ndk/26.1.10909125}"

"$QT/bin/qt-cmake" -S . -B build-android -G Ninja \
  -DQT_HOST_PATH="$QT_HOST" \
  -DANDROID_SDK_ROOT="$ANDROID_SDK" \
  -DANDROID_NDK_ROOT="$ANDROID_NDK" \
  -DPRG32QT_BUILD_APP=ON
cmake --build build-android
APK="$(find build-android/android-build/build/outputs/apk -type f -name '*.apk' -print -quit)"
for LIBRARY in libcrypto_3.so libssl_3.so; do
  if ! unzip -Z1 "$APK" | grep -q "^lib/arm64-v8a/$LIBRARY$"; then
    echo "Android APK is missing required HTTPS runtime library: $LIBRARY" >&2
    exit 1
  fi
done
