#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>
namespace prg32 {
class Framebuffer {
  public:
    static constexpr int Width = 320, Height = 200;
    Framebuffer();
    void clear(uint16_t);
    void clearIndexed(uint8_t);
    void pixel(int, int, uint16_t);
    void pixelIndexed(int, int, uint8_t);
    void rect(int, int, int, int, uint16_t);
    void rectIndexed(int, int, int, int, uint8_t);
    void setPalette(unsigned, uint16_t);
    uint16_t palette(unsigned) const;
    uint8_t nearest(uint16_t) const;
    const std::vector<uint8_t>& indices() const {
        return indices_;
    }
    const std::array<uint16_t, 256>& paletteData() const {
        return palette_;
    }
    std::vector<uint16_t> rgb565Pixels() const;
    std::vector<uint16_t> snapshotRow(int) const;
    void draw1BPP(int, int, int, int, std::span<const uint8_t>, uint16_t, const uint16_t* bg = nullptr);
    void drawRGB565(int, int, int, int, std::span<const uint16_t>, const uint16_t* transparent = nullptr);
    void drawIndexed(
        int, int, int, int, const uint8_t*, unsigned, const uint16_t*, unsigned, int transparent = -1);
    void drawBitplanes(int,
                       int,
                       int,
                       int,
                       const std::vector<std::vector<uint8_t>>&,
                       const uint16_t*,
                       unsigned,
                       int transparent = -1);
    void text8(int, int, const std::string&, uint16_t, uint16_t);

  private:
    void glyph(int, int, char, uint16_t, uint16_t);
    std::vector<uint8_t> indices_;
    std::array<uint16_t, 256> palette_{};
};
} // namespace prg32
