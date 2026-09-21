# PRG32-QT

PRG32-QT is the portable C++20 / Qt 6 host for the PRG32 cartridge ecosystem. It uses the public PRG32 cartridge ABI as its compatibility contract while sharing one Qt-free runtime across desktop, mobile, Raspberry Pi, headless tests, and Store certification.

Supported host targets are **Windows, Linux, Raspberry Pi OS/Raspbian, macOS, iOS, and Android**. The default Cartridge Store is `http://193.205.230.7:5080`.

## Highlights

- PRG2 validation, CRC, ABI/import-model and feature checks.
- RV32IMAC guest execution including compressed instructions, atomics and counters used by portable cartridges.
- PRG32 ABI 1.6 (`0x260f6136`) plus documented compatible hashes.
- 320x200 indexed/RGB565 rendering, primitives, text, tiles, playfields, parallax, platform helpers, RGB565/indexed/bitplane sprites and animation.
- AUD0 samples, tones, notes, PCM, tracks, volume and stereo pan through Qt Multimedia.
- Touch handheld controls on mobile; keyboard plus hot-plug game controller support on desktop.
- RGB LED emulation, local scores, metrics/performance ABI calls and safe unavailable-service stubs.
- Store browser, search/tag filtering, Store URL settings/test, architecture-aware downloads, persistent local cartridge import, splash and adaptive portrait/landscape player UI.
- Canonical PRG32 artwork used by the PRG32 project.
- Headless regression fixtures included for compatibility testing and live whole-Store certification tooling.

## Build and test

Portable runtime only:

```sh
cmake -S . -B build-core -G Ninja -DPRG32QT_BUILD_APP=OFF
cmake --build build-core
ctest --test-dir build-core --output-on-failure
```

Host helpers are provided in `scripts/` for Windows, Linux, Raspberry Pi OS, macOS, iOS and Android. Platform prerequisites and exact commands are documented in [docs/PLATFORMS.md](docs/PLATFORMS.md).

For an Android ARM64 debug APK with the default Qt 6.8.3 installation layout:

```sh
./scripts/build-android.sh
```

The APK is written to `build-android/android-build/build/outputs/apk/debug/android-build-debug.apk`.

To exercise every discoverable Store cartridge after building the headless runner:

```sh
python3 scripts/store-smoke.py --runner ./build-core/prg32qt-headless \
  --store http://193.205.230.7:5080 --frames 300
```

## Documentation

Project-level design and ownership documentation lives under `docs/`:

- [Vision](docs/VISION.md)
- [Architecture](docs/ARCHITECTURE.md)
- [iOS feature parity](docs/FEATURE_PARITY.md)
- [Platform support](docs/PLATFORMS.md)
- [Compatibility](docs/COMPATIBILITY.md)
- [Testing](docs/TESTING.md)
- [Licensing](docs/LICENSING.md)
- [Authorship](docs/AUTHORSHIP.md)
- [Third-party notices](docs/THIRD_PARTY_NOTICES.md)
- [Releasing](docs/RELEASING.md)

The repository-root `LICENSE` remains intentionally at the conventional GitHub location; licensing rationale and attribution are maintained in `docs/LICENSING.md` and `docs/AUTHORSHIP.md`.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md), [SECURITY.md](SECURITY.md), and [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md). Compatibility changes should include regression coverage and, when Store behavior is affected, a whole-Store certification run.
