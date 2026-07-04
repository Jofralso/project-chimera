#pragma once

#include "../audio_node.h"

namespace chimera {

class AudioInputNode : public AudioNode {
public:
    explicit AudioInputNode(uint32_t num_channels = 2);
    void process(size_t num_frames) override;
    std::string node_class() const override { return "builtin.audio_input"; }
};

class AudioOutputNode : public AudioNode {
public:
    explicit AudioOutputNode(uint32_t num_channels = 2);
    void process(size_t num_frames) override;
    std::string node_class() const override { return "builtin.audio_output"; }
};

} // namespace chimera
