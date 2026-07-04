#include "chimera/wav_loader.h"
#include <cstring>
#include <fstream>
#include <vector>

namespace chimera {

struct RiffChunk {
    char id[4];
    uint32_t size;
};

static_assert(sizeof(RiffChunk) == 8, "RiffChunk must be 8 bytes");

struct WaveFmt {
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
};

static_assert(sizeof(WaveFmt) == 16, "WaveFmt must be 16 bytes");

static bool read_chunk_header(std::ifstream& file, RiffChunk& chunk) {
    file.read(reinterpret_cast<char*>(&chunk), sizeof(chunk));
    return file.good();
}

WavInfo WavLoader::load(const std::string& path) {
    WavInfo info;

    std::ifstream file(path, std::ios::binary);
    if (!file) return info;

    RiffChunk riff;
    if (!read_chunk_header(file, riff)) return info;
    if (std::memcmp(riff.id, "RIFF", 4) != 0) return info;

    char wave_id[4];
    file.read(wave_id, 4);
    if (std::memcmp(wave_id, "WAVE", 4) != 0) return info;

    WaveFmt fmt{};
    bool found_fmt = false;
    std::vector<uint8_t> data_chunk;

    while (file.good()) {
        RiffChunk sub;
        if (!read_chunk_header(file, sub)) break;

        if (std::memcmp(sub.id, "fmt ", 4) == 0 && !found_fmt) {
            uint32_t read_size = sub.size < sizeof(WaveFmt) ? sub.size : sizeof(WaveFmt);
            file.read(reinterpret_cast<char*>(&fmt), read_size);
            if (read_size < sub.size) {
                file.seekg(sub.size - read_size, std::ios::cur);
            }
            found_fmt = true;
        } else if (std::memcmp(sub.id, "data", 4) == 0) {
            data_chunk.resize(sub.size);
            file.read(reinterpret_cast<char*>(data_chunk.data()), sub.size);
        } else {
            file.seekg(sub.size, std::ios::cur);
        }
    }

    if (!found_fmt || data_chunk.empty()) return info;

    info.sample_rate = fmt.sample_rate;
    info.num_channels = fmt.num_channels;

    uint64_t total_samples = data_chunk.size() / (fmt.bits_per_sample / 8);
    info.num_frames = total_samples / fmt.num_channels;
    info.samples.resize(total_samples);

    if (fmt.audio_format == 1) {
        // PCM integer
        if (fmt.bits_per_sample == 16) {
            auto* src = reinterpret_cast<const int16_t*>(data_chunk.data());
            size_t count = total_samples;
            for (size_t i = 0; i < count; ++i) {
                info.samples[i] = static_cast<float>(src[i]) / 32768.0f;
            }
        } else if (fmt.bits_per_sample == 24) {
            size_t count = total_samples;
            for (size_t i = 0; i < count; ++i) {
                int32_t val = (static_cast<int32_t>(data_chunk[i * 3]) << 8)
                            | (static_cast<int32_t>(data_chunk[i * 3 + 1]) << 16)
                            | (static_cast<int32_t>(data_chunk[i * 3 + 2]) << 24);
                info.samples[i] = static_cast<float>(val) / 2147483648.0f;
            }
        } else if (fmt.bits_per_sample == 32) {
            auto* src = reinterpret_cast<const int32_t*>(data_chunk.data());
            size_t count = total_samples;
            for (size_t i = 0; i < count; ++i) {
                info.samples[i] = static_cast<float>(src[i]) / 2147483648.0f;
            }
        }
    } else if (fmt.audio_format == 3) {
        // IEEE float
        auto* src = reinterpret_cast<const float*>(data_chunk.data());
        size_t count = total_samples;
        for (size_t i = 0; i < count; ++i) {
            float s = src[i];
            if (s > 1.0f) s = 1.0f;
            else if (s < -1.0f) s = -1.0f;
            info.samples[i] = s;
        }
    }

    return info;
}

} // namespace chimera
