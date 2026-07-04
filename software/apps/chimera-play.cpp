#include <chimera/engine.h>
#include <chimera/logger.h>
#include <chimera/nodes/test_tone.h>
#include <chimera/nodes/gain_node.h>
#include <chimera/nodes/sampler_node.h>
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
    uint32_t channels = 2;
    std::string backend = "auto";
    std::string device = "default";
    std::string wav_path;

    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "-d" && i + 1 < argc) {
            duration_s = std::atof(argv[++i]);
        } else if (arg == "-f" && i + 1 < argc) {
            frequency = std::atof(argv[++i]);
        } else if (arg == "-c" && i + 1 < argc) {
            channels = static_cast<uint32_t>(std::atoi(argv[++i]));
            if (channels < 1) channels = 1;
        } else if (arg == "--wav" && i + 1 < argc) {
            wav_path = argv[++i];
        } else if (arg == "--device" && i + 1 < argc) {
            device = argv[++i];
        } else if (arg == "--alsa") {
            backend = "alsa";
        } else if (arg == "--dummy") {
            backend = "dummy";
        } else if (arg == "-h" || arg == "--help") {
            std::printf("Usage: chimera-play [options]\n");
            std::printf("  -d SEC      Duration in seconds (default: 3.0)\n");
            std::printf("  -f HZ       Test tone frequency (default: 440)\n");
            std::printf("  -c CH       Number of output channels (default: 2)\n");
            std::printf("  --wav FILE  Load and play a WAV file via sampler\n");
            std::printf("  --device D  PCM device name (default: default)\n");
            std::printf("  --alsa      Force ALSA backend\n");
            std::printf("  --dummy     Force dummy backend\n");
            return 0;
        }
    }

    chimera::Engine engine;
    chimera::EngineConfig config;
    config.backend = backend;
    config.client_name = "Chimera Play";
    config.audio_device = device;
    config.num_outputs = channels;

    if (!engine.init(config)) {
        std::fprintf(stderr, "Failed to initialize engine\n");
        return 1;
    }

    auto master = std::make_unique<chimera::MasterOutputNode>(channels);
    chimera::NodeID master_id = engine.add_node(std::move(master));

    chimera::NodeID source_id = 0;

    if (!wav_path.empty()) {
        auto sampler = std::make_unique<chimera::SamplerNode>();
        if (!sampler->load_wav(wav_path)) {
            std::fprintf(stderr, "Failed to load WAV: %s\n", wav_path.c_str());
            return 1;
        }
        sampler->set_loop(true);
        sampler->trigger();

        CHIMERA_INFO("Loaded '%s' (%u Hz, %u ch, %lu frames, looping)",
                     wav_path.c_str(), sampler->sample_rate(),
                     sampler->num_channels(),
                     static_cast<unsigned long>(sampler->total_frames()));

        auto gain = std::make_unique<chimera::GainNode>(2);
        gain->set_gain(0.8f);

        auto* sampler_ptr = sampler.get();
        source_id = engine.add_node(std::move(sampler));
        chimera::NodeID gain_id = engine.add_node(std::move(gain));

        uint32_t ch = std::min(static_cast<uint32_t>(sampler_ptr->num_channels()), channels);
        for (uint32_t c = 0; c < ch; ++c) {
            engine.connect_nodes(source_id, c, gain_id, c);
            engine.connect_nodes(gain_id, c, master_id, c);
        }
    } else {
        auto tone = std::make_unique<chimera::TestToneNode>(frequency, 0.5f);
        source_id = engine.add_node(std::move(tone));

        for (uint32_t ch = 0; ch < channels; ++ch) {
            engine.connect_nodes(source_id, 0, master_id, ch);
        }
    }

    if (!engine.start()) {
        std::fprintf(stderr, "Failed to start engine\n");
        return 1;
    }

    CHIMERA_INFO("Playing for %.1f seconds via %s backend",
                 duration_s, engine.backend()->name().c_str());

    std::this_thread::sleep_for(std::chrono::duration<double>(duration_s));

    engine.stop();
    engine.shutdown();

    CHIMERA_INFO("Playback complete");
    return 0;
}
