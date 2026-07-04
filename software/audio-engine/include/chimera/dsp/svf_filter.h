#pragma once

#include <cmath>
#include <cstdint>

namespace chimera {

enum class FilterMode : uint8_t {
    LowPass,
    HighPass,
    BandPass,
    Notch
};

class StateVariableFilter {
public:
    void reset() { low_ = 0.0f; band_ = 0.0f; }

    float process(float input, float cutoff, float resonance, FilterMode mode) {
        float f = std::min(cutoff, 0.999f);
        float r = std::min(resonance, 0.999f);

        high_ = input - low_ - r * band_;
        band_ += f * high_;
        low_ += f * band_;

        switch (mode) {
            case FilterMode::LowPass:  return low_;
            case FilterMode::HighPass: return high_;
            case FilterMode::BandPass: return band_;
            case FilterMode::Notch:    return high_ + low_;
        }
        return low_;
    }

private:
    float low_ = 0.0f;
    float band_ = 0.0f;
    float high_ = 0.0f;
};

} // namespace chimera
