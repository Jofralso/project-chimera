#pragma once

#include <cmath>
#include <cstdint>

namespace chimera {

class ADSREnvelope {
public:
    void trigger() {
        value_ = 1.0f;
        active_ = true;
    }

    void release() {
        releasing_ = true;
        release_start_ = value_;
    }

    void set_decay(float seconds, float sample_rate) {
        if (seconds <= 0.0f) seconds = 0.001f;
        decay_coeff_ = std::pow(kThreshold, 1.0f / (seconds * static_cast<float>(sample_rate)));
    }

    void set_release(float seconds, float sample_rate) {
        if (seconds <= 0.0f) seconds = 0.001f;
        release_coeff_ = std::pow(kThreshold, 1.0f / (seconds * static_cast<float>(sample_rate)));
    }

    float process() {
        if (!active_) return 0.0f;

        float out = value_;

        if (releasing_) {
            value_ *= release_coeff_;
            if (value_ <= kThreshold) {
                value_ = 0.0f;
                active_ = false;
                releasing_ = false;
            }
        } else {
            value_ *= decay_coeff_;
            if (value_ <= kThreshold) {
                value_ = 0.0f;
                active_ = false;
            }
        }

        return out;
    }

    bool is_active() const { return active_; }
    float value() const { return value_; }
    void reset() { value_ = 0.0f; active_ = false; releasing_ = false; }

private:
    static constexpr float kThreshold = 0.001f;
    float value_ = 0.0f;
    float decay_coeff_ = 0.999f;
    float release_coeff_ = 0.999f;
    bool active_ = false;
    bool releasing_ = false;
    float release_start_ = 0.0f;
};

} // namespace chimera
