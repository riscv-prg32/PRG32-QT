# PRG32-QT Architecture

PRG32-QT is deliberately split into a Qt-free portable guest runtime and a thin Qt/platform host. The design follows the same guest-visible behavior as the public PRG32 runtime while making the host reusable on Windows, Linux, Raspberry Pi OS/Raspbian, macOS, iOS, and Android.

## Layer 1 — Portable C++ runtime (`src/core`)

### Cartridge and metadata

`Cartridge` validates `PRG2` headers, entry points, bounded guest-memory sizes, CRC32, ABI/import-model information, required feature bits, optional metadata trailers, and packaged `AUD0` audio blocks.

### RV32IMAC interpreter

`Rv32Cpu` interprets RV32I plus the M, A, and C extensions used by portable PRG32 cartridges. It includes integer multiplication/division corner cases, LR/SC and AMO word operations, compressed instructions, FENCE/FENCE.I no-op semantics for the single-threaded interpreter, and read-only cycle/time/instret-style counters used by optimized code.

`VirtualClock` and the ESP32-C6 timing profile separate guest execution time from host wall time. In the default
accurate mode, `cycle` is accumulated effective ESP32-C6 work, `time` is virtual nanoseconds derived from that
clock, and `instret` remains the retired guest instruction count. Instruction classes and ABI #0-#138 local work
receive deterministic charges. Network latency and host rendering latency never alter guest virtual time.

Downloaded cartridge bytes are never treated as host-native code.

### Synthetic ABI

The runtime installs the PRG32 ABI 1.6 table in guest-visible memory and maps 139 ABI entries to synthetic host-call addresses. Current ABI hash `0x260f6136` and documented compatible hashes `0x006427c2` and `0x6be6e8d0` are accepted.

The provided feature mask intentionally includes only implemented public PRG32 services: audio, multiplayer,
metrics, extended audio, tile maps, platform helpers, and sprites. Wi-Fi and keyboard/text-input services keep
safe unavailable/stub semantics and are not advertised.

### Multiplayer

ABI calls #32 through #40 follow the public PRG32 `prg32_multiplayer` contract. `MultiplayerService` keeps the
portable runtime independent of Qt and preserves the 24-byte guest snapshot layout. `QtMultiplayerService`
connects every application target to the configured Store's `/api/multiplayer` WebSocket relay, validates
1-47 character room signatures, publishes at most every 50 ms, masks input to the seven PRG32 buttons, caps
the peer set at eight, and expires snapshots after three seconds. Headless execution exposes the ABI but
reports transport availability as false when no service is installed.

### Graphics

`Framebuffer` keeps the canonical 320×200 indexed framebuffer and 256-entry RGB565 palette. RGB565 drawing is quantized through the palette, matching the public PRG32 ABI. Implemented paths include primitives, 5×7 host text in 8×8 cells, 1-bpp sprites, RGB565 sprites, packed 1/2/4/8-bpp sprites, bitplanes, palette operations, and row snapshots.

### Tiles and platform helpers

The runtime implements 8×8 tile definitions, 40×25 direct tile screens, two 64×32 playfields, absolute/relative scrolling, 8.8-style parallax factors, camera positioning, tile flags, collision queries, actor initialization/motion/step helpers, and camera follow.

### Audio

`AudioBlock` parses packaged `AUD0` samples, instruments, tracks, and events. `Runtime` implements tones, notes, unsigned 8-bit PCM, sample playback, pitch, loop ranges, per-channel volume/pan, master volume, track tempo/event playback, and note/sample-driven RGB LED pulses through the abstract `AudioSink` interface.

### Scores and metrics

Local scores and the public performance broker are implemented. The host also serves the PRG32 HTTP API on port 8080.

## Layer 2 — Qt host (`src/qt`)

`AppController` owns one runtime instance, the reference firmware's 33 ms frame timer, persistent performance and
display settings, local cartridge persistence, input multiplexing, RGB LED presentation state, and the Qt audio
backend. `StoreClient` owns Store settings, catalog/discovery requests, architecture-aware downloads, and an in-memory icon cache. `FrameItem` renders the 320×200 image without smoothing. `RasterImageItem` paints bundled and data-URL raster artwork through `QPainter`, providing consistent image rendering on Qt's Android graphics path.

`QtAudioEngine` uses Qt Multimedia and `QtMultiplayerService` uses Qt WebSockets; both implementations are
shared across all Qt targets.

## Layer 3 — Input adapters

Inputs are maintained independently and OR-combined before every emulated frame. A release from one source cannot cancel a button still held by another.

- UI/touch: QML digital D-pad with diagonals, A, B, SELECT.
- Keyboard: arrows/WASD, Z/J, X/K, Enter/Return/Space.
- Apple: GameController framework with hot-plug support.
- Windows: WinMM joystick polling.
- Linux/Raspberry Pi OS: Linux joystick devices under `/dev/input/js*`.

All adapters produce the same PRG32 bit mask.

## Layer 4 — Qt Quick UI (`qml`)

The UI provides: startup splash/tone, a viewport-constrained setup menu, Store browser, search and tag filtering, Store settings/testing, local `.prg32` import, cartridge icons, adaptive portrait/landscape player, touch controls, RGB LED/bezel feedback, controller status, and a compact non-scrolling About panel.

## Platform packaging

- Windows and Linux: standard Qt desktop executable.
- Raspberry Pi OS/Raspbian: native ARM Qt build; no architecture-specific runtime code.
- macOS: application bundle and Apple GameController integration.
- iOS: Qt iOS bundle, touch UI, Apple GameController integration, ATS exception for the configured HTTP Store.
- Android: Qt Android activity, touch UI, Internet permission, and cleartext allowance for the configured HTTP Store.

## Test architecture

1. Portable unit tests verify CRC, framebuffer behavior, input-source merging, CPU execution, and cartridge parsing.
2. `Asteroids.prg32` and `Bach.prg32` run headlessly for repeated frames.
3. Store certification enumerates and executes every cartridge exposed by the configured PRG32 Store.
4. GitHub Actions build the native Qt application on desktop/mobile targets where matching SDKs are available.

## Frame pacing, pause, and local API

The Qt host follows the ESP32-C6 firmware frame loop at one update/draw cycle every 33 ms. `QTimer` uses precise timing, and it does not schedule catch-up frames after a delay. Opening Setup from a cartridge stops the frame timer, clears input, and pauses audio. Returning to the player resumes the existing runtime state.

The default ESP32-C6 Accurate mode advances virtual time to each 33 ms boundary when work finishes early and
preserves overruns when calibrated work crosses it. The host timer paces a fast host in real time; a slow host
does not rewrite virtual counters. Unlimited removes host pacing and retains legacy one-count-per-retired-
instruction counter behavior for developer workflows. The mode is re-anchored when changed or a cartridge loads.

The host listens on TCP port 8080 and publishes the PRG32 device endpoints: `GET /api`, `/api/runtime`, `/api/games`, `/api/screenshot.bmp`, `/api/performance.json`, `/api/scores`, and `/api/memory`, plus `POST /api/games`, `/api/games/select`, and `/api/scores`. Cartridge uploads use the four `cart0` through `cart3` slots. The Setup and player screens show the active local IPv4 address and API URL.

Performance ABI calls 124 through 132 implement the public version 1 broker. A cartridge that declares a `performance_contract` or `performance` metadata value, or uses a `benchmark` or `performance` tag, receives a visible performance action. Results use the PRG32 performance JSON schema version 2.
