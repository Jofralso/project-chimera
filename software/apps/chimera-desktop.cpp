#include <chimera/engine.h>
#include <chimera/logger.h>
#include <chimera/nodes/drum_node.h>
#include <chimera/nodes/synth_node.h>
#include <chimera/nodes/step_sequencer.h>
#include <chimera/nodes/master_output.h>

#include <chimera/ui/display.h>
#include <chimera/ui/screens/synth_screen.h>
#include <chimera/ui/screens/drum_screen.h>
#include <chimera/ui/screens/sequencer_screen.h>
#include <chimera/ui/screens/mixer_screen.h>
#include <chimera/ui/screens/browser_screen.h>
#include <chimera/ui/midi_handler.h>

#include <cstdio>
#include <cstdlib>
#include <string>

int main(int argc, char** argv) {
    chimera::Logger::instance().set_level(chimera::LogLevel::Info);

    std::string backend = "auto";
    std::string device = "default";
    int window_scale = 2;
    bool touch_mode = false;
    bool help = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "--alsa") backend = "alsa";
        else if (arg == "--jack") backend = "jack";
        else if (arg == "--dummy") backend = "dummy";
        else if (arg == "--scale" && i + 1 < argc) window_scale = std::atoi(argv[++i]);
        else if (arg == "--touch") touch_mode = true;
        else if (arg == "-h" || arg == "--help") help = true;
    }

    if (touch_mode) {
        window_scale = std::max(window_scale, 3);
    }

    if (help) {
        std::printf("Usage: chimera-desktop [options]\n");
        std::printf("  --alsa      Force ALSA backend\n");
        std::printf("  --jack      Force JACK backend\n");
        std::printf("  --dummy     Force dummy backend\n");
        std::printf("  --scale N   Window scale factor (default: 2, touch: 3)\n");
        std::printf("  --touch     Touchscreen mode (larger UI, touch events)\n");
        return 0;
    }

    chimera::Engine engine;
    chimera::EngineConfig config;
    config.backend = backend;
    config.client_name = "Chimera Desktop";
    config.num_outputs = 2;
    config.audio_device = device;

    if (!engine.init(config)) {
        std::fprintf(stderr, "Failed to initialize engine\n");
        return 1;
    }

    // Build default graph: Master + Synth + Drum + Sequencer
    auto master = std::make_unique<chimera::MasterOutputNode>(2u);
    chimera::NodeID master_id = engine.add_node(std::move(master));

    auto synth = std::make_unique<chimera::SynthNode>(6);
    auto* synth_ptr = synth.get();
    chimera::NodeID synth_id = engine.add_node(std::move(synth));
    engine.connect_nodes(synth_id, 0, master_id, 0);
    engine.connect_nodes(synth_id, 1, master_id, 1);

    auto drum = std::make_unique<chimera::DrumNode>(4);
    auto* drum_ptr = drum.get();
    chimera::NodeID drum_id = engine.add_node(std::move(drum));
    engine.connect_nodes(drum_id, 0, master_id, 0);
    engine.connect_nodes(drum_id, 1, master_id, 1);

    synth_ptr->params().waveform = chimera::Waveform::Saw;
    synth_ptr->params().filter_cutoff = 0.7f;
    synth_ptr->params().filter_resonance = 0.3f;
    synth_ptr->params().attack_ms = 5.0f;
    synth_ptr->params().decay_ms = 200.0f;
    synth_ptr->params().sustain = 0.6f;
    synth_ptr->params().release_ms = 300.0f;

    drum_ptr->set_pad_decay(0, 0.5f);
    drum_ptr->set_pad_decay(1, 0.3f);
    drum_ptr->set_pad_decay(2, 0.2f);
    drum_ptr->set_pad_decay(3, 0.4f);

    auto& seq = engine.sequencer();
    seq.set_bpm(130);
    seq.set_steps_per_beat(4);
    seq.set_num_steps(16);

    seq.set_track_type(0, chimera::TrackType::Trigger);
    seq.set_track_type(1, chimera::TrackType::Trigger);
    seq.set_track_type(2, chimera::TrackType::Trigger);
    seq.set_track_type(3, chimera::TrackType::Note);

    seq.set_step(0, 0, true, 1.0f);
    seq.set_step(0, 8, true, 0.9f);
    seq.set_step(1, 4, true, 0.8f);
    seq.set_step(1, 12, true, 0.7f);
    for (uint32_t s = 0; s < 16; s += 2)
        seq.set_step(2, s, true, 0.5f);
    uint8_t bass_notes[] = {36, 36, 43, 36, 38, 38, 45, 38,
                            36, 36, 43, 36, 39, 39, 43, 36};
    for (uint32_t s = 0; s < 16; ++s)
        seq.set_step(3, s, true, 0.8f, 1.0f, bass_notes[s], 0.75f);

    seq.set_sample_rate(48000.0);
    engine.enable_sequencer(true);
    engine.set_transport(chimera::TransportState::Playing);

    CHIMERA_INFO("Desktop: synth + drum + sequencer ready");

    // Initialize UI
    chimera::ui::Display display;
    if (!display.init("Chimera Desktop", window_scale)) {
        std::fprintf(stderr, "Failed to initialize display\n");
        return 1;
    }

    if (display.touch_mode()) {
        CHIMERA_INFO("Touchscreen detected — touch/click UI enabled");
    }

    display.set_engine(&engine);

    // Push screens
    display.push_screen(std::make_unique<chimera::ui::SynthScreen>());
    display.push_screen(std::make_unique<chimera::ui::DrumScreen>());
    display.push_screen(std::make_unique<chimera::ui::SequencerScreen>());
    display.push_screen(std::make_unique<chimera::ui::MixerScreen>());

    auto browser = std::make_unique<chimera::ui::BrowserScreen>();
    browser->set_session_path(".");
    display.push_screen(std::move(browser));

    // MIDI handler — routes events to synth/drum nodes
    chimera::ui::MidiHandler midi;
    midi.init();

    // Poll MIDI events in main loop by overriding the display run
    // We use a simple approach: poll MIDI in the display loop
    // This is done by wrapping the Display run with our own loop

    // Start engine
    if (!engine.start()) {
        std::fprintf(stderr, "Failed to start engine\n");
        return 1;
    }

    CHIMERA_INFO("Desktop running — keys 1-5 for screens, TAB/Shift+TAB to cycle");
    CHIMERA_INFO("MIDI input active, connect a controller!");

    // Run loop: poll MIDI + display
    bool running = true;
    while (running) {
        // Process MIDI events
        chimera::ui::MidiEvent ev;
        while (midi.poll_event(ev)) {
            auto* s = engine.graph().find_node_by_class("builtin.synth");
            auto* d = engine.graph().find_node_by_class("builtin.drum");
            switch (ev.type) {
                case chimera::ui::MidiEvent::Type::NoteOn:
                    if (s && ev.velocity > 0) {
                        reinterpret_cast<chimera::SynthNode*>(s)->note_on(
                            ev.note, ev.velocity / 127.0f);
                    }
                    if (d && ev.note < 4) {
                        reinterpret_cast<chimera::DrumNode*>(d)->trigger(
                            ev.note, ev.velocity / 127.0f);
                    }
                    break;
                case chimera::ui::MidiEvent::Type::NoteOff:
                    if (s) {
                        reinterpret_cast<chimera::SynthNode*>(s)->note_off(ev.note);
                    }
                    break;
                case chimera::ui::MidiEvent::Type::ControlChange:
                    // Map CC 1-4 to synth filter/params
                    if (s && ev.controller >= 1 && ev.controller <= 4) {
                        auto* synth = reinterpret_cast<chimera::SynthNode*>(s);
                        float val = ev.value / 127.0f;
                        switch (ev.controller) {
                            case 1: synth->params().filter_cutoff = val; break;
                            case 2: synth->params().filter_resonance = val; break;
                            case 3: synth->params().attack_ms = val * 500.0f; break;
                            case 4: synth->params().release_ms = val * 500.0f; break;
                        }
                    }
                    // Map CC 10-13 to drum pad params (pad 0-3)
                    if (d && ev.controller >= 10 && ev.controller <= 13) {
                        auto* drum = reinterpret_cast<chimera::DrumNode*>(d);
                        int pad = ev.controller - 10;
                        float val = ev.value / 127.0f;
                        drum->set_pad_level(static_cast<uint32_t>(pad), val);
                    }
                    // Map CC 20-23 to active screen's 4 knobs
                    if (ev.controller >= 20 && ev.controller <= 23) {
                        int knob_idx = ev.controller - 20;
                        float new_val = ev.value / 127.0f;
                        auto* cur = display.current_screen();
                        if (cur) {
                            const auto* k = cur->knobs();
                            if (k && knob_idx < cur->knob_count()) {
                                // Calculate delta from value change
                                float old = k[knob_idx].value;
                                float diff = new_val - old;
                                int delta = static_cast<int>(diff / k[knob_idx].step);
                                if (delta != 0)
                                    cur->on_knob(knob_idx, delta);
                            }
                        }
                    }
                    // Map CC 24-27 to sequencer track step toggle
                    if (ev.controller >= 24 && ev.controller <= 27 && ev.value > 0) {
                        int track = ev.controller - 24;
                        if (display.current_screen() &&
                            display.current_screen()->name() == "SEQUENCER") {
                            // Toggle the current step on that track
                            auto& seq = engine.sequencer();
                            uint32_t step = seq.current_step();
                            bool cur = seq.step(static_cast<uint32_t>(track),
                                               step).active;
                            seq.set_step(static_cast<uint32_t>(track), step,
                                        !cur, 0.8f);
                        }
                    }
                    break;
                default:
                    break;
            }
        }

        // Process display events
        display.run_one_frame();

        // Check for quit
        // (display handles quit internally via SDL_QUIT)
        // We break when display quits
        if (!display.is_running()) {
            running = false;
        }
    }

    midi.shutdown();
    engine.stop();
    engine.shutdown();

    return 0;
}
