#pragma once

#include <cmath>
#include <cstdint>

namespace chimera {

enum class Waveform : uint8_t {
    Sine,
    Saw,
    Square,
    Triangle
};

class Oscillator {
public:
    void reset() { phase_ = 0.0f; }
    void set_frequency(float freq, float sample_rate) {
        phase_inc_ = freq / sample_rate;
    }
    void set_phase(float p) { phase_ = p; }
    float phase() const { return phase_; }
    float phase_inc() const { return phase_inc_; }

    float process(Waveform wave, float pw = 0.5f) {
        float out = 0.0f;
        float p = phase_ - std::floor(phase_);
        switch (wave) {
            case Waveform::Sine:
                out = std::sin(2.0f * M_PI * p);
                break;
            case Waveform::Saw:
                out = 2.0f * p - 1.0f;
                break;
            case Waveform::Square:
                out = p < pw ? 1.0f : -1.0f;
                break;
            case Waveform::Triangle:
                out = 4.0f * std::abs(p - 0.5f) - 1.0f;
                break;
        }
        phase_ += phase_inc_;
        if (phase_ >= 1.0f) phase_ -= 1.0f;
        return out;
    }

private:
    float phase_ = 0.0f;
    float phase_inc_ = 0.0f;
};

} // namespace chimera
