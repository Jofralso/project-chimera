#pragma once

#include "audio_graph.h"
#include <memory>
#include <string>
#include <vector>

namespace chimera {

struct PluginInfo {
    std::string path;
    std::string name;
    std::string vendor;
    std::string version;
    uint32_t num_audio_inputs;
    uint32_t num_audio_outputs;
    uint32_t num_params;
};

class PluginLoader {
public:
    PluginLoader() = default;
    ~PluginLoader();

    PluginLoader(const PluginLoader&) = delete;
    PluginLoader& operator=(const PluginLoader&) = delete;

    bool load(const std::string& path);
    void unload();
    bool is_loaded() const { return handle_ != nullptr; }

    const PluginInfo& info() const { return info_; }
    bool info_valid() const { return info_valid_; }

    PluginInfo scan(const std::string& path);

    void* get_symbol(const std::string& name) const;

private:
    void* handle_ = nullptr;
    PluginInfo info_;
    bool info_valid_ = false;
};

class PluginNode : public AudioNode {
public:
    PluginNode(std::unique_ptr<PluginLoader> loader);
    ~PluginNode() override;

    void process(size_t num_frames) override;
    void prepare(double sample_rate, size_t block_size) override;
    std::string node_class() const override {
        return "plugin:" + loader_->info().path;
    }

    void set_param(uint32_t index, float value);
    float get_param(uint32_t index) const;

    const PluginInfo& plugin_info() const { return loader_->info(); }

private:
    std::unique_ptr<PluginLoader> loader_;
    void* plugin_instance_ = nullptr;
};

class PluginHost {
public:
    PluginHost() = default;
    ~PluginHost() = default;

    std::vector<PluginInfo> scan_directory(const std::string& path);
    std::vector<PluginInfo> scan_standard_paths();

    NodeID instantiate(AudioGraph& graph, const std::string& plugin_path);

    bool unload(AudioGraph& graph, NodeID node_id);

    static std::vector<std::string> standard_plugin_paths();

private:
    std::vector<std::unique_ptr<PluginLoader>> loaders_;
};

} // namespace chimera
