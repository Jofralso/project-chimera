#include "chimera/session.h"
#include "chimera/logger.h"
#include "chimera/nodes/master_output.h"
#include "chimera/nodes/test_tone.h"
#include "chimera/nodes/audio_io.h"
#include "chimera/nodes/gain_node.h"
#include "chimera/nodes/sampler_node.h"

#include <chrono>
#include <cstring>
#include <fstream>
#include <sstream>

namespace chimera {

std::unique_ptr<AudioNode> create_builtin_node(const std::string& node_class) {
    if (node_class == "builtin.master_output") {
        return std::make_unique<MasterOutputNode>(0);
    }
    if (node_class == "builtin.test_tone") {
        return std::make_unique<TestToneNode>();
    }
    if (node_class == "builtin.audio_input") {
        return std::make_unique<AudioInputNode>(0);
    }
    if (node_class == "builtin.audio_output") {
        return std::make_unique<AudioOutputNode>(0);
    }
    if (node_class == "builtin.gain") {
        return std::make_unique<GainNode>(0);
    }
    if (node_class == "builtin.sampler") {
        return std::make_unique<SamplerNode>();
    }
    if (node_class.rfind("plugin:", 0) == 0) {
        return nullptr;
    }
    CHIMERA_WARN("Unknown node class: %s", node_class.c_str());
    return nullptr;
}

Session::Session(std::string path)
    : path_(std::move(path))
    , last_autosave_(std::chrono::steady_clock::now())
{
    state_.created_at = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());
    state_.modified_at = state_.created_at;
}

Session::~Session() = default;

bool Session::save(const std::string& path) {
    std::string save_path = path.empty() ? path_ : path;
    if (save_path.empty()) return false;

    state_.modified_at = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());

    return write_to_file(save_path);
}

bool Session::load(const std::string& path) {
    if (path.empty()) return false;
    path_ = path;
    return read_from_file(path);
}

void Session::set_autosave_interval(std::chrono::seconds interval) {
    autosave_interval_ = interval;
}

void Session::set_autosave_callback(AutosaveCallback cb) {
    autosave_callback_ = std::move(cb);
}

void Session::tick_autosave() {
    auto now = std::chrono::steady_clock::now();
    if (now - last_autosave_ < autosave_interval_) return;
    last_autosave_ = now;

    if (!path_.empty()) {
        std::string autosave_path = path_ + ".autosave";
        if (write_to_file(autosave_path) && autosave_callback_) {
            autosave_callback_(autosave_path);
        }
    }
}

bool Session::export_graph(AudioGraph& graph) {
    state_.graph_data.clear();
    return graph.serialize(state_.graph_data);
}

bool Session::import_graph(AudioGraph& graph,
    std::function<std::unique_ptr<AudioNode>(const std::string&)> factory)
{
    if (!factory) {
        factory = create_builtin_node;
    }
    return graph.deserialize(state_.graph_data, factory);
}

bool Session::write_to_file(const std::string& path) {
    std::ofstream file(path, std::ios::binary);
    if (!file) return false;

    uint32_t magic = 0x4348494D;
    uint32_t version = 2;

    file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    file.write(reinterpret_cast<const char*>(&version), sizeof(version));

    uint32_t name_len = static_cast<uint32_t>(state_.name.size());
    file.write(reinterpret_cast<const char*>(&name_len), sizeof(name_len));
    file.write(state_.name.data(), state_.name.size());

    file.write(reinterpret_cast<const char*>(&state_.sample_rate), sizeof(state_.sample_rate));
    file.write(reinterpret_cast<const char*>(&state_.block_size), sizeof(state_.block_size));
    file.write(reinterpret_cast<const char*>(&state_.master_volume), sizeof(state_.master_volume));

    uint64_t created = static_cast<uint64_t>(state_.created_at);
    uint64_t modified = static_cast<uint64_t>(state_.modified_at);
    file.write(reinterpret_cast<const char*>(&created), sizeof(created));
    file.write(reinterpret_cast<const char*>(&modified), sizeof(modified));

    uint32_t graph_size = static_cast<uint32_t>(state_.graph_data.size());
    file.write(reinterpret_cast<const char*>(&graph_size), sizeof(graph_size));
    if (graph_size > 0) {
        file.write(reinterpret_cast<const char*>(state_.graph_data.data()), graph_size);
    }

    return file.good();
}

bool Session::read_from_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;

    uint32_t magic, version;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    file.read(reinterpret_cast<char*>(&version), sizeof(version));

    if (magic != 0x4348494D) return false;

    uint32_t name_len;
    file.read(reinterpret_cast<char*>(&name_len), sizeof(name_len));
    state_.name.resize(name_len);
    file.read(state_.name.data(), name_len);

    file.read(reinterpret_cast<char*>(&state_.sample_rate), sizeof(state_.sample_rate));
    file.read(reinterpret_cast<char*>(&state_.block_size), sizeof(state_.block_size));
    file.read(reinterpret_cast<char*>(&state_.master_volume), sizeof(state_.master_volume));

    if (version >= 2) {
        uint64_t created, modified;
        file.read(reinterpret_cast<char*>(&created), sizeof(created));
        file.read(reinterpret_cast<char*>(&modified), sizeof(modified));
        state_.created_at = static_cast<int64_t>(created);
        state_.modified_at = static_cast<int64_t>(modified);
    }

    uint32_t graph_size;
    file.read(reinterpret_cast<char*>(&graph_size), sizeof(graph_size));
    state_.graph_data.resize(graph_size);
    if (graph_size > 0) {
        file.read(reinterpret_cast<char*>(state_.graph_data.data()), graph_size);
    }

    path_ = path;
    return file.good();
}

} // namespace chimera
