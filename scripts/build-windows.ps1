$ErrorActionPreference = "Stop"
$PSNativeCommandUseErrorActionPreference = $true
cmake -S . -B build-windows -G Ninja -DPRG32QT_BUILD_APP=ON @args
cmake --build build-windows
ctest --test-dir build-windows --output-on-failure
