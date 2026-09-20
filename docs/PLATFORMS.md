# Platform support

PRG32-QT keeps cartridge execution in portable C++20 and confines host-specific work to Qt and small controller adapters. The supported matrix is:

| Host | UI/runtime | Input | Build validation |
|---|---|---|---|
| Windows 10/11 | Qt 6 desktop | keyboard + WinMM USB/game controllers | GitHub Actions Windows Qt job |
| Linux desktop | Qt 6 desktop | keyboard + Linux joystick devices | GitHub Actions Linux Qt job |
| Raspberry Pi OS / Raspbian | Qt 6 Linux ARM | keyboard + `/dev/input/js*` controllers | native ARM64 Linux CI plus native Pi build script |
| macOS | Qt 6 desktop | keyboard + Apple GameController | GitHub Actions macOS Qt job |
| iOS | Qt 6 mobile | adaptive touch controls; Apple GameController backend is available | GitHub Actions Qt iOS build |
| Android | Qt 6 mobile | adaptive touch controls | GitHub Actions Qt Android ARM64 build |

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

The project contains no x86 assumptions. RV32 cartridge code is interpreted by the portable core, so a Pi host can be either ARM32 or ARM64 when the required Qt 6 packages are available. CI exercises the full Qt application on ARM64 Linux; final release qualification should additionally run on the intended Raspberry Pi model and Raspberry Pi OS image.

## macOS

```sh
./scripts/build-macos.sh
```

Apple GameController handles USB/Bluetooth controllers and hot-plug notifications.

## iOS

Install Xcode and a Qt iOS kit, then:

```sh
QT_ROOT=/path/to/Qt/6.8.3/ios ./scripts/build-ios.sh
```

Set `IOS_TEAM_ID` to your Apple development team ID and `IOS_DESTINATION` to a connected device destination to build a signed installable app. Set `IOS_DESTINATION` to a simulator destination for simulator builds. On Apple Silicon, the Qt iOS kit must contain arm64 simulator libraries and plugins; the tested Qt 6.8.3 kit contains arm64 device and x86_64 simulator objects, so it cannot link an arm64 simulator app.

The plain-HTTP default Store requires the included App Transport Security allowance. Prefer HTTPS for production distribution.

## Android

Install Android SDK/NDK, JDK 17 and a Qt Android kit:

```sh
QT_ROOT=/path/to/Qt/6.8.3/android_arm64_v8a ./scripts/build-android.sh
```

The manifest enables Internet access and cleartext HTTP for the current default Store.
