#include "Cartridge.h"
#include "Runtime.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <set>

namespace {
class AuditAudioSink final : public prg32::AudioSink {
  public:
    void configure(int) override {
        ++events;
    }
    void shutdown() override {
    }
    void tone(double, int, uint8_t) override {
        ++events;
    }
    void noteOn(int, int, uint8_t, int8_t) override {
        ++events;
    }
    void noteOff(int) override {
        ++events;
    }
    void playPCM(int, const std::vector<float>& samples, double, double, uint8_t, int8_t, int, int) override {
        ++events;
        pcmSamples += samples.size();
    }
    void stop(int) override {
        ++events;
    }
    void stopAll() override {
        ++events;
    }
    void setMasterVolume(uint8_t) override {
        ++events;
    }
    void setChannelVolume(int, uint8_t) override {
        ++events;
    }
    void setChannelPan(int, int8_t) override {
        ++events;
    }
    uint64_t events = 0;
    uint64_t pcmSamples = 0;
};

uint64_t framebufferHash(const std::vector<uint16_t>& pixels) {
    uint64_t hash = 1469598103934665603ull;
    for (uint16_t pixel : pixels) {
        hash ^= pixel;
        hash *= 1099511628211ull;
    }
    return hash;
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: prg32qt-headless file.prg32 [frames] [--require-performance] [--verify-media] "
                     "[--input frame:mask] [--dump-ppm path]\n";
        return 2;
    }
    std::ifstream f(argv[1], std::ios::binary);
    std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), {});
    std::string e;
    auto c = prg32::Cartridge::parse(b, e);
    if (!c) {
        std::cerr << e << "\n";
        return 3;
    }
    prg32::Runtime r;
    AuditAudioSink audio;
    r.setAudioSink(&audio);
    if (!r.load(*c, e) || !r.init(e)) {
        std::cerr << e << "\n";
        return 4;
    }
    int frames = argc > 2 ? std::stoi(argv[2]) : 300;
    bool verifyMedia = false;
    bool requirePerformance = false;
    std::map<int, uint32_t> scriptedInput;
    std::string dumpPath;
    for (int argument = 3; argument < argc; ++argument) {
        std::string option = argv[argument];
        verifyMedia |= option == "--verify-media";
        requirePerformance |= option == "--require-performance";
        if (option == "--input" && argument + 1 < argc) {
            std::string value = argv[++argument];
            size_t separator = value.find(':');
            if (separator == std::string::npos) {
                std::cerr << "invalid --input value; expected frame:mask\n";
                return 2;
            }
            scriptedInput[std::stoi(value.substr(0, separator))] =
                uint32_t(std::stoul(value.substr(separator + 1), nullptr, 0));
        }
        if (option == "--dump-ppm" && argument + 1 < argc)
            dumpPath = argv[++argument];
    }
    constexpr std::array<uint32_t, 10> inputSequence = {0, 1, 2, 4, 8, 16, 32, 64, 17, 34};
    std::set<uint64_t> frameHashes;
    size_t maximumNonBlackPixels = 0;
    for (int i = 0; i < frames; i++) {
        uint32_t input = verifyMedia ? inputSequence[size_t(i / 30) % inputSequence.size()] : 0;
        if (auto scripted = scriptedInput.find(i); scripted != scriptedInput.end())
            input = scripted->second;
        if (!r.frame(input, e)) {
            std::cerr << "frame " << i << ": " << e << "\n";
            return 5;
        }
        if (verifyMedia && i % 15 == 0) {
            const auto& pixels = r.framebuffer().rgb565Pixels();
            frameHashes.insert(framebufferHash(pixels));
            maximumNonBlackPixels = std::max(
                maximumNonBlackPixels, size_t(std::count_if(pixels.begin(), pixels.end(), [](uint16_t pixel) {
                    return pixel != 0;
                })));
        }
    }
    if (!dumpPath.empty()) {
        std::ofstream output(dumpPath, std::ios::binary);
        output << "P6\n" << prg32::Framebuffer::Width << " " << prg32::Framebuffer::Height << "\n255\n";
        for (uint16_t pixel : r.framebuffer().rgb565Pixels()) {
            const std::array<char, 3> rgb = {char(((pixel >> 11) & 31) * 255 / 31),
                                             char(((pixel >> 5) & 63) * 255 / 63),
                                             char((pixel & 31) * 255 / 31)};
            output.write(rgb.data(), rgb.size());
        }
    }
    if (requirePerformance && r.performance().state != 2) {
        std::cerr << "performance suite did not complete (state " << r.performance().state << ")\n";
        return 6;
    }
    std::cout << c->name() << ": OK (" << frames << " frames)\n";
    if (verifyMedia) {
        std::cout << "MEDIA graphics_non_black=" << maximumNonBlackPixels
                  << " unique_frame_hashes=" << frameHashes.size() << " audio_declared=" << bool(c->audio())
                  << " audio_events=" << audio.events << " pcm_samples=" << audio.pcmSamples << "\n";
        if (maximumNonBlackPixels == 0) {
            std::cerr << "media verification found no rendered pixels\n";
            return 7;
        }
    }
    return 0;
}
