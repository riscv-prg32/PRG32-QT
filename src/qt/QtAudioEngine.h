#pragma once
#include "Audio.h"
#include <QAudioFormat>
#include <QIODevice>
#include <QMutex>
#include <array>
#include <deque>
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
    /** The PRG32 firmware audio engine's output (22050 Hz interleaved stereo): played as it is,
     *  resampled to the device. Bounded to half a second so sound never lags the game. */
    void pushFirmwarePcm(const std::vector<int16_t>& stereo);
    /** What the output device has taken: its name, sink state, processed time, and the firmware
     *  frames it pulled (count and 64-bit FNV-1a over their samples, before resampling). */
    struct DeviceStatus {
        QString device;
        int state = -1;
        qint64 processedUs = 0;
        qint64 firmwareFrames = 0;
        quint64 firmwareHash = 0;
    };
    DeviceStatus deviceStatus() const;
    /** Return a bounded snapshot of recently rendered left-channel samples. */
    std::vector<float> waveformLeft() const;
    /** Return a bounded snapshot of recently rendered right-channel samples. */
    std::vector<float> waveformRight() const;

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
    mutable QMutex mutex_;
    std::vector<Voice> voices_;
    std::deque<int16_t> firmwarePcm_;
    double firmwarePosition_ = 0;
    qint64 firmwareFramesPlayed_ = 0;
    quint64 firmwareHash_ = 1469598103934665603ull;
    std::unique_ptr<QAudioSink> sink_;
    QAudioFormat format_;
    float master_ = 0.86f;
    std::array<float, 8> channelVolumes_{};
    std::array<float, 8> channelPans_{};
    std::array<float, 64> waveformLeft_{};
    std::array<float, 64> waveformRight_{};
};
