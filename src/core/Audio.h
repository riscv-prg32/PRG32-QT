#pragma once
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace prg32 {
struct AudioSample {
    std::vector<uint8_t> bytes;
    int loopStart = 0, loopEnd = 0;
    uint16_t baseNote = 69;
    bool loop = false;
};
struct AudioInstrument {
    uint16_t sampleId = 0xffff;
    uint8_t defaultVolume = 255;
    int8_t defaultPan = 0;
    uint8_t attack = 0, decay = 0, sustain = 255, release = 0;
};
struct AudioEvent {
    uint8_t deltaTicks = 0, command = 0, arg0 = 0, arg1 = 0;
};
struct AudioTrack {
    std::vector<AudioEvent> events;
};
struct AudioBlock {
    std::vector<AudioSample> samples;
    std::vector<AudioInstrument> instruments;
    std::vector<AudioTrack> tracks;
    static std::optional<AudioBlock> parse(std::span<const uint8_t>, std::string&);
};
struct RGBState {
    uint8_t r = 0, g = 0, b = 0;
    double intensity = 0.0;
};
class AudioSink {
  public:
    virtual ~AudioSink() = default;
    virtual void configure(int) = 0;
    virtual void shutdown() = 0;
    virtual void tone(double, int, uint8_t) = 0;
    virtual void noteOn(int, int, uint8_t, int8_t) = 0;
    virtual void noteOff(int) = 0;
    virtual void playPCM(int, const std::vector<float>&, double, double, uint8_t, int8_t, int, int) = 0;
    virtual void stop(int) = 0;
    virtual void stopAll() = 0;
    virtual void setMasterVolume(uint8_t) = 0;
    virtual void setChannelVolume(int, uint8_t) = 0;
    virtual void setChannelPan(int, int8_t) = 0;
};
class NullAudioSink final : public AudioSink {
  public:
    void configure(int) override {
    }
    void shutdown() override {
    }
    void tone(double, int, uint8_t) override {
    }
    void noteOn(int, int, uint8_t, int8_t) override {
    }
    void noteOff(int) override {
    }
    void playPCM(int, const std::vector<float>&, double, double, uint8_t, int8_t, int, int) override {
    }
    void stop(int) override {
    }
    void stopAll() override {
    }
    void setMasterVolume(uint8_t) override {
    }
    void setChannelVolume(int, uint8_t) override {
    }
    void setChannelPan(int, int8_t) override {
    }
};
} // namespace prg32
