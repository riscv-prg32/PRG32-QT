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

`.github/workflows/ci.yml` builds the Qt application for Windows, Linux, ARM64 Linux, macOS, iOS, Android and Android TV. ARM64 Debian Trixie is the continuous architecture/build proxy for Raspberry Pi OS/Raspbian portability; release qualification should also be performed natively on the target Pi hardware/image. Apple TV is compiled when a source-built Qt tvOS kit is configured; otherwise its job uploads an explicit limitation artifact and release qualification requires a manual build.

The same workflow runs a dedicated Qt-free Release build and uploads its Linux headless runner. Successful
desktop jobs upload their application build as a short-lived Actions artifact. These CI artifacts are diagnostic
outputs, not signed distributable packages; tag-driven source publication is handled separately by
`.github/workflows/release.yml`.

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

## TV validation

For Android TV, install the APK on an ARM64 Leanback emulator or device, confirm it appears in the TV launcher,
and verify system navigation/status UI remains hidden after focus changes. Navigate Setup and Store with a TV
remote, launch a cartridge, and verify the game-only 320:200 surface remains aspect-correct. Exercise D-pad,
A, B and Select with both the remote where available and a controller; no touch controls or player chrome may
be visible.

For Apple TV, install the signed application on hardware and repeat the fullscreen/aspect/input checks with a
Siri Remote and Apple GameController device. Record the Qt tvOS kit revision, Xcode/tvOS SDK, hardware model,
display mode and controller product. A plist/build-contract check without a tvOS Qt kit is not a platform
compile or physical-device certification.

## iOS validation

Use the build, install and launch commands in [Platform support](PLATFORMS.md). For both simulator and device, confirm that Setup renders in portrait and landscape, reports the device IP and API endpoint, and keeps its controls reachable. Load a cartridge, verify the 33 ms frame cadence, return to Setup and confirm execution pauses. Exercise Store browsing, local import, touch input, audio, `/api/runtime`, and the performance action when the selected cartridge declares performance contracts.

Validated on 2026-09-22:

- iPhone 16 Pro Max simulator, iOS 18.1, x86_64 through Rosetta on Apple Silicon: Release build, install and launch succeeded; the Setup screen rendered in portrait; `GET /api/runtime` returned the active runtime state.
- iPhone 12 Pro Max, iOS 27.0, arm64: Release build was development signed, installed and launched on the connected device; the application process remained active after launch.
- iPhone 12 Pro Max, iOS 27.0, arm64: the mobile window-sizing fix was development signed as `0.3.0-dev.16+g845ff3a9e42e`, installed, launched, and rotated through portrait and landscape with `devicectl`. Device screenshots at 1284×2778 and 2778×1284 confirmed that the application fills the display in both orientations; the player framebuffer is constrained to its 320:200 aspect ratio by the same build. Physical touch input was not actuated during this automated verification.
- The connected iPhone was updated to bundle version `3000015` (`0.3.0-dev.15+g3e45eb76c8b1`). Its Setup screen rendered and `/api/runtime` reported the same version. The full 23-entry official Store catalog was exercised: 21 portable packages loaded, advanced frames, and produced non-black framebuffer output on the device; Space Invaders and Terraforge received the expected non-portable ABI rejection. The 900-frame host media/input sweep passed all 21 portable packages with no failures. See the [complete catalog and device evidence](STORE-CERTIFICATION-2026-09-22-IPHONE.md). Physical touch/controller actuation, UI Store browsing, orientation changes, and speaker listening were not performed in this automated device session.

## Whole-Store certification

```sh
python3 scripts/store-smoke.py --runner ./build-core/prg32qt-headless --store http://193.205.230.7:5080 --frames 900
```

This enumerates the Store, downloads PRG2 portable variants and executes each supported package headlessly.
Media mode cycles neutral, directional, A, B, Select, and combined inputs; samples the framebuffer every 15
frames; rejects an entirely black run; and reports distinct hashes plus audio sink activity.
The report includes cartridge IDs, versions, pass/fail outcomes, and explicit skips for packages rejected as
non-portable ABI-table cartridges. An empty catalog, a catalog with no executed portable cartridge, or a runtime
error in a portable cartridge fails certification. CI preserves the combined report as an artifact even on failure.
The live Store is an external dependency; its state can change independently of the repository.

## Physical-device checks

Before signed releases verify startup, Store browsing/download, local import, graphics, audio and input on representative devices. On desktops also verify keyboard and controller connect/disconnect behavior; on mobile verify both portrait and landscape touch layouts.

## macOS display, media, and input validation (2026-09-22)

- Apple Silicon arm64 macOS host, native Homebrew Qt kit: application build and all four CTest targets passed.
  The separate `$HOME/Qt/6.8.3/macos` helper attempt remains unusable on this host because its QML scanner
  aborts with a NEON processor-feature mismatch.
- The live Store contained 23 entries. A 900-frame control sweep passed all 21 portable variants with nonzero
  graphics and 5–60 distinct sampled frame hashes. Audio calls were observed for 14 titles; seven declare no
  cartridge audio. Space Invaders 1.0.0 and Terraforge 1.0.0 were explicitly excluded because their packages
  are not portable ABI-table cartridges. No portable variant failed.
- The Bach audio fixture produced visible eight-channel stereo activity and the instrumented audio path emitted
  451 events in the Store run. The Qt Multimedia startup tone and game audio path were exercised; this run did
  not use calibrated acoustic capture, so it certifies engine activity rather than speaker frequency response.
- Keyboard Left/Right/Up/Down, Z, X, and Return were sent to a running Asteroids cartridge; the game framebuffer
  changed after the sequence. The source/unit contract also fixes W/A/S/D, J/K, and Space aliases to the same
  seven PRG32 mask bits.
- A Retro Games LTD `THEGamepad` USB HID joystick (`0x1c59:0x0026`) was attached. The original GameController-only
  backend did not enumerate it; after the IOKit fallback was added, the running player displayed `THEGamepad`.
  Physical button actuation was not automated, while the HID axis/hat/button mapping is shared with the asserted
  canonical input-mask constants.
- Auto/Portrait/Landscape selection, persisted settings, the Full Screen/Exit Full Screen button, `Escape`, and
  the fullscreen presentation were visually exercised. Current screenshots are stored under `docs/images/`.

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
  they are not portable ABI-table cartridges. The subsequent CI repair taught `store-smoke.py` to consume
  object-valued variants and to report these two exclusions explicitly; the 22-entry snapshot then completed
  with 20 passes, two non-portable skips, and no portable runtime failures.

## Actions run repair (2026-09-22)

Run `35691364645` exposed CI environment assumptions: the obsolete Android SDK `tools` package, a macOS SDK
without AGL, an iOS Qt path outside the helper's default home directory, and ARM64 Ubuntu's Qt 6.4.2 below the
project's minimum. The CI workflow now selects maintained Android SDK packages, a macOS 15/Xcode 16 image, the
Qt install action's exported iOS/Android paths, and a Debian Trixie ARM64 Qt kit. The Store job additionally
uses `pipefail` so a certification-script exception cannot be hidden by `tee`. Local validation covered the
portable CMake/CTest suite, Store parser unit tests, documentation consistency, and the live 22-entry Store
catalog. Hosted platform jobs remain to be confirmed by the next Actions run.

## Automatic versioning and TV target validation (2026-09-22)

- The Git-derived version was configured locally in a Qt-free build and compared with the application compile
  definition and generated package values. The untagged checkout derived the expected
  `0.3.0-dev.N+gCOMMIT` form; the macOS Qt application compiled with that definition.
- The native Apple Silicon macOS Qt application and QML cache compiled, and all four CTest targets passed.
  The Android TV Java activity compiled against Android API 34 and Qt 6.8.3's Android activity base.
- Android TV ARM64 configuration reached Qt QML import scanning. The installed target-side Qt scanner aborts
  on this host with an incompatible NEON-feature check, the same local-kit limitation as the handheld Android
  build; APK/Leanback launcher validation therefore remains assigned to the Linux CI job.
- Apple Silicon macOS was the available host. No Qt tvOS kit, Apple TV hardware, Android TV emulator or Android
  TV hardware was installed, so those target application/device checks remain required. The Apple TV CI job
  records the missing public binary-kit constraint instead of claiming a compile.
- The default Store contained 23 entries. The 900-frame media/input sweep passed all 21 portable variants with
  non-black graphics and 5–60 distinct framebuffer hashes. Space Invaders 1.0.0 and Terraforge 1.0.0 were the
  two explicit non-portable exclusions; no portable cartridge failed. Fourteen cartridges emitted audio
  events and seven declared no cartridge audio; this was instrumentation evidence, not a listening check.
