#!/usr/bin/env bash
set -euo pipefail
# Run natively on Raspberry Pi OS (Bookworm or newer) after installing Qt 6 development packages.
cmake -S . -B build-raspbian -G Ninja -DPRG32QT_BUILD_APP=ON "${@}"
cmake --build build-raspbian
ctest --test-dir build-raspbian --output-on-failure
