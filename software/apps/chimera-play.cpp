#include <chimera/engine.h>
#include <chimera/logger.h>
#include <chimera/nodes/test_tone.h>
#include <chimera/nodes/gain_node.h>
#include <chimera/nodes/sampler_node.h>
#include <chimera/nodes/drum_node.h>
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
    std::string session_path;
    bool capture_mode = false;
    bool drum_mode = false;
    float drum_bpm = 120.0f;
    std::vector<std::string> drum_samples;

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
        } else if (arg == "--session" && i + 1 < argc) {
            session_path = argv[++i];
        } else if (arg == "--capture") {
            capture_mode = true;
        } else if (arg == "--drum") {
            drum_mode = true;
        } else if (arg == "--drum-bpm" && i + 1 < argc) {
            drum_bpm = std::atof(argv[++i]);
        } else if (arg == "--drum-sample" && i + 1 < argc) {
            drum_samples.push_back(argv[++i]);
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
            std::printf("  --session F Load and play a .chimera session file\n");
            std::printf("  --capture   Audio passthrough (input -> output)\n");
            std::printf("  --drum      Drum machine mode\n");
            std::printf("  --drum-bpm  BPM for drum sequencer (default: 120)\n");
            std::printf("  --drum-sample FILE  Add drum sample (repeatable)\n");
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
    if (capture_mode) config.num_inputs = channels;

    if (!engine.init(config)) {
        std::fprintf(stderr, "Failed to initialize engine\n");
        return 1;
    }

    if (drum_mode) {
        auto master = std::make_unique<chimera::MasterOutputNode>(channels);
        chimera::NodeID master_id = engine.add_node(std::move(master));

        uint32_t num_pads = std::max(static_cast<uint32_t>(drum_samples.size()), 1u);
        auto drum = std::make_unique<chimera::DrumNode>(num_pads);
        auto* drum_ptr = drum.get();
        chimera::NodeID drum_id = engine.add_node(std::move(drum));

        for (uint32_t c = 0; c < channels; ++c) {
            engine.connect_nodes(drum_id, c % 2, master_id, c);
        }

        for (size_t i = 0; i < drum_samples.size(); ++i) {
            if (drum_ptr->load_sample(static_cast<uint32_t>(i), drum_samples[i])) {
                CHIMERA_INFO("Drum pad %zu: loaded %s", i, drum_samples[i].c_str());
                float pan = -1.0f + (2.0f * i / (drum_samples.size() - 1));
                drum_ptr->set_pad_pan(static_cast<uint32_t>(i), pan);
            }
        }

        // If no samples provided, use a test tone generator approach
        if (drum_samples.empty()) {
            CHIMERA_INFO("No drum samples, using built-in test tones");
        }

        // Configure sequencer
        auto& seq = engine.sequencer();
        seq.set_bpm(drum_bpm);
        seq.set_steps_per_beat(4);
        seq.set_num_steps(16);

        // Simple pattern: kick on 1, snare on 5, hihat on every other
        if (num_pads >= 1) seq.set_step(0, 0, true, 1.0f, 1.0f);   // kick
        if (num_pads >= 1) seq.set_step(0, 8, true, 1.0f, 1.0f);   // kick
        if (num_pads >= 2) seq.set_step(1, 4, true, 1.0f, 1.0f);   // snare
        if (num_pads >= 2) seq.set_step(1, 12, true, 1.0f, 1.0f);  // snare
        if (num_pads >= 3) {
            for (uint32_t s = 0; s < 16; s += 2) {
                seq.set_step(2, s, true, 0.6f, 1.0f); // hihat
            }
        }

        engine.enable_sequencer(true);
        engine.set_transport(chimera::TransportState::Playing);

        CHIMERA_INFO("Drum machine: %u pads, %.1f BPM", num_pads, drum_bpm);
    } else if (!session_path.empty()) {
        chimera::Session session;
        if (!session.load(session_path)) {
            std::fprintf(stderr, "Failed to load session: %s\n", session_path.c_str());
            return 1;
        }

        engine.graph().clear();
        if (!session.import_graph(engine.graph(), chimera::create_builtin_node)) {
            std::fprintf(stderr, "Failed to import session graph\n");
            return 1;
        }
        engine.graph().prepare(config.sample_rate, config.block_size);

        CHIMERA_INFO("Loaded session '%s' (%s)", session.name().c_str(), session_path.c_str());
    } else {
        auto master = std::make_unique<chimera::MasterOutputNode>(channels);
        chimera::NodeID master_id = engine.add_node(std::move(master));

        if (capture_mode) {
            auto input = std::make_unique<chimera::AudioInputNode>(channels);
            chimera::NodeID input_id = engine.add_node(std::move(input));

            for (uint32_t ch = 0; ch < channels; ++ch) {
                engine.connect_nodes(input_id, ch, master_id, ch);
            }

            CHIMERA_INFO("Capture passthrough (%u ch)", channels);
        } else if (!wav_path.empty()) {
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
            chimera::NodeID sampler_id = engine.add_node(std::move(sampler));
            chimera::NodeID gain_id = engine.add_node(std::move(gain));

            uint32_t ch = std::min(static_cast<uint32_t>(sampler_ptr->num_channels()), channels);
            for (uint32_t c = 0; c < ch; ++c) {
                engine.connect_nodes(sampler_id, c, gain_id, c);
                engine.connect_nodes(gain_id, c, master_id, c);
            }
        } else {
            auto tone = std::make_unique<chimera::TestToneNode>(frequency, 0.5f);
            chimera::NodeID tone_id = engine.add_node(std::move(tone));

            for (uint32_t ch = 0; ch < channels; ++ch) {
                engine.connect_nodes(tone_id, 0, master_id, ch);
            }

            CHIMERA_INFO("Playing %.1f Hz test tone", frequency);
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
