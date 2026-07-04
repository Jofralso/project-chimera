#include "chimera/engine.h"
#include "chimera/backends/dummy_backend.h"
#include "chimera/backends/jack_backend.h"
#include "chimera/logger.h"

#ifdef CHIMERA_HAS_ALSA
#include "chimera/backends/alsa_backend.h"
#endif

#include "chimera/nodes/drum_node.h"
#include "chimera/nodes/synth_node.h"
#include <cstring>
#include <mutex>

namespace chimera {

Engine::Engine() = default;

Engine::~Engine() {
    shutdown();
}

bool Engine::init(const EngineConfig& config) {
    config_ = config;

    std::string backend_type = config_.backend;
    if (backend_type == "auto") {
#ifdef CHIMERA_HAS_ALSA
        backend_type = "alsa";
#else
        backend_type = "dummy";
#endif
    }

    backend_ = create_backend(backend_type);
    if (!backend_) {
        CHIMERA_ERROR("Failed to create audio backend: %s", backend_type.c_str());
        state_.store(EngineState::Error);
        return false;
    }

    backend_->set_process_callback(audio_callback_bridge, this);

    if (!backend_->init(config_)) {
        CHIMERA_ERROR("Failed to initialize audio backend");
        state_.store(EngineState::Error);
        return false;
    }

    bool ok = graph_.prepare(config.sample_rate, config.block_size);
    if (!ok) {
        state_.store(EngineState::Error);
        return false;
    }

    state_.store(EngineState::Idle);
    CHIMERA_INFO("Engine initialized, backend: %s", backend_->name().c_str());
    return true;
}

void Engine::shutdown() {
    stop();
    if (backend_) {
        backend_->shutdown();
        backend_.reset();
    }
    state_.store(EngineState::Uninitialized);
}

bool Engine::start() {
    if (state_.load() != EngineState::Idle) {
        return false;
    }

    if (!backend_ || !backend_->start()) {
        state_.store(EngineState::Error);
        return false;
    }

    state_.store(EngineState::Running);
    CHIMERA_INFO("Engine started");
    return true;
}

void Engine::stop() {
    if (state_.load() != EngineState::Running) return;

    if (backend_) {
        backend_->stop();
    }

    EngineMessage msg;
    msg.type = EngineMessage::Type::Quit;
    send_message(msg);

    state_.store(EngineState::Idle);
    transport_ = TransportState::Stopped;
    transport_position_ = 0;
    CHIMERA_INFO("Engine stopped");
}

void Engine::pause() {
    if (state_.load() != EngineState::Running) return;
    state_.store(EngineState::Paused);
    transport_ = TransportState::Paused;
    CHIMERA_INFO("Engine paused");
}

void Engine::resume() {
    if (state_.load() != EngineState::Paused) return;
    state_.store(EngineState::Running);
    transport_ = TransportState::Playing;
    CHIMERA_INFO("Engine resumed");
}

bool Engine::send_message(const EngineMessage& msg) {
    return control_queue_.push(msg);
}

bool Engine::poll_message(EngineMessage& msg) {
    return reply_queue_.pop(msg);
}

NodeID Engine::add_node(std::unique_ptr<AudioNode> node) {
    std::lock_guard<std::mutex> lock(graph_mutex_);
    node->prepare(config_.sample_rate, config_.block_size);
    return graph_.add_node(std::move(node));
}

bool Engine::remove_node(NodeID id) {
    if (state_.load() == EngineState::Running) {
        EngineMessage msg;
        msg.type = EngineMessage::Type::RemoveNode;
        msg.node_id = id;
        return send_message(msg);
    }
    std::lock_guard<std::mutex> lock(graph_mutex_);
    graph_.remove_node(id);
    return true;
}

bool Engine::connect_nodes(NodeID src, size_t sp, NodeID dst, size_t dp) {
    if (state_.load() == EngineState::Running) {
        EngineMessage msg;
        msg.type = EngineMessage::Type::ConnectNodes;
        msg.node_id = src;
        msg.target_node_id = dst;
        msg.source_port = sp;
        msg.target_port = dp;
        return send_message(msg);
    }
    std::lock_guard<std::mutex> lock(graph_mutex_);
    return graph_.connect(src, sp, dst, dp);
}

bool Engine::disconnect_nodes(NodeID src, size_t sp, NodeID dst, size_t dp) {
    if (state_.load() == EngineState::Running) {
        EngineMessage msg;
        msg.type = EngineMessage::Type::DisconnectNodes;
        msg.node_id = src;
        msg.target_node_id = dst;
        msg.source_port = sp;
        msg.target_port = dp;
        return send_message(msg);
    }
    std::lock_guard<std::mutex> lock(graph_mutex_);
    return graph_.disconnect(src, sp, dst, dp);
}

void Engine::process_control_messages() {
    EngineMessage msg;
    while (control_queue_.pop(msg)) {
        switch (msg.type) {
            case EngineMessage::Type::Quit:
                return;
            case EngineMessage::Type::SetParam: {
                auto* n = graph_.node(msg.node_id);
                if (n && msg.param_index < n->num_inputs()) {
                    // passthrough: param value written to msg for custom handling
                }
                break;
            }
            case EngineMessage::Type::RemoveNode:
            case EngineMessage::Type::ConnectNodes:
            case EngineMessage::Type::DisconnectNodes:
                break;
            default:
                break;
        }
        reply_queue_.push(msg);
    }
}

void Engine::apply_pending_mutations() {
    EngineMessage msg;
    while (control_queue_.pop(msg)) {
        switch (msg.type) {
            case EngineMessage::Type::Quit:
                return;
            case EngineMessage::Type::ConnectNodes:
                graph_.connect(msg.node_id, msg.source_port,
                               msg.target_node_id, msg.target_port);
                break;
            case EngineMessage::Type::DisconnectNodes:
                graph_.disconnect(msg.node_id, msg.source_port,
                                  msg.target_node_id, msg.target_port);
                break;
            case EngineMessage::Type::RemoveNode:
                graph_.remove_node(msg.node_id);
                break;
            case EngineMessage::Type::SetParam:
                break;
            default:
                break;
        }
        reply_queue_.push(msg);
    }
}

void Engine::audio_callback_bridge(float** outputs, float** inputs,
                                   size_t num_frames, void* userdata)
{
    auto* engine = static_cast<Engine*>(userdata);
    engine->audio_callback(outputs, inputs, num_frames);
}

void Engine::audio_callback(float** outputs, float** inputs, size_t num_frames) {
    if (state_.load() != EngineState::Running) {
        if (outputs) {
            for (size_t ch = 0; ch < config_.num_outputs; ++ch) {
                if (outputs[ch]) std::memset(outputs[ch], 0, num_frames * sizeof(float));
            }
        }
        return;
    }

    std::lock_guard<std::mutex> lock(graph_mutex_);
    apply_pending_mutations();

    // Advance step sequencer and route events to DrumNode/SynthNode
    if (sequencer_enabled_ && transport_ == TransportState::Playing) {
        transport_position_ += num_frames;
        auto events = sequencer_.advance(transport_position_, config_.sample_rate);
        if (!events.empty()) {
            for (auto& [id, node] : graph_.all_nodes()) {
                if (node->node_class() == "builtin.drum") {
                    auto* drum = static_cast<DrumNode*>(node.get());
                    for (auto& e : events) {
                        if (e.type == SequencerEvent::Type::Trigger) {
                            drum->trigger(e.track, e.velocity);
                        }
                    }
                }
                if (node->node_class() == "builtin.synth") {
                    auto* synth = static_cast<SynthNode*>(node.get());
                    for (auto& e : events) {
                        if (e.type == SequencerEvent::Type::NoteOn) {
                            synth->note_on(e.note, e.velocity);
                        } else if (e.type == SequencerEvent::Type::NoteOff) {
                            synth->note_off(e.note);
                        }
                    }
                }
            }
        }
    }

    // Push capture input into AudioInputNode buffers
    for (auto& [id, node] : graph_.all_nodes()) {
        if (node->node_class() == "builtin.audio_input" && inputs) {
            size_t ch = 0;
            for (size_t c = 0; c < node->num_outputs() && c < config_.num_inputs; ++c) {
                if (inputs[c] && node->output(c) && node->output(c)->buffer.data) {
                    std::memcpy(node->output(c)->buffer.data, inputs[c],
                                num_frames * sizeof(float));
                    ch++;
                }
            }
        }
    }

    graph_.process(num_frames);

    // Route MasterOutputNode processed audio to backend outputs
    auto master = graph_.find_node_by_class("builtin.master_output");
    if (master && outputs) {
        for (size_t ch = 0; ch < config_.num_outputs; ++ch) {
            if (outputs[ch]) {
                if (ch < master->num_inputs() && master->input(ch) && master->input(ch)->buffer.data) {
                    std::memcpy(outputs[ch], master->input(ch)->buffer.data,
                                num_frames * sizeof(float));
                } else {
                    std::memset(outputs[ch], 0, num_frames * sizeof(float));
                }
            }
        }
    } else if (outputs) {
        for (size_t ch = 0; ch < config_.num_outputs; ++ch) {
            if (outputs[ch]) std::memset(outputs[ch], 0, num_frames * sizeof(float));
        }
    }
}

std::unique_ptr<AudioBackend> Engine::create_backend(const std::string& type) {
    if (type == "dummy") {
        return std::make_unique<DummyBackend>();
    }
    if (type == "jack") {
        return std::make_unique<JackBackend>();
    }
#ifdef CHIMERA_HAS_ALSA
    if (type == "alsa") {
        return std::make_unique<AlsaBackend>();
    }
#endif
    CHIMERA_WARN("Unknown backend type '%s', falling back to dummy", type.c_str());
    return std::make_unique<DummyBackend>();
}

} // namespace chimera
