#pragma once
#include <algorithm>
#include <array>
#include <cstdint>

class InputState {
  public:
    enum Source : std::size_t { Ui = 0, Keyboard = 1, Controller0 = 2, Controller1 = 3, SourceCount = 4 };
    void set(Source source, std::uint32_t mask, bool down) {
        auto& v = masks_[source];
        if (down) {
            pendingPresses_ |= mask & ~v;
            v |= mask;
        } else {
            v &= ~mask;
        }
    }
    void replace(Source source, std::uint32_t mask) {
        pendingPresses_ |= mask & ~masks_[source];
        masks_[source] = mask;
    }
    std::uint32_t value(Source source) const {
        return masks_[source];
    }
    std::uint32_t merged() const {
        std::uint32_t v = 0;
        for (auto m : masks_)
            v |= m;
        return v;
    }
    std::uint32_t takeMerged() {
        std::uint32_t result = merged() | pendingPresses_;
        pendingPresses_ = 0;
        return result;
    }
    void clear(Source source) {
        pendingPresses_ &= ~masks_[source];
        masks_[source] = 0;
    }

  private:
    std::array<std::uint32_t, SourceCount> masks_{};
    std::uint32_t pendingPresses_ = 0;
};
