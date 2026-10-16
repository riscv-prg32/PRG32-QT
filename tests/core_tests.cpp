// CI builds this suite as Release, which defines NDEBUG and would compile every assert (and the calls
// inside them) away. Keep the checks active in every configuration.
#undef NDEBUG
#include "Cartridge.h"
#include "Framebuffer.h"
#include "InputButtons.h"
#include "InputState.h"
#include "PerformanceTiming.h"
#include "RiscVDisassembler.h"
#include "Runtime.h"
#include "Rv32Cpu.h"
#include <cassert>
#include <climits>
#include <cstddef>
#include <cstring>
#include <functional>
#include <optional>
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
static uint32_t rvAddi(unsigned rd, unsigned rs1, int32_t imm) {
    return (uint32_t(imm & 0xfff) << 20) | (rs1 << 15) | (rd << 7) | 0x13u;
}
static uint32_t rvLui(unsigned rd, uint32_t imm20) {
    return (imm20 << 12) | (rd << 7) | 0x37u;
}
static uint32_t rvJalr(unsigned rd, unsigned rs1, int32_t imm) {
    return (uint32_t(imm & 0xfff) << 20) | (rs1 << 15) | (rd << 7) | 0x67u;
}
/** Tiny RV32I emitter for test cartridges whose entry points call PRG32 ABI functions. */
struct TestCode {
    enum { Ra = 1, T0 = 5, S1 = 9, A0 = 10, A1, A2, A3, A4, A5 };
    std::vector<uint32_t> words;
    void li(unsigned rd, uint32_t value) {
        uint32_t upper = (value + 0x800u) >> 12;
        if (upper)
            words.push_back(rvLui(rd, upper & 0xfffffu));
        words.push_back(rvAddi(rd, upper ? rd : 0, int32_t(value << 20) >> 20));
    }
    // Call PRG32 ABI entry `index` with up to six arguments through its synthetic host address.
    void call(uint32_t index, std::initializer_list<uint32_t> args = {}) {
        unsigned reg = A0;
        for (uint32_t value : args)
            li(reg++, value);
        words.push_back(rvLui(T0, Runtime::HostBase >> 12));
        words.push_back(rvJalr(Ra, T0, int32_t(index * 4)));
    }
    // Start an entry point: keep the return address in s1 across ABI calls.
    uint32_t begin() {
        uint32_t offset = uint32_t(words.size() * 4);
        words.push_back(rvAddi(S1, Ra, 0));
        return offset;
    }
    void end() {
        words.push_back(rvJalr(0, S1, 0));
    }
};
static constexpr uint32_t TestLoadAddress = 0x40800000u, TestActor = TestLoadAddress + 0x800u;
// Build a portable PRG32 cartridge from `code`, whose init entry is at offset 0.
static std::optional<Cartridge> testCartridge(const TestCode& code, uint32_t updateOffset, uint32_t drawOffset) {
    const size_t header = sizeof(CartHeaderV2), codeBytes = code.words.size() * 4;
    std::vector<uint8_t> b(header + codeBytes);
    memcpy(b.data(), "PRG2", 4);
    put16(b, 4, 1);
    put16(b, 6, 6);
    put16(b, 8, header);
    put16(b, 10, Cartridge::FlagAbiTable);
    put32(b, 12, TestLoadAddress);
    put32(b, 16, uint32_t(codeBytes));
    put32(b, 20, 4096);
    put32(b, 24, 0);
    put32(b, 28, updateOffset);
    put32(b, 32, drawOffset);
    memcpy(b.data() + 40, "abi-test", 8);
    put32(b, offsetof(CartHeaderV2, abiHash), Runtime::CurrentAbiHash);
    put32(b, offsetof(CartHeaderV2, importModel), Cartridge::ImportModelAbiTable);
    memcpy(b.data() + header, code.words.data(), codeBytes);
    std::string e;
    return Cartridge::parse(b, e);
}
// Load a cartridge whose init is `init` and whose update is `update`; draw just returns.
static void runTestCartridge(Runtime& runtime,
                             const std::function<void(TestCode&)>& init,
                             const std::function<void(TestCode&)>& update = {}) {
    TestCode code;
    code.begin();
    init(code);
    code.end();
    const uint32_t updateOffset = code.begin();
    if (update)
        update(code);
    code.end();
    const uint32_t drawOffset = uint32_t(code.words.size() * 4);
    code.words.push_back(rvJalr(0, TestCode::Ra, 0));
    auto cart = testCartridge(code, updateOffset, drawOffset);
    assert(cart && cart->isPortable());
    std::string e;
    assert(runtime.load(*cart, e));
    assert(runtime.init(e));
}
static bool cellIs(const Runtime& runtime, int cellX, int cellY, uint16_t color) {
    const std::vector<uint16_t> pixels = runtime.framebuffer().rgb565Pixels();
    for (int y = cellY * 8; y < cellY * 8 + 8; y++)
        for (int x = cellX * 8; x < cellX * 8 + 8; x++)
            if (pixels[y * Framebuffer::Width + x] != color)
                return false;
    return true;
}
static uint32_t guestWord(const Runtime& runtime, uint32_t address) {
    const std::vector<uint8_t> m = runtime.debugMemory(address, 4);
    return uint32_t(m[0]) | uint32_t(m[1]) << 8 | uint32_t(m[2]) << 16 | uint32_t(m[3]) << 24;
}
// The tile, playfield and platform tests below pin PRG32-QT to the firmware's prg32_tile.c and
// prg32_platform.c behaviour.
static void testFirmwareTileCompatibility() {
    {
        // An undefined tile is all zero on the firmware, so a playfield cell switched back to the
        // never-defined tile 0 clears to black instead of keeping the old pixels.
        Runtime runtime;
        runTestCartridge(
            runtime,
            [](TestCode& c) {
                c.call(81, {1, 0, 0xffff, 0xffff}); // tile_define(1, NULL, white, white)
                c.call(85, {0, 0, 0, 1});           // playfield_put(0, 0, 0, 1)
                c.call(93, {0, 0});                 // playfield_draw(0, 0)
            },
            [](TestCode& c) {
                c.call(85, {0, 0, 0, 0});
                c.call(93, {0, 0});
            });
        assert(cellIs(runtime, 0, 0, 0xffff));
        std::string e;
        assert(runtime.frame(0, e));
        assert(cellIs(runtime, 0, 0, 0));
    }
    {
        // With transparent_zero only tile 0 is skipped; other tiles still paint their background.
        Runtime runtime;
        runTestCartridge(runtime, [](TestCode& c) {
            c.call(81, {2, 0, 0, 0xffff}); // tile_define(2, NULL, black, white)
            c.call(84, {1, 2});            // playfield_clear(1, 2)
            c.call(85, {1, 1, 0, 0});      // playfield_put(1, 1, 0, 0)
            c.call(93, {1, 1});            // playfield_draw(1, transparent_zero)
        });
        assert(cellIs(runtime, 0, 0, 0xffff));
        assert(cellIs(runtime, 1, 0, 0));
    }
    {
        // playfield_draw draws nothing before the playfield exists (first playfield_clear/put).
        Runtime runtime;
        runtime.framebuffer().rect(0, 0, 8, 8, 0xffff);
        runTestCartridge(runtime, [](TestCode& c) { c.call(93, {0, 0}); });
        assert(cellIs(runtime, 0, 0, 0xffff));
    }
    {
        // tile_clear makes tile 0 a solid block of its colour; tile_present repaints dirty cells only,
        // and tile ids, like the firmware's uint8_t arguments, wrap at 256.
        Runtime runtime;
        runTestCartridge(
            runtime,
            [](TestCode& c) {
                c.call(80, {0xffff}); // tile_clear(white)
                c.call(83);           // tile_present()
            },
            [](TestCode& c) {
                c.call(81, {3, 0, 0, 0});      // tile_define(3, NULL, black, black)
                c.call(82, {1, 0, 256 + 3});   // tile_put(1, 0, 259) == tile_put(1, 0, 3)
                c.call(83);
            });
        assert(cellIs(runtime, 0, 0, 0xffff) && cellIs(runtime, 1, 0, 0xffff));
        runtime.framebuffer().rect(0, 0, 8, 8, 0);
        std::string e;
        assert(runtime.frame(0, e));
        assert(cellIs(runtime, 0, 0, 0)); // unchanged cell, not repainted
        assert(cellIs(runtime, 1, 0, 0)); // changed cell, painted with tile 3
    }
    {
        // actor_move scans the whole actor box against solid tiles and reports HIT_RIGHT.
        Runtime runtime;
        runTestCartridge(runtime, [](TestCode& c) {
            c.call(84, {0, 0});                     // playfield_clear(0, 0)
            c.call(96, {1, 1});                     // tile_flags(1, SOLID)
            c.call(85, {0, 2, 1, 1});               // solid tile at pixels x 16..23, y 8..15
            c.call(100, {TestActor, 0, 0, 4, 8, 8}); // actor_init: box x 0..7, y 4..11
            c.call(101, {TestActor, 20, 0});        // actor_move(actor, 20, 0)
        });
        assert(runtime.registerValue(10) == (1u << 2));
        assert(guestWord(runtime, TestActor) == 8);
    }
    {
        // actor_init turns a non-positive size into 1; camera_follow clamps the camera to the map.
        Runtime runtime;
        runTestCartridge(runtime, [](TestCode& c) {
            c.call(100, {TestActor, 0, 1000, 500, 0, uint32_t(-5)});
            c.call(103, {TestActor, 16, 16}); // camera_follow(actor, 16, 16)
            c.call(91);                       // camera_x()
        });
        assert(runtime.registerValue(10) == 64 * 8 - 320);
        assert(guestWord(runtime, TestActor + 16) == (1u | 1u << 16));
    }
    {
        // sprite_hitbox reports no overlap for empty boxes.
        Runtime runtime;
        runTestCartridge(runtime, [](TestCode& c) { c.call(104, {5, 5, 0, 10, 0, 0, 10, 10}); });
        assert(runtime.registerValue(10) == 0);
    }
}
int main() {
    VirtualClock virtualClock;
    virtualClock.addCycles(160'000'000);
    assert(virtualClock.nanoseconds() == 1'000'000'000);
    virtualClock.reset();
    virtualClock.addCycles(1);
    assert(virtualClock.nanoseconds() == 6);
    assert(esp32C6InstructionCycles(InstructionTimingClass::BranchTaken) >
           esp32C6InstructionCycles(InstructionTimingClass::BranchNotTaken));
    assert(esp32C6InstructionCycles(InstructionTimingClass::Divide) >
           esp32C6InstructionCycles(InstructionTimingClass::Multiply));
    uint32_t abiArguments[8]{};
    abiArguments[2] = 10;
    abiArguments[3] = 20;
    assert(esp32C6AbiCycles(56, abiArguments) > esp32C6AbiCycles(55, abiArguments));
    static_assert(prg32qt::input::Left == 1);
    static_assert(prg32qt::input::Right == 2);
    static_assert(prg32qt::input::Up == 4);
    static_assert(prg32qt::input::Down == 8);
    static_assert(prg32qt::input::A == 16);
    static_assert(prg32qt::input::B == 32);
    static_assert(prg32qt::input::Select == 64);
    static_assert((Runtime::ProvidedFeatures & Runtime::FeatureMultiplayer) != 0);
    static_assert(sizeof(MultiplayerPeer) == 24);
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
    assert(input.takeMerged() == 67);
    input.set(InputState::Keyboard, 1, true);
    input.set(InputState::Keyboard, 1, false);
    assert(input.takeMerged() == 67);
    assert(input.takeMerged() == 66);
    assert(crc32(std::span<const uint8_t>((const uint8_t*)"123456789", 9)) == 0xcbf43926u);
    const SynthParameters synth = decodeSynthId(uint16_t(3u | (12u << 2) | (15u << 6) | (2u << 10)));
    assert(synth.waveform == 3);
    assert(synth.pulseWidth == 12);
    assert(synth.cutoff == 15);
    assert(synth.resonance == 2);
    Framebuffer f;
    f.clear(0);
    f.rect(2, 3, 4, 5, 0xffff);
    assert(f.rgb565Pixels()[3 * 320 + 2] == 0xffff);
    f.rectIndexed(INT_MAX, 0, 1, 1, 7);
    f.rectIndexed(INT_MIN, 0, INT_MAX, 1, 7);
    f.rectIndexed(0, INT_MAX, 1, 1, 7);
    f.rectIndexed(0, INT_MIN, 1, INT_MAX, 7);
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
    assert(cpu.beginCall(0x1000, 0, e));
    assert(cpu.callActive());
    assert(cpu.step(e));
    assert(cpu.reg(10) == 42);
    assert(cpu.lastInstructionAddress() == 0x1000);
    assert(cpu.callActive());
    assert(cpu.step(e));
    assert(cpu.lastInstructionAddress() == 0x1004);
    assert((cpu.recentInstructionAddresses() == std::vector<uint32_t>{0x1000, 0x1004}));
    assert(!cpu.callActive());
    assert(disassembleRv32(0x1000, i1, 4) == "addi a0, zero, 42");
    assert(disassembleRv32(0x1004, i2, 4) == "jalr zero, 0(ra)");
    cpu.reset(0x1000, &m);
    assert(cpu.call(0x1000, 0, 10, e));
    assert(cpu.reg(10) == 42);
    assert(cpu.retiredInstructions() == 2);
    assert(cpu.virtualCycles() == esp32C6InstructionCycles(InstructionTimingClass::Alu) +
                                      esp32C6InstructionCycles(InstructionTimingClass::Jump));
    const uint64_t cyclesBeforeAbi = cpu.virtualCycles();
    cpu.chargeAbi(55, abiArguments);
    assert(cpu.virtualCycles() > cyclesBeforeAbi);
    cpu.reset(0x1000, &m);
    assert(cpu.virtualCycles() == 0);
    cpu.setPerformanceMode(PerformanceMode::Optimal);
    assert(cpu.call(0x1000, 0, 10, e));
    assert(cpu.virtualCycles() == cpu.retiredInstructions());
    cpu.reset(0x1000, &m);
    cpu.setPerformanceMode(PerformanceMode::Unlimited);
    assert(cpu.call(0x1000, 0, 10, e));
    assert(cpu.virtualCycles() == cpu.retiredInstructions());
    bool memoryAccessOk = false;
    m.assign(4, 0xa5);
    cpu.reset(0x1000, &m);
    cpu.store32(0x1000, 0x12345678, memoryAccessOk);
    assert(memoryAccessOk);
    assert((m == std::vector<uint8_t>{0x78, 0x56, 0x34, 0x12}));
    m.assign(4, 0xa5);
    cpu.store16(0x1003, 0x1234, memoryAccessOk);
    assert(!memoryAccessOk);
    assert(m == std::vector<uint8_t>(4, 0xa5));
    cpu.store32(0x1001, 0x12345678, memoryAccessOk);
    assert(!memoryAccessOk);
    assert(m == std::vector<uint8_t>(4, 0xa5));
    cpu.reset(0xfffffffc, &m);
    cpu.store32(0xfffffffe, 0x12345678, memoryAccessOk);
    assert(!memoryAccessOk);
    assert(m == std::vector<uint8_t>(4, 0xa5));
    testFirmwareTileCompatibility();
    std::cout << "PRG32-QT portable core tests passed\n";
}
