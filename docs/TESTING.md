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

`.github/workflows/ci.yml` builds the Qt application for Windows, Linux, ARM64 Linux, Apple Silicon macOS,
Intel macOS, iOS, Android, Android TV, and Apple TV. ARM64 Debian Trixie is the continuous architecture/build proxy for Raspberry Pi
OS/Raspbian portability; release qualification should also be performed natively on the target Pi
hardware/image. The Apple TV job builds and caches a patched Qt 6.8.3 tvOS simulator kit from checksum-pinned
source before compiling the application.

The same workflow runs a dedicated Qt-free Release build and uploads its Linux headless runner. Successful
desktop jobs upload their application build as a short-lived Actions artifact. These CI artifacts are diagnostic
outputs. For a `vX.Y.Z` tag, `.github/workflows/release.yml` repeats the platform builds, packages each output,
generates SHA-256 checksums, and publishes all assets together only after every required job succeeds. The iOS
and Apple TV outputs are unsigned simulator builds, Android outputs are debug-signed APKs, Windows is an NSIS
installer containing the default compiler-runtime-inclusive `windeployqt` output, and both macOS architectures are
`macdeployqt` DMG installers. These installers are not code-signed or notarized by the public workflow.

### 0.3.2 upstream-alignment release validation (2026-09-26)

- Compared against upstream PRG32 commit `687251f7a09720e474d5c97fabdf2844271c7963`. ABI 1.6, its 139-entry
  table, and hash `0x260f6136` are unchanged. Host-visible changes are the official HTTPS Store, refreshed
  artwork, corrected procedural synth-ID decoding, and same-tick delta-0 tracker behavior.
- The portable Release build and all four CTest targets passed on Apple Silicon macOS: core tests, Asteroids,
  Bach, and the required 605-frame performance contract. The native arm64 Homebrew Qt 6.11.2 application also
  built and passed the same four tests.
- The official HTTPS Store returned 27 entries. In both Accurate and Unlimited 900-frame media/input sweeps,
  all 25 portable cartridges passed and the two native-only cartridges (`Space Invaders` and `TerraForge`)
  were explicitly excluded. Every pass recorded non-black pixels and distinct framebuffer hashes; audio event
  and PCM counts were recorded by the runner. No physical speaker listening check was performed.
- The local Qt 6.8.3 Android host-side QML scanner aborted because its arm64 slice requires an unavailable
  processor feature. Android and Android TV APK completion, Windows Setup, Intel macOS DMG, Linux/Raspberry Pi
  packages, iOS simulator, and Apple TV remain required release-workflow jobs rather than locally claimed
  passes.
- A local `macdeployqt` pass using the Homebrew Qt kit could not create a DMG: that installation omits optional
  QML frameworks found during import scanning, and `hdiutil` reported `Device not configured`. The tagged
  workflow packages from the complete Qt 6.8.3 binary kit on separate macOS runners.
- Xcode device discovery returned only the Apple Silicon Mac and CoreDevice timed out while initializing; no
  iPhone was available to install or launch this build during the recorded local validation.

### Release-asset automation validation (2026-09-24)

- The release workflow parsed as YAML, the documentation-consistency check passed, and CMake created and
  listed a ZIP containing the macOS `PRG32.app` bundle using the same command used by the release job.
- The application rebuilt on an Apple Silicon arm64 macOS host with the Homebrew Qt 6.11.2 Core, Multimedia,
  and WebSockets packages. All four CTest targets passed. The default local Qt 6.8.3 kit lacked Qt WebSockets,
  so the available complete Homebrew kit was used instead.
- Windows, Linux, Raspberry Pi OS, iOS, Android, Android TV, and Apple TV packaging could not be executed on
  this macOS host. The release workflow uses the same platform scripts and hosted-runner configurations as CI;
  the first tagged run remains the end-to-end validation for those release assets.

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

Validated on 2026-09-25:

- PRG32-QT 0.3.1 at Git commit `89de9b15f977fc3729d35be61df7a792114e491c` was development-signed,
  installed, and launched on the wired iPhone 12 Pro Max (`iPhone13,4`, iOS 27.0). The portrait Setup screen
  respected the safe area and `/api/runtime` reported `qt-0.3.1`, ABI 1.6, and feature mask 477.
- The complete 27-entry Store snapshot produced 25 portable passes and two explicit non-portable exclusions
  on both the 900-frame host media/input sweep and the iPhone upload/select/framebuffer run, with zero portable
  failures. All four CTest targets passed. See the [complete catalog and device evidence](STORE-CERTIFICATION-2026-09-25-IPHONE.md).
- Physical touch/controller actuation, visible Store browsing, and speaker listening were not performed in
  the automated device session and remain open for hands-on release qualification.

Validated on 2026-09-22:

- iPhone 16 Pro Max simulator, iOS 18.1, x86_64 through Rosetta on Apple Silicon: Release build, install and launch succeeded; the Setup screen rendered in portrait; `GET /api/runtime` returned the active runtime state.
- iPhone 12 Pro Max, iOS 27.0, arm64: Release build was development signed, installed and launched on the connected device; the application process remained active after launch.
- iPhone 12 Pro Max, iOS 27.0, arm64: the mobile window-sizing fix was development signed as `0.3.0-dev.16+g845ff3a9e42e`, installed, launched, and rotated through portrait and landscape with `devicectl`. Device screenshots at 1284×2778 and 2778×1284 confirmed that the application fills the display in both orientations; the player framebuffer is constrained to its 320:200 aspect ratio by the same build. Physical touch input was not actuated during this automated verification.
- The connected iPhone was updated to bundle version `3000015` (`0.3.0-dev.15+g3e45eb76c8b1`). Its Setup screen rendered and `/api/runtime` reported the same version. The full 23-entry official Store catalog was exercised: 21 portable packages loaded, advanced frames, and produced non-black framebuffer output on the device; Space Invaders and Terraforge received the expected non-portable ABI rejection. The 900-frame host media/input sweep passed all 21 portable packages with no failures. See the [complete catalog and device evidence](STORE-CERTIFICATION-2026-09-22-IPHONE.md). Physical touch/controller actuation, UI Store browsing, orientation changes, and speaker listening were not performed in this automated device session.

## Whole-Store certification

```sh
python3 scripts/store-smoke.py --runner ./build-core/prg32qt-headless --store https://store.prg32.uniparthenope.it/ --frames 900
```

This enumerates the Store, downloads PRG2 portable variants and executes each supported package headlessly.
Media mode cycles neutral, directional, A, B, Select, and combined inputs; samples the framebuffer every 15
frames; rejects an entirely black run; and reports distinct hashes plus audio sink activity.
The report includes cartridge IDs, versions, pass/fail outcomes, and explicit skips for packages rejected as
non-portable ABI-table cartridges. An empty catalog, a catalog with no executed portable cartridge, or a runtime
error in a portable cartridge fails certification. CI preserves the combined report as an artifact even on failure.
The live Store is an external dependency; its state can change independently of the repository.

For a frame-exact cartridge regression, repeat `--input frame:mask` as needed and save the final framebuffer
with `--dump-ppm path`. This is used to prove title-screen transitions and graphics-layer composition without
depending on host event timing.

## Mobile safe-area and cartridge regression validation (2026-09-23)

- The arm64 iOS device target compiled with Qt 6.8.3 and Xcode/iPhoneOS SDK 27.0, was development-signed by
  team `B57UXM7RWR`, installed, and launched on an iPhone 12 Pro Max (`iPhone13,4`, iOS 27.0). CoreDevice
  screenshots at 1284×2778 and 2778×1284 verify that portrait content starts below the status/notch area,
  landscape content stays inside the side insets, Setup remains visible, and no mobile fullscreen control is
  present. See `docs/images/ios-device-safe-area-{portrait,landscape}.png`.
- The native arm64 macOS Qt application compiled. A short UI click on the centered `START / SELECT` control
  advanced Dukes of Duchesca 1.0.0 from `PRESS START` to gameplay, verifying frame-edge input latching.
- Spiriti! Napoli '97 1.2.3 was run with a frame-exact A press. The repaired ABI #94 dual-playfield composition
  produced the Naples map background instead of the opaque layer-1 tile-zero fill. Regression captures are
  stored in `docs/images/macos-player-start-select.png` and `docs/images/spiriti-background-fixed.png`.
- The live 25-entry Store sweep passed all 22 portable cartridges at 900 frames, with the three non-portable
  exclusions named and no portable failures. The complete snapshot and per-cartridge media evidence are in
  [the safe-area revision certification](STORE-CERTIFICATION-2026-09-22-SAFE-AREA.md).
- Android ARM64 configuration reached Qt's target QML import scan, where the installed target-side scanner
  aborted because it requires a different NEON host feature set. Windows, Linux, Raspberry Pi OS, Android TV,
  and Apple TV were not locally executable on this macOS host; their checked-in platform scripts and distinct
  CI jobs remain the required target verification.
- Physical rotation was exercised. The Setup page was reached and its controls were visually confirmed inside
  the safe area, but physical Setup-to-player touch actuation, external controller actuation, and speaker
  listening were not completed for this revision.

## NaCup-napoli97 multiplayer validation (2026-09-23)

- The Store `org.riscv-prg32.nacup-napoli97` 3.1.0 qemu package was downloaded through the official Store API.
  Its 64,786-byte payload matched SHA-256
  `970da9cb5f9db8888d18417d3b834b6ecb060c260293e9421591dd6f78ce93fd`.
- After implementing public PRG32 multiplayer ABI calls #32-#40, the arm64 macOS headless runner executed the
  package for 900 frames. Media verification reported 64,000 non-black pixels, 10 distinct framebuffer hashes,
  eight audio events, and zero PCM samples. Two independent Qt clients joined a live Store relay room and
  exchanged validated position, sprite, flags, input, and frame snapshots in both directions.
- The native arm64 macOS application built with the Homebrew Qt kit and all four CTest targets passed. The
  standard `$HOME/Qt/6.8.3/macos` helper remains blocked because its QML scanner aborts with a NEON
  processor-feature mismatch.
- A development-signed arm64 iOS build of Git version `0.3.0-dev.18+g3a35b448dd36` with Qt WebSockets compiled,
  installed, and launched on the wired iPhone 12 Pro Max (`iPhone13,4`, iOS 27.0). The device API accepted and
  selected the exact package in `cart0`; `/api/runtime` reported feature mask 477, `cart_loaded=true`, and 98
  frames after the three-second sample.
- Android ARM64 and Android TV ARM64 configuration reached Qt's target QML import scan, where the installed
  scanner aborted because it requires an incompatible NEON host feature set. The tvOS Qt kit was not
  installed. Windows PowerShell and a Windows runner, a Linux host, and Raspberry Pi hardware/Raspberry Pi OS
  were unavailable on this macOS host. These targets therefore have no cartridge execution result from this
  run and require their platform CI runners or physical hardware for qualification.
- The post-change 25-entry Store sweep passed all 23 portable cartridges for 900 frames, including NaCup, with
  two explicit non-portable exclusions and no portable failures. This was instrumented media evidence rather
  than a physical speaker listening check.

## Physical-device checks

Before signed releases verify startup, Store browsing/download, local import, graphics, audio and input on representative devices. On desktops also verify keyboard and controller connect/disconnect behavior; on mobile verify both portrait and landscape touch layouts.

## Apple TV source-kit and simulator validation (2026-09-24)

- Apple Silicon arm64 macOS host with Xcode 27.0 and tvOS 27.0 SDK: Qt 6.8.3 was built from the official
  checksum-pinned source archive with `patches/qt-6.8.3-tvos.patch`, then PRG32 was compiled for arm64 with
  `scripts/build-tvos.sh`.
- The unsigned application was installed and launched on an Apple TV 4K (3rd generation) tvOS 27 simulator,
  UUID `AAFB9942-9888-4A57-9604-D2EEA2490D17`, at 3840x2160. The foreground Setup screen reported `TVOS`,
  RV32IMAC at 30 FPS, 25 cartridges, local networking, and the Web API. Runtime logs confirmed a visible UIKit
  scene, Metal shader compilation, GameController discovery, and CoreAudio startup.
- No physical Apple TV, Siri Remote, or external Apple GameController was available; physical button
  actuation, display overscan, signing, and listening checks remain required for device certification.

## 0.3.0 release validation (2026-09-24)
- The Release portable build passed all four CTest targets: core, Asteroids, Bach, and the required
  `PerformanceTest.prg32 --require-performance` contract.
- Whole-Store certification executed 23 portable cartridges for 300 frames with media verification and
  recorded per-cartridge non-black-pixel, distinct-framebuffer-hash, audio-event, and PCM-sample counts. Two
  non-portable cartridges (`space_invaders` and `terraforge`) were explicitly excluded; no portable cartridge
  failed. No physical listening check was performed.
- Android and Android TV ARM64 packages built successfully with Qt 6.8.3, Android platform 34, and NDK
  26.1.10909125. The macOS Qt build compiled all C++ and QML sources but could not link because Xcode 27 no
  longer supplies AGL; CI remains pinned to macOS 15 for the Qt 6.8.3 AGL dependency.
- An x86_64 Debian Trixie container with Qt 6.8 built the Linux application and passed all four CTest targets.
  C++ formatting, documentation consistency, and Store parser unit tests passed. Windows was not locally
  executable; its CI job remains the release gate.

## macOS Setup and fullscreen regression validation (2026-09-23)

- Apple Silicon arm64 macOS host with the native Homebrew Qt kit: the application rebuilt successfully and
  all four CTest targets passed.
- The Setup page was captured through the application's documentation-screenshot path after a fullscreen
  transition. Run, Store, Import, Settings, and About actions were visibly rendered with stable geometry.
- The bundled Asteroids fixture was opened in the desktop player, entered game-only fullscreen, and returned
  to the windowed player with `Escape`. The restored title bar, Setup action, player controls, and Full Screen
  action were visually confirmed.
- `scripts/check-docs.py` passed. Windows, Linux, Raspberry Pi OS, iOS, Android, Apple TV, and Android TV were
  not locally executable during this macOS-specific UI regression check.

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

## ESP32-C6 performance-emulation validation (2026-09-25)

- Apple Silicon (`arm64`) macOS built the Qt-free core and native Homebrew Qt application. All four CTest
  targets passed: core timing/counter tests, Asteroids and Bach at 300 frames, and the 605-frame
  `PerformanceTest.prg32 --require-performance` contract. The documentation consistency check and Store smoke
  Python unit tests passed. The separate `$HOME/Qt/6.8.3/macos` kit still fails QML import scanning with its
  existing NEON processor mismatch; the native Homebrew Qt build succeeded.
- The live 27-entry default Store was certified for 900 frames in both default ESP32-C6 Accurate and explicit
  Unlimited modes with the input/media sweep. Each mode passed all 25 portable cartridges with non-black
  graphics and multiple framebuffer hashes where the workload changes; Space Invaders 1.0.0 and Terraforge
  1.0.0 were the two explicit non-portable exclusions. No portable cartridge failed.
- Accurate mode produced instrumented audio events for 17 cartridges and 157,008 PCM samples for Space Belt
  Madness. Unlimited mode produced instrumented audio events for the same declared workloads and 111,184 PCM
  samples for Space Belt Madness. These are engine instrumentation results, not an acoustic listening check.
  Mode-dependent time progression intentionally changes audio event counts and some framebuffer-hash counts.
- The reference PRG32 firmware checkout at commit `596bcf954a77a296db20f5b69756f078721232f3`
  supplied the verified 160 MHz CPU configuration and exact 33 ms frame policy. No physical ESP32-C6 board was
  connected for instruction/ABI microbenchmark calibration, so `tests/performance/reference/esp32c6.json`
  records calibration as pending and no 5%/10% physical-parity claim is made.
- Windows, Linux, Raspberry Pi OS, iOS, Android, Apple TV, and Android TV were unavailable for local compilation
  in this macOS session. Their source paths share the compiled portable core; their platform scripts and CI jobs
  remain required before merge. No physical controller, touch surface, speaker, TV, or mobile device was tested.

## 0.3.2 release qualification (2026-09-26)

- Portable core CTest, formatting, documentation consistency, workflow YAML parsing, and Store certification
  parser tests passed locally on Apple Silicon macOS. The native Homebrew Qt macOS application compiled.
- The shared Settings dialog exposes ESP32-C6 Accurate by default and Unlimited on desktop, iOS, Android,
  Apple TV, and Android TV. Only the desktop-only fullscreen checkbox remains platform-gated; the performance
  control is not hidden on mobile or TV.
- The connected iPhone 12 Pro Max (`iPhone13,4`, UDID ending `29001E`) was paired and available. The arm64 iOS
  project built against the iPhoneOS 27.0 SDK. Because the production bundle identifier belongs to a different
  Apple team, device validation used the development-only identifier `com.raffaelemontella.prg32qt` without
  changing repository metadata. The signed app installed and launched successfully; touch, rotation,
  persistence, and on-device timing were not fully certified.

## Three performance modes and status bars (2026-09-26)

- The Qt-free core build and all four CTest targets passed on Apple Silicon macOS, including the 605-frame
  performance contract. Core tests now verify that Optimal uses lightweight retired-instruction counter
  semantics separately from Accurate. Documentation consistency, Store-certification parser tests, and the
  native macOS Qt application build passed.
- The shared Settings UI was rendered from the macOS build and verified to expose Accurate (default), Optimal
  (30 FPS), Unlimited, and the disabled-by-default top/bottom status-bar switch without clipping.
- Unsigned iOS arm64, Android arm64, Android TV arm64, and Apple TV simulator builds passed. The signed iOS
  build installed on the connected iPhone 12 Pro Max under the development-only bundle identifier
  `com.raffaelemontella.prg32qt`; automated launch remained blocked while the device was locked.
- No ESP32-C6 serial device or PRG32 HTTP endpoint was available on the local network. Accurate therefore
  retains the source-verified 160 MHz clock and 33 ms firmware frame policy, but the physical instruction/ABI
  coefficient calibration remains pending in `tests/performance/reference/esp32c6.json`; no unmeasured parity
  claim is made.
- Windows, Linux, and Raspberry Pi OS were not locally runnable on this macOS host. Their platform scripts and
  CI jobs remain required before merge.

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
