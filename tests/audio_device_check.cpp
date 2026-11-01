// Plays a cartridge's sound through the computer's real audio output with the PRG32 firmware
// audio engine, as the desktop app does, and checks what the device took:
//
//     prg32qt_audio_device_check cartridge.prg32 [seconds]
//
// The cartridge runs in real time (one frame every 33 ms); each frame's engine PCM goes to
// QtAudioEngine, whose QAudioSink pulls it for the default output device. Passes when the device
// has processed the time, pulled the frames, and the frames it pulled are exactly the engine's
// samples (FNV-1a before resampling). Exit 77 (a CTest skip) when the computer has no output.
#include "Cartridge.h"
#include "FirmwareAudio.h"
#include "QtAudioEngine.h"
#include "Runtime.h"
#include <QAudioDevice>
#include <QCoreApplication>
#include <QMediaDevices>
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    if (argc < 2) {
        std::fprintf(stderr, "usage: prg32qt_audio_device_check cartridge.prg32 [seconds]\n");
        return 2;
    }
    if (QMediaDevices::audioOutputs().isEmpty()) {
        std::printf("SKIP: this computer has no audio output device\n");
        return 77;
    }
    const int seconds = argc > 2 ? std::atoi(argv[2]) : 3;
    std::ifstream file(argv[1], std::ios::binary);
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), {});
    std::string error;
    auto cartridge = prg32::Cartridge::parse(bytes, error);
    prg32::Runtime runtime;
    prg32::NullAudioSink silent;
    prg32::FirmwareAudio firmware;
    runtime.setAudioSink(&silent);
    if (!cartridge || !firmware.attach(runtime, *cartridge, bytes, error) ||
        !runtime.load(*cartridge, error) || !runtime.init(error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 3;
    }
    QtAudioEngine output;
    std::vector<int16_t> pushed;
    float waveformPeak = 0.f;
    long long framesRun = 0;
    QTimer frames;
    QObject::connect(&frames, &QTimer::timeout, [&] {
        std::string frameError;
        if (!runtime.frame(0, frameError)) {
            std::fprintf(stderr, "frame: %s\n", frameError.c_str());
            app.exit(4);
            return;
        }
        ++framesRun;
        firmware.frameDone();
        auto pcm = firmware.take();
        pushed.insert(pushed.end(), pcm.begin(), pcm.end());
        output.pushFirmwarePcm(pcm);
        // Telemetry reads this copy: it must show the firmware engine's sound, not only tones.
        for (float sample : output.waveformLeft())
            waveformPeak = std::max(waveformPeak, std::fabs(sample));
    });
    frames.setTimerType(Qt::PreciseTimer); // as AppController: a coarse timer runs about 6% fast
    frames.start(33);
    QTimer::singleShot(seconds * 1000, &app, [&] { app.exit(0); });
    if (int code = app.exec())
        return code;
    frames.stop();
    const auto status = output.deviceStatus();
    quint64 hash = 1469598103934665603ull;
    for (qint64 i = 0; i < status.firmwareFrames * 2 && i < qint64(pushed.size()); ++i) {
        hash ^= quint16(pushed[size_t(i)]);
        hash *= 1099511628211ull;
    }
    const bool timeOk = status.processedUs >= (seconds - 1) * 1000000LL;
    const bool framesOk = status.firmwareFrames >= (seconds - 1) * prg32::FirmwareAudio::SampleRate;
    const bool samplesOk = hash == status.firmwareHash;
    const bool waveformOk = waveformPeak > 0.f;
    std::printf("{\"device\": \"%s\", \"sink_state\": %d, \"processed_us\": %lld, \"frames_run\": %lld, "
                "\"firmware_frames_played\": %lld, "
                "\"frames_pushed\": %zu, \"played_equal_engine\": %s, \"queued_frames\": %lld, "
                "\"dropped_frames\": %lld, "
                "\"underrun_device_frames\": %lld, \"waveform_peak\": %.3f}\n",
                status.device.toUtf8().constData(),
                status.state,
                static_cast<long long>(status.processedUs),
                framesRun,
                static_cast<long long>(status.firmwareFrames),
                pushed.size() / 2,
                samplesOk ? "true" : "false",
                static_cast<long long>(status.queuedFrames),
                static_cast<long long>(status.droppedFrames),
                static_cast<long long>(status.underrunDeviceFrames),
                double(waveformPeak));
    // At the app's real-time pace the queue must never overflow: a dropped frame means lag.
    const bool pacingOk = status.droppedFrames == 0;
    return timeOk && framesOk && samplesOk && waveformOk && pacingOk ? 0 : 1;
}
