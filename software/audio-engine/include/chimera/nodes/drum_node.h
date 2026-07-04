#pragma once

#include "../audio_node.h"
#include "../ring_buffer.h"
#include "../wav_loader.h"
#include "../dsp/adsr.h"
#include <atomic>
#include <string>
#include <vector>

namespace chimera {

struct DrumTrigger {
    uint32_t pad_index;
    float velocity;
};

struct DrumPadParams {
    float tune = 1.0f;
    float decay = 0.5f;
    float pan = 0.0f;
    float level = 1.0f;
};

struct DrumPadState {
    std::vector<float> sample_data;
    uint32_t sample_rate = 0;
    uint16_t num_channels = 0;
    uint64_t total_frames = 0;

    DrumPadParams params;

    ADSREnvelope envelope;
    float position = 0.0f;
    bool active = false;
    float velocity = 1.0f;
};

class DrumNode : public AudioNode {
public:
    explicit DrumNode(uint32_t num_pads = 8);

    void process(size_t num_frames) override;
    void prepare(double sample_rate, size_t block_size) override;
    void release() override;
    std::string node_class() const override { return "builtin.drum"; }

    bool load_sample(uint32_t pad, const std::string& path);
    void trigger(uint32_t pad, float velocity = 1.0f);

    void set_pad_tune(uint32_t pad, float tune);
    void set_pad_decay(uint32_t pad, float decay);
    void set_pad_pan(uint32_t pad, float pan);
    void set_pad_level(uint32_t pad, float level);

    uint32_t num_pads() const { return num_pads_; }
    const DrumPadState& pad(uint32_t index) const { return pads_[index]; }

    RingBuffer<DrumTrigger>& trigger_queue() { return trigger_queue_; }

    bool is_pad_loaded(uint32_t pad) const {
        return pad < num_pads_ && pads_[pad].total_frames > 0;
    }

private:
    uint32_t num_pads_;
    std::vector<DrumPadState> pads_;
    RingBuffer<DrumTrigger> trigger_queue_{256};
    double sample_rate_{48000.0};
    size_t block_size_{256};

    float pan_gain_left(float pan) const { return (1.0f - pan) * 0.5f; }
    float pan_gain_right(float pan) const { return (1.0f + pan) * 0.5f; }
};

} // namespace chimera
