#include "chimera/nodes/test_tone.h"
#include <cmath>

namespace chimera {

TestToneNode::TestToneNode(float frequency, float amplitude)
    : AudioNode("Test Tone", NodeType::Source)
    , frequency_(frequency)
    , amplitude_(amplitude)
{
    add_output({"Signal", PortDirection::Output, PortDataType::Audio});
}

void TestToneNode::prepare(double sample_rate, size_t block_size) {
    AudioNode::prepare(sample_rate, block_size);
    sample_rate_ = sample_rate;
    phase_ = 0.0;
}

void TestToneNode::process(size_t num_frames) {
    auto* out = output(0);
    if (!out) return;

    float* data = out->buffer.data;
    double phase_inc = frequency_ / sample_rate_;

    for (size_t i = 0; i < num_frames; ++i) {
        data[i] = static_cast<float>(std::sin(2.0 * M_PI * phase_) * amplitude_);
        phase_ += phase_inc;
        if (phase_ >= 1.0) phase_ -= 1.0;
    }
}

} // namespace chimera
