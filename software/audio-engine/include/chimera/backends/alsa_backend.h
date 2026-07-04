#pragma once

#include "../audio_backend.h"
#include <atomic>
#include <memory>
#include <string>
#include <thread>

namespace chimera {

class AlsaBackend : public AudioBackend {
public:
    AlsaBackend() = default;
    ~AlsaBackend() override;

    bool init(const EngineConfig& config) override;
    bool start() override;
    void stop() override;
    void shutdown() override;

    bool running() const override { return running_.load(); }
    std::string name() const override { return "ALSA"; }

    void set_process_callback(
        void (*callback)(float**, float**, size_t, void*),
        void* userdata) override;

private:
    std::atomic<bool> running_{false};
    std::unique_ptr<std::thread> thread_;

    void (*process_callback_)(float**, float**, size_t, void*) = nullptr;
    void* userdata_ = nullptr;

    size_t block_size_ = 256;
    double sample_rate_ = 48000.0;
    size_t num_inputs_ = 0;
    size_t num_outputs_ = 2;
    std::string device_ = "default";

    void* pcm_handle_ = nullptr;
    void* capture_handle_ = nullptr;

    void thread_func();
    bool open_device();
    bool open_capture_device();
    void close_device();
    void close_capture_device();
};

} // namespace chimera
