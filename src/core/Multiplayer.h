// Platform-neutral PRG32 multiplayer service boundary and public snapshot layout.
#pragma once
#include <cstdint>
#include <string>
namespace prg32 {
/** Packed guest-visible state written by multiplayer ABI call #40. */
struct MultiplayerPeer {
    uint32_t playerId = 0;
    int16_t x = 0;
    int16_t y = 0;
    uint16_t sprite = 0;
    uint16_t flags = 0;
    uint32_t input = 0;
    uint32_t frame = 0;
    uint32_t lastSeenMs = 0;
};
static_assert(sizeof(MultiplayerPeer) == 24);

/** Host transport used by PRG32 ABI calls #32 through #40. */
class MultiplayerService {
  public:
    virtual ~MultiplayerService() = default;
    virtual void initialize() = 0;
    virtual bool available() const = 0;
    virtual int join(const std::string& signature, uint32_t flags) = 0;
    virtual int leave() = 0;
    virtual void tick(uint32_t nowMs) = 0;
    virtual int setLocalState(int16_t x, int16_t y, uint16_t sprite, uint16_t flags) = 0;
    virtual int setInput(uint32_t input) = 0;
    virtual int peerCount(uint32_t nowMs) = 0;
    virtual int peer(int index, uint32_t nowMs, MultiplayerPeer& output) = 0;
};
} // namespace prg32
