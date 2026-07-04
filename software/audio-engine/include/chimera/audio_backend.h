#pragma once

#include <string>

namespace chimera {

struct EngineConfig;

class AudioBackend {
public:
    virtual ~AudioBackend() = default;

    virtual bool init(const EngineConfig& config) = 0;
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual void shutdown() = 0;

    virtual bool running() const = 0;
    virtual std::string name() const = 0;

    virtual void set_process_callback(
        void (*callback)(float** outputs, float** inputs, size_t num_frames, void* userdata),
        void* userdata) = 0;
};

} // namespace chimera
