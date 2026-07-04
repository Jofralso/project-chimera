#pragma once

#include "../audio_backend.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

namespace chimera {

class DummyBackend : public AudioBackend {
public:
    DummyBackend() = default;
    ~DummyBackend() override;

    bool init(const EngineConfig& config) override;
    bool start() override;
    void stop() override;
    void shutdown() override;

    bool running() const override { return running_.load(); }
    std::string name() const override { return "Dummy"; }

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

    void thread_func();
};

} // namespace chimera
