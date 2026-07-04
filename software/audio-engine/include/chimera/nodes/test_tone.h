#pragma once

#include "../audio_node.h"

namespace chimera {

class TestToneNode : public AudioNode {
public:
    explicit TestToneNode(float frequency = 440.0f, float amplitude = 0.3f);

    void process(size_t num_frames) override;
    void prepare(double sample_rate, size_t block_size) override;
    std::string node_class() const override { return "builtin.test_tone"; }

    void set_frequency(float freq) { frequency_ = freq; }
    void set_amplitude(float amp) { amplitude_ = amp; }

private:
    float frequency_;
    float amplitude_;
    double sample_rate_{48000.0};
    double phase_{0.0};
};

} // namespace chimera
