# Changelog

All notable changes to PRG32-QT will be documented here. The project follows Semantic Versioning once compatibility guarantees are established.

## [Unreleased]

- Expanded tag-driven GitHub Releases with checksummed build assets for every supported platform, including an
  explicit Apple TV limitation report when a Qt tvOS kit is unavailable on the release runner.
- Kept Setup and Store windowed when the persistent desktop player-fullscreen preference is enabled, restored
  all Setup actions after fullscreen transitions, and made Escape reliably return a fullscreen player to a
  window.
- Restored Android and Android TV builds by using Qt's case-sensitive public Android application interface
  header, and refreshed GitHub Actions to their Node.js 24-compatible major releases.
- Implemented the public PRG32 multiplayer ABI calls #32-#40 on every Qt target through the configured
  Cartridge Store WebSocket relay, including room validation, snapshot exchange, input masking, peer expiry,
  and the public 24-byte player-state layout; NaCup-napoli97 now passes headless execution.
- Made iOS player/setup content follow native safe-area insets, removed fullscreen UI from iOS and Android,
  kept Setup touchable, and centered the combined Start/Select control below the landscape game surface.
- Latched input press edges until the next emulated frame so short touch/controller taps reliably start games;
  Dukes of Duchesca now advances from its `PRESS START` title screen.
- Fixed ABI dual-playfield calls #94/#95 to composite layer 1 with transparent tile zero and to wrap scrolled
  map coordinates, restoring Spiriti! Napoli '97 background artwork.
- Added frame-exact headless input injection and framebuffer capture options for cartridge regression evidence.
- Fixed iOS window sizing across portrait/landscape rotation and preserved the 320:200 game-surface aspect ratio in both touch-player layouts.
- Added automatic Git-derived application/package versioning with one source-archive fallback and consistent
  runtime, About, Android and Apple bundle metadata.
- Added Apple TV and Android TV build targets, controller-first input, game-only fullscreen presentation,
  Android TV Leanback packaging and immersive system UI, platform documentation, and CI coverage.

- Added persistent Auto/Portrait/Landscape player selection and fullscreen TV mode with player, keyboard, and
  settings controls; desktop fullscreen shows only the aspect-correct game surface and exits with Escape; added
  current macOS Store/settings/player screenshots.
- Added a macOS IOKit fallback for generic USB HID joysticks/gamepads not exposed through Apple GameController.
- Expanded whole-Store certification with a 900-frame control sweep, framebuffer evidence, and audio-event/PCM
  evidence for every portable cartridge.
- Corrected CI runner/SDK selections for Android, macOS, iOS, and ARM64 Linux; repaired Store certification
  for object-valued variants and made pipeline failures propagate instead of appearing green.
- Added GitHub Actions CI artifacts and tag-driven continuous delivery with version validation, portable
  release tests, checksummed source archives, and generated GitHub Releases.
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
