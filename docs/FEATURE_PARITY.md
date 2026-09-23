# PRG32 Feature Compatibility

The public PRG32 ABI is the compatibility baseline for this checklist. A feature is marked implemented only when PRG32-QT has a corresponding code path rather than merely documenting an intention.

| PRG32 capability | PRG32-QT implementation |
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
| Touch D-pad with diagonals, A, B, Start/Select and frame-edge latching | `qml/Main.qml`, `InputState` |
| Adaptive portrait/landscape player | `qml/Main.qml` |
| iOS/Android native safe-area integration | `MobileSafeArea_ios`, `MobileSafeArea_android`, QML player/setup container |
| Startup splash and startup tone | QML + `QtAudioEngine` |
| Store browser/downloader | `StoreClient`, QML |
| Default Store `http://193.205.230.7:5080` | build definition / `StoreClient` |
| Store URL persistence, settings, test connection | `QSettings`, QML |
| Search and tag filtering | QML |
| Store cartridge icons | `StoreClient` image cache, `RasterImageItem`, QML |
| Local `.prg32` import | Qt `FileDialog` + `AppController` |
| Save successfully launched cartridges in app data | `AppController` |
| Local score APIs/current player | `Runtime` |
| Performance ABI calls | `Runtime` |
| Multiplayer rooms and player snapshots | `MultiplayerService`, `QtMultiplayerService`, `Runtime` ABI #32-#40 |
| Safe unavailable Wi-Fi semantics | `Runtime` |
| Band/keyboard/text-input baseline stub semantics | `Runtime` |
| Canonical PRG32 artwork | `assets/` |
| PRG32 regression cartridges | `tests/fixtures/` + CTest |

## Qt-only extensions

PRG32-QT adds desktop keyboard input and external game-controller support without changing guest ABI semantics. The combined input mask is identical to the iOS touch mask.
