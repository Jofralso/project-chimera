#pragma once

#include "../audio_node.h"

namespace chimera {

class GainNode : public AudioNode {
public:
    explicit GainNode(uint32_t num_channels = 2);

    void process(size_t num_frames) override;
    std::string node_class() const override { return "builtin.gain"; }

    void set_gain(float gain);
    float gain() const { return target_gain_; }

    uint32_t num_channels() const { return num_channels_; }

private:
    uint32_t num_channels_;
    float target_gain_{1.0f};
    float current_gain_{1.0f};
};

} // namespace chimera
