# Contributing to PRG32-QT

Contributions are welcome. Please keep changes focused, portable, and testable on the supported Qt targets.

## Development workflow

1. Fork the repository and create a topic branch from `main`.
2. Configure and run the portable core tests before changing platform code:

   ```sh
   cmake -S . -B build-core -G Ninja -DPRG32QT_BUILD_APP=OFF
   cmake --build build-core
   ctest --test-dir build-core --output-on-failure
   ```

3. When changing cartridge execution, ABI dispatch, graphics, input, or Store handling, add or update regression tests.
4. When Store compatibility may be affected, run the Store-wide smoke test against the configured Store.
5. Build the Qt application on each platform affected by the change.
6. Open a pull request describing behavior changes, tests performed, and known limitations.

## Coding guidelines

- Use portable C++20 in `src/core`; do not introduce Qt dependencies there.
- Keep platform-specific code behind the Qt/application layer or platform packaging directories.
- Preserve backward compatibility with documented PRG32 ABI hashes unless a deliberate compatibility break is discussed first.
- Prefer small changes with regression coverage.
- Format C/C++ sources with the repository `.clang-format`.
- Do not commit build directories, signing credentials, generated provisioning data, or downloaded Store cartridges.

## Bug reports

Include the host platform, Qt version, cartridge name/version, Store URL if relevant, reproducible steps, and console output. For cartridge execution failures, include the ABI hash and whether the failure reproduces with `prg32qt-headless`.
