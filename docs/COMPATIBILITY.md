# PRG32 Compatibility

PRG32-QT targets portable ABI-table PRG32 cartridges and follows the public PRG32 implementation as its behavioral parity baseline.

## Current target

- PRG2 package ABI major 1.
- RISC-V RV32IMAC guest code, including RV32A word atomics and compressed instructions.
- PRG32 ABI 1.6, current hash `0x260f6136`.
- Compatible hashes `0x006427c2` and `0x6be6e8d0`.
- 139-entry ABI table.
- Provided features exactly matching the implemented public PRG32 ABI: audio, multiplayer, metrics,
  audio-plus, tilemap, platformer, and sprites.
- 320×200 indexed framebuffer with a 256-entry RGB565 palette.
- `AUD0` packaged audio.
- Store API `/api/games` and Store discovery ABI `prg32-store-discovery-1.0`.
- Multiplayer ABI calls #32-#40 and the Store `/api/multiplayer` WebSocket snapshot protocol.

Hardware-specific cartridges that bypass the portable ABI and depend directly on ESP32 peripherals are outside this host's compatibility target.

## Regression baseline

`tests/fixtures/Asteroids.prg32` and `tests/fixtures/Bach.prg32` are included as compatibility fixtures and must both initialize and execute repeated frames in CTest. `Bach` is particularly useful for exercising packaged audio metadata and audio ABI calls.

## Store compatibility

`scripts/store-smoke.py` enumerates the configured Store, downloads every offered portable cartridge, validates it, initializes the runtime, and executes frames. Store certification is separate from deterministic pull-request CI because it depends on external network availability.
