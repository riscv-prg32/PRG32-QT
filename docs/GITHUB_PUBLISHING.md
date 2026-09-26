# GitHub Publication Checklist

Suggested repository name: `PRG32-QT`

Suggested description:

> Portable C++20/Qt 6 PRG32 cartridge player for Windows, Linux, Raspberry Pi, macOS, iOS, and Android.

Suggested topics:

`prg32`, `risc-v`, `riscv`, `qt`, `qt6`, `cpp`, `emulator`, `windows`, `linux`, `raspberry-pi`, `ios`, `android`, `macos`, `retro-gaming`, `education`

## First push

```sh
git init
git add .
git commit -m "Initial PRG32-QT open-source release"
git branch -M main
git remote add origin git@github.com:OWNER/PRG32-QT.git
git push -u origin main
```

If publishing under the `riscv-prg32` organization, replace `OWNER` with `riscv-prg32`.

## Recommended repository settings

- Enable Issues and GitHub Actions.
- Enable Private Vulnerability Reporting if available.
- Protect `main` and require the `Portable core`, `Windows / Qt`, `Linux / Qt`, `Raspberry Pi compatible / ARM64 Linux`, `macOS / Qt`, `iOS / Qt`, and `Android / Qt` checks before merge once the workflows are confirmed on the repository's runners.
- Require pull requests for changes to `main`.
- Enable Dependabot alerts and security updates.
- Add the suggested topics and repository description.
- Verify the canonical PRG32 logo before publishing signed App Store / Play Store binaries.

## First release

After CI and Store certification are green, update `CHANGELOG.md` if necessary and push an annotated tag:

```sh
git tag -a v0.3.0 -m "PRG32-QT 0.3.0"
git push origin v0.3.0
```

The release workflow creates reproducible source archives and native installation packages: a Qt-deployed
Windows NSIS Setup executable, macOS DMGs for Apple Silicon and Intel, Debian packages for Linux x86-64 and Raspberry Pi OS
ARM64, iOS and Apple TV simulator application bundles, and Android/Android TV APKs. It verifies the expected
asset count and publishes SHA-256 manifests with the GitHub Release. Development-signed iPhone applications
remain device/team-specific and are validated before tagging rather than published as a reusable binary.
