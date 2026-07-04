#include "chimera/backends/dummy_backend.h"
#include "chimera/engine.h"
#include "chimera/logger.h"

#include <cstring>
#include <thread>

namespace chimera {

DummyBackend::~DummyBackend() {
    shutdown();
}

bool DummyBackend::init(const EngineConfig& config) {
    sample_rate_ = config.sample_rate;
    block_size_ = config.block_size;
    num_inputs_ = config.num_inputs;
    num_outputs_ = config.num_outputs;
    CHIMERA_INFO("Dummy backend initialized: %g Hz, %zu frames", sample_rate_, block_size_);
    return true;
}

bool DummyBackend::start() {
    if (running_.load()) return false;
    if (!process_callback_) return false;

    running_.store(true);
    thread_ = std::make_unique<std::thread>([this]() { thread_func(); });
    CHIMERA_INFO("Dummy backend started");
    return true;
}

void DummyBackend::stop() {
    running_.store(false);
    if (thread_ && thread_->joinable()) {
        thread_->join();
    }
    thread_.reset();
    CHIMERA_INFO("Dummy backend stopped");
}

void DummyBackend::shutdown() {
    stop();
}

void DummyBackend::set_process_callback(
    void (*callback)(float**, float**, size_t, void*),
    void* userdata)
{
    process_callback_ = callback;
    userdata_ = userdata;
}

void DummyBackend::thread_func() {
    double dt = static_cast<double>(block_size_) / sample_rate_;
    auto period = std::chrono::duration<double>(dt);

    auto* outputs = new float*[num_outputs_];
    auto* inputs = new float*[num_inputs_];

    for (size_t i = 0; i < num_outputs_; ++i) {
        outputs[i] = new float[block_size_]();
    }
    for (size_t i = 0; i < num_inputs_; ++i) {
        inputs[i] = new float[block_size_]();
    }

    while (running_.load()) {
        process_callback_(outputs, inputs, block_size_, userdata_);
        std::this_thread::sleep_for(period);
    }

    for (size_t i = 0; i < num_outputs_; ++i) delete[] outputs[i];
    for (size_t i = 0; i < num_inputs_; ++i) delete[] inputs[i];
    delete[] outputs;
    delete[] inputs;
}

} // namespace chimera
