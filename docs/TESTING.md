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

Confirm that Setup renders the PRG32 logo with every button contained inside the viewport. Open Browse Store and verify the catalog loads with distinct cartridge icons and legible action labels. Open About and verify the logo, description, authors, university/lab information and license all fit without scrolling. Finally, confirm the application process remains alive and `adb logcat` contains neither a fatal exception nor a Qt main-library loading failure.

## iOS validation

Use the build, install and launch commands in [Platform support](PLATFORMS.md). For both simulator and device, confirm that Setup renders in portrait and landscape, reports the device IP and API endpoint, and keeps its controls reachable. Load a cartridge, verify the 33 ms frame cadence, return to Setup and confirm execution pauses. Exercise Store browsing, local import, touch input, audio, `/api/runtime`, and the performance action when the selected cartridge declares performance contracts.

Validated on 2026-09-22:

- iPhone 16 Pro Max simulator, iOS 18.1, x86_64 through Rosetta on Apple Silicon: Release build, install and launch succeeded; the Setup screen rendered in portrait; `GET /api/runtime` returned the active runtime state.
- iPhone 12 Pro Max, iOS 27.0, arm64: Release build was development signed, installed and launched on the connected device; the application process remained active after launch.

## Whole-Store certification

```sh
python3 scripts/store-smoke.py --runner ./build-core/prg32qt-headless --store http://193.205.230.7:5080 --frames 300
```

This enumerates the Store, downloads PRG2 cartridges and executes every package headlessly. It is intentionally separated from deterministic PR CI because the Store is an external service.

## Physical-device checks

Before signed releases verify startup, Store browsing/download, local import, graphics, audio and input on representative devices. On desktops also verify keyboard and controller connect/disconnect behavior; on mobile verify both portrait and landscape touch layouts.

The `prg32qt_performance_contract` test executes the public reference performance cartridge for 605 frames and requires its broker state to be complete. This covers descriptor validation, case lifecycle, sample recording, aggregate calculation, and performance ABI return values.

## Readability-pass validation (2026-09-22)

- Portable core: Apple Silicon (`arm64`) macOS host, Ninja build in `build-core`; all four CTest targets
  passed, including both 300-frame cartridges and the 605-frame performance contract.
- Qt desktop host: Apple Silicon (`arm64`) macOS host with Homebrew Qt, full application build in
  `build-macos-brew` succeeded. The default `scripts/build-macos.sh` path selected an incompatible locally
  installed Qt tool whose QML import scanner requires a different processor feature set; the source was
  therefore validated with the native Homebrew Qt kit.
- Android recompiled the portable library, headless runner, tests, and generated Qt metadata through the QML
  type-registration step. Packaging could not finish because the configured host-side Qt tool reports an
  incompatible processor/NEON requirement.
- The iOS simulator Xcode build reached target dependency and toolchain evaluation but could not create its
  module-session cache because this validation environment does not permit writes to Xcode DerivedData.
- Windows, Linux, and Raspberry Pi OS were not executable in this local macOS validation session. Their build
  helpers remain the CI entry points, and the workflow records each target as a distinct, visible job rather
  than silently omitting it.
- The default Store catalog snapshot contained 22 cartridges. Twenty qemu variants completed 300 headless
  frames. `it.uniparthenope.space_invaders` 1.0.0 and `it.uniparthenope.terraforge` 1.0.0 were rejected because
  they are not portable ABI-table cartridges. The repository's existing `store-smoke.py` also cannot consume
  the current object-valued `variants.qemu` catalog entry; this pre-existing certification-tool defect was not
  changed in the behavior-preserving readability pass and requires a separate fix.
