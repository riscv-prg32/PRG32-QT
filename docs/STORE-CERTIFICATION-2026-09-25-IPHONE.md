# Official Store certification — iPhone, 2026-09-25

The official Store at `http://193.205.230.7:5080` returned 27 entries. The tested application was Git commit
`89de9b15f977fc3729d35be61df7a792114e491c`, version `0.3.1`, development-signed by team `B57UXM7RWR`, and
installed on a wired iPhone 12 Pro Max (`iPhone13,4`, arm64, iOS 27.0). The live `/api/runtime` endpoint
reported `qt-0.3.1`, portable ABI 1.6, feature mask 477, and ABI hash 638542134.

The Release iOS build, signing validation, installation, launch, portrait Setup rendering, and landscape
rotation completed. The arm64 macOS host ran all four CTest targets and executed every portable Store package
for 900 frames with media verification and the built-in input sweep. The device run uploaded and selected each
package through the application's LAN API, sampled 1.2 seconds of execution, confirmed frame progression, and
compared two 320×200 BMP framebuffer captures. `Host audio events` and `PCM samples` are instrumentation, not
an acoustic listening result. A single distinct device capture is acceptable for a static scene because frame
progression was verified independently.

| Store ID | Version | Host result | Host non-black pixels | Host hashes | Host audio events | PCM samples | iPhone result | iPhone frames | iPhone non-black pixels | iPhone captures |
| --- | --- | --- | ---: | ---: | ---: | ---: | --- | ---: | ---: | ---: |
| `it.uniparthenope.asteroids` | 1.0.0 | Pass | 567 | 60 | 0 | 0 | Pass | 40 | 567 | 2 |
| `it.uniparthenope.audiotest` | 1.0.0 | Pass | 3834 | 10 | 87 | 0 | Pass | 40 | 3793 | 2 |
| `it.uniparthenope.bachdemo` | 1.0.0 | Pass | 9495 | 3 | 451 | 0 | Pass | 40 | 9772 | 2 |
| `it.uniparthenope.blackjack` | 1.0.0 | Pass | 64000 | 17 | 239 | 0 | Pass | 41 | 63856 | 1 |
| `it.uniparthenope.breakout` | 1.0.0 | Pass | 3599 | 60 | 0 | 0 | Pass | 41 | 3599 | 2 |
| `it.uniparthenope.debris` | 1.0.0 | Pass | 7813 | 54 | 14 | 0 | Pass | 40 | 1849 | 2 |
| `it.uniparthenope.devicedemo` | 1.0.0 | Pass | 25951 | 45 | 22 | 0 | Pass | 39 | 11650 | 2 |
| `org.riscv-prg32.dukesofduchesca-napoli97` | 1.0.0 | Pass | 64000 | 19 | 25 | 0 | Pass | 38 | 64000 | 1 |
| `org.riscv-prg32.fairwind-napoli97` | 4.0.0 | Pass | 64000 | 10 | 8 | 0 | Pass | 40 | 64000 | 1 |
| `it.uniparthenope.frogger` | 1.0.0 | Pass | 63998 | 58 | 1 | 0 | Pass | 40 | 64000 | 2 |
| `org.uniparthenope.prg32.inputtester` | 1.0.0 | Pass | 1169 | 11 | 64 | 0 | Pass | 41 | 113 | 1 |
| `it.uniparthenope.moana_lemon_c` | 1.0.0 | Pass | 63996 | 47 | 153 | 0 | Pass | 39 | 63996 | 2 |
| `org.riscv-prg32.nacup-napoli97` | 3.1.0 | Pass | 64000 | 10 | 8 | 0 | Pass | 42 | 64000 | 1 |
| `org.riscv-prg32.outbun-napoli97` | 1.1.0 | Pass | 64000 | 60 | 30 | 0 | Pass | 41 | 64000 | 2 |
| `it.uniparthenope.pacman` | 1.0.0 | Pass | 5600 | 19 | 0 | 0 | Pass | 42 | 5600 | 2 |
| `it.uniparthenope.platformer` | 1.0.0 | Pass | 62718 | 20 | 0 | 0 | Pass | 41 | 62718 | 2 |
| `it.uniparthenope.poing` | 1.0.0 | Pass | 17435 | 53 | 8 | 0 | Pass | 41 | 24356 | 2 |
| `it.uniparthenope.pong` | 1.0.0 | Pass | 1167 | 60 | 0 | 0 | Pass | 41 | 24356 | 2 |
| `it.uniparthenope.raycaster` | 1.0.0 | Pass | 61562 | 51 | 0 | 0 | Pass | 40 | 61562 | 2 |
| `it.uniparthenope.performancetest` | 1.0.0 | Pass | 63211 | 13 | 0 | 0 | Pass | 41 | 63211 | 2 |
| `prg32.spacebeltmadness` | 1.0.0 | Pass | 8013 | 41 | 341 | 111184 | Pass | 40 | 10205 | 2 |
| `it.uniparthenope.space_invaders` | 1.0.0 | Excluded: non-portable ABI | — | — | — | — | Excluded: same parser rejection | — | — | — |
| `org.riscv-prg32.spiriti-napoli97` | 1.2.3 | Pass | 64000 | 8 | 131 | 0 | Pass | 39 | 64000 | 2 |
| `it.uniparthenope.terraforge` | 1.0.0 | Excluded: non-portable ABI | — | — | — | — | Excluded: same parser rejection | — | — | — |
| `it.uniparthenope.grendizer_c` | 1.0.0 | Pass | 64000 | 36 | 318 | 0 | Pass | 41 | 1833 | 2 |
| `it.uniparthenope.wing_commander` | 1.0.0 | Pass | 11094 | 60 | 68 | 0 | Pass | 39 | 10961 | 2 |
| `it.uniparthenope.youhavegotpizza` | 1.0.0 | Pass | 55598 | 52 | 18 | 0 | Pass | 42 | 47461 | 2 |

Totals: 25 portable passes, two explicit non-portable exclusions, and zero portable failures on both the host
and iPhone. `Space Invaders` and `TerraForge` uploaded to storage but their selection returned the expected
`PRG32-QT accepts portable ABI-table PRG32 cartridges only` rejection. The performance cartridge exposed its
device performance action and `/api/performance.json` reported the run in progress; the deterministic
`--require-performance` CTest contract passed on the host.

The automated device session did not physically actuate touchscreen controls, connect an Apple
GameController or generic USB HID controller, browse the Store through the visible UI, or perform a speaker
listening check. Those checks require hands-on interaction and remain open for release qualification. The host
input sweep covered neutral, directional, A, B, Select, and combined masks for every portable package; its
audio results must not be described as audible output.
