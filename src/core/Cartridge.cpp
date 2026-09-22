#include "Cartridge.h"
#include <algorithm>
#include <cstring>
namespace prg32 {
static uint16_t rd16(const uint8_t* p) {
    return uint16_t(p[0]) | uint16_t(p[1]) << 8;
}
static uint32_t rd32(const uint8_t* p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
uint32_t crc32(std::span<const uint8_t> d) {
    uint32_t c = ~0u;
    for (uint8_t b : d) {
        c ^= b;
        for (int i = 0; i < 8; i++)
            c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return ~c;
}
std::string Cartridge::name() const {
    size_t n = 0;
    while (n < sizeof(h_.name) && h_.name[n])
        ++n;
    return n ? std::string(h_.name, n) : "cartridge";
}
std::string Cartridge::metadataJson() const {
    for (auto& m : metadata_)
        if (m.type == "META")
            return {m.data.begin(), m.data.end()};
    return {};
}
std::optional<Cartridge> Cartridge::parse(std::span<const uint8_t> b, std::string& e, bool validateCRC) {
    if (b.size() < 72 || std::memcmp(b.data(), "PRG2", 4)) {
        e = "not a PRG2 cartridge";
        return std::nullopt;
    }
    Cartridge c;
    std::memcpy(&c.h_, b.data(), sizeof(c.h_));
    if (c.h_.abiMajor != 1) {
        e = "unsupported cartridge ABI major";
        return std::nullopt;
    }
    if (c.h_.headerSize < 72 || c.h_.headerSize > b.size() || c.h_.codeSize == 0 ||
        c.h_.codeSize > b.size() - c.h_.headerSize) {
        e = "invalid PRG2 header or code size";
        return std::nullopt;
    }
    if (c.h_.memSize > MaximumGuestBytes ||
        uint64_t(c.h_.codeSize) + c.h_.memSize + 128u * 1024u > MaximumGuestBytes) {
        e = "invalid PRG2 guest memory size";
        return std::nullopt;
    }
    uint64_t guestBytes =
        std::max<uint64_t>(uint64_t(c.h_.codeSize) + c.h_.memSize + 128u * 1024u, 256u * 1024u);
    if (uint64_t(c.h_.loadAddr) + guestBytes > 0x100000000ull) {
        e = "PRG2 guest memory wraps address space";
        return std::nullopt;
    }
    for (uint32_t o : {c.h_.initOffset, c.h_.updateOffset, c.h_.drawOffset})
        if (o >= c.h_.codeSize) {
            e = "PRG2 entry point outside code payload";
            return std::nullopt;
        }
    if (c.h_.headerSize >= 100) {
        CartHeaderV2 v{};
        std::memcpy(&v, b.data(), sizeof(v));
        c.abiHash_ = v.abiHash;
        c.requiredFeatures_ = v.requiredFeatures;
        c.optionalFeatures_ = v.optionalFeatures;
        c.isaFlags_ = v.isaFlags;
        c.relocationOffset_ = v.relocationOffset;
        c.relocationCount_ = v.relocationCount;
        c.importModel_ = v.importModel;
    }
    c.payload_.assign(b.begin() + c.h_.headerSize, b.begin() + c.h_.headerSize + c.h_.codeSize);
    if (validateCRC && c.h_.payloadCrc32 && crc32(c.payload_) != c.h_.payloadCrc32) {
        e = "PRG2 payload CRC32 mismatch";
        return std::nullopt;
    }
    size_t trailing = c.h_.headerSize + c.h_.codeSize;
    if ((c.h_.flags & FlagAudioBlock) && trailing + 40 <= b.size() &&
        !std::memcmp(b.data() + trailing, "AUD0", 4)) {
        uint32_t size = rd32(b.data() + trailing + 36);
        if (size < 40 || trailing + size > b.size()) {
            e = "invalid trailing AUD0 block";
            return std::nullopt;
        }
        auto a = AudioBlock::parse(b.subspan(trailing, size), e);
        if (!a)
            return std::nullopt;
        c.audio_ = std::move(*a);
        trailing += size;
    }
    for (size_t i = trailing; i + 16 <= b.size(); ++i) {
        if (std::memcmp(b.data() + i, "PRG32META", 9))
            continue;
        if (b[i + 9] != 1)
            continue;
        uint16_t count = rd16(b.data() + i + 10);
        uint32_t size = rd32(b.data() + i + 12);
        if (size < 16 || i + size > b.size())
            continue;
        size_t p = i + 16, end = i + size;
        bool ok = true;
        for (uint16_t n = 0; n < count; n++) {
            if (p + 8 > end) {
                ok = false;
                break;
            }
            std::string type((const char*)b.data() + p, 4);
            uint32_t len = rd32(b.data() + p + 4);
            p += 8;
            if (p + len > end) {
                ok = false;
                break;
            }
            c.metadata_.push_back({type, {b.begin() + p, b.begin() + p + len}});
            p += len;
        }
        if (ok)
            break;
        c.metadata_.clear();
    }
    return c;
}
} // namespace prg32
