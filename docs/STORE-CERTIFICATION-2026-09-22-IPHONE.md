# Official Store certification — iPhone, 2026-09-22

The official Store at `http://193.205.230.7:5080` returned the following 23-entry catalog. The tested application was Git commit `3e45eb7`, version `0.3.0-dev.15+g3e45eb76c8b1`, installed as bundle version `3000015` on a wired iPhone 12 Pro Max (`iPhone13,4`, arm64, iOS 27.0). Its `/api/runtime` endpoint independently reported `qt-0.3.0-dev.15+g3e45eb76c8b1`.

The portable host runner executed 900 frames per package with media verification and an input sweep. The device run uploaded and selected each Store package through the application's LAN API, confirmed frame progression, and counted non-black pixels in the device's 320×200 BMP framebuffer. Two framebuffer captures were compared for each device run; one distinct capture does not by itself indicate a failure because the brief sampling window may cover a static scene. `Audio` below is the host runner's audio-event count, not an acoustic measurement. The seven rows with zero audio events reported `audio=not-declared`; the other 14 reported `audio=active`. PCM sample counts were zero for all entries. The device's `cart.audio` field reports embedded sample data only and does not represent synthesized audio activity.

| Store ID | Version | Host result | Host non-black pixels | Host framebuffer hashes | Host audio events | iPhone result | iPhone non-black pixels | iPhone distinct captures |
| --- | --- | --- | ---: | ---: | ---: | --- | ---: | ---: |
| `it.uniparthenope.asteroids` | 1.0.0 | Pass | 567 | 60 | 0 | Pass | 567 | 2 |
| `it.uniparthenope.audiotest` | 1.0.0 | Pass | 3834 | 10 | 87 | Pass | 3793 | 2 |
| `it.uniparthenope.bachdemo` | 1.0.0 | Pass | 9495 | 5 | 451 | Pass | 9580 | 2 |
| `it.uniparthenope.blackjack` | 1.0.0 | Pass | 64000 | 17 | 239 | Pass | 63856 | 1 |
| `it.uniparthenope.breakout` | 1.0.0 | Pass | 3599 | 60 | 0 | Pass | 3599 | 2 |
| `it.uniparthenope.debris` | 1.0.0 | Pass | 7765 | 54 | 14 | Pass | 1849 | 2 |
| `it.uniparthenope.devicedemo` | 1.0.0 | Pass | 26164 | 45 | 22 | Pass | 11641 | 2 |
| `org.riscv-prg32.dukesofduchesca-napoli97` | 1.0.0 | Pass | 64000 | 19 | 37 | Pass | 64000 | 2 |
| `it.uniparthenope.frogger` | 1.0.0 | Pass | 63998 | 58 | 1 | Pass | 63998 | 2 |
| `org.uniparthenope.prg32.inputtester` | 1.0.0 | Pass | 1169 | 11 | 64 | Pass | 113 | 1 |
| `it.uniparthenope.moana_lemon_c` | 1.0.0 | Pass | 63996 | 47 | 153 | Pass | 63996 | 1 |
| `it.uniparthenope.pacman` | 1.0.0 | Pass | 5600 | 19 | 0 | Pass | 5584 | 2 |
| `it.uniparthenope.platformer` | 1.0.0 | Pass | 62418 | 19 | 0 | Pass | 62418 | 1 |
| `it.uniparthenope.poing` | 1.0.0 | Pass | 17435 | 53 | 8 | Pass | 12947 | 2 |
| `it.uniparthenope.pong` | 1.0.0 | Pass | 1167 | 60 | 0 | Pass | 1167 | 2 |
| `it.uniparthenope.raycaster` | 1.0.0 | Pass | 61562 | 51 | 0 | Pass | 61562 | 1 |
| `it.uniparthenope.performancetest` | 1.0.0 | Pass | 63211 | 13 | 0 | Pass | 63211 | 2 |
| `it.uniparthenope.space_invaders` | 1.0.0 | Excluded: non-portable ABI | — | — | — | Excluded: same parser rejection | — | — |
| `org.riscv-prg32.spiriti-napoli97` | 1.2.3 | Pass | 64000 | 8 | 154 | Pass | 64000 | 2 |
| `it.uniparthenope.terraforge` | 1.0.0 | Excluded: non-portable ABI | — | — | — | Excluded: same parser rejection | — | — |
| `it.uniparthenope.grendizer_c` | 1.0.0 | Pass | 64000 | 36 | 318 | Pass | 1710 | 2 |
| `it.uniparthenope.wing_commander` | 1.0.0 | Pass | 16928 | 60 | 68 | Pass | 16824 | 2 |
| `it.uniparthenope.youhavegotpizza` | 1.0.0 | Pass | 55598 | 52 | 18 | Pass | 47461 | 2 |

Totals: 21 portable passes, two explicit non-portable exclusions, zero portable failures on both the host and iPhone. The iPhone frame count advanced by 22–27 frames during each sampled run. The iPhone Setup screen rendered and displayed its LAN API endpoint. All four CTest targets passed on the arm64 macOS host.

The device automation did not actuate physical touch controls or a controller, change orientation, browse the Store through the visible UI, or perform a speaker listening check. Thus the device run certifies loading, execution, and framebuffer output, while the host run supplies the full input/media sweep and instrumented audio evidence. These physical/UI checks remain open for release qualification; audio-event counts must not be described as audible playback.
