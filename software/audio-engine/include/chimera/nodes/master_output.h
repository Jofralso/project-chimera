#pragma once

#include "../audio_node.h"

namespace chimera {

class MasterOutputNode : public AudioNode {
public:
    MasterOutputNode();

    void process(size_t num_frames) override;
    std::string node_class() const override { return "builtin.master_output"; }

    void set_volume(float vol) { volume_ = vol; }
    float volume() const { return volume_; }

private:
    float volume_{1.0f};
};

} // namespace chimera
