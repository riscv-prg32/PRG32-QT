#pragma once
#include "Audio.h"
#include <QAudioFormat>
#include <QIODevice>
#include <QMutex>
#include <array>
#include <memory>
#include <vector>
class QAudioSink;
class QtAudioEngine final : public QIODevice, public prg32::AudioSink {
    Q_OBJECT
  public:
    explicit QtAudioEngine(QObject* parent = nullptr);
    ~QtAudioEngine() override;
    void configure(int mode) override;
    void shutdown() override;
    void tone(double frequency, int durationMs, uint8_t volume) override;
    void noteOn(int channel, int note, uint8_t velocity, int8_t pan) override;
    void synthNoteOn(
        int channel, int note, uint8_t velocity, int8_t pan, prg32::SynthParameters parameters) override;
    void noteOff(int channel) override;
    void playPCM(int channel,
                 const std::vector<float>& samples,
                 double sourceRate,
                 double pitch,
                 uint8_t volume,
                 int8_t pan,
                 int loopStart,
                 int loopEnd) override;
    void stop(int channel) override;
    void stopAll() override;
    void setMasterVolume(uint8_t volume) override;
    void setChannelVolume(int channel, uint8_t volume) override;
    void setChannelPan(int channel, int8_t pan) override;
    qint64 readData(char* data, qint64 maxlen) override;
    qint64 writeData(const char*, qint64) override {
        return -1;
    }
    qint64 bytesAvailable() const override {
        return 32768 + QIODevice::bytesAvailable();
    }

  private:
    enum class Kind { Oscillator, Synth, Pcm };
    struct Voice {
        Kind kind = Kind::Pcm;
        int channel = 0;
        double phase = 0, frequency = 440, position = 0, increment = 1;
        float filterLow = 0, filterBand = 0;
        uint32_t noiseState = 0x7ffff8u;
        prg32::SynthParameters synth;
        std::vector<float> samples;
        int loopStart = -1, loopEnd = -1;
        float volume = 1, pan = 0;
        bool active = true;
    };
    void ensureStarted();
    void render(float* left, float* right, int frames);
    void encode(char* dst, int frames, const std::vector<float>& interleaved);
    QMutex mutex_;
    std::vector<Voice> voices_;
    std::unique_ptr<QAudioSink> sink_;
    QAudioFormat format_;
    float master_ = 0.86f;
    std::array<float, 8> channelVolumes_{};
    std::array<float, 8> channelPans_{};
};
