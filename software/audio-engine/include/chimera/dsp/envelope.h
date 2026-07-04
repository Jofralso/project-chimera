#pragma once

#include <cmath>
#include <cstdint>

namespace chimera {

class Envelope {
public:
    enum class Stage : uint8_t {
        Idle,
        Attack,
        Decay,
        Sustain,
        Release
    };

    static constexpr float kThreshold = 0.001f;

    void set_params(float attack_ms, float decay_ms, float sustain,
                    float release_ms, float sample_rate) {
        if (sample_rate <= 0.0f) return;
        float a_sec = std::max(attack_ms / 1000.0f, 0.001f);
        float d_sec = std::max(decay_ms / 1000.0f, 0.001f);
        float r_sec = std::max(release_ms / 1000.0f, 0.001f);
        float a_samples = a_sec * sample_rate;
        float d_samples = d_sec * sample_rate;
        float r_samples = r_sec * sample_rate;
        attack_rate_ = 1.0f - std::pow(kThreshold, 1.0f / a_samples);
        decay_coeff_ = std::pow(kThreshold, 1.0f / d_samples);
        sustain_level_ = std::max(0.0f, std::min(1.0f, sustain));
        release_coeff_ = std::pow(kThreshold, 1.0f / r_samples);
    }

    void trigger() {
        stage_ = Stage::Attack;
        value_ = 0.0f;
    }

    void release() {
        if (stage_ != Stage::Idle) {
            stage_ = Stage::Release;
        }
    }

    float process() {
        switch (stage_) {
            case Stage::Idle:
                return 0.0f;

            case Stage::Attack:
                value_ += (1.0f - value_) * attack_rate_;
                if (value_ >= 0.999f) {
                    value_ = 1.0f;
                    stage_ = Stage::Decay;
                }
                return value_;

            case Stage::Decay:
                value_ = sustain_level_ + (value_ - sustain_level_) * decay_coeff_;
                if (std::fabs(value_ - sustain_level_) < 0.001f) {
                    value_ = sustain_level_;
                    stage_ = Stage::Sustain;
                }
                return value_;

            case Stage::Sustain:
                return sustain_level_;

            case Stage::Release:
                value_ *= release_coeff_;
                if (value_ < 0.001f) {
                    value_ = 0.0f;
                    stage_ = Stage::Idle;
                }
                return value_;
        }
        return 0.0f;
    }

    Stage stage() const { return stage_; }
    float value() const { return value_; }
    bool is_active() const { return stage_ != Stage::Idle; }
    void reset() { stage_ = Stage::Idle; value_ = 0.0f; }

private:
    Stage stage_ = Stage::Idle;
    float value_ = 0.0f;
    float attack_rate_ = 0.999f;
    float decay_coeff_ = 0.999f;
    float sustain_level_ = 0.5f;
    float release_coeff_ = 0.999f;
};

} // namespace chimera
