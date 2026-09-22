// Portable RV32IMAC interpreter interface for the architecture's Qt-free guest-execution layer.
#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
namespace prg32 {
/**
 * Interprets the RV32I base ISA and the M, A, and C extensions over a bounded byte vector.
 *
 * Guest addresses are translated relative to the base supplied to reset(). All memory operations report
 * bounds failures through their `ok` output, and register x0 remains hard-wired to zero. Synthetic host-call
 * addresses are intercepted before instruction fetch and never refer to host-native executable memory.
 */
class Rv32Cpu {
  public:
    /** Callback invoked with an index in the synthetic PRG32 ABI table. */
    using HostCall = std::function<void(uint32_t)>;
    /** Reset architectural state and attach the guest memory backing store. */
    void reset(uint32_t base, std::vector<uint8_t>* memory);
    /** Install the callback used when execution reaches the configured host-call range. */
    void setHostCall(HostCall c) {
        host_ = std::move(c);
    }
    /** Define the first synthetic host address and the number of four-byte table entries. */
    void setHostRange(uint32_t base, uint32_t count) {
        hostBase_ = base;
        hostCount_ = count;
    }
    /** Execute a guest function until it returns or consumes `budget` instructions. */
    bool call(uint32_t entry, uint32_t a0, uint64_t budget, std::string& error);
    /** Fetch, decode, and execute one instruction, reporting traps through `error`. */
    bool step(std::string& error);
    /** Read an integer register; reads of x0 always return zero. */
    uint32_t reg(unsigned i) const {
        return i ? x_[i] : 0;
    }
    /** Write an integer register; writes to x0 are discarded. */
    void setReg(unsigned i, uint32_t v) {
        if (i)
            x_[i] = v;
    }
    /** Return the current guest program counter. */
    uint32_t pc() const {
        return pc_;
    }
    /** Load an unsigned byte from guest memory and set `ok` when the address is valid. */
    uint8_t load8(uint32_t a, bool& ok) const;
    /** Load a little-endian halfword from guest memory. */
    uint16_t load16(uint32_t a, bool& ok) const;
    /** Load a little-endian word from guest memory. */
    uint32_t load32(uint32_t a, bool& ok) const;
    /** Store a byte and invalidate any outstanding LR/SC reservation. */
    void store8(uint32_t a, uint8_t v, bool& ok);
    /** Store a little-endian halfword. */
    void store16(uint32_t a, uint16_t v, bool& ok);
    /** Store a little-endian word. */
    void store32(uint32_t a, uint32_t v, bool& ok);
    /** Return the guest address corresponding to memory offset zero. */
    uint32_t base() const {
        return base_;
    }

  private:
    uint32_t sext(uint32_t v, unsigned bits) const;
    bool exec32(uint32_t ins, std::string& e);
    bool exec16(uint16_t ins, std::string& e);
    std::array<uint32_t, 32> x_{};
    uint32_t pc_ = 0, base_ = 0, hostBase_ = 0, hostCount_ = 0;
    uint64_t retired_ = 0;
    uint32_t reservation_ = 0;
    bool hasReservation_ = false;
    std::vector<uint8_t>* mem_ = nullptr;
    HostCall host_;
    static constexpr uint32_t ReturnSentinel = 0xfffffffcu;
};
} // namespace prg32
