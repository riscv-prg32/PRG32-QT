# Rectangle-clipping Store certification — 2026-10-02

Commit-under-test was based on `9f49443`. The Apple Silicon macOS host used the native arm64 headless runner
built with Qt 6.11.2. Both runs used the default Store, 900 frames, media verification, and the standard input
sweep. `px/hash` records final non-black pixels and distinct framebuffer hashes; `events/pcm` records audio
events and PCM samples. No physical output device or listening check was performed, so these counters do not
claim audible output.

| Cartridge | Version | Accurate px/hash | Accurate events/pcm | Unlimited px/hash | Unlimited events/pcm | Result |
|---|---:|---:|---:|---:|---:|---|
| `it.uniparthenope.asteroids` | 1.0.0 | 567/60 | 0/0 | 567/60 | 0/0 | Pass |
| `it.uniparthenope.audiotest` | 1.0.0 | 3834/10 | 87/0 | 3834/10 | 87/0 | Pass |
| `it.uniparthenope.bachdemo` | 1.0.0 | 12183/19 | 814/0 | 9495/5 | 451/0 | Pass |
| `it.uniparthenope.blackjack` | 1.0.0 | 64000/17 | 239/0 | 64000/17 | 239/0 | Pass |
| `it.uniparthenope.breakout` | 1.0.0 | 3599/60 | 0/0 | 3599/60 | 0/0 | Pass |
| `prg32.cockroaches` | 1.1.0 | 61992/50 | 647/39336 | 61669/3 | 12/252 | Pass |
| `it.uniparthenope.debris` | 1.0.0 | 7765/54 | 14/0 | 7815/54 | 14/0 | Pass |
| `it.uniparthenope.devicedemo` | 1.0.0 | 25951/46 | 22/0 | 25951/47 | 22/0 | Pass |
| `org.riscv-prg32.drumfight-napoli97` | 1.0.0 | 31476/55 | 154/0 | 31476/16 | 30/0 | Pass |
| `org.riscv-prg32.dukesofduchesca-napoli97` | 1.0.0 | 64000/19 | 1024/0 | 64000/19 | 99/0 | Pass |
| `org.riscv-prg32.fairwind-napoli97` | 4.2.0 | 64000/10 | 108/0 | 64000/10 | 9/0 | Pass |
| `it.uniparthenope.frogger` | 1.0.0 | 63998/58 | 1/0 | 63998/58 | 1/0 | Pass |
| `org.riscv-prg32.galleria2007` | 0.1.0 | 50164/34 | 262/0 | 20445/2 | 6/0 | Pass |
| `org.riscv-prg32.galleria2007.en` | 0.1.0 | 50159/34 | 262/0 | 20502/2 | 6/0 | Pass |
| `org.uniparthenope.prg32.inputtester` | 1.0.0 | 1169/11 | 64/0 | 1169/11 | 64/0 | Pass |
| `it.uniparthenope.moana_lemon_c` | 1.0.0 | 63996/47 | 153/0 | 63996/47 | 153/0 | Pass |
| `org.uniparthenope.prg32.moana-lemon-apocalypse` | 1.1.0 | 64000/49 | 575/830200 | 64000/49 | 575/830200 | Pass |
| `org.riscv-prg32.nacup-napoli97` | 3.1.0 | 64000/10 | 108/0 | 64000/10 | 12/0 | Pass |
| `org.riscv-prg32.outbun-napoli97` | 1.1.0 | 64000/60 | 139/0 | 64000/60 | 143/0 | Pass |
| `it.uniparthenope.pacman` | 1.0.0 | 5600/19 | 0/0 | 5600/19 | 0/0 | Pass |
| `it.uniparthenope.platformer` | 1.0.0 | 62718/24 | 0/0 | 62718/24 | 0/0 | Pass |
| `it.uniparthenope.poing` | 1.0.0 | 17435/53 | 8/0 | 17435/53 | 8/0 | Pass |
| `it.uniparthenope.pong` | 1.0.0 | 1167/60 | 0/0 | 1167/60 | 0/0 | Pass |
| `it.uniparthenope.raycaster` | 1.0.0 | 61562/51 | 0/0 | 61562/51 | 0/0 | Pass |
| `it.uniparthenope.performancetest` | 1.0.0 | 63211/13 | 0/0 | 63211/13 | 0/0 | Pass |
| `prg32.spacebeltmadness` | 1.1.0 | 64000/42 | 2143/130552 | 64000/42 | 667/91176 | Pass |
| `it.uniparthenope.space_invaders` | 1.0.0 | — | — | — | — | Excluded: non-portable ABI table |
| `org.riscv-prg32.spiriti-napoli97` | 1.2.3 | 64000/8 | 547/0 | 64000/8 | 151/0 | Pass |
| `it.uniparthenope.terraforge` | 1.0.0 | — | — | — | — | Excluded: non-portable ABI table |
| `it.uniparthenope.grendizer_c` | 1.0.0 | 64000/36 | 318/0 | 64000/36 | 318/0 | Pass |
| `it.uniparthenope.wing_commander` | 1.0.0 | 11094/60 | 68/0 | 11094/60 | 68/0 | Pass |
| `it.uniparthenope.youhavegotpizza` | 1.0.0 | 55598/52 | 18/0 | 55598/52 | 18/0 | Pass |

Both summaries were `catalog=32 passed=30 skipped_nonportable=2 failed=0`.
