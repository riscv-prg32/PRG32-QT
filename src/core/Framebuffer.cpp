#include "Framebuffer.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <climits>
#include <unordered_map>
namespace prg32 {
Framebuffer::Framebuffer() : indices_(Width * Height) {
    for (unsigned i = 0; i < 256; i++) {
        unsigned r = (i >> 5) & 7, g = (i >> 2) & 7, b = i & 3;
        palette_[i] = uint16_t((r * 31 / 7) << 11 | (g * 63 / 7) << 5 | (b * 31 / 3));
    }
    palette_[0] = 0;
    palette_[255] = 0xffff;
}
uint8_t Framebuffer::nearest(uint16_t c) const {
    int br = INT_MAX, bi = 0, r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
    for (int i = 0; i < 256; i++) {
        auto p = palette_[i];
        int dr = r - int((p >> 11) & 31), dg = g - int((p >> 5) & 63), db = b - int(p & 31),
            d = dr * dr + dg * dg + db * db;
        if (d < br) {
            br = d;
            bi = i;
            if (!d)
                break;
        }
    }
    return uint8_t(bi);
}
void Framebuffer::clear(uint16_t c) {
    clearIndexed(nearest(c));
}
void Framebuffer::clearIndexed(uint8_t i) {
    std::fill(indices_.begin(), indices_.end(), i);
}
void Framebuffer::pixel(int x, int y, uint16_t c) {
    pixelIndexed(x, y, nearest(c));
}
void Framebuffer::pixelIndexed(int x, int y, uint8_t i) {
    if (unsigned(x) < Width && unsigned(y) < Height)
        indices_[y * Width + x] = i;
}
void Framebuffer::rect(int x, int y, int w, int h, uint16_t c) {
    rectIndexed(x, y, w, h, nearest(c));
}
void Framebuffer::rectIndexed(int x, int y, int w, int h, uint8_t i) {
    if (w <= 0 || h <= 0)
        return;
    int x0 = std::max(0, x), y0 = std::max(0, y), x1 = std::min(Width, x + w), y1 = std::min(Height, y + h);
    if (x0 >= x1 || y0 >= y1)
        return;
    for (int yy = y0; yy < y1; yy++)
        std::fill(indices_.begin() + yy * Width + x0, indices_.begin() + yy * Width + x1, i);
}
void Framebuffer::setPalette(unsigned i, uint16_t c) {
    if (i < 256)
        palette_[i] = c;
}
uint16_t Framebuffer::palette(unsigned i) const {
    return palette_[i & 255];
}
std::vector<uint16_t> Framebuffer::rgb565Pixels() const {
    std::vector<uint16_t> out(indices_.size());
    for (size_t i = 0; i < out.size(); i++)
        out[i] = palette_[indices_[i]];
    return out;
}
std::vector<uint16_t> Framebuffer::snapshotRow(int y) const {
    if (y < 0 || y >= Height)
        return {};
    std::vector<uint16_t> r(Width);
    for (int x = 0; x < Width; x++)
        r[x] = palette_[indices_[y * Width + x]];
    return r;
}
void Framebuffer::draw1BPP(
    int x, int y, int w, int h, std::span<const uint8_t> b, uint16_t fg, const uint16_t* bg) {
    auto fi = nearest(fg);
    uint8_t bi = bg ? nearest(*bg) : 0;
    for (int py = 0; py < h; py++)
        for (int px = 0; px < w; px++) {
            int bit = py * w + px, by = bit >> 3, sh = 7 - (bit & 7);
            if (size_t(by) >= b.size())
                return;
            if ((b[by] >> sh) & 1)
                pixelIndexed(x + px, y + py, fi);
            else if (bg)
                pixelIndexed(x + px, y + py, bi);
        }
}
void Framebuffer::drawRGB565(int x, int y, int w, int h, std::span<const uint16_t> p, const uint16_t* t) {
    if (w <= 0 || h <= 0 || p.size() < size_t(w * h))
        return;
    for (int yy = 0; yy < h; yy++)
        for (int xx = 0; xx < w; xx++) {
            uint16_t c = p[yy * w + xx];
            if (!t || c != *t)
                pixel(x + xx, y + yy, c);
        }
}
void Framebuffer::drawIndexed(
    int x, int y, int w, int h, const uint8_t* p, unsigned bpp, const uint16_t* pal, unsigned pc, int tr) {
    if (!p || !pal || !(bpp == 1 || bpp == 2 || bpp == 4 || bpp == 8) || w <= 0 || h <= 0)
        return;
    unsigned mask = (1u << bpp) - 1;
    for (int k = 0; k < w * h; k++) {
        int bo = k * bpp, byte = bo >> 3, bit = bo & 7;
        unsigned value = unsigned(p[byte]) << 8;
        if (bit + bpp > 8)
            value |= p[byte + 1];
        int shift = 16 - int(bpp) - bit;
        unsigned idx = (value >> std::max(0, shift)) & mask;
        if (int(idx) == tr || idx >= pc)
            continue;
        pixel(x + k % w, y + k / w, pal[idx]);
    }
}
void Framebuffer::drawBitplanes(int x,
                                int y,
                                int w,
                                int h,
                                const std::vector<std::vector<uint8_t>>& planes,
                                const uint16_t* pal,
                                unsigned pc,
                                int tr) {
    if (planes.empty() || planes.size() > 8 || !pal)
        return;
    for (int k = 0; k < w * h; k++) {
        unsigned idx = 0;
        for (size_t pn = 0; pn < planes.size(); pn++) {
            size_t by = k >> 3;
            int sh = 7 - (k & 7);
            if (by < planes[pn].size() && ((planes[pn][by] >> sh) & 1))
                idx |= 1u << pn;
        }
        if (int(idx) == tr || idx >= pc)
            continue;
        pixel(x + k % w, y + k / w, pal[idx]);
    }
}
static const std::unordered_map<char, std::array<uint8_t, 7>> font = {
    {' ', {0, 0, 0, 0, 0, 0, 0}},        {'?', {14, 17, 1, 2, 4, 0, 4}},
    {'!', {4, 4, 4, 4, 4, 0, 4}},        {'.', {0, 0, 0, 0, 0, 0, 4}},
    {',', {0, 0, 0, 0, 0, 4, 8}},        {':', {0, 4, 0, 0, 4, 0, 0}},
    {'-', {0, 0, 0, 31, 0, 0, 0}},       {'+', {0, 4, 4, 31, 4, 4, 0}},
    {'/', {1, 2, 4, 8, 16, 0, 0}},       {'0', {14, 17, 19, 21, 25, 17, 14}},
    {'1', {4, 12, 4, 4, 4, 4, 14}},      {'2', {14, 17, 1, 2, 4, 8, 31}},
    {'3', {30, 1, 1, 14, 1, 1, 30}},     {'4', {2, 6, 10, 18, 31, 2, 2}},
    {'5', {31, 16, 30, 1, 1, 17, 14}},   {'6', {6, 8, 16, 30, 17, 17, 14}},
    {'7', {31, 1, 2, 4, 8, 8, 8}},       {'8', {14, 17, 17, 14, 17, 17, 14}},
    {'9', {14, 17, 17, 15, 1, 2, 12}},   {'A', {14, 17, 17, 31, 17, 17, 17}},
    {'B', {30, 17, 17, 30, 17, 17, 30}}, {'C', {14, 17, 16, 16, 16, 17, 14}},
    {'D', {28, 18, 17, 17, 17, 18, 28}}, {'E', {31, 16, 16, 30, 16, 16, 31}},
    {'F', {31, 16, 16, 30, 16, 16, 16}}, {'G', {14, 17, 16, 23, 17, 17, 15}},
    {'H', {17, 17, 17, 31, 17, 17, 17}}, {'I', {14, 4, 4, 4, 4, 4, 14}},
    {'J', {7, 2, 2, 2, 2, 18, 12}},      {'K', {17, 18, 20, 24, 20, 18, 17}},
    {'L', {16, 16, 16, 16, 16, 16, 31}}, {'M', {17, 27, 21, 21, 17, 17, 17}},
    {'N', {17, 25, 21, 19, 17, 17, 17}}, {'O', {14, 17, 17, 17, 17, 17, 14}},
    {'P', {30, 17, 17, 30, 16, 16, 16}}, {'Q', {14, 17, 17, 17, 21, 18, 13}},
    {'R', {30, 17, 17, 30, 20, 18, 17}}, {'S', {15, 16, 16, 14, 1, 1, 30}},
    {'T', {31, 4, 4, 4, 4, 4, 4}},       {'U', {17, 17, 17, 17, 17, 17, 14}},
    {'V', {17, 17, 17, 17, 17, 10, 4}},  {'W', {17, 17, 17, 21, 21, 21, 10}},
    {'X', {17, 17, 10, 4, 10, 17, 17}},  {'Y', {17, 17, 10, 4, 4, 4, 4}},
    {'Z', {31, 1, 2, 4, 8, 16, 31}}};
void Framebuffer::glyph(int x, int y, char c, uint16_t fg, uint16_t bg) {
    char u = char(std::toupper((unsigned char)c));
    auto it = font.find(u);
    if (it == font.end())
        it = font.find('?');
    rect(x, y, 8, 8, bg);
    for (int ry = 0; ry < 7; ry++)
        for (int rx = 0; rx < 5; rx++)
            if ((it->second[ry] >> (4 - rx)) & 1)
                pixel(x + rx + 1, y + ry, fg);
}
void Framebuffer::text8(int x, int y, const std::string& s, uint16_t fg, uint16_t bg) {
    int cx = x;
    for (char c : s) {
        if (c == '\n') {
            cx = x;
            continue;
        }
        glyph(cx, y, c, fg, bg);
        cx += 8;
    }
}
} // namespace prg32
