#include "RiscVDisassembler.h"
#include <array>
#include <cstdio>

namespace prg32 {
namespace {
constexpr std::array<const char*, 32> RegisterNames = {
    "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2", "s0", "s1", "a0",  "a1",  "a2", "a3", "a4", "a5",
    "a6",   "a7", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"};

int32_t signExtend(uint32_t value, unsigned bits) {
    const uint32_t sign = 1u << (bits - 1);
    return int32_t((value ^ sign) - sign);
}

std::string formatImmediate(const char* operation, uint32_t destination, uint32_t source, int32_t immediate) {
    char text[128];
    std::snprintf(text,
                  sizeof text,
                  "%s %s, %s, %d",
                  operation,
                  RegisterNames[destination],
                  RegisterNames[source],
                  immediate);
    return text;
}

std::string formatMemory(const char* operation, uint32_t reg, uint32_t base, int32_t offset) {
    char text[128];
    std::snprintf(
        text, sizeof text, "%s %s, %d(%s)", operation, RegisterNames[reg], offset, RegisterNames[base]);
    return text;
}
} // namespace

std::string disassembleRv32(uint32_t address, uint32_t instruction, unsigned width) {
    if (width == 2) {
        const uint16_t compressed = uint16_t(instruction);
        const uint32_t quadrant = compressed & 3u;
        const uint32_t function3 = compressed >> 13;
        const uint32_t destination = (compressed >> 7) & 31u;
        char text[112];
        if (quadrant == 1 && (function3 == 0 || function3 == 2)) {
            const int32_t immediate = signExtend(((compressed >> 2) & 31u) | ((compressed >> 7) & 0x20u), 6);
            std::snprintf(text,
                          sizeof text,
                          "%s %s, %d",
                          function3 == 0 ? "c.addi" : "c.li",
                          RegisterNames[destination],
                          immediate);
            return text;
        }
        if (quadrant == 1 && function3 == 3 && destination != 2) {
            const int32_t immediate = signExtend(((compressed >> 2) & 31u) | ((compressed >> 7) & 0x20u), 6);
            std::snprintf(text, sizeof text, "c.lui %s, %d", RegisterNames[destination], immediate << 12);
            return text;
        }
        if (quadrant == 0 && (function3 == 2 || function3 == 6)) {
            const uint32_t first = 8u + ((compressed >> 2) & 7u);
            const uint32_t base = 8u + ((compressed >> 7) & 7u);
            const uint32_t offset =
                ((compressed >> 4) & 0x4u) | ((compressed >> 7) & 0x38u) | ((compressed << 1) & 0x40u);
            std::snprintf(text,
                          sizeof text,
                          "%s %s, %u(%s)",
                          function3 == 2 ? "c.lw" : "c.sw",
                          RegisterNames[first],
                          offset,
                          RegisterNames[base]);
            return text;
        }
        if (quadrant == 2 && function3 == 2) {
            const uint32_t offset =
                ((compressed >> 2) & 0x1cu) | ((compressed >> 7) & 0x20u) | ((compressed << 4) & 0xc0u);
            std::snprintf(text, sizeof text, "c.lwsp %s, %u(sp)", RegisterNames[destination], offset);
            return text;
        }
        if (quadrant == 2 && function3 == 6) {
            const uint32_t source = (compressed >> 2) & 31u;
            const uint32_t offset = ((compressed >> 7) & 0x3cu) | ((compressed >> 1) & 0xc0u);
            std::snprintf(text, sizeof text, "c.swsp %s, %u(sp)", RegisterNames[source], offset);
            return text;
        }
        if (quadrant == 2 && function3 == 4) {
            const uint32_t source = (compressed >> 2) & 31u;
            const bool high = (compressed & 0x1000u) != 0;
            if (!high && source == 0)
                std::snprintf(text, sizeof text, "c.jr %s", RegisterNames[destination]);
            else if (!high)
                std::snprintf(
                    text, sizeof text, "c.mv %s, %s", RegisterNames[destination], RegisterNames[source]);
            else if (destination == 0 && source == 0)
                std::snprintf(text, sizeof text, "c.ebreak");
            else if (source == 0)
                std::snprintf(text, sizeof text, "c.jalr %s", RegisterNames[destination]);
            else
                std::snprintf(
                    text, sizeof text, "c.add %s, %s", RegisterNames[destination], RegisterNames[source]);
            return text;
        }
        std::snprintf(text, sizeof text, ".2byte 0x%04x", compressed);
        return text;
    }
    const uint32_t opcode = instruction & 0x7fu;
    const uint32_t destination = (instruction >> 7) & 31u;
    const uint32_t function3 = (instruction >> 12) & 7u;
    const uint32_t source1 = (instruction >> 15) & 31u;
    const uint32_t source2 = (instruction >> 20) & 31u;
    const uint32_t function7 = instruction >> 25;
    if (opcode == 0x37 || opcode == 0x17) {
        const char* operation = opcode == 0x37 ? "lui" : "auipc";
        char text[96];
        std::snprintf(
            text, sizeof text, "%s %s, 0x%x", operation, RegisterNames[destination], instruction >> 12);
        return text;
    }
    if (opcode == 0x6f) {
        const uint32_t encoded = ((instruction >> 31) << 20) | (((instruction >> 12) & 0xff) << 12) |
                                 (((instruction >> 20) & 1) << 11) | (((instruction >> 21) & 0x3ff) << 1);
        char text[96];
        std::snprintf(text,
                      sizeof text,
                      "jal %s, 0x%08x",
                      RegisterNames[destination],
                      address + uint32_t(signExtend(encoded, 21)));
        return text;
    }
    if (opcode == 0x67)
        return formatMemory("jalr", destination, source1, int32_t(instruction) >> 20);
    if (opcode == 0x03) {
        constexpr std::array<const char*, 8> Names = {"lb", "lh", "lw", "?", "lbu", "lhu", "?", "?"};
        return formatMemory(Names[function3], destination, source1, int32_t(instruction) >> 20);
    }
    if (opcode == 0x23) {
        constexpr std::array<const char*, 8> Names = {"sb", "sh", "sw", "?", "?", "?", "?", "?"};
        const uint32_t encoded = ((instruction >> 25) << 5) | ((instruction >> 7) & 31u);
        return formatMemory(Names[function3], source2, source1, signExtend(encoded, 12));
    }
    if (opcode == 0x13) {
        constexpr std::array<const char*, 8> Names = {
            "addi", "slli", "slti", "sltiu", "xori", "srli", "ori", "andi"};
        const char* operation = Names[function3];
        int32_t immediate = int32_t(instruction) >> 20;
        if (function3 == 5 && (instruction & 0x40000000u))
            operation = "srai";
        if (function3 == 1 || function3 == 5)
            immediate &= 31;
        return formatImmediate(operation, destination, source1, immediate);
    }
    if (opcode == 0x33) {
        constexpr std::array<const char*, 8> Base = {"add", "sll", "slt", "sltu", "xor", "srl", "or", "and"};
        constexpr std::array<const char*, 8> Multiply = {
            "mul", "mulh", "mulhsu", "mulhu", "div", "divu", "rem", "remu"};
        const char* operation = function7 == 1 ? Multiply[function3] : Base[function3];
        if (function7 == 0x20 && function3 == 0)
            operation = "sub";
        else if (function7 == 0x20 && function3 == 5)
            operation = "sra";
        char text[112];
        std::snprintf(text,
                      sizeof text,
                      "%s %s, %s, %s",
                      operation,
                      RegisterNames[destination],
                      RegisterNames[source1],
                      RegisterNames[source2]);
        return text;
    }
    if (opcode == 0x63) {
        constexpr std::array<const char*, 8> Names = {"beq", "bne", "?", "?", "blt", "bge", "bltu", "bgeu"};
        const uint32_t encoded = ((instruction >> 31) << 12) | (((instruction >> 7) & 1) << 11) |
                                 (((instruction >> 25) & 0x3f) << 5) | (((instruction >> 8) & 15) << 1);
        char text[112];
        std::snprintf(text,
                      sizeof text,
                      "%s %s, %s, 0x%08x",
                      Names[function3],
                      RegisterNames[source1],
                      RegisterNames[source2],
                      address + uint32_t(signExtend(encoded, 13)));
        return text;
    }
    if (opcode == 0x0f)
        return function3 == 1 ? "fence.i" : "fence";
    if (opcode == 0x73)
        return instruction == 0x00000073u ? "ecall" : instruction == 0x00100073u ? "ebreak" : "system";
    char text[48];
    std::snprintf(text, sizeof text, ".4byte 0x%08x", instruction);
    return text;
}
} // namespace prg32
