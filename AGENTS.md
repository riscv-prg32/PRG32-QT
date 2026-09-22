# PRG32-QT Repository Instructions

PRG32-QT supports Linux, Windows, Raspberry Pi OS, macOS, iOS, and Android. Every change must preserve a buildable and working application on all six targets. Platform-specific changes must retain portable core behavior and must not break another supported target. Update the relevant platform and testing documentation whenever requirements, build commands, packaging, deployment, or runtime behavior changes.

Before completing a change:

1. Run the portable CMake and CTest suite.
2. Build and exercise every locally available affected target.
3. Keep CI coverage and platform helpers valid for targets that cannot be executed locally.
4. Record tested devices, simulators, architectures, and any remaining environment limitation in `docs/TESTING.md` or `docs/PLATFORMS.md`.
