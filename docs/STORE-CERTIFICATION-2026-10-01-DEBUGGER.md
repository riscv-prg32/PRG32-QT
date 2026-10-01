# Debugger release Store certification — 2026-10-01

This run certifies the PRG32-QT 0.4.0 debugger release candidate after the resumable instruction execution,
breakpoint, telemetry, and Qt host changes. It used the native Apple Silicon macOS headless runner built by
`scripts/build-macos.sh` and the live default Store.

```sh
/opt/homebrew/bin/python3 scripts/store-smoke.py \
  --runner ./build-macos/prg32qt-headless \
  --store https://store.prg32.uniparthenope.it/ \
  --frames 900
```

The Store exposed 31 entries. All 29 portable ABI-table cartridges passed the 900-frame input sweep and media
checks; the two non-portable packages were explicitly excluded. No physical speaker listening check was
performed, so audio results below certify engine events and PCM production rather than audible output.

| Cartridge | Version | Graphics: non-black pixels / hashes | Audio events / PCM samples |
|---|---:|---:|---:|
| `it.uniparthenope.asteroids` | 1.0.0 | 567 / 60 | 0 / 0 |
| `it.uniparthenope.audiotest` | 1.0.0 | 3,834 / 10 | 87 / 0 |
| `it.uniparthenope.bachdemo` | 1.0.0 | 12,183 / 19 | 814 / 0 |
| `it.uniparthenope.blackjack` | 1.0.0 | 64,000 / 17 | 239 / 0 |
| `it.uniparthenope.breakout` | 1.0.0 | 3,599 / 60 | 0 / 0 |
| `prg32.cockroaches` | 1.1.0 | 61,992 / 50 | 647 / 39,336 |
| `it.uniparthenope.debris` | 1.0.0 | 7,765 / 54 | 14 / 0 |
| `it.uniparthenope.devicedemo` | 1.0.0 | 25,951 / 46 | 22 / 0 |
| `org.riscv-prg32.dukesofduchesca-napoli97` | 1.0.0 | 64,000 / 19 | 1,024 / 0 |
| `org.riscv-prg32.fairwind-napoli97` | 4.2.0 | 64,000 / 10 | 108 / 0 |
| `it.uniparthenope.frogger` | 1.0.0 | 63,998 / 58 | 1 / 0 |
| `org.riscv-prg32.galleria2007` | 0.1.0 | 50,164 / 34 | 262 / 0 |
| `org.riscv-prg32.galleria2007.en` | 0.1.0 | 50,159 / 34 | 262 / 0 |
| `org.uniparthenope.prg32.inputtester` | 1.0.0 | 1,169 / 11 | 64 / 0 |
| `it.uniparthenope.moana_lemon_c` | 1.0.0 | 63,996 / 47 | 153 / 0 |
| `org.uniparthenope.prg32.moana-lemon-apocalypse` | 1.1.0 | 64,000 / 49 | 575 / 830,200 |
| `org.riscv-prg32.nacup-napoli97` | 3.1.0 | 64,000 / 10 | 108 / 0 |
| `org.riscv-prg32.outbun-napoli97` | 1.1.0 | 64,000 / 60 | 139 / 0 |
| `it.uniparthenope.pacman` | 1.0.0 | 5,600 / 19 | 0 / 0 |
| `it.uniparthenope.platformer` | 1.0.0 | 62,718 / 24 | 0 / 0 |
| `it.uniparthenope.poing` | 1.0.0 | 17,435 / 53 | 8 / 0 |
| `it.uniparthenope.pong` | 1.0.0 | 1,167 / 60 | 0 / 0 |
| `it.uniparthenope.raycaster` | 1.0.0 | 61,562 / 51 | 0 / 0 |
| `it.uniparthenope.performancetest` | 1.0.0 | 63,211 / 13 | 0 / 0 |
| `prg32.spacebeltmadness` | 1.1.0 | 64,000 / 42 | 2,143 / 130,552 |
| `org.riscv-prg32.spiriti-napoli97` | 1.2.3 | 64,000 / 8 | 547 / 0 |
| `it.uniparthenope.grendizer_c` | 1.0.0 | 64,000 / 36 | 318 / 0 |
| `it.uniparthenope.wing_commander` | 1.0.0 | 11,094 / 60 | 68 / 0 |
| `it.uniparthenope.youhavegotpizza` | 1.0.0 | 55,598 / 52 | 18 / 0 |

Explicit non-portable exclusions:

- `it.uniparthenope.space_invaders` 1.0.0
- `it.uniparthenope.terraforge` 1.0.0

The final result was `catalog=31 passed=29 skipped_nonportable=2 failed=0`.
