#include "chimera/nodes/step_sequencer.h"
#include <cmath>
#include <cstdlib>

namespace chimera {

StepSequencer::StepSequencer(uint32_t num_pads, uint32_t num_steps)
    : num_pads_(num_pads)
    , num_steps_(num_steps)
{
    steps_.resize(num_pads_, std::vector<StepData>(num_steps_));
}

void StepSequencer::set_num_steps(uint32_t num) {
    if (num < 1) num = 1;
    if (num > 64) num = 64;
    num_steps_ = num;
    for (auto& row : steps_) {
        row.resize(num_steps_);
    }
    if (current_step_ >= num_steps_) current_step_ = 0;
}

StepData& StepSequencer::step(uint32_t pad, uint32_t step) {
    return steps_[pad % num_pads_][step % num_steps_];
}

const StepData& StepSequencer::step(uint32_t pad, uint32_t step) const {
    return steps_[pad % num_pads_][step % num_steps_];
}

void StepSequencer::set_step(uint32_t pad, uint32_t step, bool active,
                              float velocity, float probability) {
    if (pad < num_pads_ && step < num_steps_) {
        auto& s = steps_[pad][step];
        s.active = active;
        s.velocity = std::max(0.0f, std::min(1.0f, velocity));
        s.probability = std::max(0.0f, std::min(1.0f, probability));
    }
}

void StepSequencer::toggle_step(uint32_t pad, uint32_t step) {
    if (pad < num_pads_ && step < num_steps_) {
        steps_[pad][step].active = !steps_[pad][step].active;
    }
}

void StepSequencer::reset() {
    last_step_frame_ = 0;
    current_step_ = 0;
}

std::vector<StepTrigger> StepSequencer::advance(uint64_t transport_frames,
                                                 double sample_rate) {
    std::vector<StepTrigger> triggers;

    if (bpm_ <= 0.0f || steps_per_beat_ == 0) return triggers;

    double fps = frames_per_step();
    if (fps <= 0.0) return triggers;

    uint64_t step_frame = static_cast<uint64_t>((transport_frames / fps) * fps);

    if (step_frame != last_step_frame_ && transport_frames > 0) {
        uint32_t new_step = static_cast<uint32_t>(
            static_cast<double>(transport_frames) / fps) % num_steps_;

        current_step_ = new_step;

        for (uint32_t p = 0; p < num_pads_; ++p) {
            auto& s = steps_[p][current_step_];
            if (s.active) {
                float prob_roll = static_cast<float>(std::rand()) / RAND_MAX;
                if (prob_roll <= s.probability) {
                    triggers.push_back({p, current_step_, s.velocity});
                }
            }
        }

        last_step_frame_ = step_frame;
    }

    return triggers;
}

} // namespace chimera
