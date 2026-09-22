# Releasing PRG32-QT

1. Update `CHANGELOG.md`; change `VERSION.txt` only when selecting a new semantic-version development line.
2. Run the portable suite and both iOS-baseline cartridge fixtures.
3. Confirm CI is green for Windows, Linux, ARM64 Linux/Raspberry-Pi-compatible, macOS, iOS, Android and Android TV. Confirm the Apple TV job compiled with the configured Qt tvOS kit, or attach the explicitly reported kit limitation and complete the build manually before release.
4. Run the Store certification workflow against the default Store and review every cartridge result.
5. Verify the canonical PRG32 artwork and platform packaging metadata.
6. Tag `vX.Y.Z` and push the tag. `.github/workflows/release.yml` verifies that CMake derives exactly the tag version, reruns the portable release suite, creates `.tar.gz` and `.zip` source
   archives plus `SHA256SUMS.txt`, and publishes them in a GitHub Release with generated notes.

The release workflow may also be started manually for an existing tag. It never invents or moves a tag: the
specified tag must already exist and must have the exact `vX.Y.Z` form.

An exact release tag produces `X.Y.Z`. Other Git checkouts produce `X.Y.Z-dev.N+gCOMMIT`, where `N` is the
distance from the nearest release tag (or the repository commit count before the first tag). Apple
`CFBundleVersion` and Android `versionCode` use the corresponding generated monotonic numeric build value.

Binary/app-store releases should additionally be installed and launched on representative physical hardware for each target family, with audio, touch/keyboard, controller hot-plug, local import and Store download exercised before signing/publication.
