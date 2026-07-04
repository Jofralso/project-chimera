#include "chimera/nodes/master_output.h"
#include <algorithm>
#include <cmath>

namespace chimera {

MasterOutputNode::MasterOutputNode(uint32_t num_channels)
    : AudioNode("Master Output", NodeType::Sink)
    , num_channels_(num_channels)
{
    for (uint32_t i = 0; i < num_channels; ++i) {
        add_input({"Ch " + std::to_string(i + 1), PortDirection::Input, PortDataType::Audio});
    }
}

void MasterOutputNode::process(size_t num_frames) {
    for (size_t ch = 0; ch < num_inputs(); ++ch) {
        auto* in = input(ch);
        if (!in || !in->buffer.data) continue;
        float* data = in->buffer.data;
        for (size_t i = 0; i < num_frames; ++i) {
            data[i] = std::clamp(data[i] * volume_, -1.0f, 1.0f);
        }
    }
}

} // namespace chimera
