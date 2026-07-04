#include "chimera/audio_graph.h"
#include "chimera/nodes/master_output.h"
#include "chimera/nodes/test_tone.h"
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

    std::printf("\n%d test(s) failed\n", failures);
    return failures > 0 ? 1 : 0;
}
