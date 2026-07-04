#include "chimera/nodes/master_output.h"
#include <algorithm>
#include <cmath>

namespace chimera {

MasterOutputNode::MasterOutputNode()
    : AudioNode("Master Output", NodeType::Sink)
{
    add_input({"Left In", PortDirection::Input, PortDataType::Audio});
    add_input({"Right In", PortDirection::Input, PortDataType::Audio});
}

void MasterOutputNode::process(size_t num_frames) {
    auto* left_in = input(0);
    auto* right_in = input(1);

    if (!left_in || !right_in) return;

    float* left_data = left_in->buffer.data;
    float* right_data = right_in->buffer.data;

    for (size_t i = 0; i < num_frames; ++i) {
        float l = left_data[i] * volume_;
        float r = right_data[i] * volume_;
        left_data[i] = std::clamp(l, -1.0f, 1.0f);
        right_data[i] = std::clamp(r, -1.0f, 1.0f);
    }
}

} // namespace chimera
