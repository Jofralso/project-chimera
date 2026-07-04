#include "chimera/nodes/sampler_node.h"
#include <algorithm>
#include <cstring>
#include <cmath>

namespace chimera {

SamplerNode::SamplerNode()
    : AudioNode("Sampler", NodeType::Source)
{
    add_output({"Left", PortDirection::Output, PortDataType::Audio});
    add_output({"Right", PortDirection::Output, PortDataType::Audio});
}

bool SamplerNode::load_wav(const std::string& path) {
    WavLoader loader;
    WavInfo info = loader.load(path);
    if (info.samples.empty() || info.num_channels == 0) return false;

    sample_data_ = std::move(info.samples);
    sample_rate_ = info.sample_rate;
    num_channels_ = info.num_channels;
    total_frames_ = info.num_frames;
    position_ = 0;
    playing_ = false;

    while (num_outputs() < num_channels_) {
        add_output({"Ch " + std::to_string(num_outputs() + 1),
                    PortDirection::Output, PortDataType::Audio});
    }

    return true;
}

void SamplerNode::trigger() {
    position_ = 0;
    playing_ = true;
}

void SamplerNode::release() {
    playing_ = false;
}

void SamplerNode::process(size_t num_frames) {
    size_t out_ch = num_outputs();
    for (size_t ch = 0; ch < out_ch; ++ch) {
        auto* out = output(ch);
        if (out && out->buffer.data) {
            std::memset(out->buffer.data, 0, num_frames * sizeof(float));
        }
    }

    if (!playing_ || sample_data_.empty()) return;

    size_t ch = std::min(static_cast<size_t>(num_channels_), out_ch);
    uint64_t remain = total_frames_ - position_;

    for (size_t i = 0; i < num_frames && remain > 0; ++i) {
        if (position_ >= total_frames_) {
            if (loop_) {
                position_ = 0;
            } else {
                playing_ = false;
                return;
            }
        }

        for (size_t c = 0; c < ch; ++c) {
            auto* out = output(c);
            if (out && out->buffer.data) {
                size_t src_idx = (position_ * num_channels_) + c;
                if (src_idx < sample_data_.size()) {
                    out->buffer.data[i] = sample_data_[src_idx];
                }
            }
        }
        position_++;
        remain--;
    }
}

} // namespace chimera
