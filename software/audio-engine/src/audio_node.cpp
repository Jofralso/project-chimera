#include "chimera/audio_node.h"

namespace chimera {

NodeID AudioNode::next_id_ = 1;

AudioNode::AudioNode(std::string name, NodeType type, NodeID id)
    : name_(std::move(name))
    , type_(type)
{
    if (id != 0) {
        id_ = id;
        if (id >= next_id_) next_id_ = id + 1;
    } else {
        id_ = next_id_++;
    }
}

Port* AudioNode::input(size_t index) {
    return (index < inputs_.size()) ? &inputs_[index] : nullptr;
}

Port* AudioNode::output(size_t index) {
    return (index < outputs_.size()) ? &outputs_[index] : nullptr;
}

const Port* AudioNode::input(size_t index) const {
    return (index < inputs_.size()) ? &inputs_[index] : nullptr;
}

const Port* AudioNode::output(size_t index) const {
    return (index < outputs_.size()) ? &outputs_[index] : nullptr;
}

Port* AudioNode::add_input(PortDescriptor desc) {
    inputs_.emplace_back(std::move(desc), 0);
    return &inputs_.back();
}

Port* AudioNode::add_output(PortDescriptor desc) {
    outputs_.emplace_back(std::move(desc), 0);
    return &outputs_.back();
}

void AudioNode::prepare(double sample_rate, size_t block_size) {
    for (auto& p : inputs_) {
        p.buffer = AudioBuffer(block_size, 1);
    }
    for (auto& p : outputs_) {
        p.buffer = AudioBuffer(block_size, 1);
    }
}

void AudioNode::release() {
    inputs_.clear();
    outputs_.clear();
}

} // namespace chimera
