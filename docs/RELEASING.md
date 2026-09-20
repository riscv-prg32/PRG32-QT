# Releasing PRG32-QT

1. Update `CHANGELOG.md`, `CMakeLists.txt` project version and `CITATION.cff`.
2. Run the portable suite and both iOS-baseline cartridge fixtures.
3. Confirm CI is green for Windows, Linux, ARM64 Linux/Raspberry-Pi-compatible, macOS, iOS and Android.
4. Run the Store certification workflow against the default Store and review every cartridge result.
5. Verify the canonical PRG32 artwork and platform packaging metadata.
6. Tag `vX.Y.Z` and push the tag. The release workflow creates source archives.

Binary/app-store releases should additionally be installed and launched on representative physical hardware for each target family, with audio, touch/keyboard, controller hot-plug, local import and Store download exercised before signing/publication.
