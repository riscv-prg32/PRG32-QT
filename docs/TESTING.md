# Testing

PRG32-QT uses layered validation.

## Portable deterministic suite

```sh
cmake -S . -B build-core -G Ninja -DPRG32QT_BUILD_APP=OFF
cmake --build build-core
ctest --test-dir build-core --output-on-failure
```

The suite includes core unit tests plus `Asteroids.prg32` and `Bach.prg32` included as compatibility fixtures. Each fixture is initialized and executed for 300 frames, covering graphics/runtime behavior and the audio-heavy cartridge path.

## Platform compilation

`.github/workflows/ci.yml` builds the Qt application for Windows, Linux, ARM64 Linux, macOS, iOS and Android. ARM64 Linux is the continuous build proxy for Raspberry Pi OS/Raspbian portability; release qualification should also be performed natively on the target Pi hardware/image.

To reproduce the Android ARM64 compilation and debug packaging locally after installing the dependencies in [Platform support](PLATFORMS.md):

```sh
./scripts/build-android.sh
```

A successful run compiles the native host library, deploys its Qt/QML dependencies, and runs Gradle `assembleDebug`. Verify that `build-android/android-build/build/outputs/apk/debug/android-build-debug.apk` exists. This is a build/package check; the physical-device checks below remain required for release qualification.

The Android smoke test was verified on an ARM64 Android API 35 emulator. Install and launch the debug APK with:

```sh
adb install -r build-android/android-build/build/outputs/apk/debug/android-build-debug.apk
adb shell monkey -p org.riscvprg32.prg32qt \
  -c android.intent.category.LAUNCHER 1
```

Confirm that Setup renders, Browse Store loads the catalog, the application process remains alive, and `adb logcat` contains neither a fatal exception nor a Qt main-library loading failure.

## Whole-Store certification

```sh
python3 scripts/store-smoke.py --runner ./build-core/prg32qt-headless --store http://193.205.230.7:5080 --frames 300
```

This enumerates the Store, downloads PRG2 cartridges and executes every package headlessly. It is intentionally separated from deterministic PR CI because the Store is an external service.

## Physical-device checks

Before signed releases verify startup, Store browsing/download, local import, graphics, audio and input on representative devices. On desktops also verify keyboard and controller connect/disconnect behavior; on mobile verify both portrait and landscape touch layouts.

The `prg32qt_performance_contract` test executes the public reference performance cartridge for 605 frames and requires its broker state to be complete. This covers descriptor validation, case lifecycle, sample recording, aggregate calculation, and performance ABI return values.
