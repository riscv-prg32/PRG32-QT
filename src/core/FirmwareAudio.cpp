#include "FirmwareAudio.h"
#include <algorithm>
#include <cstring>

extern "C" {
#include "audio_port.h"
#include "prg32_audio.h"
}

namespace prg32 {
namespace {
constexpr uint32_t FirstAudioCall = 5, LastAudioCall = 25; // the portable ABI's audio entries

/** The cartridge's AUD0 block, where the console looks for it: after the header and the code. */
std::vector<uint8_t> audioBlockOf(std::span<const uint8_t> data) {
    if (data.size() < 20)
        return {};
    const uint32_t headerSize = uint32_t(data[8]) | uint32_t(data[9]) << 8;
    const uint32_t codeSize = uint32_t(data[16]) | uint32_t(data[17]) << 8 | uint32_t(data[18]) << 16 |
                              uint32_t(data[19]) << 24;
    const uint64_t offset = uint64_t(headerSize) + codeSize;
    if (offset + 40 > data.size() || std::memcmp(data.data() + offset, "AUD0", 4) != 0)
        return {};
    const uint8_t* block = data.data() + offset;
    const uint32_t blockSize = uint32_t(block[36]) | uint32_t(block[37]) << 8 | uint32_t(block[38]) << 16 |
                               uint32_t(block[39]) << 24;
    if (blockSize < 40 || offset + blockSize > data.size())
        return {};
    return {block, block + blockSize};
}
} // namespace

bool FirmwareAudio::attach(Runtime& runtime, std::span<const uint8_t> cartridge, std::string& error) {
    runtime_ = &runtime;
    frames_ = 0;
    audioNanoseconds_ = 0;
    if (prg32_kit_audio_start() != 0) {
        error = "the audio engine did not start";
        return false;
    }
    block_ = audioBlockOf(cartridge);
    if (prg32_kit_audio_load(block_.data(), block_.size()) != 0) {
        error = "failed to load cartridge AUDIO block";
        return false;
    }
    runtime.setHostHooks([this](uint32_t i) { before(i); }, [this](uint32_t i) { after(i); });
    return true;
}

void FirmwareAudio::frameDone() {
    ++frames_;
    catchUp();
}

void FirmwareAudio::catchUp() {
    const uint64_t now = frames_ * VirtualClock::FramePeriodNanoseconds;
    if (now <= audioNanoseconds_)
        return;
    const uint64_t ms = (now - audioNanoseconds_) / 1000000ull;
    if (ms == 0)
        return;
    audioNanoseconds_ += ms * 1000000ull;
    prg32_kit_audio_advance_ms(uint32_t(std::min<uint64_t>(ms, 60000)));
}

std::vector<int16_t> FirmwareAudio::take() {
    const size_t frames = prg32_kit_audio_frames();
    const int16_t* pcm = prg32_kit_audio_pcm();
    std::vector<int16_t> out(pcm, pcm + frames * 2);
    prg32_kit_audio_clear();
    return out;
}

uint64_t FirmwareAudio::pcmHash() const {
    return prg32_kit_audio_hash();
}

void FirmwareAudio::before(uint32_t index) {
    if (index < FirstAudioCall || index > LastAudioCall)
        return;
    catchUp(); // the call takes effect at the start of the frame that makes it
    for (int n = 0; n < 8; ++n)
        args_[n] = runtime_->registerValue(10 + n);
}

void FirmwareAudio::after(uint32_t index) {
    if (index < FirstAudioCall || index > LastAudioCall)
        return;
    const uint32_t* a = args_;
    auto& runtime = *runtime_;
    auto u8 = [](uint32_t v) { return uint8_t(v); };
    auto i8 = [](uint32_t v) { return int8_t(uint8_t(v)); };
    switch (index) {
    case 5: // prg32_buzzer_tone, prg32_buzzer_play_notes, prg32_buzzer_sample_u8: obsolete, silent
    case 7:
    case 8:
        ++buzzerCalls_;
        break;
    case 6:
        prg32_audio_note(u8(a[0]), u8(a[1]), u8(a[2]), u8(a[3]), a[4]);
        break;
    case 9: {
        prg32_audio_config_t config{};
        config.sample_rate = SampleRate;
        config.mode = PRG32_AUDIO_MODE_STEREO;
        config.max_voices = 8;
        if (a[0]) {
            const auto bytes = runtime.debugMemory(a[0], 9);
            if (bytes.size() == 9) {
                const uint32_t mode = uint32_t(bytes[4]) | uint32_t(bytes[5]) << 8;
                config.mode = mode == PRG32_AUDIO_MODE_MONO ? PRG32_AUDIO_MODE_MONO : PRG32_AUDIO_MODE_STEREO;
                config.max_voices = bytes[8] && bytes[8] < 8 ? bytes[8] : 8;
            }
        }
        config.gpio_bclk = config.gpio_lrclk = config.gpio_data = config.gpio_sd = -1;
        runtime.setHostResult(prg32_audio_init(&config) ? 1 : 0);
        break;
    }
    case 10:
        prg32_audio_shutdown();
        break;
    case 11:
        runtime.setHostResult(uint32_t(prg32_audio_get_mode()));
        break;
    case 12:
        runtime.setHostResult(uint32_t(prg32_audio_play_sample(uint16_t(a[0]), u8(a[1]), uint16_t(a[2]))));
        break;
    case 13:
        runtime.setHostResult(
            uint32_t(prg32_audio_play_sample_pan(uint16_t(a[0]), u8(a[1]), uint16_t(a[2]), i8(a[3]))));
        break;
    case 14:
        prg32_audio_stop_channel(int32_t(a[0]));
        break;
    case 15:
        prg32_audio_stop_all();
        break;
    case 16:
        prg32_audio_note_on(u8(a[0]), u8(a[1]), u8(a[2]), u8(a[3]));
        break;
    case 17:
        prg32_audio_note_on_pan(u8(a[0]), u8(a[1]), u8(a[2]), u8(a[3]), i8(a[4]));
        break;
    case 18:
        prg32_audio_note_off(u8(a[0]));
        break;
    case 19: {
        const uint32_t count = std::min<uint32_t>(a[4], 256);
        const auto bytes = runtime.debugMemory(a[3], size_t(count) * 4);
        std::vector<prg32_midi_note_t> notes;
        for (size_t n = 0; n + 4 <= bytes.size(); n += 4)
            notes.push_back({bytes[n], uint16_t(bytes[n + 2] | bytes[n + 3] << 8)});
        prg32_audio_notes(u8(a[0]), u8(a[1]), u8(a[2]), notes.data(), notes.size());
        break;
    }
    case 20:
        prg32_audio_play_track(uint16_t(a[0]));
        break;
    case 21:
        prg32_audio_stop_track();
        break;
    case 22:
        prg32_audio_set_tempo(uint16_t(a[0]));
        break;
    case 23:
        prg32_audio_set_master_volume(u8(a[0]));
        break;
    case 24:
        prg32_audio_set_channel_volume(u8(a[0]), u8(a[1]));
        break;
    case 25:
        prg32_audio_set_channel_pan(u8(a[0]), i8(a[1]));
        break;
    default:
        break;
    }
}
} // namespace prg32
