#pragma once

#include "../audio_backend.h"
#include <atomic>
#include <memory>
#include <string>
#include <vector>

namespace chimera {

class JackBackend : public AudioBackend {
public:
    JackBackend() = default;
    ~JackBackend() override;

    bool init(const EngineConfig& config) override;
    bool start() override;
    void stop() override;
    void shutdown() override;

    bool running() const override { return running_.load(); }
    std::string name() const override { return "JACK"; }

    void set_process_callback(
        void (*callback)(float**, float**, size_t, void*),
        void* userdata) override;

private:
    std::atomic<bool> running_{false};

    void (*process_callback_)(float**, float**, size_t, void*) = nullptr;
    void* userdata_ = nullptr;

    size_t block_size_ = 256;
    double sample_rate_ = 48000.0;
    size_t num_inputs_ = 0;
    size_t num_outputs_ = 2;
    std::string client_name_ = "Chimera";

    // JACK handles (opaque)
    void* client_ = nullptr;
    void* dl_handle_ = nullptr;

    // Port arrays
    std::vector<void*> output_ports_;
    std::vector<void*> input_ports_;

    // Buffer arrays (reused each cycle)
    std::vector<float*> output_buffers_;
    std::vector<float*> input_buffers_;

    // Dynamically resolved JACK function pointers
    struct JackFuncs;
    JackFuncs* j_ = nullptr;

    bool load_jack_library();
    void unload_jack_library();
    bool create_ports();

    static int process_callback_thunk(uint32_t nframes, void* arg);
    static void shutdown_callback_thunk(void* arg);
};

} // namespace chimera
