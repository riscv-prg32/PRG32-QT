# Security Policy

## Supported versions

Security fixes are applied to the current `main` branch and the latest tagged release.

## Reporting a vulnerability

Please do not disclose a suspected vulnerability in a public issue before maintainers have had an opportunity to review it. Prefer GitHub Private Vulnerability Reporting when it is enabled for the repository. If private reporting is unavailable, contact the repository owner through their public GitHub profile and request a private channel.

Reports should include affected versions, reproduction steps, impact, and any proposed mitigation. Cartridge files and Store responses are untrusted input; parser, emulator, networking, filesystem, and memory-safety issues are therefore security-sensitive.

## Local device API

PRG32-QT advertises an unauthenticated development API as `_prg32._tcp.local.` and listens on TCP port 8080.
The API can upload and run cartridges, inspect runtime state and memory statistics, capture the framebuffer, and
read or submit local scores. Use it only on trusted local networks. Do not forward or expose port 8080 to the
public Internet. Bonjour/mDNS is discovery metadata, not authentication or authorization.
