#pragma once

#include "audio_graph.h"
#include "audio_node.h"
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace chimera {

struct SessionState {
    std::string name{"Untitled"};
    double sample_rate{48000.0};
    size_t block_size{256};
    float master_volume{1.0f};
    int64_t created_at;
    int64_t modified_at;
    std::vector<uint8_t> graph_data;
};

std::unique_ptr<AudioNode> create_builtin_node(const std::string& node_class);

class Session {
public:
    using AutosaveCallback = std::function<void(const std::string& path)>;

    explicit Session(std::string path = "");
    ~Session();

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    bool save(const std::string& path = "");
    bool load(const std::string& path);

    void set_name(const std::string& name) { state_.name = name; }
    const std::string& name() const { return state_.name; }
    const std::string& path() const { return path_; }

    void set_autosave_interval(std::chrono::seconds interval);
    void set_autosave_callback(AutosaveCallback cb);

    void tick_autosave();

    bool export_graph(AudioGraph& graph);
    bool import_graph(AudioGraph& graph,
                      std::function<std::unique_ptr<AudioNode>(const std::string& node_class)> factory = nullptr);

private:
    SessionState state_;
    std::string path_;
    std::chrono::seconds autosave_interval_{60};
    std::chrono::steady_clock::time_point last_autosave_;
    AutosaveCallback autosave_callback_;

    bool write_to_file(const std::string& path);
    bool read_from_file(const std::string& path);
};

} // namespace chimera
