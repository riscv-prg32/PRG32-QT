# PRG32-iOS Feature Parity

The attached PRG32-iOS source tree is the behavioral baseline for this checklist. A feature is marked implemented only when PRG32-QT has a corresponding code path rather than merely documenting an intention.

| PRG32-iOS capability | PRG32-QT implementation |
|---|---|
| PRG2 header, bounds, entry-point and CRC validation | `src/core/Cartridge.*` |
| ABI-table portability/import validation | `Cartridge`, `Runtime` |
| ABI 1.6 / 139 host calls / compatible hashes | `Runtime` |
| RV32I/M/A/C guest execution | `Rv32Cpu` |
| Counter CSR reads used by optimized code | `Rv32Cpu` |
| 320×200 indexed framebuffer + RGB565 palette | `Framebuffer` |
| Pixel, rectangle, text, row snapshot | `Framebuffer`, `Runtime` |
| 1-bpp, RGB565, indexed and bitplane sprites | `Framebuffer`, `Runtime` |
| Sprite frames and animation helpers | `Runtime` |
| Tile screen, dual playfields, scrolling, parallax, camera | `Runtime` |
| Platform tile flags, collision, actor move/step, camera follow | `Runtime` |
| Tone/note/u8 PCM audio | `Runtime`, `QtAudioEngine` |
| `AUD0` samples/instruments/tracks | `Audio`, `Runtime` |
| Volume, channel volume, stereo pan, tempo | `Runtime`, `QtAudioEngine` |
| RGB LED emulation driven by audio/runtime activity | `Runtime`, QML player |
| Touch D-pad with diagonals, A, B, SELECT | `qml/Main.qml` |
| Adaptive portrait/landscape player | `qml/Main.qml` |
| Startup splash and startup tone | QML + `QtAudioEngine` |
| Store browser/downloader | `StoreClient`, QML |
| Default Store `http://193.205.230.7:5080` | build definition / `StoreClient` |
| Store URL persistence, settings, test connection | `QSettings`, QML |
| Search and tag filtering | QML |
| Store cartridge icons | `StoreClient::iconUrl`, QML |
| Local `.prg32` import | Qt `FileDialog` + `AppController` |
| Save successfully launched cartridges in app data | `AppController` |
| Local score APIs/current player | `Runtime` |
| Performance ABI calls | `Runtime` |
| Safe unavailable Wi-Fi/multiplayer semantics | `Runtime` |
| Band/keyboard/text-input baseline stub semantics | `Runtime` |
| Canonical PRG32 artwork from attached iOS source | `assets/` |
| iOS baseline regression cartridges | `tests/fixtures/` + CTest |

## Qt-only extensions

PRG32-QT adds desktop keyboard input and external game-controller support without changing guest ABI semantics. The combined input mask is identical to the iOS touch mask.
