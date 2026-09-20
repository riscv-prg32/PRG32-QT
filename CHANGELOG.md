# Changelog

All notable changes to PRG32-QT will be documented here. The project follows Semantic Versioning once compatibility guarantees are established.

## [Unreleased]

## [0.3.0] - 2026-09-21

- Completed feature-parity implementation against the attached PRG32-iOS baseline: audio/AUD0, graphics, tiles/playfields, platform helpers, sprites/bitplanes, local scores, RGB LED and metrics.
- Expanded RV32IMAC execution with atomics and counter CSRs used by portable cartridges.
- Added Store/search/settings/local-import/adaptive-player behavior from the iOS baseline.
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
