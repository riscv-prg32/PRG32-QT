#include "Cartridge.h"
#include "Framebuffer.h"
#include "InputButtons.h"
#include "InputState.h"
#include "Runtime.h"
#include "Rv32Cpu.h"
#include <cassert>
#include <cstring>
#include <iostream>
using namespace prg32;
static void put16(std::vector<uint8_t>& b, size_t o, uint16_t v) {
    b[o] = v;
    b[o + 1] = v >> 8;
}
static void put32(std::vector<uint8_t>& b, size_t o, uint32_t v) {
    for (int i = 0; i < 4; i++)
        b[o + i] = v >> (8 * i);
}
int main() {
    static_assert(prg32qt::input::Left == 1);
    static_assert(prg32qt::input::Right == 2);
    static_assert(prg32qt::input::Up == 4);
    static_assert(prg32qt::input::Down == 8);
    static_assert(prg32qt::input::A == 16);
    static_assert(prg32qt::input::B == 32);
    static_assert(prg32qt::input::Select == 64);
    InputState input;
    input.set(InputState::Keyboard, 1, true);
    input.set(InputState::Ui, 16, true);
    assert(input.merged() == 17);
    input.replace(InputState::Controller0, 64 | 2);
    assert(input.merged() == 83);
    input.set(InputState::Keyboard, 1, false);
    assert(input.merged() == 82);
    input.clear(InputState::Ui);
    assert(input.merged() == 66);
    assert(crc32(std::span<const uint8_t>((const uint8_t*)"123456789", 9)) == 0xcbf43926u);
    Framebuffer f;
    f.clear(0);
    f.rect(2, 3, 4, 5, 0xffff);
    assert(f.rgb565Pixels()[3 * 320 + 2] == 0xffff);
    f.setPalette(7, 0x1234);
    f.pixelIndexed(1, 1, 7);
    assert(f.rgb565Pixels()[321] == 0x1234);
    std::vector<uint8_t> b(sizeof(CartHeaderV2) + 4);
    memcpy(b.data(), "PRG2", 4);
    put16(b, 4, 1);
    put16(b, 6, 1);
    put16(b, 8, sizeof(CartHeaderV2));
    put32(b, 12, 0x40800000);
    put32(b, 16, 4);
    put32(b, 20, 4096);
    put32(b, 24, 0);
    put32(b, 28, 0);
    put32(b, 32, 0);
    memcpy(b.data() + 40, "test", 4);
    b[sizeof(CartHeaderV2) + 0] = 0x13;
    b[sizeof(CartHeaderV2) + 1] = 0;
    b[sizeof(CartHeaderV2) + 2] = 0;
    b[sizeof(CartHeaderV2) + 3] = 0;
    put32(b, 36, crc32(std::span<const uint8_t>(b.data() + sizeof(CartHeaderV2), 4)));
    put32(b, sizeof(CartHeaderV1), Runtime::CurrentAbiHash);
    std::string e;
    auto c = Cartridge::parse(b, e);
    assert(c && c->name() == "test");
    std::vector<uint8_t> m(64); // addi a0,x0,42 ; jalr x0,0(ra)
    uint32_t i1 = (42u << 20) | (10u << 7) | 0x13u, i2 = (1u << 15) | 0x67u;
    memcpy(m.data(), &i1, 4);
    memcpy(m.data() + 4, &i2, 4);
    Rv32Cpu cpu;
    cpu.reset(0x1000, &m);
    assert(cpu.call(0x1000, 0, 10, e));
    assert(cpu.reg(10) == 42);
    std::cout << "PRG32-QT portable core tests passed\n";
}
