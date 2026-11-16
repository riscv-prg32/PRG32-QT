# SDK 5d2e650 Store certification — 2026-11-15

The working tree under test aligns PRG32-QT with PRG32 SDK `main` `5d2e650` (multiplayer server discovery,
public fallback, Store `latest_version`). The Apple Silicon macOS host used the native arm64 Release headless
runner (`-DCMAKE_BUILD_TYPE=Release -DPRG32QT_BUILD_APP=OFF`), the default Store, 900 frames, media verification,
the standard input sweep, and one shared `--cache-dir`, so both runs certified identical package bytes. `px/hash`
records final non-black pixels and distinct framebuffer hashes; `events/pcm` records audio events and PCM samples.
No listening check was performed, so these counters do not claim audible output.

Summary: Accurate and Unlimited both `catalog=38 passed=36 skipped_nonportable=2 failed=0`.

| Cartridge | Version | Accurate px/hash | Accurate events/pcm | Unlimited px/hash | Unlimited events/pcm | Result |
|---|---:|---:|---:|---:|---:|---|
| `it.uniparthenope.asteroids` | 1.0.0 | 567/60 | 0/0 | 567/60 | 0/0 | Pass |
| `it.uniparthenope.audiotest` | 1.0.0 | 3834/10 | 87/0 | 3834/10 | 87/0 | Pass |
| `org.riscv-prg32.azzurra-pro-tennis` | 2.2.0 | 64000/46 | 334/0 | 64000/2 | 16/0 | Pass |
| `it.uniparthenope.bachdemo` | 1.0.0 | 12183/19 | 814/0 | 9495/3 | 475/0 | Pass |
| `it.uniparthenope.blackjack` | 1.0.0 | 64000/17 | 239/0 | 64000/17 | 239/0 | Pass |
| `it.uniparthenope.breakout` | 1.0.0 | 3599/60 | 0/0 | 3599/60 | 0/0 | Pass |
| `prg32.cockroaches` | 1.1.0 | 61992/50 | 647/39336 | 61669/2 | 9/168 | Pass |
| `it.uniparthenope.debris` | 1.0.0 | 7765/54 | 14/0 | 7813/54 | 14/0 | Pass |
| `it.uniparthenope.devicedemo` | 1.0.0 | 26164/44 | 22/0 | 26164/43 | 22/0 | Pass |
| `org.riscv-prg32.drumfight-napoli97` | 1.1.0 | 31476/54 | 179/514810 | 31476/11 | 21/70551 | Pass |
| `org.riscv-prg32.dukesofduchesca-napoli97` | 2.4.0 | 60890/44 | 841/0 | 60285/20 | 75/0 | Pass |
| `org.riscv-prg32.entomo-napoli2010` | 1.0.0 | 64000/54 | 537/0 | 64000/54 | 251/0 | Pass |
| `org.riscv-prg32.fairwind-napoli97` | 5.0.0 | 64000/49 | 1262/0 | 64000/49 | 109/0 | Pass |
| `it.uniparthenope.frogger` | 1.0.0 | 63998/58 | 1/0 | 63998/58 | 1/0 | Pass |
| `org.riscv-prg32.galleria2007` | 0.2.0 | 55990/46 | 268/14599 | 27617/20 | 12/0 | Pass |
| `org.riscv-prg32.galleria2007.en` | 0.2.0 | 56000/46 | 268/14599 | 27963/24 | 10/0 | Pass |
| `org.uniparthenope.prg32.inputtester` | 1.0.0 | 1169/11 | 64/0 | 1169/11 | 64/0 | Pass |
| `org.uniparthenope.prg32.moana-lemon-apocalypse` | 1.1.0 | 64000/49 | 575/830200 | 64000/49 | 575/830200 | Pass |
| `org.riscv-prg32.naprider-napoli97` | 2.0.0 | 49963/45 | 537/0 | 49963/45 | 127/0 | Pass |
| `org.riscv-prg32.outbun-napoli97` | 1.1.0 | 64000/60 | 139/0 | 64000/60 | 38/0 | Pass |
| `it.uniparthenope.pacman` | 1.0.0 | 5600/19 | 0/0 | 5600/19 | 0/0 | Pass |
| `it.uniparthenope.platformer` | 1.0.0 | 62418/20 | 0/0 | 62418/12 | 0/0 | Pass |
| `it.uniparthenope.poing` | 1.0.0 | 17435/53 | 8/0 | 17435/53 | 8/0 | Pass |
| `org.riscv-prg32.pompeii-the-last-song-of-vesuvius` | 2.1.0 | 64000/60 | 876/0 | 64000/60 | 876/0 | Pass |
| `it.uniparthenope.pong` | 1.0.0 | 1167/60 | 0/0 | 1167/60 | 0/0 | Pass |
| `it.uniparthenope.raycaster` | 1.0.0 | 61562/51 | 0/0 | 61562/51 | 0/0 | Pass |
| `org.riscv-prg32.sensational-soccer` | 1.1.0 | 64000/60 | 321/0 | 64000/60 | 209/0 | Pass |
| `it.uniparthenope.performancetest` | 1.0.0 | 63211/13 | 0/0 | 63211/13 | 0/0 | Pass |
| `org.riscv-prg32.skipass-madness-after-christmas` | 2.1.0 | 64000/40 | 1745/0 | 64000/9 | 345/0 | Pass |
| `prg32.spacebeltmadness` | 1.1.0 | 64000/42 | 2143/130552 | 64000/42 | 311/84644 | Pass |
| `it.uniparthenope.space_invaders` | 1.0.0 | — | — | — | — | Non-portable exclusion |
| `org.riscv-prg32.spiriti-napoli97` | 3.0.0 | 64000/58 | 657/0 | 64000/6 | 86/0 | Pass |
| `it.uniparthenope.terraforge` | 1.0.0 | — | — | — | — | Non-portable exclusion |
| `org.riscv-prg32.the-trerrote-connection` | 1.1.1 | 64000/31 | 1116/0 | 64000/1 | 0/0 | Pass |
| `it.uniparthenope.grendizer_c` | 1.0.0 | 64000/36 | 318/0 | 64000/36 | 318/0 | Pass |
| `it.uniparthenope.wing_commander` | 1.0.0 | 17043/60 | 68/0 | 17043/60 | 68/0 | Pass |
| `org.riscv-prg32.you-have-got-pizza-deluxe` | 1.0.0 | 64000/40 | 541/0 | 64000/1 | 0/0 | Pass |
| `it.uniparthenope.youhavegotpizza` | 1.0.0 | 55598/52 | 18/0 | 55598/52 | 18/0 | Pass |

## Multiplayer

`scripts/store-multiplayer.py` ran every Store cartridge whose entry declares multiplayer on several consoles in one
process, each with its own `prg32-multiplayer-1.0` connection, walking each game to its online lobby with scripted
keys. A pass means every console was admitted with its own player ID (ABI #139) and saw every other console as a
peer (ABI #39/#40). `discover` used a local PRG32 Multiplayer Server (`5411482`) found by LAN discovery.

| Cartridge | Version | 2 consoles, discovery | 4 consoles, discovery | 2 consoles, `wss://multi.prg32.uniparthenope.it` |
|---|---:|---|---|---|
| `org.riscv-prg32.azzurra-pro-tennis` | 2.2.0 | Pass (peers 1/1) | Pass (peers 3/3/3/3) | Pass (peers 1/1) |
| `org.riscv-prg32.drumfight-napoli97` | 1.1.0 | Pass (peers 1/1) | Pass (peers 3/3/3/3) | Pass (peers 1/1) |
| `org.riscv-prg32.fairwind-napoli97` | 5.0.0 | Pass (peers 1/1) | Pass (peers 3/3/3/3) | Pass (peers 1/1) |
| `org.riscv-prg32.galleria2007` | 0.2.0 | Pass (peers 1/1) | Pass (peers 3/3/3/3) | Pass (peers 1/1) |
| `org.riscv-prg32.galleria2007.en` | 0.2.0 | Pass (peers 1/1) | Pass (peers 3/3/3/3) | Pass (peers 1/1) |
| `org.riscv-prg32.outbun-napoli97` | 1.1.0 | Pass (peers 1/1) | Pass (peers 3/3/3/3) | Pass (peers 1/1) |
| `org.riscv-prg32.sensational-soccer` | 1.1.0 | Pass (peers 1/1) | Pass (peers 3/3/3/3) | Pass (peers 1/1) |
| `org.riscv-prg32.skipass-madness-after-christmas` | 2.1.0 | Pass (peers 1/1) | Pass (peers 3/3/3/3) | Pass (peers 1/1) |

Dukes of Duchesca 2.4.0, NapRider 2.0.0, Pompeii 2.1.0 and Spiriti 3.0.0 mention multiplayer in their
entries but declare `multiplayer: false`; they are certified above as single-player cartridges.
