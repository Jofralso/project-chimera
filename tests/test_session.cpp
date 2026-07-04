#include "chimera/session.h"
#include "chimera/audio_graph.h"
#include "chimera/nodes/master_output.h"
#include "chimera/nodes/test_tone.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

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
    chimera::Session session("/tmp/chimera_test_session.bin");

    TEST("default name is Untitled", session.name() == "Untitled");

    session.set_name("Test Session");
    TEST("name updated", session.name() == "Test Session");

    {
        chimera::AudioGraph graph;
        auto tone = std::make_unique<chimera::TestToneNode>(440.0f, 0.5f);
        auto master = std::make_unique<chimera::MasterOutputNode>();
        graph.add_node(std::move(tone));
        graph.add_node(std::move(master));

        TEST("export graph succeeds", session.export_graph(graph));
    }

    TEST("save succeeds", session.save());

    {
        chimera::Session loaded;
        TEST("load succeeds", loaded.load("/tmp/chimera_test_session.bin"));
        TEST("loaded name matches", loaded.name() == "Test Session");

        chimera::AudioGraph restored;
        TEST("import graph succeeds", loaded.import_graph(restored));
        auto ids = restored.all_node_ids();
        TEST("graph has 2 nodes", ids.size() == 2);

        auto* n1 = restored.node(ids[0]);
        auto* n2 = restored.node(ids[1]);
        TEST("both nodes valid", n1 != nullptr && n2 != nullptr);
    }

    {
        chimera::Session session2("/tmp/chimera_test_autosave.bin");
        session2.set_autosave_interval(std::chrono::seconds(0));
        int cb_called = 0;
        session2.set_autosave_callback([&](const std::string&) {
            cb_called++;
        });
        session2.tick_autosave();
        TEST("autosave callback invoked", cb_called == 1);
    }

    std::remove("/tmp/chimera_test_session.bin");
    std::remove("/tmp/chimera_test_session.bin.autosave");

    std::printf("\n%d test(s) failed\n", failures);
    return failures > 0 ? 1 : 0;
}
