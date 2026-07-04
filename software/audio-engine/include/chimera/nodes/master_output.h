#pragma once

#include "../audio_node.h"

namespace chimera {

class MasterOutputNode : public AudioNode {
public:
    explicit MasterOutputNode(uint32_t num_channels = 2);

    void process(size_t num_frames) override;
    std::string node_class() const override { return "builtin.master_output"; }

    uint32_t num_channels() const { return num_channels_; }
    void set_volume(float vol) { volume_ = vol; }
    float volume() const { return volume_; }

private:
    uint32_t num_channels_;
    float volume_{1.0f};
};

} // namespace chimera
