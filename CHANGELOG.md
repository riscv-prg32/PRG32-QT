# Changelog

All notable changes to PRG32-QT will be documented here. The project follows Semantic Versioning once compatibility guarantees are established.

## [Unreleased]

- Made the Android ARM64 build helper configure the host Qt, Android SDK and NDK explicitly, and documented the reproducible debug APK workflow and artifact.

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

## Unreleased

- Matched the ESP32-C6 33 ms cartridge frame period and paused cartridge execution from Setup.
- Added the PRG32 local HTTP API, device IP display, screenshot and four cartridge slots.
- Implemented the version 1 cartridge performance broker and schema version 2 results.
- Revised portrait and landscape layouts for compact mobile screens.
