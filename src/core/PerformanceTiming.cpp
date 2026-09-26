#include "PerformanceTiming.h"

#include <algorithm>
#include <limits>

namespace prg32 {

uint64_t VirtualClock::nanoseconds() const {
    const uint64_t seconds = cycles_ / FrequencyHz;
    const uint64_t remainder = cycles_ % FrequencyHz;
    if (seconds > std::numeric_limits<uint64_t>::max() / 1'000'000'000ull)
        return std::numeric_limits<uint64_t>::max();
    const uint64_t remainderNanoseconds = remainder * 1'000'000'000ull / FrequencyHz;
    const uint64_t wholeNanoseconds = seconds * 1'000'000'000ull;
    if (wholeNanoseconds > std::numeric_limits<uint64_t>::max() - remainderNanoseconds)
        return std::numeric_limits<uint64_t>::max();
    return wholeNanoseconds + remainderNanoseconds;
}

uint32_t esp32C6InstructionCycles(InstructionTimingClass timingClass) {
    switch (timingClass) {
    case InstructionTimingClass::Alu:
        return 1;
    case InstructionTimingClass::Shift:
        return 2;
    case InstructionTimingClass::BranchNotTaken:
        return 1;
    case InstructionTimingClass::BranchTaken:
        return 3;
    case InstructionTimingClass::Jump:
        return 3;
    case InstructionTimingClass::Load:
        return 3;
    case InstructionTimingClass::Store:
        return 2;
    case InstructionTimingClass::Multiply:
        return 4;
    case InstructionTimingClass::Divide:
        return 16;
    case InstructionTimingClass::Atomic:
        return 8;
    case InstructionTimingClass::Csr:
        return 2;
    case InstructionTimingClass::Fence:
        return 1;
    }
    return 1;
}

uint64_t esp32C6AbiCycles(uint32_t call, const uint32_t arguments[8]) {
    constexpr uint64_t Base = 120;
    constexpr uint64_t Pixel = 5;
    if (call == 51 || call == 80 || call == 137)
        return Base + 320ull * 200ull * Pixel;
    if (call == 55 || call == 135)
        return Base + Pixel;
    if (call == 56 || call == 136) {
        const uint64_t width = std::min<uint32_t>(arguments[2], 320);
        const uint64_t height = std::min<uint32_t>(arguments[3], 200);
        return Base + width * height * Pixel;
    }
    if (call == 57)
        return Base + 8ull * 8ull * Pixel;
    if (call == 83 || call == 93 || call == 94 || call == 95)
        return Base + 40ull * 25ull * 64ull * Pixel;
    if (call == 105)
        return Base + 64ull * Pixel;
    if (call == 106)
        return Base + 16ull * 16ull * 7ull;
    if (call == 108)
        return Base +
               uint64_t(std::min<uint32_t>(arguments[2], 320)) * std::min<uint32_t>(arguments[3], 200) * 7ull;
    if (call == 113)
        return Base + 24ull * 24ull * 7ull;
    if (call == 8)
        return Base + uint64_t(std::min<uint32_t>(arguments[1], 1'000'000)) * 3ull;
    if (call == 7)
        return Base + uint64_t(std::min<uint32_t>(arguments[1], 65'536)) * 12ull;
    // ABI #32-#40 charge deterministic local work only; network latency is deliberately excluded.
    return Base;
}

} // namespace prg32
