#pragma once
// The PRG32 firmware's own audio engine (components/prg32_audio of the PRG32 SDK), driven by
// a Runtime's host calls, so PRG32-QT renders exactly what the console mixes: 8 voices, the
// SID-like synth and its filter, ADSR envelopes, samples, the tracker and the stereo mix.
// Built when PRG32QT_FIRMWARE_AUDIO is ON (CMake option, needs PRG32_SDK_DIR).
#include "Runtime.h"
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace prg32 {
class FirmwareAudio {
  public:
    static constexpr int SampleRate = 22050;

    /** Start the engine as the console does for a cartridge: its AUD0 block (if any) replaces the assets.
     *  The Runtime's own AudioSink must be a NullAudioSink: the engine makes the sound. */
    bool attach(Runtime& runtime, std::span<const uint8_t> cartridge, std::string& error);
    /** One frame (update + draw) has run: render the audio up to the next frame's start.
     *
     *  The scheduling boundary (the same in the PRG32 firmware under QEMU and in the
     *  Construction Kit's browser core): frame N, counting from 0 after init, starts at
     *  N x 33 ms, and every audio call made by init or by that frame takes effect at its
     *  start, whatever the instruction count. The engine renders whole 20 ms buffers. */
    void frameDone();
    /** Interleaved signed 16-bit stereo frames mixed since the last take (left, right, ...). */
    std::vector<int16_t> take();
    /** 64-bit FNV-1a over every sample mixed since attach (equal on every build of the same sound). */
    uint64_t pcmHash() const;
    uint32_t obsoleteBuzzerCalls() const {
        return buzzerCalls_;
    }

  private:
    void before(uint32_t index);
    void after(uint32_t index);
    void catchUp();
    Runtime* runtime_ = nullptr;
    std::vector<uint8_t> block_;
    uint64_t frames_ = 0;             // frames run since attach
    uint64_t audioNanoseconds_ = 0;   // cartridge time the engine has reached
    uint32_t args_[8]{};
    uint32_t buzzerCalls_ = 0;
};
} // namespace prg32
