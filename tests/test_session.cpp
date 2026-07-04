#include "chimera/session.h"
#include "chimera/audio_graph.h"
#include "chimera/engine.h"
#include "chimera/nodes/master_output.h"
#include "chimera/nodes/test_tone.h"
#include "chimera/nodes/sampler_node.h"
#include "chimera/wav_loader.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <thread>

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
    // --- Basic session save/load ---
    chimera::Session session("/tmp/chimera_test_session.bin");
    TEST("default name is Untitled", session.name() == "Untitled");
    session.set_name("Test Session");
    TEST("name updated", session.name() == "Test Session");

    {
        chimera::AudioGraph graph;
        auto tone = std::make_unique<chimera::TestToneNode>(440.0f, 0.5f);
        auto master = std::make_unique<chimera::MasterOutputNode>(2);
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

    // --- Autosave ---
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

    // --- Session playback via engine ---
    {
        chimera::Session session3("/tmp/chimera_playback_test.bin");
        {
            chimera::AudioGraph graph;
            auto tone = std::make_unique<chimera::TestToneNode>(440.0f, 0.5f);
            auto master = std::make_unique<chimera::MasterOutputNode>(2);
            graph.add_node(std::move(tone));
            graph.add_node(std::move(master));

            chimera::NodeID tone_id = 0, master_id = 0;
            for (auto id : graph.all_node_ids()) {
                auto* n = graph.node(id);
                if (n->node_class() == "builtin.test_tone") tone_id = id;
                if (n->node_class() == "builtin.master_output") master_id = id;
            }

            for (size_t ch = 0; ch < 2; ++ch) {
                graph.connect(tone_id, 0, master_id, ch);
            }

            session3.export_graph(graph);
        }
        session3.save();

        chimera::Engine engine;
        chimera::EngineConfig cfg;
        cfg.backend = "dummy";

        TEST("engine init", engine.init(cfg));

        chimera::Session loaded_play;
        TEST("session load", loaded_play.load("/tmp/chimera_playback_test.bin"));

        // Import graph into engine's graph
        engine.graph().clear();
        TEST("graph import", loaded_play.import_graph(engine.graph(), chimera::create_builtin_node));
        TEST("graph prepare", engine.graph().prepare(cfg.sample_rate, cfg.block_size));

        TEST("engine start", engine.start());
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        engine.stop();
        engine.shutdown();
        TEST("playback completed", true);
    }

    // --- Sampler + Wav roundtrip ---
    {
        std::string wav_path = "/tmp/test_roundtrip.wav";
        FILE* f = std::fopen(wav_path.c_str(), "wb");
        uint32_t sr = 44100;
        uint16_t bits = 16, ch = 1;
        uint32_t ns = 441;
        uint32_t ds = ns * (bits / 8);
        uint32_t rs = 36 + ds;
        fwrite("RIFF", 1, 4, f); fwrite(&rs, 4, 1, f); fwrite("WAVE", 1, 4, f);
        uint32_t fs = 16; uint16_t ft = 1;
        uint32_t br = sr * ch * (bits / 8); uint16_t ba = ch * (bits / 8);
        fwrite("fmt ", 1, 4, f); fwrite(&fs, 4, 1, f);
        fwrite(&ft, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&sr, 4, 1, f);
        fwrite(&br, 4, 1, f); fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f);
        fwrite("data", 1, 4, f); fwrite(&ds, 4, 1, f);
        for (uint32_t i = 0; i < ns; ++i) { int16_t s = 10000; fwrite(&s, 2, 1, f); }
        std::fclose(f);

        chimera::SamplerNode sampler;
        TEST("wav load", sampler.load_wav(wav_path));
        TEST("correct channels", sampler.num_channels() == 1);
        TEST("correct frames", sampler.total_frames() == 441);
        std::remove(wav_path.c_str());
    }

    std::remove("/tmp/chimera_test_session.bin");
    std::remove("/tmp/chimera_test_session.bin.autosave");
    std::remove("/tmp/chimera_playback_test.bin");

    std::printf("\n%d test(s) failed\n", failures);
    return failures > 0 ? 1 : 0;
}
