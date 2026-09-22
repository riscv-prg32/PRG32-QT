// RV32IMAC fetch, memory, and instruction semantics for the portable architecture layer.
#include "Rv32Cpu.h"
#include <cstring>
#include <limits>
namespace prg32 {
void Rv32Cpu::reset(uint32_t b, std::vector<uint8_t>* m) {
    base_ = b;
    mem_ = m;
    x_.fill(0);
    pc_ = b;
    retired_ = 0;
    hasReservation_ = false;
}
uint32_t Rv32Cpu::sext(uint32_t v, unsigned n) const {
    uint32_t m = 1u << (n - 1);
    return (v ^ m) - m;
}
uint8_t Rv32Cpu::load8(uint32_t a, bool& ok) const {
    uint64_t o = uint64_t(a) - base_;
    ok = mem_ && a >= base_ && o < mem_->size();
    return ok ? (*mem_)[o] : 0;
}
uint16_t Rv32Cpu::load16(uint32_t a, bool& ok) const {
    bool a0, a1;
    auto v = load8(a, a0) | uint16_t(load8(a + 1, a1)) << 8;
    ok = a0 && a1;
    return v;
}
uint32_t Rv32Cpu::load32(uint32_t a, bool& ok) const {
    bool a0, a1;
    uint16_t l = load16(a, a0), h = load16(a + 2, a1);
    ok = a0 && a1;
    return l | uint32_t(h) << 16;
}
void Rv32Cpu::store8(uint32_t a, uint8_t v, bool& ok) {
    uint64_t o = uint64_t(a) - base_;
    ok = mem_ && a >= base_ && o < mem_->size();
    if (ok) {
        (*mem_)[o] = v;
        hasReservation_ = false;
    }
}
void Rv32Cpu::store16(uint32_t a, uint16_t v, bool& ok) {
    bool a0, a1;
    store8(a, v, a0);
    store8(a + 1, v >> 8, a1);
    ok = a0 && a1;
}
void Rv32Cpu::store32(uint32_t a, uint32_t v, bool& ok) {
    bool a0, a1;
    store16(a, v, a0);
    store16(a + 2, v >> 16, a1);
    ok = a0 && a1;
}
bool Rv32Cpu::call(uint32_t entry, uint32_t a0, uint64_t budget, std::string& e) {
    pc_ = entry;
    x_[1] = ReturnSentinel;
    x_[10] = a0;
    x_[2] = base_ + uint32_t(mem_->size() & ~15u);
    while (pc_ != ReturnSentinel && budget--) {
        if (!step(e))
            return false;
    }
    if (pc_ != ReturnSentinel) {
        e = "instruction budget exhausted";
        return false;
    }
    return true;
}
bool Rv32Cpu::step(std::string& e) {
    if (pc_ >= hostBase_ && pc_ < hostBase_ + hostCount_ * 4u && ((pc_ - hostBase_) & 3) == 0) {
        uint32_t i = (pc_ - hostBase_) / 4;
        if (host_)
            host_(i);
        pc_ = x_[1];
        x_[0] = 0;
        return true;
    }
    bool ok;
    uint16_t lo = load16(pc_, ok);
    if (!ok) {
        e = "instruction fetch outside cartridge memory";
        return false;
    }
    if ((lo & 3) != 3)
        return exec16(lo, e);
    uint32_t ins = load32(pc_, ok);
    if (!ok) {
        e = "truncated 32-bit instruction";
        return false;
    }
    return exec32(ins, e);
}
bool Rv32Cpu::exec32(uint32_t i, std::string& e) {
    // R-type/common fields (RISC-V unprivileged ISA): opcode[6:0], rd[11:7], funct3[14:12],
    // rs1[19:15], rs2[24:20], and funct7[31:25]. The ISA names are retained because they are the
    // conventional vocabulary used by the specification.
    uint32_t op = i & 0x7f, rd = (i >> 7) & 31, f3 = (i >> 12) & 7, rs1 = (i >> 15) & 31,
             rs2 = (i >> 20) & 31, f7 = i >> 25;
    uint32_t a = x_[rs1], b = x_[rs2], npc = pc_ + 4;
    auto wr = [&](uint32_t v) {
        if (rd)
            x_[rd] = v;
    };
    bool ok = true;
    switch (op) {
    case 0x37:
        // LUI: place the U-type immediate in rd[31:12].
        wr(i & 0xfffff000);
        break;
    case 0x17:
        // AUIPC: add the U-type immediate to the address of this instruction.
        wr(pc_ + (i & 0xfffff000));
        break;
    case 0x6f: {
        // JAL J-immediate layout: inst[31] -> imm[20], inst[19:12] -> imm[19:12],
        // inst[20] -> imm[11], inst[30:21] -> imm[10:1]; imm[0] is zero.
        uint32_t im = ((i >> 31) << 20) | (((i >> 12) & 0xff) << 12) | (((i >> 20) & 1) << 11) |
                      (((i >> 21) & 0x3ff) << 1);
        wr(npc);
        npc = pc_ + sext(im, 21);
        break;
    }
    case 0x67: {
        // JALR: write the sequential PC and jump to rs1 + signed I-immediate, clearing target bit zero.
        int32_t im = int32_t(i) >> 20;
        uint32_t t = (a + im) & ~1u;
        wr(npc);
        npc = t;
        break;
    }
    case 0x63: {
        // B-immediate layout: inst[31] -> imm[12], inst[7] -> imm[11],
        // inst[30:25] -> imm[10:5], inst[11:8] -> imm[4:1]; imm[0] is zero.
        uint32_t im =
            ((i >> 31) << 12) | (((i >> 7) & 1) << 11) | (((i >> 25) & 0x3f) << 5) | (((i >> 8) & 15) << 1);
        bool t = false;
        switch (f3) {
        case 0:
            // BEQ: branch when the source registers compare equal.
            t = a == b;
            break;
        case 1:
            // BNE: branch when the source registers compare unequal.
            t = a != b;
            break;
        case 4:
            // BLT: signed less-than comparison.
            t = int32_t(a) < int32_t(b);
            break;
        case 5:
            // BGE: signed greater-than-or-equal comparison.
            t = int32_t(a) >= int32_t(b);
            break;
        case 6:
            // BLTU: unsigned less-than comparison.
            t = a < b;
            break;
        case 7:
            // BGEU: unsigned greater-than-or-equal comparison.
            t = a >= b;
            break;
        default:
            e = "bad branch";
            return false;
        }
        if (t)
            npc = pc_ + sext(im, 13);
        break;
    }
    case 0x03: {
        // LOAD: funct3 selects LB, LH, LW, LBU, or LHU from rs1 + signed I-immediate.
        int32_t im = int32_t(i) >> 20;
        uint32_t ad = a + im, v = 0;
        if (f3 == 0) {
            v = sext(load8(ad, ok), 8);
        } else if (f3 == 1) {
            v = sext(load16(ad, ok), 16);
        } else if (f3 == 2) {
            v = load32(ad, ok);
        } else if (f3 == 4) {
            v = load8(ad, ok);
        } else if (f3 == 5) {
            v = load16(ad, ok);
        } else {
            e = "bad load";
            return false;
        }
        if (!ok) {
            e = "load outside memory";
            return false;
        }
        wr(v);
        break;
    }
    case 0x23: {
        // S-immediate layout: inst[31:25] -> imm[11:5], inst[11:7] -> imm[4:0].
        // funct3 selects SB, SH, or SW.
        uint32_t im = ((i >> 25) << 5) | ((i >> 7) & 31), ad = a + sext(im, 12);
        if (f3 == 0)
            store8(ad, b, ok);
        else if (f3 == 1)
            store16(ad, b, ok);
        else if (f3 == 2)
            store32(ad, b, ok);
        else {
            e = "bad store";
            return false;
        }
        if (!ok) {
            e = "store outside memory";
            return false;
        }
        break;
    }
    case 0x13: {
        // OP-IMM: ADDI, SLTI, SLTIU, XORI, ORI, ANDI, SLLI, and SRLI/SRAI.
        uint32_t im = i >> 20;
        switch (f3) {
        case 0:
            wr(a + sext(im, 12));
            break;
        case 2:
            wr(int32_t(a) < int32_t(sext(im, 12)));
            break;
        case 3:
            wr(a < sext(im, 12));
            break;
        case 4:
            wr(a ^ sext(im, 12));
            break;
        case 6:
            wr(a | sext(im, 12));
            break;
        case 7:
            wr(a & sext(im, 12));
            break;
        case 1:
            wr(a << (im & 31));
            break;
        case 5:
            wr((im & 0x400) ? uint32_t(int32_t(a) >> (im & 31)) : a >> (im & 31));
            break;
        default:
            e = "bad op-imm";
            return false;
        }
        break;
    }
    case 0x33: {
        // OP: the base integer register operations and the funct7=1 M-extension operations.
        if (f7 == 1) {
            switch (f3) {
            case 0:
                wr(uint32_t(uint64_t(a) * b));
                break;
            case 1:
                wr(uint32_t((int64_t(int32_t(a)) * int64_t(int32_t(b))) >> 32));
                break;
            case 2:
                wr(uint32_t((int64_t(int32_t(a)) * int64_t(uint64_t(b))) >> 32));
                break;
            case 3:
                wr(uint32_t((uint64_t(a) * uint64_t(b)) >> 32));
                break;
            case 4: {
                int32_t aa = int32_t(a), bb = int32_t(b);
                wr(!bb ? 0xffffffffu
                       : (aa == INT32_MIN && bb == -1 ? uint32_t(INT32_MIN) : uint32_t(aa / bb)));
                break;
            }
            case 5:
                wr(b ? a / b : 0xffffffffu);
                break;
            case 6: {
                int32_t aa = int32_t(a), bb = int32_t(b);
                wr(!bb ? a : (aa == INT32_MIN && bb == -1 ? 0u : uint32_t(aa % bb)));
                break;
            }
            case 7:
                wr(b ? a % b : a);
                break;
            }
        } else
            switch (f3) {
            case 0:
                wr(f7 == 0x20 ? a - b : a + b);
                break;
            case 1:
                wr(a << (b & 31));
                break;
            case 2:
                wr(int32_t(a) < int32_t(b));
                break;
            case 3:
                wr(a < b);
                break;
            case 4:
                wr(a ^ b);
                break;
            case 5:
                wr(f7 == 0x20 ? uint32_t(int32_t(a) >> (b & 31)) : a >> (b & 31));
                break;
            case 6:
                wr(a | b);
                break;
            case 7:
                wr(a & b);
                break;
            }
        break;
    }
    case 0x2f: {
        // AMO: RV32A word-width LR/SC and atomic read-modify-write operations.
        if (f3 != 2) {
            e = "illegal atomic";
            return false;
        }
        uint32_t ad = a, old = load32(ad, ok);
        if (!ok) {
            e = "atomic outside memory";
            return false;
        }
        uint32_t funct5 = (i >> 27) & 31;
        switch (funct5) {
        case 0b00010:
            // LR.W: load a word and establish a reservation for its address.
            if (rs2) {
                e = "illegal lr.w";
                return false;
            }
            reservation_ = ad;
            hasReservation_ = true;
            wr(old);
            break;
        case 0b00011: {
            // SC.W: store only if the address retains the reservation; rd receives success status.
            if (hasReservation_ && reservation_ == ad) {
                store32(ad, b, ok);
                wr(0);
            } else
                wr(1);
            hasReservation_ = false;
            break;
        }
        case 0b00001:
            // AMOSWAP.W: atomically replace memory with rs2.
            store32(ad, b, ok);
            wr(old);
            break;
        case 0b00000:
            // AMOADD.W: atomically add rs2 to the prior memory word.
            store32(ad, old + b, ok);
            wr(old);
            break;
        case 0b00100:
            // AMOXOR.W: atomically XOR memory with rs2.
            store32(ad, old ^ b, ok);
            wr(old);
            break;
        case 0b01100:
            // AMOAND.W: atomically AND memory with rs2.
            store32(ad, old & b, ok);
            wr(old);
            break;
        case 0b01000:
            // AMOOR.W: atomically OR memory with rs2.
            store32(ad, old | b, ok);
            wr(old);
            break;
        case 0b10000:
            // AMOMIN.W: signed minimum.
            store32(ad, int32_t(old) < int32_t(b) ? old : b, ok);
            wr(old);
            break;
        case 0b10100:
            // AMOMAX.W: signed maximum.
            store32(ad, int32_t(old) > int32_t(b) ? old : b, ok);
            wr(old);
            break;
        case 0b11000:
            // AMOMINU.W: unsigned minimum.
            store32(ad, std::min(old, b), ok);
            wr(old);
            break;
        case 0b11100:
            // AMOMAXU.W: unsigned maximum.
            store32(ad, std::max(old, b), ok);
            wr(old);
            break;
        default:
            e = "unsupported atomic";
            return false;
        }
        if (!ok) {
            e = "atomic store outside memory";
            return false;
        }
        break;
    }
    case 0x0f:
        break;
    case 0x73: {
        if (i == 0x00000073) {
            npc = ReturnSentinel;
            break;
        }
        if (i == 0x00100073) {
            e = "ebreak";
            return false;
        }
        uint32_t csr = (i >> 20) & 0xfff, v = 0;
        if (csr == 0xC00 || csr == 0xC01 || csr == 0xC02)
            v = uint32_t(retired_);
        else if (csr == 0xC80 || csr == 0xC81 || csr == 0xC82)
            v = uint32_t(retired_ >> 32);
        else {
            e = "unsupported CSR";
            return false;
        }
        if ((f3 == 2 || f3 == 3 || f3 == 6 || f3 == 7) && rs1 == 0)
            wr(v);
        else {
            e = "write to read-only CSR";
            return false;
        }
        break;
    }
    default:
        e = "unsupported RV32 opcode 0x" + std::to_string(op);
        return false;
    }
    pc_ = npc;
    x_[0] = 0;
    retired_++;
    return true;
}
// RV32C decoder covering the integer C extension emitted by normal RV32IMAC toolchains.
bool Rv32Cpu::exec16(uint16_t c, std::string& e) {
    uint32_t q = c & 3, f3 = c >> 13, npc = pc_ + 2;
    auto r = [&](unsigned z) { return x_[8 + z]; };
    auto wr = [&](unsigned d, uint32_t v) {
        if (d)
            x_[d] = v;
    };
    bool ok = true;
    if (q == 0) {
        unsigned rd = 8 + ((c >> 2) & 7), rs1 = 8 + ((c >> 7) & 7);
        if (f3 == 0) {
            uint32_t im =
                ((c >> 7) & 0xf) << 6 | ((c >> 11) & 3) << 4 | ((c >> 5) & 1) << 3 | ((c >> 6) & 1) << 2;
            if (!im) {
                e = "illegal c.addi4spn";
                return false;
            }
            wr(rd, x_[2] + im);
        } else if (f3 == 2) {
            uint32_t im = ((c >> 5) & 1) << 6 | ((c >> 10) & 7) << 3 | ((c >> 6) & 1) << 2;
            wr(rd, load32(r((c >> 7) & 7) + im, ok));
            if (!ok)
                return false;
        } else if (f3 == 6) {
            uint32_t im = ((c >> 5) & 1) << 6 | ((c >> 10) & 7) << 3 | ((c >> 6) & 1) << 2;
            store32(r((c >> 7) & 7) + im, r((c >> 2) & 7), ok);
            if (!ok)
                return false;
        } else {
            e = "unsupported C q0";
            return false;
        }
    } else if (q == 1) {
        unsigned rd = (c >> 7) & 31;
        if (f3 == 0) {
            uint32_t im = ((c >> 12) & 1) << 5 | ((c >> 2) & 31);
            wr(rd, x_[rd] + sext(im, 6));
        } else if (f3 == 1 || f3 == 5) {
            uint32_t im = ((c >> 12) & 1) << 11 | ((c >> 8) & 1) << 10 | ((c >> 9) & 3) << 8 |
                          ((c >> 6) & 1) << 7 | ((c >> 7) & 1) << 6 | ((c >> 2) & 1) << 5 |
                          ((c >> 11) & 1) << 4 | ((c >> 3) & 7) << 1;
            if (f3 == 1)
                wr(1, npc);
            npc = pc_ + sext(im, 12);
        } else if (f3 == 2) {
            uint32_t im = ((c >> 12) & 1) << 5 | ((c >> 2) & 31);
            wr(rd, sext(im, 6));
        } else if (f3 == 3) {
            uint32_t im = ((c >> 12) & 1) << 17 | ((c >> 2) & 31) << 12;
            if (rd == 2) {
                uint32_t z = ((c >> 12) & 1) << 9 | ((c >> 4) & 1) << 8 | ((c >> 3) & 1) << 7 |
                             ((c >> 5) & 1) << 6 | ((c >> 2) & 1) << 5 | ((c >> 6) & 1) << 4;
                wr(2, x_[2] + sext(z, 10));
            } else
                wr(rd, sext(im, 18));
        } else if (f3 == 4) {
            unsigned d = 8 + ((c >> 7) & 7), sub = (c >> 10) & 3;
            if (sub < 2) {
                uint32_t sh = ((c >> 12) & 1) << 5 | ((c >> 2) & 31);
                wr(d, sub == 0 ? r((c >> 7) & 7) >> sh : uint32_t(int32_t(r((c >> 7) & 7)) >> sh));
            } else if (sub == 2) {
                uint32_t im = ((c >> 12) & 1) << 5 | ((c >> 2) & 31);
                wr(d, r((c >> 7) & 7) & sext(im, 6));
            } else {
                unsigned s = 8 + ((c >> 2) & 7), fn = ((c >> 12) & 1) << 2 | ((c >> 5) & 3);
                switch (fn) {
                case 0:
                    wr(d, x_[d] - x_[s]);
                    break;
                case 1:
                    wr(d, x_[d] ^ x_[s]);
                    break;
                case 2:
                    wr(d, x_[d] | x_[s]);
                    break;
                case 3:
                    wr(d, x_[d] & x_[s]);
                    break;
                default:
                    e = "RV64-only C arithmetic";
                    return false;
                }
            }
        } else if (f3 == 6 || f3 == 7) {
            uint32_t im = ((c >> 12) & 1) << 8 | ((c >> 5) & 3) << 6 | ((c >> 2) & 1) << 5 |
                          ((c >> 10) & 3) << 3 | ((c >> 3) & 3) << 1;
            bool t = (r((c >> 7) & 7) == 0);
            if ((f3 == 7) ? !t : t)
                npc = pc_ + sext(im, 9);
        } else {
            e = "unsupported C q1";
            return false;
        }
    } else {
        unsigned rd = (c >> 7) & 31, rs2 = (c >> 2) & 31;
        if (f3 == 0) {
            uint32_t sh = ((c >> 12) & 1) << 5 | ((c >> 2) & 31);
            wr(rd, x_[rd] << sh);
        } else if (f3 == 2) {
            uint32_t im = ((c >> 2) & 3) << 6 | ((c >> 12) & 1) << 5 | ((c >> 4) & 7) << 2;
            wr(rd, load32(x_[2] + im, ok));
            if (!ok)
                return false;
        } else if (f3 == 4) {
            if (((c >> 12) & 1) == 0) {
                if (rs2 == 0)
                    npc = x_[rd] & ~1u;
                else
                    wr(rd, x_[rs2]);
            } else {
                if (rd == 0 && rs2 == 0) {
                    e = "c.ebreak";
                    return false;
                }
                if (rs2 == 0) {
                    wr(1, npc);
                    npc = x_[rd] & ~1u;
                } else
                    wr(rd, x_[rd] + x_[rs2]);
            }
        } else if (f3 == 6) {
            uint32_t im = ((c >> 7) & 3) << 6 | ((c >> 9) & 15) << 2;
            store32(x_[2] + im, x_[rs2], ok);
            if (!ok)
                return false;
        } else {
            e = "unsupported C q2";
            return false;
        }
    }
    pc_ = npc;
    x_[0] = 0;
    retired_++;
    return true;
}
} // namespace prg32
