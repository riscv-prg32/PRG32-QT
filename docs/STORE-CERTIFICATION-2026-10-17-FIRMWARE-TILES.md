# Firmware-tile Store certification — 2026-10-17

Commit-under-test was `a7dd97a` (firmware tile, playfield and platform ABI #80–#105 port plus hardened Store
downloads). The Apple Silicon macOS 26.6.2 host used the native arm64 Release headless runner built with Ninja
and Apple Clang; the core suite asserts remain active under Release. Both runs used the default Store, 900
frames, media verification, the standard input sweep, and one shared `--cache-dir`, so the Accurate and
Unlimited runs certified identical package bytes. `px/hash` records final non-black pixels and distinct
framebuffer hashes; `events/pcm` records audio events and PCM samples. No physical output device or listening
check was performed, so these counters do not claim audible output.

An earlier attempt the same day downloaded six packages before the Store returned HTTP 503 for every further
request and then became unreachable; that run is not counted. The recorded runs started after the catalog
endpoint answered HTTP 200 again.

| Cartridge | Version | Accurate px/hash | Accurate events/pcm | Unlimited px/hash | Unlimited events/pcm | Result |
|---|---:|---:|---:|---:|---:|---|
| `it.uniparthenope.asteroids` | 1.0.0 | 567/60 | 0/0 | 567/60 | 0/0 | Pass |
| `it.uniparthenope.audiotest` | 1.0.0 | 3834/10 | 87/0 | 3834/10 | 87/0 | Pass |
| `it.uniparthenope.bachdemo` | 1.0.0 | 12183/19 | 814/0 | 9495/3 | 475/0 | Pass |
| `it.uniparthenope.blackjack` | 1.0.0 | 64000/17 | 239/0 | 64000/17 | 239/0 | Pass |
| `it.uniparthenope.breakout` | 1.0.0 | 3599/60 | 0/0 | 3599/60 | 0/0 | Pass |
| `prg32.cockroaches` | 1.1.0 | 61992/50 | 647/39336 | 57173/1 | 5/0 | Pass |
| `it.uniparthenope.debris` | 1.0.0 | 7765/54 | 14/0 | 7766/54 | 14/0 | Pass |
| `it.uniparthenope.devicedemo` | 1.0.0 | 26164/44 | 22/0 | 26164/43 | 22/0 | Pass |
| `org.riscv-prg32.drumfight-napoli97` | 1.1.0 | 31476/54 | 179/514810 | 31476/10 | 21/70551 | Pass |
| `org.riscv-prg32.dukesofduchesca-napoli97` | 2.4.0 | 60890/44 | 841/0 | 60290/14 | 69/0 | Pass |
| `org.riscv-prg32.entomo-napoli2010` | 1.0.0 | 64000/54 | 537/0 | 64000/54 | 236/0 | Pass |
| `org.riscv-prg32.fairwind-napoli97` | 4.2.0 | 64000/10 | 108/0 | 64000/10 | 9/0 | Pass |
| `it.uniparthenope.frogger` | 1.0.0 | 63998/58 | 1/0 | 63998/58 | 1/0 | Pass |
| `org.riscv-prg32.galleria2007` | 0.1.0 | 50164/34 | 262/0 | 20445/2 | 6/0 | Pass |
| `org.riscv-prg32.galleria2007.en` | 0.1.0 | 50159/34 | 262/0 | 20502/2 | 6/0 | Pass |
| `org.uniparthenope.prg32.inputtester` | 1.0.0 | 1169/11 | 64/0 | 1169/11 | 64/0 | Pass |
| `org.uniparthenope.prg32.moana-lemon-apocalypse` | 1.1.0 | 64000/49 | 575/830200 | 64000/49 | 575/830200 | Pass |
| `org.riscv-prg32.naprider-napoli97` | 2.0.0 | 49963/45 | 537/0 | 49963/45 | 123/0 | Pass |
| `org.riscv-prg32.outbun-napoli97` | 1.1.0 | 64000/60 | 139/0 | 64000/60 | 38/0 | Pass |
| `it.uniparthenope.pacman` | 1.0.0 | 5600/19 | 0/0 | 5600/19 | 0/0 | Pass |
| `it.uniparthenope.platformer` | 1.0.0 | 62418/20 | 0/0 | 62418/11 | 0/0 | Pass |
| `it.uniparthenope.poing` | 1.0.0 | 17435/53 | 8/0 | 17435/53 | 8/0 | Pass |
| `it.uniparthenope.pong` | 1.0.0 | 1167/60 | 0/0 | 1167/60 | 0/0 | Pass |
| `it.uniparthenope.raycaster` | 1.0.0 | 61562/51 | 0/0 | 61562/51 | 0/0 | Pass |
| `it.uniparthenope.performancetest` | 1.0.0 | 63211/13 | 0/0 | 63211/13 | 0/0 | Pass |
| `prg32.spacebeltmadness` | 1.1.0 | 64000/42 | 2143/130552 | 64000/42 | 297/83536 | Pass |
| `it.uniparthenope.space_invaders` | 1.0.0 | — | — | — | — | Excluded: non-portable ABI table |
| `org.riscv-prg32.spiriti-napoli97` | 3.0.0 | 64000/58 | 657/0 | 64000/5 | 66/0 | Pass |
| `it.uniparthenope.terraforge` | 1.0.0 | — | — | — | — | Excluded: non-portable ABI table |
| `it.uniparthenope.grendizer_c` | 1.0.0 | 64000/36 | 318/0 | 64000/36 | 318/0 | Pass |
| `it.uniparthenope.wing_commander` | 1.0.0 | 17043/60 | 68/0 | 17043/60 | 68/0 | Pass |
| `it.uniparthenope.youhavegotpizza` | 1.0.0 | 55598/52 | 18/0 | 55598/52 | 18/0 | Pass |

Both summaries were `catalog=32 passed=30 skipped_nonportable=2 failed=0`.

## Catalog changes since 2026-10-02

- Added: `org.riscv-prg32.entomo-napoli2010` 1.0.0 and `org.riscv-prg32.naprider-napoli97` 2.0.0.
- Updated: `org.riscv-prg32.drumfight-napoli97` 1.1.0 (now emits PCM), `org.riscv-prg32.dukesofduchesca-napoli97`
  2.4.0, and `org.riscv-prg32.spiriti-napoli97` 3.0.0.
- No longer listed: `it.uniparthenope.moana_lemon_c` and `org.riscv-prg32.nacup-napoli97`.

## Rendering differences against 0.4.1

Each cached package was run for 300 input-swept frames with `--dump-ppm` on both this commit and the 0.4.1
runner (`e9104f5`). The final frames were byte-identical for 28 of the 30 portable cartridges. The two that
differ, `it.uniparthenope.platformer` and `it.uniparthenope.wing_commander`, differ only in tile cells: tiles
now paint their defined background (coins on a black cell, wall tiles with a filled background), matching the
firmware's `prg32_tile.c` behaviour now ported for the tile and playfield ABI calls. Actor positions, HUD text, score,
and sprites are unchanged in both frames. The cartridges were not compared against physical PRG32 hardware in
this run.
