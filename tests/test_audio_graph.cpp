#include "chimera/audio_graph.h"
#include "chimera/nodes/master_output.h"
#include "chimera/nodes/test_tone.h"
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
    chimera::AudioGraph graph;

    auto tone = std::make_unique<chimera::TestToneNode>(440.0f, 0.5f);
    auto master = std::make_unique<chimera::MasterOutputNode>();

    chimera::NodeID tone_id = graph.add_node(std::move(tone));
    chimera::NodeID master_id = graph.add_node(std::move(master));

    TEST("add_node returns valid IDs", tone_id > 0 && master_id > 0);
    TEST("node lookup works", graph.node(tone_id) != nullptr);
    TEST("master node lookup works", graph.node(master_id) != nullptr);

    TEST("connect returns true", graph.connect(tone_id, 0, master_id, 0));
    TEST("connect returns true for right channel", graph.connect(tone_id, 0, master_id, 1));

    TEST("prepare succeeds", graph.prepare(48000.0, 256));

    graph.process(256);

    auto* master_node = static_cast<chimera::MasterOutputNode*>(graph.node(master_id));
    TEST("master node is accessible", master_node != nullptr);

    const auto& conns = graph.connections();
    TEST("connections recorded", conns.size() == 2);

    auto order = graph.processing_order();
    TEST("processing order has nodes", order.size() > 0);

    graph.remove_node(tone_id);
    TEST("node removed", graph.node(tone_id) == nullptr);

    graph.clear();
    TEST("graph cleared", graph.node(master_id) == nullptr);

    std::printf("\n%d test(s) failed\n", failures);
    return failures > 0 ? 1 : 0;
}
