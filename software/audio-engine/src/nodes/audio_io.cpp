#include "chimera/nodes/audio_io.h"
#include <cstring>

namespace chimera {

AudioInputNode::AudioInputNode(uint32_t num_channels)
    : AudioNode("Audio Input", NodeType::Source)
{
    for (uint32_t i = 0; i < num_channels; ++i) {
        std::string nm = "Out " + std::to_string(i + 1);
        add_output({std::move(nm), PortDirection::Output, PortDataType::Audio});
    }
}

void AudioInputNode::process(size_t num_frames) {
    for (size_t i = 0; i < num_outputs(); ++i) {
        auto* out = output(i);
        if (out && out->buffer.data) {
            std::memset(out->buffer.data, 0, num_frames * sizeof(float));
        }
    }
}

AudioOutputNode::AudioOutputNode(uint32_t num_channels)
    : AudioNode("Audio Output", NodeType::Sink)
{
    for (uint32_t i = 0; i < num_channels; ++i) {
        std::string nm = "In " + std::to_string(i + 1);
        add_input({std::move(nm), PortDirection::Input, PortDataType::Audio});
    }
}

void AudioOutputNode::process(size_t num_frames) {
    float peak = 0.0f;
    for (size_t i = 0; i < num_inputs(); ++i) {
        auto* in = input(i);
        if (!in || !in->buffer.data) continue;
        for (size_t s = 0; s < num_frames; ++s) {
            float abs_val = in->buffer.data[s] < 0 ? -in->buffer.data[s] : in->buffer.data[s];
            if (abs_val > peak) peak = abs_val;
        }
    }
    (void)peak;
}

} // namespace chimera
