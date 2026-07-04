#pragma once

#include "chimera/audio_backend.h"

namespace chimera {

class NullBackend : public AudioBackend {
public:
    NullBackend();
    ~NullBackend() override;

    bool init(const EngineConfig& config) override;
    bool start() override;
    void stop() override;
    void shutdown() override;

    bool running() const override;
    std::string name() const override;

    void set_process_callback(
        void (*callback)(float**, float**, size_t, void*),
        void* userdata) override;

private:
    void thread_func();

    double sample_rate_ = 48000.0;
    size_t block_size_ = 256;
    size_t num_inputs_ = 0;
    size_t num_outputs_ = 2;
    std::string client_name_ = "Chimera";
    
    std::atomic<bool> running_{false};
    std::atomic<bool> ready_{false};
    std::chrono::duration<double> period_;
    std::unique_ptr<std::thread> thread_;
    
    void (*process_callback_) = nullptr;
    void* userdata_ = nullptr;
    
    float** outputs_ = nullptr;
    float** inputs_ = nullptr;
};

} // namespace chimera
