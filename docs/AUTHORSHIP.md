# Authorship and Project Relationship

## PRG32-QT

PRG32-QT is part of the PRG32 educational software ecosystem and is maintained as an open-source Qt/C++ implementation of the portable PRG32 cartridge host.

Project lead and copyright holder documented by the baseline project:

- **Raffaele Montella** — project lead

Student contributors named by the PRG32-iOS baseline UI:

- **Simone Boscaglia** — student contributor
- **Ivan Cafiero** — student contributor

The repository metadata in `CITATION.cff` is intended for machine-readable citation. Contributor history in Git is the authoritative record for subsequent changes.

## Relationship to PRG32-iOS

The attached `PRG32-iOS` implementation is used as the behavioral parity baseline for PRG32-QT. PRG32-QT is not a Swift-to-C++ line-by-line wrapper: it independently implements the same portable guest-facing behavior in C++20 and Qt 6 so that the runtime can be shared across desktop, mobile, Raspberry Pi, and headless environments.

## Relationship to upstream PRG32

The public `riscv-prg32/PRG32` project defines the cartridge and ABI ecosystem that PRG32-QT targets. PRG32-QT is a host implementation, not the ESP32-C6 firmware itself.
