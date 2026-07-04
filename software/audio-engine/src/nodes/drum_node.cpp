#include "chimera/nodes/drum_node.h"
#include <cstring>
#include <cmath>

namespace chimera {

DrumNode::DrumNode(uint32_t num_pads)
    : AudioNode("Drum", NodeType::Source)
    , num_pads_(num_pads)
{
    pads_.resize(num_pads_);

    add_output({"Left", PortDirection::Output, PortDataType::Audio});
    add_output({"Right", PortDirection::Output, PortDataType::Audio});
}

void DrumNode::prepare(double sample_rate, size_t block_size) {
    AudioNode::prepare(sample_rate, block_size);
    sample_rate_ = sample_rate;
    block_size_ = block_size;

    for (auto& pad : pads_) {
        if (pad.total_frames > 0) {
            pad.envelope.set_decay(pad.params.decay, sample_rate);
            pad.envelope.set_release(pad.params.decay * 0.5f, sample_rate);
        }
    }
}

void DrumNode::release() {
    for (auto& pad : pads_) {
        pad.sample_data.clear();
        pad.sample_data.shrink_to_fit();
        pad.total_frames = 0;
        pad.active = false;
        pad.position = 0.0f;
        pad.envelope.reset();
    }
    AudioNode::release();
}

bool DrumNode::load_sample(uint32_t pad, const std::string& path) {
    if (pad >= num_pads_) return false;

    WavLoader loader;
    WavInfo info = loader.load(path);
    if (info.num_frames == 0) return false;

    auto& state = pads_[pad];
    state.sample_data = std::move(info.samples);
    state.sample_rate = info.sample_rate;
    state.num_channels = info.num_channels;
    state.total_frames = info.num_frames;
    state.position = 0.0f;
    state.active = false;
    state.envelope.reset();

    if (sample_rate_ > 0) {
        state.envelope.set_decay(state.params.decay, sample_rate_);
        state.envelope.set_release(state.params.decay * 0.5f, sample_rate_);
    }

    return true;
}

void DrumNode::trigger(uint32_t pad, float velocity) {
    trigger_queue_.push({pad, velocity});
}

void DrumNode::set_pad_tune(uint32_t pad, float tune) {
    if (pad < num_pads_) pads_[pad].params.tune = std::max(0.5f, std::min(2.0f, tune));
}

void DrumNode::set_pad_decay(uint32_t pad, float decay) {
    if (pad < num_pads_) {
        pads_[pad].params.decay = std::max(0.01f, decay);
        if (sample_rate_ > 0) {
            pads_[pad].envelope.set_decay(pads_[pad].params.decay, sample_rate_);
            pads_[pad].envelope.set_release(pads_[pad].params.decay * 0.5f, sample_rate_);
        }
    }
}

void DrumNode::set_pad_pan(uint32_t pad, float pan) {
    if (pad < num_pads_) pads_[pad].params.pan = std::max(-1.0f, std::min(1.0f, pan));
}

void DrumNode::set_pad_level(uint32_t pad, float level) {
    if (pad < num_pads_) pads_[pad].params.level = std::max(0.0f, std::min(1.0f, level));
}

void DrumNode::process(size_t num_frames) {
    auto* left = output(0);
    auto* right = output(1);

    if (!left || !right) return;

    std::memset(left->buffer.data, 0, num_frames * sizeof(float));
    std::memset(right->buffer.data, 0, num_frames * sizeof(float));

    DrumTrigger trig;
    while (trigger_queue_.pop(trig)) {
        if (trig.pad_index < num_pads_) {
            auto& pad = pads_[trig.pad_index];
            if (pad.total_frames == 0) continue;
            pad.envelope.trigger();
            pad.velocity = trig.velocity;
            pad.position = 0.0f;
            pad.active = true;
        }
    }

    for (uint32_t p = 0; p < num_pads_; ++p) {
        auto& pad = pads_[p];
        if (!pad.active || pad.total_frames == 0) continue;

        float l_gain = pan_gain_left(pad.params.pan);
        float r_gain = pan_gain_right(pad.params.pan);
        float level = pad.params.level * pad.velocity;

        for (size_t f = 0; f < num_frames; ++f) {
            float env = pad.envelope.process();
            if (!pad.active) break;

            uint64_t idx = static_cast<uint64_t>(pad.position);
            if (idx >= pad.total_frames) {
                pad.active = false;
                break;
            }

            if (pad.num_channels == 2) {
                float s_l = pad.sample_data[idx * 2] * env * level;
                float s_r = pad.sample_data[idx * 2 + 1] * env * level;
                left->buffer.data[f] += s_l;
                right->buffer.data[f] += s_r;
            } else {
                float s = pad.sample_data[idx * pad.num_channels] * env * level;
                left->buffer.data[f] += s * l_gain;
                right->buffer.data[f] += s * r_gain;
            }

            pad.position += pad.params.tune;
        }
    }
}

} // namespace chimera
