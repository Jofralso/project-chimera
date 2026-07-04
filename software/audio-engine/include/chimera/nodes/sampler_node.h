#pragma once

#include "../audio_node.h"
#include "../wav_loader.h"
#include <atomic>
#include <string>
#include <vector>

namespace chimera {

class SamplerNode : public AudioNode {
public:
    SamplerNode();

    void process(size_t num_frames) override;
    std::string node_class() const override { return "builtin.sampler"; }

    bool load_wav(const std::string& path);

    void trigger();
    void release();
    void set_loop(bool loop) { loop_ = loop; }
    bool loop() const { return loop_; }

    bool is_playing() const { return playing_; }
    uint64_t position() const { return position_; }
    uint64_t total_frames() const { return total_frames_; }
    uint32_t sample_rate() const { return sample_rate_; }
    uint16_t num_channels() const { return num_channels_; }

private:
    std::vector<float> sample_data_;
    uint32_t sample_rate_{0};
    uint16_t num_channels_{0};
    uint64_t total_frames_{0};

    std::atomic<bool> playing_{false};
    std::atomic<bool> loop_{false};
    uint64_t position_{0};
};

} // namespace chimera
