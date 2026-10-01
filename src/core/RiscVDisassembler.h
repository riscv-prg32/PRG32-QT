#pragma once
#include <cstdint>
#include <string>

namespace prg32 {
/** Decode one RV32IMAC instruction into GNU-style assembly text. */
std::string disassembleRv32(uint32_t address, uint32_t instruction, unsigned width);
} // namespace prg32
