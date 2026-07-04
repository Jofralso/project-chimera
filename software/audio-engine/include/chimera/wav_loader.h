#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace chimera {

struct WavInfo {
    std::vector<float> samples;
    uint32_t sample_rate{0};
    uint16_t num_channels{0};
    uint64_t num_frames{0};
};

class WavLoader {
public:
    WavInfo load(const std::string& path);

private:
    bool parse_header(const uint8_t* data, size_t size, WavInfo& info);
};

} // namespace chimera
