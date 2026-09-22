# Platform support

PRG32-QT keeps cartridge execution in portable C++20 and confines host-specific work to Qt and small controller adapters. The supported matrix is:

| Host | UI/runtime | Input | Build validation |
|---|---|---|---|
| Windows 10/11 | Qt 6 desktop | keyboard + WinMM USB/game controllers | GitHub Actions Windows Qt job |
| Linux desktop | Qt 6 desktop | keyboard + Linux joystick devices | GitHub Actions Linux Qt job |
| Raspberry Pi OS / Raspbian | Qt 6 Linux ARM | keyboard + `/dev/input/js*` controllers | ARM64 Debian Trixie Qt build proxy in CI plus native Pi build script |
| macOS | Qt 6 desktop | keyboard + Apple GameController + generic USB HID joystick/gamepad fallback | GitHub Actions macOS Qt job |
| iOS | Qt 6 mobile | adaptive touch controls; Apple GameController backend is available | GitHub Actions Qt iOS build |
| Android | Qt 6 mobile | adaptive touch controls | GitHub Actions Qt Android ARM64 build |
| Apple TV | Qt 6 tvOS | Siri Remote / Apple GameController | CI contract check and Qt-tvOS-kit build when configured |
| Android TV | Qt 6 Android/Leanback | TV remote and Android game controller key events | GitHub Actions Qt Android TV ARM64 build |

## Common requirements

CMake 3.21+, a C++20 compiler, Qt 6.5+ with Core, Gui, Quick, QuickControls2, Network and Multimedia. CI currently uses Qt 6.8.3 where binary Qt kits are installed.

## Windows

Install Qt 6 desktop and Ninja, then from PowerShell:

```powershell
./scripts/build-windows.ps1
```

WinMM is used only for legacy/native joystick discovery and button polling; the emulator core has no Windows dependency.

## Linux

Install the Qt 6 development modules and Ninja:

```sh
./scripts/build-linux.sh
```

Access to `/dev/input/js*` may depend on local device permissions/group policy.

## Raspberry Pi OS / Raspbian

On a current Raspberry Pi OS release with Qt 6 packages installed, build natively:

```sh
sudo apt update
sudo apt install cmake ninja-build g++ qt6-base-dev qt6-declarative-dev qt6-multimedia-dev
./scripts/build-raspbian.sh
```

The project contains no x86 assumptions. RV32 cartridge code is interpreted by the portable core, so a Pi host can be either ARM32 or ARM64 when the required Qt 6 packages are available. CI exercises the full Qt application in an ARM64 Debian Trixie container; this is an architecture proxy, not a Raspberry Pi OS image. Ubuntu 24.04's Qt 6.4.2 is below the application's Qt 6.5 minimum. Final release qualification must run on the intended Raspberry Pi model and Raspberry Pi OS image with a sufficiently recent Qt kit.

## macOS

```sh
./scripts/build-macos.sh
```

Apple GameController handles supported USB/Bluetooth controllers and hot-plug notifications. An IOKit HID
fallback covers generic USB joysticks/gamepads that macOS exposes on Generic Desktop usage `Joystick` or
`Game Pad` but does not surface through GameController. X/Y axes and hats map to movement, HID buttons 1/2 map
to A/B, and buttons 7–10 map to Select.
CI uses the macOS 15 runner with Xcode 16 because the Qt 6.8.3 binary kit still links Apple's AGL framework,
which is absent from the newer Xcode 26 SDK on `macos-latest`.

Store Settings exposes Auto, Portrait, and Landscape player layouts. Fullscreen can be toggled from the player,
with `F11`, or with `Control+Command+F`; `Escape` returns to a window. On desktop, fullscreen deliberately hides
all chrome and touch controls and displays only the aspect-correct game surface for keyboard/controller play.
Both choices persist through `QSettings`.

## iOS

Install Xcode and a Qt iOS kit, then:

```sh
QT_ROOT=/path/to/Qt/6.8.3/ios ./scripts/build-ios.sh
```

Set `IOS_TEAM_ID` to your Apple development team ID and `IOS_DESTINATION` to a connected device destination to build a signed installable app. Set `IOS_DESTINATION` to a simulator destination for simulator builds.

The helper selects an `arm64` device build or an `x86_64` simulator build from `IOS_DESTINATION`. Qt's binary iOS package provides x86_64 simulator libraries, so Apple Silicon hosts run the simulator build with Rosetta installed. `IOS_ARCHS` can override the selected architecture for a custom Qt kit.

Build, install and launch an iPhone simulator app with:

```sh
IOS_DESTINATION='platform=iOS Simulator,id=<simulator-udid>' ./scripts/build-ios.sh
xcrun simctl install <simulator-udid> build-ios-sim/Release-iphonesimulator/PRG32.app
xcrun simctl launch --terminate-running-process <simulator-udid> org.riscv-prg32.prg32qt
```

Build and deploy to a development-signed physical device with:

```sh
IOS_TEAM_ID=<team-id> \
IOS_DESTINATION='platform=iOS,id=<device-udid>' \
./scripts/build-ios.sh
xcrun devicectl device install app --device <device-udid> \
  build-ios-device/Release-iphoneos/PRG32.app
xcrun devicectl device process launch --device <device-udid> \
  org.riscv-prg32.prg32qt
```

The plain-HTTP default Store requires the included App Transport Security allowance. Prefer HTTPS for production distribution.

## Apple TV

Apple TV uses the portable runtime and Apple GameController backend, built with a Qt kit compiled for tvOS.
The player is always a game-only, aspect-correct fullscreen surface. Siri Remote and game-controller D-pad,
A/B and Menu/Options input map to PRG32 input; touch and desktop window controls are not shown.

```sh
QT_ROOT=/path/to/Qt/tvos \
QT_HOST_ROOT=/path/to/Qt/macos \
TVOS_DESTINATION='generic/platform=tvOS Simulator' \
./scripts/build-tvos.sh
```

For a signed device build, set `TVOS_TEAM_ID` and use `platform=tvOS,id=<device-udid>`. The helper selects
`appletvsimulator`/`x86_64` or `appletvos`/`arm64`; `TVOS_ARCHS` can override the architecture. Qt's public
online installer and `install-qt-action` do not currently publish an open-source tvOS binary kit, so CI records
that limitation unless `PRG32QT_TVOS_QT_ROOT` and `PRG32QT_TVOS_QT_HOST_ROOT` identify a preinstalled kit. That
limitation artifact is not a successful Apple TV compile; release qualification still requires one.

## Android

Install an Android SDK with platform 34, NDK 26.1.10909125 (r26b), JDK 17 or newer, Ninja, and matching Qt host and Android ARM64 kits. Both Qt kits must include Core, Gui, Quick, QuickControls2, Network and Multimedia. With Qt 6.8.3 in the standard macOS locations, build with:

```sh
./scripts/build-android.sh
```

The helper defaults to `$HOME/Qt/6.8.3/android_arm64_v8a`, `$HOME/Qt/6.8.3/macos`, `$HOME/Library/Android/sdk`, and NDK `26.1.10909125`. Override any nonstandard installation explicitly:

```sh
QT_ROOT=/path/to/Qt/android_arm64_v8a \
QT_HOST_PATH=/path/to/Qt/host \
ANDROID_SDK_ROOT=/path/to/android-sdk \
ANDROID_NDK_ROOT=/path/to/android-ndk \
./scripts/build-android.sh
```

The build creates an ARM64 debug APK at `build-android/android-build/build/outputs/apk/debug/android-build-debug.apk` with application ID `org.riscvprg32.prg32qt`. It is debug-signed by Gradle; configure a release keystore and release packaging separately before distribution. The manifest enables Internet access and cleartext HTTP for the current default Store and retains Qt's required activity metadata and file provider.

## Android TV

Android TV reuses the Android ARM64 Qt kit and SDK/NDK requirements:

```sh
./scripts/build-android-tv.sh
```

The APK below `build-android-tv/android-build/build/outputs/apk/` uses application ID
`org.riscvprg32.prg32qt.tv`. Its manifest requires Leanback, makes touch optional, fixes landscape orientation,
and advertises a TV banner. Its activity reapplies immersive fullscreen whenever it regains focus. In the
player, remote/gamepad keys feed the PRG32 D-pad, A, B and Select masks. The game surface is letterboxed to
320:200 and navigation/touch chrome is hidden.
