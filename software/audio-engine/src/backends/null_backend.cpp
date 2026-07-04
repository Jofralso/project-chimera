#include "chimera/audio_node.h"
#include "chimera/audio_backend.h"
#include "chimera/engine.h"

#include <thread>
#include <chrono>

namespace chimera {

class NullBackend : public AudioBackend {
public:
    NullBackend() = default;
    ~NullBackend() override {
        shutdown();
    }

    bool init(const EngineConfig& config) override {
        sample_rate_ = config.sample_rate;
        block_size_ = config.block_size;
        num_inputs_ = config.num_inputs;
        num_outputs_ = config.num_outputs;
        client_name_ = config.client_name;
        return true;
    }

    bool start() override {
        if (running_.load()) return false;
        if (!process_callback_) return false;
        
        running_.store(true);
        period_ = std::chrono::duration<double>(static_cast<double>(block_size_) / sample_rate_);
        
        ready_ = true;
        thread_ = std::make_unique<std::thread>(&NullBackend::thread_func, this);
        return true;
    }

    void stop() override {
        running_.store(false);
        ready_.store(false);
        if (thread_ && thread_->joinable()) {
            thread_->join();
        }
        thread_.reset();
    }

    void shutdown() override {
        stop();
    }

    bool running() const override {
        return running_.load();
    }

    std::string name() const override {
        return "null";
    }

    void set_process_callback(
        void (*callback)(float**, float**, size_t, void*),
        void* userdata) override
    {
        process_callback_ = callback;
        userdata_ = userdata;
    }

private:
    void thread_func() {
        while (!ready_.load() || !running_.load()) {
            std::this_thread::sleep_for(std::chrono::microseconds(1000));
        }

        while (running_.load()) {
            process_callback_(outputs_, inputs_, block_size_, userdata_);
            std::this_thread::sleep_for(period_);
        }
    }

    double sample_rate_ = 48000.0;
    size_t block_size_ = 256;
    size_t num_inputs_ = 0;
    size_t num_outputs_ = 2;
    std::string client_name_ = "Chimera";
    
    std::atomic<bool> running_{false};
    std::atomic<bool> ready_{false};
    std::chrono::duration<double> period_;
    std::unique_ptr<std::thread> thread_;
    
    void (*process_callback_)(float**, float**, size_t, void*) = nullptr;
    void* userdata_ = nullptr;
    
    float** outputs_ = nullptr;
    float** inputs_ = nullptr;
};

} // namespace chimera
