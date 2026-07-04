#pragma once

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <new>

namespace chimera {

struct AudioBuffer {
    float* data{nullptr};
    size_t num_frames{0};
    size_t num_channels{0};

    AudioBuffer() = default;

    AudioBuffer(size_t frames, size_t channels)
        : num_frames(frames)
        , num_channels(channels)
    {
        size_t bytes = frames * channels * sizeof(float);
        data = static_cast<float*>(allocate_realtime(bytes));
    }

    ~AudioBuffer() {
        if (data) {
            free_realtime(data, num_frames * num_channels * sizeof(float));
        }
    }

    AudioBuffer(AudioBuffer&& other) noexcept
        : data(other.data)
        , num_frames(other.num_frames)
        , num_channels(other.num_channels)
    {
        other.data = nullptr;
        other.num_frames = 0;
        other.num_channels = 0;
    }

    AudioBuffer& operator=(AudioBuffer&& other) noexcept {
        if (this != &other) {
            this->~AudioBuffer();
            data = other.data;
            num_frames = other.num_frames;
            num_channels = other.num_channels;
            other.data = nullptr;
            other.num_frames = 0;
            other.num_channels = 0;
        }
        return *this;
    }

    AudioBuffer(const AudioBuffer&) = delete;
    AudioBuffer& operator=(const AudioBuffer&) = delete;

    void clear() {
        if (data) {
            std::memset(data, 0, num_frames * num_channels * sizeof(float));
        }
    }

    float* channel(size_t ch) {
        return data + ch * num_frames;
    }

    const float* channel(size_t ch) const {
        return data + ch * num_frames;
    }

    static void* allocate_realtime(size_t bytes);
    static void free_realtime(void* ptr, size_t bytes);
};

} // namespace chimera
