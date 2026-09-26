#pragma once

#include <cstdint>

namespace prg32 {

/** Selects cartridge-visible execution timing behavior. */
enum class PerformanceMode {
    Esp32C6Accurate,
    Unlimited,
};

/** Instruction groups used by the calibrated ESP32-C6 cartridge timing profile. */
enum class InstructionTimingClass {
    Alu,
    Shift,
    BranchNotTaken,
    BranchTaken,
    Jump,
    Load,
    Store,
    Multiply,
    Divide,
    Atomic,
    Csr,
    Fence,
};

/**
 * Deterministic virtual clock for the cartridge-visible ESP32-C6 profile.
 *
 * The 160 MHz frequency is the CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ value in the reference PRG32 firmware at
 * commit 596bcf954a77a296db20f5b69756f078721232f3. This is a calibrated effective model, not a model of the
 * ESP32-C6 pipeline, caches, buses, or peripherals.
 */
class VirtualClock {
  public:
    static constexpr uint64_t FrequencyHz = 160'000'000ull;
    static constexpr uint64_t FramePeriodNanoseconds = 33'000'000ull;
    static constexpr uint64_t FramePeriodCycles = FrequencyHz * FramePeriodNanoseconds / 1'000'000'000ull;

    void reset() {
        cycles_ = 0;
    }
    void addCycles(uint64_t cycles) {
        cycles_ += cycles;
    }
    void advanceTo(uint64_t cycles) {
        if (cycles_ < cycles)
            cycles_ = cycles;
    }
    [[nodiscard]] uint64_t cycles() const {
        return cycles_;
    }
    [[nodiscard]] uint64_t nanoseconds() const;

  private:
    uint64_t cycles_ = 0;
};

/** Return the deterministic effective cycle charge for one decoded instruction class. */
[[nodiscard]] uint32_t esp32C6InstructionCycles(InstructionTimingClass timingClass);

/** Return the deterministic local-processing charge for one public PRG32 ABI call. */
[[nodiscard]] uint64_t esp32C6AbiCycles(uint32_t call, const uint32_t arguments[8]);

} // namespace prg32
