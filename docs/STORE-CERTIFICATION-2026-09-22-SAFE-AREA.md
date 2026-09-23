# Cartridge Store certification — mobile safe-area revision

> This historical snapshot predates PRG32-QT multiplayer support. The later 2026-09-23 validation in
> `docs/TESTING.md` executes NaCup-napoli97 successfully and supersedes its exclusion below.

The default Store returned 25 entries on 2026-09-23. The current arm64 macOS headless runner executed every
portable qemu package for 900 frames with the media/input sweep. Twenty-two packages passed and three known
non-portable packages were explicitly excluded; no portable package failed.

| Cartridge ID | Version | Result | Maximum non-black pixels | Frame hashes | Audio events |
|---|---:|---|---:|---:|---:|
| `it.uniparthenope.asteroids` | 1.0.0 | Pass | 567 | 60 | 0 |
| `it.uniparthenope.audiotest` | 1.0.0 | Pass | 3834 | 10 | 87 |
| `it.uniparthenope.bachdemo` | 1.0.0 | Pass | 9495 | 5 | 451 |
| `it.uniparthenope.blackjack` | 1.0.0 | Pass | 64000 | 17 | 239 |
| `it.uniparthenope.breakout` | 1.0.0 | Pass | 3599 | 60 | 0 |
| `it.uniparthenope.debris` | 1.0.0 | Pass | 7766 | 54 | 14 |
| `it.uniparthenope.devicedemo` | 1.0.0 | Pass | 25951 | 45 | 22 |
| `org.riscv-prg32.dukesofduchesca-napoli97` | 1.0.0 | Pass | 64000 | 19 | 99 |
| `it.uniparthenope.frogger` | 1.0.0 | Pass | 63998 | 58 | 1 |
| `org.uniparthenope.prg32.inputtester` | 1.0.0 | Pass | 1169 | 11 | 64 |
| `it.uniparthenope.moana_lemon_c` | 1.0.0 | Pass | 63996 | 47 | 153 |
| `org.riscv-prg32.nacup-napoli97` | 3.1.0 | Non-portable exclusion (multiplayer) | — | — | — |
| `it.uniparthenope.pacman` | 1.0.0 | Pass | 5600 | 19 | 0 |
| `it.uniparthenope.platformer` | 1.0.0 | Pass | 62718 | 24 | 0 |
| `it.uniparthenope.poing` | 1.0.0 | Pass | 17435 | 53 | 8 |
| `it.uniparthenope.pong` | 1.0.0 | Pass | 1167 | 60 | 0 |
| `it.uniparthenope.raycaster` | 1.0.0 | Pass | 61562 | 51 | 0 |
| `it.uniparthenope.performancetest` | 1.0.0 | Pass | 63211 | 13 | 0 |
| `prg32.spacebeltmadness` | 1.0.0 | Pass | 8013 | 41 | 613 |
| `it.uniparthenope.space_invaders` | 1.0.0 | Non-portable exclusion | — | — | — |
| `org.riscv-prg32.spiriti-napoli97` | 1.2.3 | Pass | 64000 | 8 | 151 |
| `it.uniparthenope.terraforge` | 1.0.0 | Non-portable exclusion | — | — | — |
| `it.uniparthenope.grendizer_c` | 1.0.0 | Pass | 64000 | 36 | 318 |
| `it.uniparthenope.wing_commander` | 1.0.0 | Pass | 11094 | 60 | 68 |
| `it.uniparthenope.youhavegotpizza` | 1.0.0 | Pass | 55598 | 52 | 18 |

`audio_events` is instrumentation from the audit sink, not a physical speaker-listening claim. All packages
reported zero PCM samples; audio-active titles used note/tone/track events. Spiriti received the full input
sweep and a separate frame-exact A press verified its restored map background. Dukes received the full sweep
and a separate native Qt UI tap verified its title-to-gameplay transition.
