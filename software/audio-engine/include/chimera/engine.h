#pragma once

#include "audio_backend.h"
#include "audio_graph.h"
#include "ring_buffer.h"
#include <atomic>
#include <functional>
#include <memory>
#include <string>

namespace chimera {

struct EngineConfig {
    std::string client_name{"Chimera"};
    double sample_rate{48000.0};
    size_t block_size{256};
    size_t num_inputs{0};
    size_t num_outputs{2};
    std::string audio_device{"default"};
    std::string backend{"auto"};
};

enum class EngineState {
    Uninitialized,
    Idle,
    Running,
    Paused,
    Error
};

enum class TransportState : uint8_t {
    Stopped,
    Playing,
    Paused,
    Loop
};

struct EngineMessage {
    enum class Type {
        Start,
        Stop,
        SetParam,
        LoadGraph,
        Quit,
        ConnectNodes,
        DisconnectNodes,
        RemoveNode
    };

    Type type;
    NodeID node_id{0};
    NodeID target_node_id{0};
    size_t param_index{0};
    size_t source_port{0};
    size_t target_port{0};
    float param_value{0.0f};
};

class Engine {
public:
    Engine();
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    bool init(const EngineConfig& config);
    void shutdown();

    bool start();
    void stop();
    void pause();
    void resume();

    EngineState state() const { return state_.load(); }
    const EngineConfig& config() const { return config_; }

    // Transport
    TransportState transport() const { return transport_; }
    void set_transport(TransportState ts) { transport_ = ts; }
    uint64_t transport_position() const { return transport_position_; }
    void set_transport_position(uint64_t pos) { transport_position_ = pos; }
    bool loop() const { return transport_ == TransportState::Loop; }
    void set_loop(bool l) { transport_ = l ? TransportState::Loop : TransportState::Playing; }

    AudioGraph& graph() { return graph_; }
    const AudioGraph& graph() const { return graph_; }

    AudioBackend* backend() { return backend_.get(); }
    const AudioBackend* backend() const { return backend_.get(); }

    bool send_message(const EngineMessage& msg);
    bool poll_message(EngineMessage& msg);

    double sample_rate() const { return config_.sample_rate; }
    size_t block_size() const { return config_.block_size; }

    NodeID add_node(std::unique_ptr<AudioNode> node);
    bool remove_node(NodeID id);
    bool connect_nodes(NodeID src, size_t sp, NodeID dst, size_t dp);
    bool disconnect_nodes(NodeID src, size_t sp, NodeID dst, size_t dp);

private:
    EngineConfig config_;
    std::atomic<EngineState> state_{EngineState::Uninitialized};

    std::mutex graph_mutex_;
    AudioGraph graph_;
    RingBuffer<EngineMessage> control_queue_{4096};
    RingBuffer<EngineMessage> reply_queue_{4096};

    std::unique_ptr<AudioBackend> backend_;

    TransportState transport_{TransportState::Stopped};
    std::atomic<uint64_t> transport_position_{0};

    void process_control_messages();
    void apply_pending_mutations();

    static void audio_callback_bridge(float** outputs, float** inputs,
                                      size_t num_frames, void* userdata);
    void audio_callback(float** outputs, float** inputs, size_t num_frames);

    std::unique_ptr<AudioBackend> create_backend(const std::string& type);
};

} // namespace chimera
