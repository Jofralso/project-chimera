#include "chimera/wav_loader.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>

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
    // Generate a small 16-bit mono WAV in memory by writing to /tmp
    std::string path = "/tmp/test_wav_loader.wav";

    // Create 16-bit mono WAV: 0.1s of 440Hz at 44100
    {
        FILE* f = std::fopen(path.c_str(), "wb");
        TEST("open file", f != nullptr);
        if (!f) return 1;

        uint32_t sr = 44100;
        uint16_t bits = 16;
        uint16_t ch = 1;
        uint32_t num_samples = static_cast<uint32_t>(sr * 0.1);
        uint32_t data_size = num_samples * (bits / 8);

        uint32_t riff_size = 36 + data_size;
        fwrite("RIFF", 1, 4, f);
        fwrite(&riff_size, 4, 1, f);
        fwrite("WAVE", 1, 4, f);

        uint32_t fmt_size = 16;
        uint16_t fmt_tag = 1; // PCM
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
            float t = static_cast<float>(i) / static_cast<float>(sr);
            int16_t s = static_cast<int16_t>(16000.0f * sinf(2.0f * 3.14159f * 440.0f * t));
            fwrite(&s, 2, 1, f);
        }

        std::fclose(f);
    }

    chimera::WavLoader loader;
    chimera::WavInfo info = loader.load(path);

    TEST("loaded sample rate", info.sample_rate == 44100);
    TEST("loaded channels", info.num_channels == 1);
    TEST("loaded frames", info.num_frames == 4410);
    TEST("loaded samples not empty", !info.samples.empty());

    float peak = 0.0f;
    for (auto s : info.samples) {
        float abs_s = s < 0 ? -s : s;
        if (abs_s > peak) peak = abs_s;
    }
    TEST("peak amplitude reasonable", peak > 0.1f && peak <= 0.5f);

    std::remove(path.c_str());

    // Test 24-bit WAV
    std::string path24 = "/tmp/test_wav_24bit.wav";
    {
        FILE* f = std::fopen(path24.c_str(), "wb");
        TEST("open 24-bit file", f != nullptr);
        if (!f) return 1;

        uint32_t sr = 48000;
        uint16_t bits = 24;
        uint16_t ch = 2;
        uint32_t num_samples = 480;
        uint32_t data_size = num_samples * ch * (bits / 8);

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
            for (uint16_t c = 0; c < ch; ++c) {
                int32_t s = (i % 100 < 50) ? 8388607 : -8388608;
                uint8_t bytes[3] = {
                    static_cast<uint8_t>(s & 0xFF),
                    static_cast<uint8_t>((s >> 8) & 0xFF),
                    static_cast<uint8_t>((s >> 16) & 0xFF)
                };
                fwrite(bytes, 3, 1, f);
            }
        }
        std::fclose(f);
    }

    chimera::WavInfo info24 = loader.load(path24);
    TEST("24-bit sample rate", info24.sample_rate == 48000);
    TEST("24-bit channels", info24.num_channels == 2);
    TEST("24-bit frames", info24.num_frames == 480);
    TEST("24-bit samples not empty", !info24.samples.empty());

    std::remove(path24.c_str());

    std::printf("\n%d test(s) failed\n", failures);
    return failures > 0 ? 1 : 0;
}
