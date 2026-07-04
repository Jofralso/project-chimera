#pragma once

#include <cstdint>
#include <vector>
#include <string>

namespace chimera {

struct StepData {
    bool active = false;
    float velocity = 1.0f;
    float probability = 1.0f;
};

struct StepTrigger {
    uint32_t pad;
    uint32_t step;
    float velocity;
};

class StepSequencer {
public:
    StepSequencer(uint32_t num_pads = 16, uint32_t num_steps = 16);

    void set_bpm(float bpm) { bpm_ = bpm; }
    float bpm() const { return bpm_; }

    void set_steps_per_beat(uint32_t spb) { steps_per_beat_ = spb; }
    uint32_t steps_per_beat() const { return steps_per_beat_; }

    void set_num_steps(uint32_t num);
    uint32_t num_steps() const { return num_steps_; }
    uint32_t num_pads() const { return num_pads_; }

    StepData& step(uint32_t pad, uint32_t step);
    const StepData& step(uint32_t pad, uint32_t step) const;

    void set_step(uint32_t pad, uint32_t step, bool active,
                  float velocity = 1.0f, float probability = 1.0f);
    void toggle_step(uint32_t pad, uint32_t step);

    void set_pattern_name(const std::string& name) { name_ = name; }
    const std::string& pattern_name() const { return name_; }

    void reset();
    void set_current_step(uint32_t step) { current_step_ = step; }
    uint32_t current_step() const { return current_step_; }

    std::vector<StepTrigger> advance(uint64_t transport_frames,
                                     double sample_rate);

private:
    uint32_t num_pads_;
    uint32_t num_steps_;
    float bpm_ = 120.0f;
    uint32_t steps_per_beat_ = 4;
    std::vector<std::vector<StepData>> steps_;
    std::string name_ = "Pattern 1";

    uint64_t last_step_frame_ = 0;
    uint32_t current_step_ = 0;

    double frames_per_step() const {
        return (60.0 / bpm_) / steps_per_beat_ * 48000.0;
    }
};

} // namespace chimera
