# Third-Party and Upstream Notices

## PRG32 ecosystem

PRG32-QT interoperates with the public PRG32 cartridge format and ABI. Upstream project: `riscv-prg32/PRG32`.

No upstream source file is required at runtime. The regression cartridges under `tests/fixtures/` are retained solely to verify compatible cartridge execution.

## Qt

PRG32-QT uses Qt 6 modules Core, Gui, Quick, QuickControls2, Network, and Multimedia. Qt is an external dependency and is not redistributed in this source archive. Review the Qt licensing terms for the exact distribution used to build binary packages.

## Native platform APIs

PRG32-QT uses Apple GameController on Apple platforms, WinMM joystick APIs on Windows, and the Linux joystick API (`/dev/input/js*`) on Linux/Raspberry Pi OS for physical-controller input. These are platform APIs, not bundled third-party libraries.
