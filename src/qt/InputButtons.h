#pragma once

#include <cstdint>

/** Canonical PRG32 input-mask bits shared by keyboard, touch, and controllers. */
namespace prg32qt::input {
constexpr std::uint32_t Left = 1u << 0;
constexpr std::uint32_t Right = 1u << 1;
constexpr std::uint32_t Up = 1u << 2;
constexpr std::uint32_t Down = 1u << 3;
constexpr std::uint32_t A = 1u << 4;
constexpr std::uint32_t B = 1u << 5;
constexpr std::uint32_t Select = 1u << 6;
constexpr std::uint32_t DirectionMask = Left | Right | Up | Down;
} // namespace prg32qt::input
