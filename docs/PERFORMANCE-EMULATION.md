# ESP32-C6 cartridge-visible performance emulation

PRG32-QT defaults to **Accurate (ESP32-C6)**. This mode models performance that a portable PRG32 cartridge can
observe; it does not emulate the ESP32-C6 SoC, FreeRTOS scheduler, radio, caches, buses, DMA, display registers,
or peripherals. Downloaded cartridge code continues to run only in the portable RV32IMAC interpreter.

## Timing contract

The profile clock is 160 MHz and the frame period is exactly 33 ms. Those values come from
`CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ=160` and `PRG32_FRAME_MS=33` in reference PRG32 firmware commit
`596bcf954a77a296db20f5b69756f078721232f3`. One documented constant defines each value.

Guest instructions receive deterministic effective costs by decoded class: ALU, shift, taken/not-taken branch,
jump, load, store, multiply, divide/remainder, atomic, CSR, and fence. Compressed instructions use the class of
the operation they encode. PRG32 ABI #0-#138 calls additionally charge deterministic local processing work;
graphics and audio-copy calls scale with bounded pixels, tiles, events, or samples. Internet latency is excluded.

In accurate mode:

- `instret` counts retired guest instructions;
- `cycle` reports accumulated effective ESP32-C6-profile cycles;
- `time` reports virtual nanoseconds derived from the 160 MHz clock;
- ABI #0 and #124 use the same virtual time; and
- a frame that finishes early advances to its 33 ms boundary, while an overrun remains visible and increments
  the late-frame diagnostic.

Host wall time is only pacing. The Qt host uses its precise 33 ms timer for a fast machine. If the host is too
slow, execution falls behind without compressing or replacing virtual time. `/api/performance.json` publishes
the selected mode, clock frequency, and late-frame count. Pausing Setup stops execution; loading a cartridge or
changing modes re-anchors the frame schedule.

**Optimal (30 FPS)** retains the precise 33 ms host timer but removes modeled ESP32-C6 instruction and ABI
costs. Its counters use the lightweight one-count-per-retired-instruction behavior. It is intended for smooth,
fixed-cadence play when physical-device performance matching is not required.

**Unlimited** is persistent and selectable in Settings, or available to the headless runner as `--unlimited`.
It removes the 33 ms host pacing and uses the same lightweight counter behavior as Optimal.

## Calibration and acceptance

The checked-in profile is an initial effective model whose frequency and frame policy are verified against the
physical firmware source. The instruction and ABI coefficients are deliberately centralized in
`PerformanceTiming.cpp`; they are not a claim of cycle-exact microarchitecture measurement. Physical benchmark
capture remains required before claiming the target tolerances: at most 5% error for instruction and deterministic
graphics microbenchmarks, at most 10% for mixed frames, and exact frame-overrun classification.

Reference provenance is machine-readable in `tests/performance/reference/esp32c6.json`. Updating a coefficient
requires updating that file with the board, firmware commit, toolchain, repetitions, warmup, distribution, and
absolute/percentage error. QEMU or host measurements must never be labeled ESP32-C6 calibration.
