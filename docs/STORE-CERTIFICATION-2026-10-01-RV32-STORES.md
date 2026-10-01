# RV32 store-boundary certification — 2026-10-01

This certification covers the fix that makes RV32 halfword and word stores validate their complete guest-memory
range before changing memory. The tested tree was based on PRG32-QT 0.3.3 (`a13b892`) with the fix applied.

## Host and local validation

- Apple Silicon arm64, macOS 26.6.2 (25G83), AppleClang 21.0.0.21000334.
- The Release portable build passed all four CTest targets: core, Asteroids, Bach, and the 605-frame required
  performance contract.
- A Debug AddressSanitizer and UndefinedBehaviorSanitizer build passed the same four targets. Leak detection is
  unsupported by the host Apple sanitizer runtime and was disabled; address and undefined-behavior checks were
  active.
- The native arm64 application built with Qt 6.11.2, passed all four CTest targets, launched, rendered the Setup
  page to a 2200x1520 capture, and exited cleanly.
- The Store sweep was instrumented media evidence, not a physical speaker listening check. No controller was
  physically actuated; the headless input sweep exercised the cartridge input masks.

Windows, Linux, Raspberry Pi OS, iOS, Android, Android TV, and Apple TV builds and device checks were unavailable
on this macOS host. Their platform CI runners and physical-device qualification remain required before release.

## Official Store snapshot

Command:

```sh
/usr/bin/python3 scripts/store-smoke.py \
  --runner /tmp/prg32qt-audit/prg32qt-headless \
  --store https://store.prg32.uniparthenope.it/ \
  --frames 900
```

The catalog contained 29 entries: 27 portable cartridges passed and two non-portable cartridges were explicitly
excluded. No portable cartridge failed.

| Cartridge ID | Version | Result | Non-black pixels | Distinct hashes | Audio | Events | PCM samples |
|---|---:|---|---:|---:|---|---:|---:|
| `it.uniparthenope.asteroids` | 1.0.0 | PASS | 567 | 60 | not declared | 0 | 0 |
| `it.uniparthenope.audiotest` | 1.0.0 | PASS | 3,834 | 10 | active | 87 | 0 |
| `it.uniparthenope.bachdemo` | 1.0.0 | PASS | 12,183 | 19 | active | 814 | 0 |
| `it.uniparthenope.blackjack` | 1.0.0 | PASS | 64,000 | 17 | active | 239 | 0 |
| `it.uniparthenope.breakout` | 1.0.0 | PASS | 3,599 | 60 | not declared | 0 | 0 |
| `prg32.cockroaches` | 1.1.0 | PASS | 61,992 | 50 | active | 647 | 39,336 |
| `it.uniparthenope.debris` | 1.0.0 | PASS | 7,765 | 54 | active | 14 | 0 |
| `it.uniparthenope.devicedemo` | 1.0.0 | PASS | 25,951 | 46 | active | 22 | 0 |
| `org.riscv-prg32.dukesofduchesca-napoli97` | 1.0.0 | PASS | 64,000 | 19 | active | 1,024 | 0 |
| `org.riscv-prg32.fairwind-napoli97` | 4.2.0 | PASS | 64,000 | 10 | active | 108 | 0 |
| `it.uniparthenope.frogger` | 1.0.0 | PASS | 63,998 | 58 | active | 1 | 0 |
| `org.uniparthenope.prg32.inputtester` | 1.0.0 | PASS | 1,169 | 11 | active | 64 | 0 |
| `it.uniparthenope.moana_lemon_c` | 1.0.0 | PASS | 63,996 | 47 | active | 153 | 0 |
| `org.uniparthenope.prg32.moana-lemon-apocalypse` | 1.1.0 | PASS | 64,000 | 49 | active | 575 | 830,200 |
| `org.riscv-prg32.nacup-napoli97` | 3.1.0 | PASS | 64,000 | 10 | active | 108 | 0 |
| `org.riscv-prg32.outbun-napoli97` | 1.1.0 | PASS | 64,000 | 60 | active | 139 | 0 |
| `it.uniparthenope.pacman` | 1.0.0 | PASS | 5,600 | 19 | not declared | 0 | 0 |
| `it.uniparthenope.platformer` | 1.0.0 | PASS | 62,718 | 24 | not declared | 0 | 0 |
| `it.uniparthenope.poing` | 1.0.0 | PASS | 17,435 | 53 | active | 8 | 0 |
| `it.uniparthenope.pong` | 1.0.0 | PASS | 1,167 | 60 | not declared | 0 | 0 |
| `it.uniparthenope.raycaster` | 1.0.0 | PASS | 61,562 | 51 | not declared | 0 | 0 |
| `it.uniparthenope.performancetest` | 1.0.0 | PASS | 63,211 | 13 | not declared | 0 | 0 |
| `prg32.spacebeltmadness` | 1.1.0 | PASS | 64,000 | 42 | active | 2,143 | 130,552 |
| `it.uniparthenope.space_invaders` | 1.0.0 | SKIP | — | — | — | — | — |
| `org.riscv-prg32.spiriti-napoli97` | 1.2.3 | PASS | 64,000 | 8 | active | 547 | 0 |
| `it.uniparthenope.terraforge` | 1.0.0 | SKIP | — | — | — | — | — |
| `it.uniparthenope.grendizer_c` | 1.0.0 | PASS | 64,000 | 36 | active | 318 | 0 |
| `it.uniparthenope.wing_commander` | 1.0.0 | PASS | 11,094 | 60 | active | 68 | 0 |
| `it.uniparthenope.youhavegotpizza` | 1.0.0 | PASS | 55,598 | 52 | active | 18 | 0 |

Both skipped packages were rejected with the expected message: PRG32-QT accepts portable ABI-table PRG32
cartridges only.
