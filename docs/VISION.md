# PRG32-QT Vision

PRG32-QT is the portable Qt/C++ host for the PRG32 educational RISC-V cartridge ecosystem. Its goal is to let the same portable `.prg32` cartridge run with the same observable behavior on desktop computers, mobile devices, Raspberry Pi systems, and automated/headless test hosts.

## Principles

1. **One portable runtime core.** Cartridge parsing, RV32IMAC execution, ABI dispatch, graphics, audio sequencing, tile/platformer helpers, scores, and deterministic host behavior live in Qt-free C++20 under `src/core`.
2. **Thin platform integration.** Qt 6 provides windows, touch UI, networking, audio output, persistent settings, and packaging. Native code is restricted to small adapters where Qt has no equivalent, notably physical game-controller discovery.
3. **PRG32 compatibility before platform specialization.** The attached PRG32-iOS implementation is the parity baseline for host behavior. The public PRG32 ABI/cartridge contracts remain the interoperability contract.
4. **Downloaded cartridges are passive guest data.** PRG32-QT interprets RV32IMAC guest instructions and exposes only the PRG32 ABI. It does not load downloaded native code into the host process.
5. **Educational clarity.** The implementation favors explicit subsystems and testable boundaries so students can study the CPU, ABI, graphics, input, audio, and Store layers independently.
6. **Reproducible compatibility.** Portable core tests, iOS-baseline cartridge fixtures, and Store-wide certification are first-class release gates.

## Supported hosts

The intended host set is Windows, Linux, Raspberry Pi OS/Raspbian, macOS, iOS, and Android. Desktop systems add keyboard and physical game-controller input; mobile systems retain the adaptive touch console controls from the iOS baseline.

## Non-goals

PRG32-QT is not an ESP32 peripheral emulator and does not pretend to provide Wi-Fi or multiplayer services when the iOS baseline does not. Cartridges requiring unavailable host features are rejected from their declared feature mask rather than being given misleading partial behavior.
