// PRG32 ABI dispatch and portable runtime services for the architecture's guest/host boundary.
#include "Runtime.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
namespace prg32 {
namespace {
// Byte offsets in the packed actor record exposed by ABI calls #100-#103.
constexpr uint32_t ActorXOffset = 0;
constexpr uint32_t ActorYOffset = 4;
constexpr uint32_t ActorVelocityXOffset = 8;
constexpr uint32_t ActorVelocityYOffset = 12;
constexpr uint32_t ActorWidthOffset = 16;
constexpr uint32_t ActorHeightOffset = 18;
constexpr uint32_t ActorStatusOffset = 20;
constexpr uint32_t ActorLayerOffset = 22;
constexpr uint32_t ActorReservedOffset = 23;
} // namespace

static void wr16(std::vector<uint8_t>& m, size_t o, uint16_t v) {
    m[o] = v;
    m[o + 1] = v >> 8;
}
static void wr32(std::vector<uint8_t>& m, size_t o, uint32_t v) {
    for (int i = 0; i < 4; i++)
        m[o + i] = v >> (i * 8);
}
static int si(uint32_t v) {
    return int(int32_t(v));
}
void Runtime::drawTile(int px, int py, uint16_t id, bool transparentZero) {
    if (transparentZero && id == 0)
        return;
    auto it = tiles_.find(id);
    if (it == tiles_.end())
        return;
    uint16_t bg = it->second.bg;
    fb_.draw1BPP(px, py, 8, 8, it->second.bits, it->second.fg, transparentZero ? nullptr : &bg);
}
void Runtime::drawPlayfield(int layer, bool transparentZero) {
    if (layer < 0 || layer > 1)
        return;
    int originX = cameraX_ * parallaxX_[layer] / 256 + scrollX_[layer];
    int originY = cameraY_ * parallaxY_[layer] / 256 + scrollY_[layer];
    int startX = originX >= 0 ? originX / 8 : (originX - 7) / 8;
    int startY = originY >= 0 ? originY / 8 : (originY - 7) / 8;
    int offsetX = originX - startX * 8;
    int offsetY = originY - startY * 8;
    for (int screenY = -offsetY, row = 0; screenY < Framebuffer::Height; screenY += 8, ++row)
        for (int screenX = -offsetX, column = 0; screenX < Framebuffer::Width; screenX += 8, ++column) {
            int mapX = (startX + column) % 64;
            int mapY = (startY + row) % 32;
            if (mapX < 0)
                mapX += 64;
            if (mapY < 0)
                mapY += 32;
            drawTile(screenX, screenY, playfield_[layer][mapY * 64 + mapX], transparentZero);
        }
}
uint16_t Runtime::tileAt(int l, int x, int y) const {
    int tx = x >> 3, ty = y >> 3;
    if (l < 0 || l > 1 || tx < 0 || ty < 0 || tx >= 64 || ty >= 32)
        return 0;
    return playfield_[l][ty * 64 + tx];
}
bool Runtime::solidAt(int l, int x, int y, uint32_t mask) const {
    return (tileFlags_[tileAt(l, x, y)] & mask) != 0;
}
bool Runtime::load(const Cartridge& c, std::string& e) {
    if (!c.isPortable()) {
        e = "PRG32-QT accepts portable ABI-table PRG32 cartridges only";
        return false;
    }
    if (c.abiHash() && c.abiHash() != CurrentAbiHash && c.abiHash() != CompatHash0 &&
        c.abiHash() != CompatHash1) {
        e = "incompatible PRG32 ABI hash";
        return false;
    }
    if (c.requiredFeatures() & ~ProvidedFeatures) {
        char b[96];
        std::snprintf(b,
                      sizeof b,
                      "cartridge requires unavailable features 0x%08x",
                      c.requiredFeatures() & ~ProvidedFeatures);
        e = b;
        return false;
    }
    cart_ = c;
    perf_ = {};
    activePerfCase_ = -1;
    base_ = c.header().loadAddr ? c.header().loadAddr : 0x40800000u;
    uint64_t guest =
        std::max<uint64_t>(uint64_t(c.header().codeSize) + c.header().memSize + 128u * 1024u, 256u * 1024u);
    abiOffset_ = uint32_t((guest + 15) & ~15ull);
    scratchOffset_ = abiOffset_ + 4096;
    mem_.assign(size_t(scratchOffset_) + 4096, 0);
    if (c.payload().size() > mem_.size()) {
        e = "payload exceeds memory";
        return false;
    }
    std::copy(c.payload().begin(), c.payload().end(), mem_.begin());
    cpu_.reset(base_, &mem_);
    cpu_.setHostRange(HostBase, 139);
    cpu_.setHostCall([this](uint32_t i) { hostCall(i); });
    writeAbiTable();
    loaded_ = true;
    started_ = std::chrono::steady_clock::now();
    channelVolumes_.fill(255);
    channelPans_.fill(0);
    return true;
}
void Runtime::writeAbiTable() {
    size_t o = abiOffset_;
    wr32(mem_, o, 0x49424150u);
    wr16(mem_, o + 4, 1);
    wr16(mem_, o + 6, 6);
    wr16(mem_, o + 8, uint16_t(20 + 139 * 4));
    wr16(mem_, o + 10, 139);
    wr32(mem_, o + 12, CurrentAbiHash);
    wr32(mem_, o + 16, ProvidedFeatures);
    for (uint32_t i = 0; i < 139; i++)
        wr32(mem_, o + 20 + i * 4, HostBase + i * 4);
}
bool Runtime::init(std::string& e) {
    return loaded_ && cpu_.call(base_ + cart_.header().initOffset, guestPtr(abiOffset_), 20'000'000, e);
}
bool Runtime::frame(uint32_t in, std::string& e) {
    if (!loaded_)
        return false;
    input_ = in;
    auto n = nowUs();
    if (lastAudioUs_)
        advanceAudio(double(n - lastAudioUs_) / 1000.0);
    lastAudioUs_ = n;
    if (!cpu_.call(base_ + cart_.header().updateOffset, guestPtr(abiOffset_), 10'000'000, e))
        return false;
    return cpu_.call(base_ + cart_.header().drawOffset, guestPtr(abiOffset_), 10'000'000, e);
}
void Runtime::stop() {
    track_.reset();
    if (multiplayer_)
        multiplayer_->leave();
    if (audio_) {
        audio_->stopAll();
        audio_->shutdown();
    }
}
std::string Runtime::guestString(uint32_t p, size_t max) const {
    std::string s;
    bool ok;
    for (size_t i = 0; i < max; i++) {
        auto c = cpu_.load8(p + i, ok);
        if (!ok || !c)
            break;
        s.push_back(char(c));
    }
    return s;
}
bool Runtime::writeCString(uint32_t p, const std::string& s, size_t cap) {
    if (!cap)
        return false;
    bool ok = true;
    size_t n = std::min(cap - 1, s.size());
    for (size_t i = 0; i < n && ok; i++)
        cpu_.store8(p + i, uint8_t(s[i]), ok);
    if (ok)
        cpu_.store8(p + n, 0, ok);
    return ok;
}
uint64_t Runtime::nowUs() const {
    return uint64_t(
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started_)
            .count());
}
void Runtime::pulseLED(int note, uint8_t velocity) {
    double hue = double((note * 37) % 360) / 360.0, x = hue * 6;
    int sector = int(x);
    double f = x - sector;
    uint8_t q = uint8_t(255 * (1 - f)), t = uint8_t(255 * f);
    switch (sector % 6) {
    case 0:
        led_ = {255, t, 0, double(velocity) / 255 * .42};
        break;
    case 1:
        led_ = {q, 255, 0, double(velocity) / 255 * .42};
        break;
    case 2:
        led_ = {0, 255, t, double(velocity) / 255 * .42};
        break;
    case 3:
        led_ = {0, q, 255, double(velocity) / 255 * .42};
        break;
    case 4:
        led_ = {t, 0, 255, double(velocity) / 255 * .42};
        break;
    default:
        led_ = {255, 0, q, double(velocity) / 255 * .42};
    }
    if (ledCallback_)
        ledCallback_(led_);
}
void Runtime::playSample(int id, uint8_t vol, uint16_t pitch, int8_t pan) {
    if (!cart_.audio() || id < 0 || size_t(id) >= cart_.audio()->samples.size())
        return;
    auto& s = cart_.audio()->samples[id];
    std::vector<float> pcm(s.bytes.size());
    for (size_t i = 0; i < s.bytes.size(); i++)
        pcm[i] = (float(s.bytes[i]) - 128.f) / 128.f;
    if (audio_)
        audio_->playPCM(0,
                        pcm,
                        22050.,
                        double(std::max<uint16_t>(1, pitch)) / 1024.,
                        vol,
                        pan,
                        s.loop ? s.loopStart : -1,
                        s.loop ? s.loopEnd : -1);
    pulseLED(69, vol);
}
void Runtime::noteOn(int ch, int inst, int note, uint8_t vol, int8_t pan) {
    if (cart_.audio() && inst >= 0 && size_t(inst) < cart_.audio()->instruments.size()) {
        auto& i = cart_.audio()->instruments[inst];
        if (!(i.sampleId & 0x8000) && i.sampleId < cart_.audio()->samples.size()) {
            auto& s = cart_.audio()->samples[i.sampleId];
            std::vector<float> pcm(s.bytes.size());
            for (size_t k = 0; k < s.bytes.size(); k++)
                pcm[k] = (float(s.bytes[k]) - 128.f) / 128.f;
            double pitch = std::pow(2.0, double(note - (s.baseNote ? s.baseNote : 69)) / 12.0);
            if (audio_)
                audio_->playPCM(
                    ch, pcm, 22050., pitch, vol, pan, s.loop ? s.loopStart : -1, s.loop ? s.loopEnd : -1);
            pulseLED(note, vol);
            return;
        }
    }
    if (audio_)
        audio_->noteOn(ch, note, vol, pan);
    pulseLED(note, vol);
}
void Runtime::playTrack(int id) {
    if (!cart_.audio() || id < 0 || size_t(id) >= cart_.audio()->tracks.size())
        return;
    track_ = TrackState{id, 0, 0};
    trackElapsedMs_ = 0;
    lastAudioUs_ = nowUs();
    processTrack();
}
void Runtime::advanceAudio(double ms) {
    if (!track_)
        return;
    trackElapsedMs_ += std::clamp(ms, 0.0, 1000.0);
    for (int steps = 0; track_ && steps < 1000; steps++) {
        double mpt = 60000.0 / (std::clamp(tempo_, 30, 300) * 4.0);
        if (trackElapsedMs_ < mpt)
            break;
        trackElapsedMs_ -= mpt;
        if (track_->ticks > 0)
            --track_->ticks;
        processTrack();
    }
}
void Runtime::processTrack() {
    if (!track_ || !cart_.audio()) {
        track_.reset();
        return;
    }
    for (int n = 0; track_ && track_->ticks == 0 && n < 1000; n++) {
        auto& events = cart_.audio()->tracks[track_->id].events;
        if (track_->index < 0 || size_t(track_->index) >= events.size()) {
            track_.reset();
            break;
        }
        auto ev = events[track_->index++];
        track_->ticks = ev.deltaTicks;
        switch (ev.command) {
        case 1: {
            int ch = ev.arg0 & 7;
            int8_t pan = cart_.audio()->instruments.size() > size_t(ch)
                             ? cart_.audio()->instruments[ch].defaultPan
                             : 0;
            noteOn(ch, ch, ev.arg1, 255, pan);
            break;
        }
        case 2:
            if (audio_)
                audio_->noteOff(ev.arg0);
            break;
        case 3:
            channelVolumes_[ev.arg0 & 7] = ev.arg1;
            if (audio_)
                audio_->setChannelVolume(ev.arg0 & 7, ev.arg1);
            break;
        case 4:
            channelPans_[ev.arg0 & 7] = int8_t(ev.arg1);
            if (audio_)
                audio_->setChannelPan(ev.arg0 & 7, int8_t(ev.arg1));
            break;
        case 5:
            tempo_ = std::clamp(int(ev.arg0 ? ev.arg0 : 120), 30, 300);
            break;
        case 6:
            playSample(ev.arg0, ev.arg1, 1024, 0);
            break;
        case 7: {
            int t = (int(ev.arg1) << 8) | ev.arg0;
            if (t >= 0 && size_t(t) < events.size()) {
                track_->index = t;
                track_->ticks = ev.deltaTicks;
            } else
                track_.reset();
            break;
        }
        case 255:
            track_.reset();
            break;
        default:
            break;
        }
    }
}
void Runtime::drawRGBSprite(int x, int y, int w, int h, uint32_t p, std::optional<uint16_t> tr) {
    if (w <= 0 || h <= 0 || int64_t(w) * h >= 1000000)
        return;
    bool ok = true;
    std::vector<uint16_t> px(size_t(w) * h);
    for (size_t n = 0; n < px.size(); n++) {
        px[n] = cpu_.load16(p + uint32_t(n * 2), ok);
        if (!ok)
            return;
    }
    if (tr)
        fb_.drawRGB565(x, y, w, h, px, &*tr);
    else
        fb_.drawRGB565(x, y, w, h, px);
}
void Runtime::drawIndexedSprite(int x, int y, uint32_t p, uint32_t frame, bool bitplanes) {
    bool ok = true;
    uint32_t pix = cpu_.load32(p, ok), pal = cpu_.load32(p + 4, ok);
    uint16_t w = cpu_.load16(p + 8, ok), h = cpu_.load16(p + 10, ok), fc = cpu_.load16(p + 12, ok),
             pc = cpu_.load16(p + 14, ok);
    uint8_t bpp = cpu_.load8(p + 16, ok);
    int tr = int(int16_t(cpu_.load16(p + 18, ok)));
    if (!ok || !w || !h || !pc || pc > 256 || !(bpp == 1 || bpp == 2 || bpp == 4 || bpp == 8))
        return;
    fc = std::max<uint16_t>(1, fc);
    std::vector<uint16_t> pv(pc);
    for (unsigned i = 0; i < pc; i++)
        pv[i] = cpu_.load16(pal + i * 2, ok);
    uint32_t fi = frame % fc;
    if (bitplanes) {
        size_t planeBytes = (size_t(w) * h + 7) / 8, frameBytes = planeBytes * bpp;
        std::vector<std::vector<uint8_t>> planes(bpp, std::vector<uint8_t>(planeBytes));
        for (unsigned pn = 0; pn < bpp; pn++)
            for (size_t i = 0; i < planeBytes; i++)
                planes[pn][i] = cpu_.load8(pix + uint32_t(fi * frameBytes + pn * planeBytes + i), ok);
        fb_.drawBitplanes(x, y, w, h, planes, pv.data(), pc, tr < 0 ? -1 : tr);
    } else {
        size_t bytes = (size_t(w) * h * bpp + 7) / 8;
        std::vector<uint8_t> packed(bytes);
        for (size_t i = 0; i < bytes; i++)
            packed[i] = cpu_.load8(pix + uint32_t(fi * bytes + i), ok);
        fb_.drawIndexed(x, y, w, h, packed.data(), bpp, pv.data(), pc, tr < 0 ? -1 : tr);
    }
}
uint16_t Runtime::actorMove(uint32_t p, int dx, int dy) {
    bool ok = true;
    int x = si(cpu_.load32(p + ActorXOffset, ok));
    int y = si(cpu_.load32(p + ActorYOffset, ok));
    int w = cpu_.load16(p + ActorWidthOffset, ok);
    int h = cpu_.load16(p + ActorHeightOffset, ok);
    int l = cpu_.load8(p + ActorLayerOffset, ok);
    uint16_t st = 0;
    int sx = (dx > 0) - (dx < 0);
    for (int n = 0; n < std::abs(dx); n++) {
        int nx = x + sx, edge = sx > 0 ? nx + w - 1 : nx;
        if (solidAt(l, edge, y, 1) || solidAt(l, edge, y + h - 1, 1)) {
            st |= sx > 0 ? 4 : 2;
            break;
        }
        x = nx;
    }
    int sy = (dy > 0) - (dy < 0);
    for (int n = 0; n < std::abs(dy); n++) {
        int ny = y + sy, edge = sy > 0 ? ny + h - 1 : ny;
        if (solidAt(l, x, edge, 1) || solidAt(l, x + w - 1, edge, 1)) {
            st |= sy > 0 ? 1 : 8;
            break;
        }
        y = ny;
    }
    uint32_t fl = tileFlags_[tileAt(l, x + w / 2, y + h / 2)];
    if (fl & 4)
        st |= 16;
    if (fl & 8)
        st |= 32;
    cpu_.store32(p + ActorXOffset, uint32_t(int32_t(x)), ok);
    cpu_.store32(p + ActorYOffset, uint32_t(int32_t(y)), ok);
    cpu_.store16(p + ActorStatusOffset, st, ok);
    return st;
}
uint16_t Runtime::actorStep(uint32_t p, uint32_t in, int move, int jump, int gravity, int maxFall) {
    bool ok = true;
    int vx = 0;
    if (in & 1)
        vx -= move;
    if (in & 2)
        vx += move;
    int vy = si(cpu_.load32(p + ActorVelocityYOffset, ok));
    uint16_t old = cpu_.load16(p + ActorStatusOffset, ok);
    if ((in & 16) && (old & 1))
        vy = -std::abs(jump);
    vy = std::min(maxFall, vy + gravity);
    cpu_.store32(p + ActorVelocityXOffset, uint32_t(int32_t(vx)), ok);
    cpu_.store32(p + ActorVelocityYOffset, uint32_t(int32_t(vy)), ok);
    auto st = actorMove(p, vx, vy);
    if (st & 1)
        cpu_.store32(p + ActorVelocityYOffset, 0, ok);
    return st;
}
void Runtime::cameraFollow(uint32_t p, int dx, int dy) {
    bool ok = true;
    int x = si(cpu_.load32(p + ActorXOffset, ok));
    int y = si(cpu_.load32(p + ActorYOffset, ok));
    int tx = x - 160;
    int ty = y - 100;
    if (std::abs(tx - cameraX_) > dx)
        cameraX_ = tx - (tx > cameraX_ ? dx : -dx);
    if (std::abs(ty - cameraY_) > dy)
        cameraY_ = ty - (ty > cameraY_ ? dy : -dy);
}
int Runtime::scoreGet(const std::string& g, int idx, uint32_t out) {
    std::vector<Score> s;
    for (auto& x : scores_)
        if (x.game == g)
            s.push_back(x);
    std::sort(s.begin(), s.end(), [](auto& a, auto& b) { return a.score > b.score; });
    if (idx < 0 || size_t(idx) >= s.size())
        return -1;
    bool ok = true;
    writeCString(out, s[idx].game, 24);
    writeCString(out + 24, s[idx].player, 24);
    cpu_.store32(out + 48, s[idx].score, ok);
    return ok ? 0 : -1;
}
uint32_t Runtime::performanceCall(uint32_t i, uint32_t p, uint32_t arg) {
    bool ok = true;
    auto u16 = [&](uint32_t o) { return cpu_.load16(p + o, ok); };
    auto u32 = [&](uint32_t o) { return cpu_.load32(p + o, ok); };
    auto put = [&](uint32_t o, uint32_t v) { cpu_.store32(p + o, v, ok); };
    if (i == 125) {
        if (!p || u16(0) != 1 || u16(2) < 12 || perf_.state == 1)
            return uint32_t(-1);
        uint32_t version = u32(4), name = u32(8);
        if (!ok || !name)
            return uint32_t(-1);
        uint32_t seq = perf_.sequence + 1;
        perf_ = {};
        perf_.sequence = seq;
        perf_.state = 1;
        perf_.suiteVersion = version;
        perf_.name = guestString(name, 31);
        perf_.started = nowUs();
        activePerfCase_ = -1;
        return 0;
    }
    if (i == 126) {
        if (perf_.state != 1 || activePerfCase_ >= 0 || perf_.cases.size() >= 12 || !p || !arg ||
            arg > 100000 || u16(0) != 1 || u16(2) < 20)
            return uint32_t(-1);
        PerfCase c;
        c.index = u32(4);
        c.mode = u32(8);
        auto name = u32(12), goal = u32(16);
        if (!ok || !name)
            return uint32_t(-1);
        c.name = guestString(name, 31);
        c.goal = goal ? guestString(goal, 63) : "";
        c.samples.reserve(arg);
        perf_.cases.push_back(std::move(c));
        activePerfCase_ = int(perf_.cases.size() - 1);
        return 0;
    }
    if (i == 127) {
        if (activePerfCase_ < 0 || !p || u16(0) != 1 || u16(2) < 28)
            return uint32_t(-1);
        auto& c = perf_.cases[size_t(activePerfCase_)];
        if (c.samples.size() >= c.samples.capacity())
            return uint32_t(-2);
        uint32_t update = u32(8), draw = u32(12), present = u32(16), total = u32(20);
        if (!ok)
            return uint32_t(-1);
        if (!total)
            total = update + draw + present;
        c.samples.push_back(total);
        c.updateTotal += update;
        c.drawTotal += draw;
        c.presentTotal += present;
        c.frameTotal += total;
        if (total > 33333)
            ++c.missed;
        return 0;
    }
    if (i == 128) {
        if (activePerfCase_ < 0)
            return uint32_t(-1);
        auto& c = perf_.cases[size_t(activePerfCase_)];
        if (c.samples.empty()) {
            activePerfCase_ = -1;
            perf_.cases.pop_back();
            return uint32_t(-2);
        }
        auto sorted = c.samples;
        std::sort(sorted.begin(), sorted.end());
        auto pct = [&](size_t n) {
            return sorted[std::min(sorted.size() - 1, (sorted.size() * n + 99) / 100 - 1)];
        };
        c.frames = uint32_t(sorted.size());
        c.first = 0;
        for (size_t j = 0; j < size_t(activePerfCase_); ++j)
            c.first += perf_.cases[j].frames;
        c.min = sorted.front();
        c.max = sorted.back();
        c.mean = uint32_t(c.frameTotal / c.frames);
        c.p50 = pct(50);
        c.p95 = pct(95);
        c.p99 = pct(99);
        c.update = uint32_t(c.updateTotal / c.frames);
        c.draw = uint32_t(c.drawTotal / c.frames);
        c.present = uint32_t(c.presentTotal / c.frames);
        c.samples.clear();
        activePerfCase_ = -1;
        return 0;
    }
    if (i == 129) {
        if (perf_.state != 1 || activePerfCase_ >= 0)
            return uint32_t(-1);
        perf_.state = 2;
        perf_.ended = nowUs();
        return 0;
    }
    if (i == 130) {
        if (perf_.state != 1)
            return uint32_t(-1);
        perf_.state = 3;
        perf_.ended = nowUs();
        activePerfCase_ = -1;
        return 0;
    }
    if (i == 131) {
        if (!p || u16(0) != 1 || u16(2) < 40)
            return uint32_t(-1);
        put(4, perf_.state);
        put(8, uint32_t(perf_.cases.size()));
        put(12, activePerfCase_ < 0 ? uint32_t(-1) : uint32_t(activePerfCase_));
        for (uint32_t o = 16; o < 40; o += 4)
            put(o, 0);
        return ok ? 0 : uint32_t(-1);
    }
    if (i == 132) {
        if (perf_.state != 2 || !p)
            return uint32_t(-1); // summary is a fixed public ABI struct
        auto str = [&](uint32_t off, const std::string& s, size_t cap) {
            for (size_t j = 0; j < cap; ++j)
                cpu_.store8(p + off + uint32_t(j), j < s.size() ? uint8_t(s[j]) : 0, ok);
        };
        str(0, "prg32-qt-" + std::to_string(perf_.sequence), 72);
        str(72, "qt-host", 40);
        str(112, "qt", 24);
        str(136, "qt-quick", 24);
        str(216, perf_.name, 40);
        str(256, "release", 12);
        str(268, "network", 20);
        uint32_t frames = 0, min = UINT32_MAX, max = 0, missed = 0, screenCount = 0;
        uint64_t total = 0, update = 0, draw = 0, present = 0;
        for (const auto& c : perf_.cases) {
            frames += c.frames;
            total += c.frameTotal;
            update += c.updateTotal;
            draw += c.drawTotal;
            present += c.presentTotal;
            min = std::min(min, c.min);
            max = std::max(max, c.max);
            missed += c.missed;
            screenCount = std::max(screenCount, c.index + 1);
        }
        cpu_.store32(p + 296, uint32_t(perf_.started), ok);
        cpu_.store32(p + 300, uint32_t(perf_.started >> 32), ok);
        put(304, uint32_t(perf_.ended - perf_.started));
        put(308, frames);
        put(320, screenCount);
        put(324, total ? uint32_t(100000000ull * frames / total) : 0);
        put(328, min == UINT32_MAX ? 0 : min);
        put(332, frames ? uint32_t(total / frames) : 0);
        put(348, max);
        put(352, missed);
        put(356, frames ? uint32_t(update / frames) : 0);
        put(360, frames ? uint32_t(draw / frames) : 0);
        put(364, frames ? uint32_t(present / frames) : 0);
        return ok ? 0 : uint32_t(-1);
    }
    return uint32_t(-1);
}
void Runtime::hostCall(uint32_t i) {
    // PRG32 ABI arguments follow the RISC-V integer calling convention: a0-a7 are x10-x17 and the
    // scalar result is returned in a0. Every numeric case below is a stable public table index.
    auto a = [&](int n) { return cpu_.reg(10 + n); };
    auto ret = [&](uint32_t v) { cpu_.setReg(10, v); };
    bool ok = true;
    auto i8 = [&](uint32_t v) { return int8_t(uint8_t(v)); };
    switch (i) {
    case 0:
        // ABI #0 time_ms(): monotonic milliseconds since this runtime was loaded.
        ret(uint32_t(nowUs() / 1000));
        break;
    case 1:
    case 3:
    case 4:
        ret(input_);
        break;
    case 2:
        ret(a(0) == 0 ? input_ & 0x7f : (input_ >> 8) & 0x7f);
        break;
    case 5: {
        // ABI #5 tone(frequency_hz, duration_ms, amplitude_10bit).
        uint8_t v = uint8_t(std::min<uint32_t>(255, a(2) * 255 / 1023));
        if (audio_)
            audio_->tone(a(0), int(a(1)), v);
        pulseLED(a(0) % 128, v);
        break;
    }
    case 6:
        noteOn(a(0), a(1), a(2), uint8_t(a(3)), 0);
        break;
    case 7: {
        for (uint32_t n = 0; n < a(1); n++) {
            auto hz = cpu_.load16(a(0) + n * 4, ok), ms = cpu_.load16(a(0) + n * 4 + 2, ok);
            if (audio_)
                audio_->tone(hz, ms, 180);
        }
        break;
    }
    case 8: {
        std::vector<float> p(a(1));
        for (uint32_t n = 0; n < a(1); n++)
            p[n] = (float(cpu_.load8(a(0) + n, ok)) - 128) / 128;
        if (audio_)
            audio_->playPCM(0, p, a(2), 1, 255, 0, -1, -1);
        pulseLED();
        break;
    }
    case 9:
        if (audio_)
            audio_->configure(2);
        ret(1);
        break;
    case 10:
        if (audio_)
            audio_->shutdown();
        break;
    case 11:
        ret(2);
        break;
    case 12:
        playSample(a(0), uint8_t(a(1)), uint16_t(a(2)), 0);
        ret(cart_.audio() && a(0) < cart_.audio()->samples.size() ? 0 : uint32_t(-1));
        break;
    case 13:
        playSample(a(0), uint8_t(a(1)), uint16_t(a(2)), i8(a(3)));
        ret(cart_.audio() && a(0) < cart_.audio()->samples.size() ? 0 : uint32_t(-1));
        break;
    case 14:
        if (audio_)
            audio_->stop(si(a(0)));
        break;
    case 15:
        if (audio_)
            audio_->stopAll();
        break;
    case 16:
        noteOn(a(0), a(1), a(2), uint8_t(a(3)), channelPans_[a(0) & 7]);
        break;
    case 17:
        noteOn(a(0), a(1), a(2), uint8_t(a(3)), i8(a(4)));
        break;
    case 18:
        if (audio_)
            audio_->noteOff(a(0));
        break;
    case 19:
        if (a(4) > 0)
            noteOn(a(0), a(1), cpu_.load8(a(3), ok), uint8_t(a(2)), 0);
        break;
    case 20:
        playTrack(a(0));
        break;
    case 21:
        track_.reset();
        break;
    case 22:
        tempo_ = std::clamp(si(a(0)), 30, 300);
        break;
    case 23:
        masterVolume_ = uint8_t(a(0));
        if (audio_)
            audio_->setMasterVolume(masterVolume_);
        break;
    case 24:
        channelVolumes_[a(0) & 7] = uint8_t(a(1));
        if (audio_)
            audio_->setChannelVolume(a(0) & 7, uint8_t(a(1)));
        break;
    case 25:
        channelPans_[a(0) & 7] = i8(a(1));
        if (audio_)
            audio_->setChannelPan(a(0) & 7, i8(a(1)));
        break;
    case 26:
    case 31:
        ret(uint32_t(-1));
        break;
    case 27:
    case 28:
    case 29:
    case 30:
        ret(0);
        break;
    case 32:
        // ABI #32 multiplayer_init(): initialize the shared Store WebSocket transport.
        if (multiplayer_)
            multiplayer_->initialize();
        break;
    case 33:
        // ABI #33 multiplayer_available(): the transport is compiled into every Qt target.
        ret(multiplayer_ && multiplayer_->available() ? 1 : 0);
        break;
    case 34:
        // ABI #34 multiplayer_join(signature, flags): signatures use [A-Za-z0-9_.:-]{1,47}.
        ret(multiplayer_ ? uint32_t(multiplayer_->join(guestString(a(0), 48), a(1))) : uint32_t(-1));
        break;
    case 35:
        ret(multiplayer_ ? uint32_t(multiplayer_->leave()) : uint32_t(-1));
        break;
    case 36:
        if (multiplayer_)
            multiplayer_->tick(uint32_t(nowUs() / 1000));
        break;
    case 37:
        ret(multiplayer_ ? uint32_t(multiplayer_->setLocalState(
                               int16_t(a(0)), int16_t(a(1)), uint16_t(a(2)), uint16_t(a(3))))
                         : uint32_t(-1));
        break;
    case 38:
        ret(multiplayer_ ? uint32_t(multiplayer_->setInput(a(0))) : uint32_t(-1));
        break;
    case 39:
        ret(multiplayer_ ? uint32_t(multiplayer_->peerCount(uint32_t(nowUs() / 1000))) : 0);
        break;
    case 40: {
        // ABI #40 multiplayer_get_peer(index, out): write the public 24-byte player snapshot field by field.
        MultiplayerPeer peer;
        if (!multiplayer_ || multiplayer_->peer(si(a(0)), uint32_t(nowUs() / 1000), peer) != 0) {
            ret(uint32_t(-1));
            break;
        }
        cpu_.store32(a(1), peer.playerId, ok);
        cpu_.store16(a(1) + 4, uint16_t(peer.x), ok);
        cpu_.store16(a(1) + 6, uint16_t(peer.y), ok);
        cpu_.store16(a(1) + 8, peer.sprite, ok);
        cpu_.store16(a(1) + 10, peer.flags, ok);
        cpu_.store32(a(1) + 12, peer.input, ok);
        cpu_.store32(a(1) + 16, peer.frame, ok);
        cpu_.store32(a(1) + 20, peer.lastSeenMs, ok);
        ret(ok ? 0 : uint32_t(-1));
        break;
    }
    case 41:
        ret(1);
        break;
    case 42:
        ret(uint32_t(-1));
        break;
    case 43:
    case 45:
    case 46:
        ret(0);
        break;
    case 44:
        ret(0);
        break;
    case 47:
        // ABI #47 log_clear().
        log_.clear();
        break;
    case 48:
        log_.push_back(char(a(0) & 255));
        break;
    case 49:
        log_ += guestString(a(0));
        break;
    case 50: {
        char b[16];
        std::snprintf(b, sizeof b, "%08X", a(0));
        log_ += b;
        break;
    }
    case 51:
        // ABI #51 screen_clear(color565).
        fb_.clear(uint16_t(a(0)));
        break;
    case 52:
        if (present_)
            present_();
        break;
    case 53:
        fullscreen_ = a(0) != 0;
        break;
    case 54:
        ret(fullscreen_ ? 1 : 0);
        break;
    case 55:
        // ABI #55 draw_pixel(x, y, color565).
        fb_.pixel(si(a(0)), si(a(1)), uint16_t(a(2)));
        break;
    case 56:
        // ABI #56 draw_rect(x, y, width, height, color565).
        fb_.rect(si(a(0)), si(a(1)), si(a(2)), si(a(3)), uint16_t(a(4)));
        break;
    case 57:
        // ABI #57 draw_text(x, y, utf8, foreground565, background565).
        fb_.text8(si(a(0)), si(a(1)), guestString(a(2)), uint16_t(a(3)), uint16_t(a(4)));
        break;
    case 58: {
        auto row = fb_.snapshotRow(si(a(0)));
        size_t n = std::min<size_t>(row.size(), a(2));
        for (size_t j = 0; j < n; j++)
            cpu_.store16(a(1) + uint32_t(j * 2), row[j], ok);
        ret(uint32_t(n));
        break;
    }
    case 59:
    case 62:
    case 63:
    case 64:
    case 65:
    case 66:
    case 67:
    case 68:
        break;
    case 60:
        ret(0);
        break;
    case 61:
        writeCString(guestPtr(scratchOffset_), "NONE", 16);
        ret(guestPtr(scratchOffset_));
        break;
    case 69:
    case 71:
        fb_.clear(uint16_t(a(2)));
        fb_.text8(8, 72, guestString(a(0)), uint16_t(a(3)), uint16_t(a(2)));
        fb_.text8(8, 88, guestString(a(1)), uint16_t(a(4)), uint16_t(a(2)));
        break;
    case 70:
    case 72:
        fb_.clear(uint16_t(a(3)));
        fb_.text8(8, 72, guestString(a(0)), uint16_t(a(4)), uint16_t(a(3)));
        if (present_)
            present_();
        break;
    case 73:
        fb_.clear(0);
        fb_.text8(80, 96, "PRG32", 0xffff, 0);
        if (present_)
            present_();
        break;
    case 74:
        if (a(0))
            fb_.text8(
                si(a(1)), si(a(2)), "IN:" + std::to_string(a(3)) + " F:" + std::to_string(a(4)), 0xffff, 0);
        break;
    case 75:
        break;
    case 76:
    case 78:
        break;
    case 77:
    case 79:
        ret(0);
        break;
    case 80:
        // ABI #80 tile_screen_clear(color565).
        fb_.clear(uint16_t(a(0)));
        tileScreen_.fill(0);
        break;
    case 81: {
        TileDef t{};
        if (a(1))
            for (int y = 0; y < 8; y++)
                t.bits[y] = cpu_.load8(a(1) + y, ok);
        t.fg = uint16_t(a(2));
        t.bg = uint16_t(a(3));
        tiles_[uint16_t(a(0))] = t;
        break;
    }
    case 82: {
        int x = si(a(0)), y = si(a(1));
        if (x >= 0 && y >= 0 && x < 40 && y < 25)
            tileScreen_[y * 40 + x] = uint16_t(a(2));
        break;
    }
    case 83:
        for (int y = 0; y < 25; y++)
            for (int x = 0; x < 40; x++)
                drawTile(x * 8, y * 8, tileScreen_[y * 40 + x]);
        if (present_)
            present_();
        break;
    case 84: {
        int l = si(a(0));
        if (l >= 0 && l < 2)
            playfield_[l].fill(uint16_t(a(1)));
        break;
    }
    case 85: {
        int l = si(a(0)), x = si(a(1)), y = si(a(2));
        if (l >= 0 && l < 2 && x >= 0 && y >= 0 && x < 64 && y < 32)
            playfield_[l][y * 64 + x] = uint16_t(a(3));
        break;
    }
    case 86: {
        int l = si(a(0)), x = si(a(1)), y = si(a(2));
        ret(l >= 0 && l < 2 && x >= 0 && y >= 0 && x < 64 && y < 32 ? playfield_[l][y * 64 + x] : 0);
        break;
    }
    case 87: {
        int l = si(a(0));
        if (l >= 0 && l < 2) {
            scrollX_[l] = si(a(1));
            scrollY_[l] = si(a(2));
        }
        break;
    }
    case 88: {
        int l = si(a(0));
        if (l >= 0 && l < 2) {
            scrollX_[l] += si(a(1));
            scrollY_[l] += si(a(2));
        }
        break;
    }
    case 89: {
        int l = si(a(0));
        if (l >= 0 && l < 2) {
            parallaxX_[l] = si(a(1));
            parallaxY_[l] = si(a(2));
        }
        break;
    }
    case 90:
        cameraX_ = si(a(0));
        cameraY_ = si(a(1));
        break;
    case 91:
        ret(uint32_t(cameraX_));
        break;
    case 92:
        ret(uint32_t(cameraY_));
        break;
    case 93:
        // ABI #93 playfield_draw(layer, transparent_zero).
        drawPlayfield(si(a(0)), a(1) != 0);
        break;
    case 94:
        drawPlayfield(0);
        drawPlayfield(1, true);
        break;
    case 95:
        drawPlayfield(0);
        drawPlayfield(1, true);
        if (present_)
            present_();
        break;
    case 96:
        tileFlags_[uint16_t(a(0))] = uint8_t(a(1));
        break;
    case 97:
        ret(tileFlags_[uint16_t(a(0))] & 255);
        break;
    case 98:
        ret(tileAt(si(a(0)), si(a(1)), si(a(2))));
        break;
    case 99:
        ret(solidAt(si(a(0)), si(a(1)), si(a(2)), 1) ? 1 : 0);
        break;
    case 100: {
        // ABI #100 actor_init(actor, layer, x, y, width, height); offsets use the Actor* constants above.
        uint32_t p = a(0);
        cpu_.store32(p + ActorXOffset, a(2), ok);
        cpu_.store32(p + ActorYOffset, a(3), ok);
        cpu_.store32(p + ActorVelocityXOffset, 0, ok);
        cpu_.store32(p + ActorVelocityYOffset, 0, ok);
        cpu_.store16(p + ActorWidthOffset, uint16_t(a(4)), ok);
        cpu_.store16(p + ActorHeightOffset, uint16_t(a(5)), ok);
        cpu_.store16(p + ActorStatusOffset, 0, ok);
        cpu_.store8(p + ActorLayerOffset, uint8_t(a(1)), ok);
        cpu_.store8(p + ActorReservedOffset, 0, ok);
        break;
    }
    case 101:
        ret(actorMove(a(0), si(a(1)), si(a(2))));
        break;
    case 102:
        ret(actorStep(a(0), a(1), si(a(2)), si(a(3)), si(a(4)), si(a(5))));
        break;
    case 103:
        cameraFollow(a(0), si(a(1)), si(a(2)));
        break;
    case 104: {
        int ax = si(a(0)), ay = si(a(1)), aw = si(a(2)), ah = si(a(3)), bx = si(a(4)), by = si(a(5)),
            bw = si(a(6)), bh = si(a(7));
        ret(ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by);
        break;
    }
    case 105: {
        std::array<uint8_t, 8> b{};
        for (int n = 0; n < 8; n++)
            b[n] = cpu_.load8(a(2) + n, ok);
        uint16_t bg = uint16_t(a(4));
        fb_.draw1BPP(si(a(0)), si(a(1)), 8, 8, b, uint16_t(a(3)), &bg);
        break;
    }
    case 106:
        drawRGBSprite(si(a(0)), si(a(1)), 16, 16, a(2), {});
        break;
    case 107:
        ret(!a(1) || !a(2) ? 0 : (a(0) / a(2)) % a(1));
        break;
    case 108:
        drawRGBSprite(si(a(0)),
                      si(a(1)),
                      si(a(2)),
                      si(a(3)),
                      a(4) + a(5) * uint32_t(std::max(0, si(a(2)) * si(a(3)) * 2)),
                      uint16_t(a(6)));
        break;
    case 109: {
        uint32_t p = a(0);
        cpu_.store32(p, a(1), ok);
        cpu_.store16(p + 4, uint16_t(a(2)), ok);
        cpu_.store16(p + 6, uint16_t(a(3)), ok);
        cpu_.store16(p + 8, uint16_t(a(4)), ok);
        cpu_.store16(p + 10, uint16_t(a(5)), ok);
        cpu_.store32(p + 12, 0, ok);
        cpu_.store32(p + 16, 0, ok);
        cpu_.store16(p + 20, uint16_t(a(6)), ok);
        break;
    }
    case 110: {
        uint32_t p = a(0), ms = cpu_.load16(p + 10, ok), count = cpu_.load16(p + 8, ok);
        if (ms && count) {
            cpu_.store32(p + 12, (a(1) / ms) % count, ok);
            cpu_.store32(p + 16, a(1), ok);
        }
        break;
    }
    case 111: {
        uint32_t p = a(0), frames = cpu_.load32(p, ok), fr = cpu_.load32(p + 12, ok);
        int w = cpu_.load16(p + 4, ok), h = cpu_.load16(p + 6, ok);
        uint16_t tr = cpu_.load16(p + 20, ok);
        drawRGBSprite(si(a(1)), si(a(2)), w, h, frames + fr * uint32_t(w * h * 2), tr);
        break;
    }
    case 112:
        scores_.push_back({guestString(a(0)), guestString(a(1)), a(2)});
        ret(0);
        break;
    case 113:
        drawRGBSprite(si(a(0)), si(a(1)), 24, 24, a(2), {});
        break;
    case 114:
        writeCString(a(0), currentPlayer_, a(1));
        ret(0);
        break;
    case 115:
        currentPlayer_ = guestString(a(0));
        ret(0);
        break;
    case 116:
        ret(0);
        break;
    case 117:
        scores_.push_back({guestString(a(0)), currentPlayer_, a(1)});
        ret(0);
        break;
    case 118:
        ret(uint32_t(-1));
        break;
    case 119: {
        auto g = guestString(a(0));
        ret(uint32_t(std::count_if(scores_.begin(), scores_.end(), [&](auto& s) { return s.game == g; })));
        break;
    }
    case 120:
        ret(uint32_t(scoreGet(guestString(a(0)), si(a(1)), a(2))));
        break;
    case 121:
        ret(0);
        break;
    case 122:
        drawIndexedSprite(si(a(0)), si(a(1)), a(2), a(3), false);
        break;
    case 123:
        drawIndexedSprite(si(a(0)), si(a(1)), a(2), a(3), true);
        break;
    case 124: {
        // ABI #124 performance_time_us(): return a 64-bit monotonic timestamp in a0:a1.
        auto n = nowUs();
        ret(uint32_t(n));
        cpu_.setReg(11, uint32_t(n >> 32));
        break;
    }
    case 125:
        // ABI #125-#132: version-1 performance-suite lifecycle and result broker.
    case 126:
    case 127:
    case 128:
    case 129:
    case 130:
    case 131:
    case 132:
        ret(performanceCall(i, a(0), a(1)));
        break;
    case 133:
        // ABI #133 palette_set(index, color565).
        fb_.setPalette(a(0), uint16_t(a(1)));
        break;
    case 134:
        ret(fb_.palette(a(0)));
        break;
    case 135:
        fb_.pixelIndexed(si(a(0)), si(a(1)), uint8_t(a(2)));
        break;
    case 136:
        fb_.rectIndexed(si(a(0)), si(a(1)), si(a(2)), si(a(3)), uint8_t(a(4)));
        break;
    case 137:
        fb_.clearIndexed(uint8_t(a(0)));
        break;
    case 138: {
        // ABI #138 random_range_inclusive(lower, upper), using the runtime's xorshift32 state.
        uint32_t lo = a(0), hi = a(1);
        if (hi <= lo) {
            ret(lo);
            break;
        }
        rng_ ^= rng_ << 13;
        rng_ ^= rng_ >> 17;
        rng_ ^= rng_ << 5;
        ret(lo + rng_ % ((hi - lo) + 1));
        break;
    }
    default:
        ret(uint32_t(-1));
        break;
    }
}
} // namespace prg32
