# Testing

PRG32-QT uses layered validation.

## Portable deterministic suite

```sh
cmake -S . -B build-core -G Ninja -DPRG32QT_BUILD_APP=OFF
cmake --build build-core
ctest --test-dir build-core --output-on-failure
```

The suite includes core unit tests plus `Asteroids.prg32` and `Bach.prg32` copied from the attached PRG32-iOS implementation. Each fixture is initialized and executed for 300 frames, covering graphics/runtime behavior and the audio-heavy cartridge path.

## Platform compilation

`.github/workflows/ci.yml` builds the Qt application for Windows, Linux, ARM64 Linux, macOS, iOS and Android. ARM64 Linux is the continuous build proxy for Raspberry Pi OS/Raspbian portability; release qualification should also be performed natively on the target Pi hardware/image.

## Whole-Store certification

```sh
python3 scripts/store-smoke.py --runner ./build-core/prg32qt-headless --store http://193.205.230.7:5080 --frames 300
```

This enumerates the Store, downloads PRG2 cartridges and executes every package headlessly. It is intentionally separated from deterministic PR CI because the Store is an external service.

## Physical-device checks

Before signed releases verify startup, Store browsing/download, local import, graphics, audio and input on representative devices. On desktops also verify keyboard and controller connect/disconnect behavior; on mobile verify both portrait and landscape touch layouts.
