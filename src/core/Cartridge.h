// PRG2 cartridge and metadata declarations for the portable package-validation layer.
#pragma once
#include "Audio.h"
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>
namespace prg32 {
#pragma pack(push, 1)
/** Serialized PRG2 version-1 header; packing is part of the public cartridge format. */
struct CartHeaderV1 {
    char magic[4];
    uint16_t abiMajor, abiMinor, headerSize, flags;
    uint32_t loadAddr, codeSize, memSize, initOffset, updateOffset, drawOffset, payloadCrc32;
    char name[32];
};
/** Version-2 extension carrying ABI identity, feature masks, ISA flags, and import metadata. */
struct CartHeaderV2 {
    CartHeaderV1 v1;
    uint32_t abiHash, requiredFeatures, optionalFeatures, isaFlags, relocationOffset, relocationCount,
        importModel;
};
#pragma pack(pop)
/** One typed metadata trailer copied from the cartridge package. */
struct MetadataBlock {
    std::string type;
    std::vector<uint8_t> data;
};
/** Validated, immutable-in-use representation of one portable PRG2 cartridge. */
class Cartridge {
  public:
    static constexpr uint16_t FlagAudioBlock = 1u << 0, FlagMultiplayer = 1u << 1, FlagAbiTable = 1u << 2,
                              FlagRelocatable = 1u << 3;
    static constexpr uint32_t ImportModelAbiTable = 1;
    static constexpr uint32_t MaximumGuestBytes = 64u * 1024u * 1024u;
    /** Parse and validate a complete package, returning no value and an explanatory error on rejection. */
    static std::optional<Cartridge> parse(std::span<const uint8_t>, std::string&, bool validateCRC = true);
    const CartHeaderV1& header() const {
        return h_;
    }
    uint32_t abiHash() const {
        return abiHash_;
    }
    uint32_t requiredFeatures() const {
        return requiredFeatures_;
    }
    uint32_t optionalFeatures() const {
        return optionalFeatures_;
    }
    uint32_t isaFlags() const {
        return isaFlags_;
    }
    uint32_t importModel() const {
        return importModel_;
    }
    bool isPortable() const {
        return (h_.flags & FlagAbiTable) && importModel_ == ImportModelAbiTable;
    }
    const std::vector<uint8_t>& payload() const {
        return payload_;
    }
    const std::vector<MetadataBlock>& metadata() const {
        return metadata_;
    }
    const std::optional<AudioBlock>& audio() const {
        return audio_;
    }
    std::string name() const;
    std::string metadataJson() const;

  private:
    CartHeaderV1 h_{};
    uint32_t abiHash_ = 0, requiredFeatures_ = 0, optionalFeatures_ = 0, isaFlags_ = 0, relocationOffset_ = 0,
             relocationCount_ = 0, importModel_ = 0;
    std::vector<uint8_t> payload_;
    std::vector<MetadataBlock> metadata_;
    std::optional<AudioBlock> audio_;
};
uint32_t crc32(std::span<const uint8_t>);
} // namespace prg32
