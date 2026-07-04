#include "chimera/audio_graph.h"
#include "chimera/plugin_host.h"
#include <cstdio>
#include <cstdlib>

static int failures = 0;

#define TEST(name, expr) do { \
    if (!(expr)) { \
        std::fprintf(stderr, "FAIL: %s (%s:%d)\n", name, __FILE__, __LINE__); \
        failures++; \
    } else { \
        std::printf("PASS: %s\n", name); \
    } \
} while(0)

int main() {
    chimera::PluginHost host;

    auto plugins = host.scan_directory("/nonexistent");
    TEST("scan missing dir returns empty", plugins.empty());

    {
        chimera::PluginLoader loader;
        auto info = loader.scan("chimera_gain_plugin.so");
        TEST("scan gain plugin", loader.info_valid());
        TEST("plugin name is Gain", info.name == "Gain");
        TEST("plugin vendor is Chimera", info.vendor == "Chimera");
        TEST("plugin version is 1.0.0", info.version == "1.0.0");
    }

    {
        chimera::AudioGraph graph;
        chimera::PluginHost local_host;

        auto id = local_host.instantiate(graph, "chimera_gain_plugin.so");
        TEST("instantiate gain plugin", id != 0);

        auto* node = graph.node(id);
        TEST("node exists in graph", node != nullptr);
        TEST("node has 1 input", node->num_inputs() == 1);
        TEST("node has 1 output", node->num_outputs() == 1);

        graph.prepare(48000.0, 256);
        graph.process(256);

        auto* plugin_node = static_cast<chimera::PluginNode*>(node);
        TEST("node is a PluginNode", plugin_node != nullptr);

        if (plugin_node) {
            float val = plugin_node->get_param(0);
            TEST("default gain is 1.0", val == 1.0f);

            plugin_node->set_param(0, 0.5f);
            val = plugin_node->get_param(0);
            TEST("gain set to 0.5", val == 0.5f);
        }
    }

    std::printf("\n%d test(s) failed\n", failures);
    return failures > 0 ? 1 : 0;
}
