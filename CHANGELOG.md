# Changelog

All notable changes to PRG32-QT will be documented here. The project follows Semantic Versioning once compatibility guarantees are established.

## [Unreleased]

- Reformatted the C++ implementation for reviewability and added an enforced formatting, documentation,
  six-platform build, deterministic-test, and Cartridge Store certification workflow; runtime behavior and
  public contracts are unchanged.
- Matched the ESP32-C6 33 ms cartridge frame period and paused cartridge execution from Setup.
- Added the PRG32 local HTTP API, device IP display, screenshot and four cartridge slots.
- Implemented the version 1 cartridge performance broker and schema version 2 results.
- Revised portrait and landscape layouts for compact mobile screens.
- Fixed the iOS helper to select the Qt-compatible x86_64 simulator architecture while retaining arm64 device builds, and documented simulator and physical-device validation.
- Added repository instructions requiring Linux, Windows, Raspberry Pi OS, macOS, iOS and Android to remain buildable and working.
- Made the Android ARM64 build helper configure the host Qt, Android SDK and NDK explicitly, and documented the reproducible debug APK workflow and artifact.
- Restored the Qt-required Android activity metadata and file provider, set stable package/version metadata, and verified startup and Store browsing on an ARM64 API 35 emulator.
- Fixed Android raster rendering and compact-screen layouts so PRG32 and cartridge artwork is visible, Setup controls remain within the viewport, Store actions remain legible, and the complete About content fits without scrolling.

## [0.3.0] - 2026-09-21

- Completed feature-parity implementation against the public PRG32 runtime: audio/AUD0, graphics, tiles/playfields, platform helpers, sprites/bitplanes, local scores, RGB LED and metrics.
- Expanded RV32IMAC execution with atomics and counter CSRs used by portable cartridges.
- Added Store/search/settings/local-import/adaptive-player behavior for the Qt host.
- Added Windows and Linux/Raspberry Pi controller backends and build automation.
- Added Windows, Linux and ARM64-Linux CI alongside macOS, iOS and Android.
- Consolidated vision, architecture, licensing and authorship documentation under `docs/`.
- Added iOS-baseline Asteroids and Bach cartridge regression fixtures.

## [0.2.0] - 2026-09-20

- Renamed the project to **PRG32-QT**.
- Added independent UI, keyboard, and external-controller input sources.
- Added desktop keyboard mappings for LEFT, RIGHT, UP, DOWN, SELECT, A, and B.
- Added macOS USB/Bluetooth controller discovery and hot-plug support via GameController.
- Mapped D-pad/left stick, A/B, and Menu/Options to the PRG32 input mask.


## [0.1.0] - 2026-09-20

Initial public-source preparation.
