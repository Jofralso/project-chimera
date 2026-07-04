#include "chimera/nodes/gain_node.h"
#include <algorithm>
#include <cmath>

namespace chimera {

GainNode::GainNode(uint32_t num_channels)
    : AudioNode("Gain", NodeType::Processor)
    , num_channels_(num_channels)
{
    for (uint32_t i = 0; i < num_channels; ++i) {
        add_input({"In " + std::to_string(i + 1), PortDirection::Input, PortDataType::Audio});
        add_output({"Out " + std::to_string(i + 1), PortDirection::Output, PortDataType::Audio});
    }
}

void GainNode::set_gain(float gain) {
    target_gain_ = std::clamp(gain, 0.0f, 2.0f);
}

void GainNode::process(size_t num_frames) {
    float ramp = (target_gain_ - current_gain_) / static_cast<float>(num_frames);

    for (uint32_t ch = 0; ch < num_channels_; ++ch) {
        auto* in = input(ch);
        auto* out = output(ch);
        if (!in || !out || !in->buffer.data || !out->buffer.data) continue;

        float g = current_gain_;
        for (size_t i = 0; i < num_frames; ++i) {
            g += ramp;
            out->buffer.data[i] = in->buffer.data[i] * g;
        }
    }

    current_gain_ = target_gain_;
}

} // namespace chimera
