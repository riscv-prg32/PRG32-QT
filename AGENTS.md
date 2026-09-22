# AGENTS.md — Engineering Contract for PRG32-QT

PRG32-QT supports Linux, Windows, Raspberry Pi OS, macOS, iOS, and Android. Every change must preserve
a buildable and working application on all six targets.

## 1. Code quality bar (non-negotiable)

- Write source and headers for peer review: use one statement per line, Doxygen documentation for public
  interfaces, and local commentary for bit manipulation, ABI calls, binary layouts, and non-obvious control
  flow.
- Do not introduce compound one-line statements, unexplained magic numbers, or single-letter identifiers
  except conventional loop counters and documented RISC-V ISA field names.
- `.clang-format` is authoritative. Format changed C++ files before committing; CI runs
  `clang-format --dry-run --Werror`.
- Changes to `src/core` or cartridge/ABI parsing must identify the applicable PRG32 ABI call, RISC-V opcode,
  or binary-format field.

## 2. Build matrix (every push and pull request)

The platform build scripts are the source of truth. CI must exercise Windows (`build-windows.ps1`), Linux
(`build-linux.sh`), macOS (`build-macos.sh`), Raspberry Pi OS (`build-raspbian.sh`), Android
(`build-android.sh`), and iOS (`build-ios.sh`). A platform that cannot be verified on an available runner must
be named, with the reason and manual evidence, rather than silently skipped.

## 3. Test and certification matrix

- Run `ctest --output-on-failure`, including core tests, the Asteroids and Bach headless fixtures, and the
  `PerformanceTest.prg32 --require-performance` contract fixture.
- Any release candidate or change to the portable runtime, Qt host, cartridge parser, or ABI must run every
  cartridge published by the default Cartridge Store through the headless harness. Preserve the catalog
  snapshot (IDs and versions) and per-cartridge result as a CI artifact.
- If the Store is unreachable in CI, certify manually before merge and attach the evidence.

## 4. Documentation currency

- Keep `README.md`, `CHANGELOG.md`, and the relevant `docs/` files synchronized with behavior, ABI, builds,
  and supported platforms. Write precise, implementation-backed technical prose rather than aspirational
  claims.
- Run the documentation-consistency check before merge and resolve all code/documentation drift.
- Add a changelog entry for every user-visible or ABI-visible change.

## 5. Automated consistency checks

CI runs `scripts/check-docs.py`. It verifies the ABI call table against the architecture/compatibility
documentation, reconciles platform scripts with `README.md` and `docs/PLATFORMS.md`, checks public header
documentation, and rejects structurally invalid Markdown documentation.

## 6. Definition of done

1. `clang-format --dry-run --Werror` passes.
2. All six platform builds pass, or unavailable targets are explicitly recorded and manually certified.
3. CTest and required Cartridge Store certification pass.
4. Documentation consistency passes.
5. Diff review confirms that behavioral changes are intentional and documented.

Before completing any change, build every locally available affected target and record tested devices,
simulators, architectures, and remaining environment limitations in `docs/TESTING.md` or
`docs/PLATFORMS.md`.
