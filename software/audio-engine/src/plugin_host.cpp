#include "chimera/plugin_host.h"
#include "chimera/logger.h"
#include "chimera/port.h"
#include "chimera/plugin.h"

#include <dlfcn.h>
#include <cstdlib>
#include <cstring>
#include <filesystem>

namespace chimera {

PluginLoader::~PluginLoader() {
    unload();
}

bool PluginLoader::load(const std::string& path) {
    if (handle_) unload();

    handle_ = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle_) {
        CHIMERA_ERROR("Failed to load plugin %s: %s", path.c_str(), dlerror());
        return false;
    }

    info_ = scan(path);
    if (!info_valid_) {
        CHIMERA_WARN("Plugin %s loaded but missing chimera_plugin_get_vtable symbol", path.c_str());
        dlclose(handle_);
        handle_ = nullptr;
        return false;
    }

    CHIMERA_INFO("Loaded plugin: %s v%s by %s", info_.name.c_str(), info_.version.c_str(), info_.vendor.c_str());
    return true;
}

void PluginLoader::unload() {
    if (handle_) {
        dlclose(handle_);
        handle_ = nullptr;
    }
    info_valid_ = false;
}

void* PluginLoader::get_symbol(const std::string& name) const {
    if (!handle_) return nullptr;
    return dlsym(handle_, name.c_str());
}

PluginInfo PluginLoader::scan(const std::string& path) {
    PluginInfo info;
    info.path = path;

    void* scan_handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!scan_handle) return info;

    auto get_vtable = reinterpret_cast<chimera_plugin_get_vtable_fn>(
        dlsym(scan_handle, "chimera_plugin_get_vtable"));

    if (!get_vtable) {
        dlclose(scan_handle);
        return info;
    }

    const auto* vtable = get_vtable();
    if (!vtable || !vtable->create) {
        dlclose(scan_handle);
        return info;
    }

    auto* temp = vtable->create(48000.0, 256);
    if (!temp || !temp->descriptor) {
        dlclose(scan_handle);
        return info;
    }

    const auto* desc = temp->descriptor;
    info.name = desc->name;
    info.vendor = desc->vendor;
    info.version = desc->version;
    info.num_audio_inputs = desc->num_audio_inputs;
    info.num_audio_outputs = desc->num_audio_outputs;
    info.num_params = desc->num_params;

    vtable->destroy(temp);
    dlclose(scan_handle);
    info_valid_ = true;
    return info;
}

PluginNode::PluginNode(std::unique_ptr<PluginLoader> loader)
    : AudioNode(loader->info().name, NodeType::Processor)
    , loader_(std::move(loader))
{
    for (uint32_t i = 0; i < loader_->info().num_audio_inputs; ++i) {
        std::string nm = "In " + std::to_string(i + 1);
        add_input({std::move(nm), PortDirection::Input, PortDataType::Audio});
    }
    for (uint32_t i = 0; i < loader_->info().num_audio_outputs; ++i) {
        std::string nm = "Out " + std::to_string(i + 1);
        add_output({std::move(nm), PortDirection::Output, PortDataType::Audio});
    }
}

PluginNode::~PluginNode() {
    if (plugin_instance_) {
        auto get_vtable = reinterpret_cast<chimera_plugin_get_vtable_fn>(
            loader_->get_symbol("chimera_plugin_get_vtable"));
        if (get_vtable) {
            const auto* vtable = get_vtable();
            if (vtable && vtable->destroy) {
                vtable->destroy(static_cast<ChimeraPlugin*>(plugin_instance_));
            }
        }
    }
}

void PluginNode::prepare(double sample_rate, size_t block_size) {
    AudioNode::prepare(sample_rate, block_size);

    auto get_vtable = reinterpret_cast<chimera_plugin_get_vtable_fn>(
        loader_->get_symbol("chimera_plugin_get_vtable"));
    if (!get_vtable) return;

    const auto* vtable = get_vtable();
    if (!vtable || !vtable->create) return;

    plugin_instance_ = vtable->create(sample_rate, static_cast<uint32_t>(block_size));
    CHIMERA_INFO("Plugin instance created: %s", loader_->info().name.c_str());
}

void PluginNode::process(size_t num_frames) {
    if (!plugin_instance_) return;

    auto get_vtable = reinterpret_cast<chimera_plugin_get_vtable_fn>(
        loader_->get_symbol("chimera_plugin_get_vtable"));
    if (!get_vtable) return;

    const auto* vtable = get_vtable();
    if (!vtable || !vtable->process) return;

    auto* plugin = static_cast<ChimeraPlugin*>(plugin_instance_);
    uint32_t ni = static_cast<uint32_t>(num_inputs());
    uint32_t no = static_cast<uint32_t>(num_outputs());

    std::vector<ChimeraPort> in_ports(ni);
    std::vector<ChimeraPort> out_ports(no);

    for (uint32_t i = 0; i < ni; ++i) {
        in_ports[i].data = input(i)->buffer.data;
        in_ports[i].num_frames = static_cast<uint32_t>(num_frames);
    }
    for (uint32_t i = 0; i < no; ++i) {
        out_ports[i].data = output(i)->buffer.data;
        out_ports[i].num_frames = static_cast<uint32_t>(num_frames);
    }

    vtable->process(plugin, in_ports.data(), ni, out_ports.data(), no,
                    static_cast<uint32_t>(num_frames));
}

void PluginNode::set_param(uint32_t index, float value) {
    if (!plugin_instance_) return;

    auto get_vtable = reinterpret_cast<chimera_plugin_get_vtable_fn>(
        loader_->get_symbol("chimera_plugin_get_vtable"));
    if (!get_vtable) return;

    const auto* vtable = get_vtable();
    if (vtable && vtable->set_param) {
        vtable->set_param(static_cast<ChimeraPlugin*>(plugin_instance_), index, value);
    }
}

float PluginNode::get_param(uint32_t index) const {
    if (!plugin_instance_) return 0.0f;

    auto get_vtable = reinterpret_cast<chimera_plugin_get_vtable_fn>(
        loader_->get_symbol("chimera_plugin_get_vtable"));
    if (!get_vtable) return 0.0f;

    const auto* vtable = get_vtable();
    if (vtable && vtable->get_param) {
        return vtable->get_param(static_cast<ChimeraPlugin*>(plugin_instance_), index);
    }
    return 0.0f;
}

std::vector<std::string> PluginHost::standard_plugin_paths() {
    std::vector<std::string> paths;

    const char* home = std::getenv("HOME");
    if (home) {
        paths.push_back(std::string(home) + "/.chimera/plugins");
    }

    paths.push_back("/usr/lib/chimera/plugins");
    paths.push_back("/usr/local/lib/chimera/plugins");

    const char* chimera_path = std::getenv("CHIMERA_PLUGIN_PATH");
    if (chimera_path) {
        std::string env_path(chimera_path);
        size_t pos = 0;
        while ((pos = env_path.find(':')) != std::string::npos) {
            paths.push_back(env_path.substr(0, pos));
            env_path.erase(0, pos + 1);
        }
        if (!env_path.empty()) {
            paths.push_back(env_path);
        }
    }

    return paths;
}

std::vector<PluginInfo> PluginHost::scan_standard_paths() {
    std::vector<PluginInfo> all;
    for (const auto& p : standard_plugin_paths()) {
        auto found = scan_directory(p);
        all.insert(all.end(), found.begin(), found.end());
    }
    return all;
}

std::vector<PluginInfo> PluginHost::scan_directory(const std::string& path) {
    std::vector<PluginInfo> results;

    if (!std::filesystem::exists(path)) {
        CHIMERA_WARN("Plugin directory does not exist: %s", path.c_str());
        return results;
    }

    for (const auto& entry : std::filesystem::directory_iterator(path)) {
        if (!entry.is_regular_file()) continue;
        auto ext = entry.path().extension().string();
        if (ext != ".so" && ext != ".dll" && ext != ".dylib") continue;

        PluginLoader scanner;
        auto info = scanner.scan(entry.path().string());
        if (scanner.info_valid()) {
            results.push_back(info);
            CHIMERA_INFO("Found plugin: %s (%s)", info.name.c_str(), entry.path().c_str());
        }
    }

    return results;
}

NodeID PluginHost::instantiate(AudioGraph& graph, const std::string& plugin_path) {
    auto loader = std::make_unique<PluginLoader>();
    if (!loader->load(plugin_path)) {
        CHIMERA_ERROR("Failed to instantiate plugin: %s", plugin_path.c_str());
        return 0;
    }

    auto node = std::make_unique<PluginNode>(std::move(loader));
    NodeID id = graph.add_node(std::move(node));
    CHIMERA_INFO("Plugin instantiated as node %u", id);
    return id;
}

bool PluginHost::unload(AudioGraph& graph, NodeID node_id) {
    graph.remove_node(node_id);
    CHIMERA_INFO("Plugin node %u unloaded", node_id);
    return true;
}

} // namespace chimera
