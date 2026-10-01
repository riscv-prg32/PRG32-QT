#!/bin/sh
set -eu

NDK_VERSION="${ANDROID_NDK_VERSION:-26.1.10909125}"
MAX_ATTEMPTS=3
attempt=1

if [ -z "${ANDROID_HOME:-}" ]; then
  echo "ANDROID_HOME is not set" >&2
  exit 1
fi

while [ "$attempt" -le "$MAX_ATTEMPTS" ]; do
  echo "Installing Android NDK $NDK_VERSION (attempt $attempt/$MAX_ATTEMPTS)"
  if sdkmanager "ndk;$NDK_VERSION"; then
    exit 0
  fi

  # sdkmanager can leave a partial NDK directory after a truncated archive download. Removing only this exact
  # generated package allows the next attempt to download and unpack a clean archive.
  cmake -E remove_directory "$ANDROID_HOME/ndk/$NDK_VERSION"
  attempt=$((attempt + 1))
done

echo "Unable to install Android NDK $NDK_VERSION after $MAX_ATTEMPTS attempts" >&2
exit 1
