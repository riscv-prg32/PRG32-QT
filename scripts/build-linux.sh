#!/usr/bin/env bash
set -euo pipefail
cmake -S . -B build-linux -G Ninja -DPRG32QT_BUILD_APP=ON "${@}"
cmake --build build-linux
ctest --test-dir build-linux --output-on-failure
