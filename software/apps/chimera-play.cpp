#include <chimera/engine.h>
#include <chimera/logger.h>
#include <chimera/nodes/test_tone.h>
#include <chimera/nodes/master_output.h>
#include <chimera/nodes/audio_io.h>
#include <chimera/session.h>
#include <chimera/plugin_host.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

int main(int argc, char** argv) {
    chimera::Logger::instance().set_level(chimera::LogLevel::Info);

    double duration_s = 3.0;
    float frequency = 440.0f;
    std::string backend = "auto";
    std::string device = "default";

    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "-d" && i + 1 < argc) {
            duration_s = std::atof(argv[++i]);
        } else if (arg == "-f" && i + 1 < argc) {
            frequency = std::atof(argv[++i]);
        } else if (arg == "--device" && i + 1 < argc) {
            device = argv[++i];
        } else if (arg == "--alsa") {
            backend = "alsa";
        } else if (arg == "--dummy") {
            backend = "dummy";
        } else if (arg == "-h" || arg == "--help") {
            std::printf("Usage: chimera-play [options]\n");
            std::printf("  -d SEC     Duration in seconds (default: 3.0)\n");
            std::printf("  -f HZ      Test tone frequency (default: 440)\n");
            std::printf("  --device D PCM device name (default: default)\n");
            std::printf("  --alsa     Force ALSA backend\n");
            std::printf("  --dummy    Force dummy backend\n");
            return 0;
        }
    }

    chimera::Engine engine;
    chimera::EngineConfig config;
    config.backend = backend;
    config.client_name = "Chimera Play";
    config.audio_device = device;

    if (!engine.init(config)) {
        std::fprintf(stderr, "Failed to initialize engine\n");
        return 1;
    }

    CHIMERA_INFO("Playing %.1f Hz test tone for %.1f seconds via %s backend",
                 frequency, duration_s, engine.backend()->name().c_str());

    auto tone = std::make_unique<chimera::TestToneNode>(frequency, 0.5f);
    auto master = std::make_unique<chimera::MasterOutputNode>();

    chimera::NodeID tone_id = engine.add_node(std::move(tone));
    chimera::NodeID master_id = engine.add_node(std::move(master));

    engine.connect_nodes(tone_id, 0, master_id, 0);
    engine.connect_nodes(tone_id, 0, master_id, 1);

    if (!engine.start()) {
        std::fprintf(stderr, "Failed to start engine\n");
        return 1;
    }

    std::this_thread::sleep_for(std::chrono::duration<double>(duration_s));

    engine.stop();
    engine.shutdown();

    CHIMERA_INFO("Playback complete");
    return 0;
}
