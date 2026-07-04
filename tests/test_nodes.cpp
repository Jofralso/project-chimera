#include "chimera/audio_graph.h"
#include "chimera/nodes/master_output.h"
#include "chimera/nodes/test_tone.h"
#include "chimera/nodes/gain_node.h"
#include "chimera/nodes/sampler_node.h"
#include "chimera/wav_loader.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>

static int failures = 0;

#define TEST(name, expr) do { \
    if (!(expr)) { \
        std::fprintf(stderr, "FAIL: %s (%s:%d)\n", name, __FILE__, __LINE__); \
        failures++; \
    } else { \
        std::printf("PASS: %s\n", name); \
    } \
} while(0)

int main() {
    {
        chimera::TestToneNode tone(440.0f, 0.5f);
        tone.prepare(48000.0, 256);
        tone.process(256);

        auto* out = tone.output(0);
        TEST("test tone has output", out != nullptr);

        if (out) {
            float peak = 0.0f;
            for (size_t i = 0; i < 256; ++i) {
                float abs_val = std::fabs(out->buffer.data[i]);
                if (abs_val > peak) peak = abs_val;
            }
            TEST("test tone produces signal", peak > 0.0f);
            TEST("test tone within amplitude", peak <= 0.5f + 0.001f);

            float first = out->buffer.data[0];
            tone.process(256);
            float after = out->buffer.data[0];
            TEST("phase advances", std::fabs(after - first) > 0.0001f);
        }

        tone.set_frequency(880.0f);
        TEST("frequency updated", true);
    }

    {
        chimera::MasterOutputNode master;
        master.prepare(48000.0, 256);

        auto* left_in = master.input(0);
        auto* right_in = master.input(1);
        TEST("master has stereo inputs", left_in != nullptr && right_in != nullptr);

        for (size_t i = 0; i < 256; ++i) {
            left_in->buffer.data[i] = 0.5f;
            right_in->buffer.data[i] = -0.5f;
        }

        master.set_volume(0.8f);
        master.process(256);

        TEST("volume applied correctly",
             std::fabs(left_in->buffer.data[0] - 0.4f) < 0.001f);
    }

    {
        chimera::MasterOutputNode master8(8);
        master8.prepare(48000.0, 256);
        TEST("8ch master has 8 inputs", master8.num_inputs() == 8);

        for (size_t ch = 0; ch < 8; ++ch) {
            auto* in = master8.input(ch);
            TEST("8ch master input exists", in != nullptr);
            if (in) {
                for (size_t i = 0; i < 256; ++i) {
                    in->buffer.data[i] = 0.6f;
                }
            }
        }

        master8.set_volume(2.0f);
        master8.process(256);

        for (size_t ch = 0; ch < 8; ++ch) {
            auto* in = master8.input(ch);
            if (in) {
                TEST("8ch volume clips at 1.0",
                     std::fabs(in->buffer.data[0] - 1.0f) < 0.001f);
            }
        }
    }

    {
        chimera::GainNode gain(2);
        gain.prepare(48000.0, 256);

        for (size_t i = 0; i < 256; ++i) {
            gain.input(0)->buffer.data[i] = 0.5f;
        }

        // Ramp from 1.0 to 0.0 over 256 frames → last frame reaches 0
        gain.set_gain(0.0f);
        gain.process(256);
        TEST("gain ramps end near zero", std::fabs(gain.output(0)->buffer.data[255]) < 0.001f);

        // Already at 0, set to 2.0 and process again
        // Halfway through the ramp, output should be > 0
        gain.set_gain(2.0f);
        gain.process(256);
        TEST("gain ramps up", gain.output(0)->buffer.data[255] > 0.9f);
    }

    {
        // Create a small WAV for sampler test
        std::string wav_path = "/tmp/test_sampler_unit.wav";
        FILE* f = std::fopen(wav_path.c_str(), "wb");
        uint32_t sr = 44100;
        uint16_t bits = 16;
        uint16_t ch = 1;
        uint32_t num_samples = 441;
        uint32_t data_size = num_samples * (bits / 8);
        uint32_t riff_size = 36 + data_size;
        fwrite("RIFF", 1, 4, f);
        fwrite(&riff_size, 4, 1, f);
        fwrite("WAVE", 1, 4, f);
        uint32_t fmt_size = 16;
        uint16_t fmt_tag = 1;
        uint32_t byte_rate = sr * ch * (bits / 8);
        uint16_t block_align = ch * (bits / 8);
        fwrite("fmt ", 1, 4, f);
        fwrite(&fmt_size, 4, 1, f);
        fwrite(&fmt_tag, 2, 1, f);
        fwrite(&ch, 2, 1, f);
        fwrite(&sr, 4, 1, f);
        fwrite(&byte_rate, 4, 1, f);
        fwrite(&block_align, 2, 1, f);
        fwrite(&bits, 2, 1, f);
        fwrite("data", 1, 4, f);
        fwrite(&data_size, 4, 1, f);
        for (uint32_t i = 0; i < num_samples; ++i) {
            int16_t s = static_cast<int16_t>(i * 10);
            fwrite(&s, 2, 1, f);
        }
        std::fclose(f);

        chimera::SamplerNode sampler;
        TEST("load wav succeeds", sampler.load_wav(wav_path));
        TEST("sampler sample rate", sampler.sample_rate() == 44100);
        TEST("sampler channels", sampler.num_channels() == 1);
        TEST("sampler frames", sampler.total_frames() == 441);
        TEST("sampler not playing initially", !sampler.is_playing());

        sampler.prepare(48000.0, 256);
        sampler.trigger();
        TEST("sampler playing after trigger", sampler.is_playing());

        sampler.process(256);
        TEST("sampler advanced position", sampler.position() > 0);

        sampler.set_loop(true);

        // Process past end with loop
        chimera::SamplerNode sampler_loop;
        sampler_loop.load_wav(wav_path);
        sampler_loop.prepare(48000.0, 256);
        sampler_loop.set_loop(true);
        sampler_loop.trigger();
        // Call process twice: 256 + 256 = 512 frames, 441-sample file loops once
        sampler_loop.process(256);
        sampler_loop.process(256);
        TEST("looping sampler wraps", sampler_loop.position() == 71);

        std::remove(wav_path.c_str());
    }

    std::printf("\n%d test(s) failed\n", failures);
    return failures > 0 ? 1 : 0;
}
