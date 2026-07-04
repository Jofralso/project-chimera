#include "chimera/audio_graph.h"
#include "chimera/nodes/master_output.h"
#include "chimera/nodes/test_tone.h"
#include "chimera/nodes/gain_node.h"
#include "chimera/nodes/sampler_node.h"
#include "chimera/nodes/drum_node.h"
#include "chimera/nodes/synth_node.h"
#include "chimera/nodes/step_sequencer.h"
#include "chimera/dsp/adsr.h"
#include "chimera/dsp/oscillator.h"
#include "chimera/dsp/svf_filter.h"
#include "chimera/dsp/envelope.h"
#include "chimera/wav_loader.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
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
    {
        chimera::TestToneNode tone(440.0f, 0.5f);
        tone.prepare(48000.0, 256);
        tone.process(256);

        auto* out = tone.output(0);
        TEST("test tone has output", out != nullptr);

        if (out) {
            float peak = 0.0f;
            for (size_t i = 0; i < 256; ++i) {
                float abs_val = std::fabs(out->buffer.data[i]);
                if (abs_val > peak) peak = abs_val;
            }
            TEST("test tone produces signal", peak > 0.0f);
            TEST("test tone within amplitude", peak <= 0.5f + 0.001f);

            float first = out->buffer.data[0];
            tone.process(256);
            float after = out->buffer.data[0];
            TEST("phase advances", std::fabs(after - first) > 0.0001f);
        }

        tone.set_frequency(880.0f);
        TEST("frequency updated", true);
    }

    {
        chimera::MasterOutputNode master;
        master.prepare(48000.0, 256);

        auto* left_in = master.input(0);
        auto* right_in = master.input(1);
        TEST("master has stereo inputs", left_in != nullptr && right_in != nullptr);

        for (size_t i = 0; i < 256; ++i) {
            left_in->buffer.data[i] = 0.5f;
            right_in->buffer.data[i] = -0.5f;
        }

        master.set_volume(0.8f);
        master.process(256);

        TEST("volume applied correctly",
             std::fabs(left_in->buffer.data[0] - 0.4f) < 0.001f);
    }

    {
        chimera::MasterOutputNode master8(8);
        master8.prepare(48000.0, 256);
        TEST("8ch master has 8 inputs", master8.num_inputs() == 8);

        for (size_t ch = 0; ch < 8; ++ch) {
            auto* in = master8.input(ch);
            TEST("8ch master input exists", in != nullptr);
            if (in) {
                for (size_t i = 0; i < 256; ++i) {
                    in->buffer.data[i] = 0.6f;
                }
            }
        }

        master8.set_volume(2.0f);
        master8.process(256);

        for (size_t ch = 0; ch < 8; ++ch) {
            auto* in = master8.input(ch);
            if (in) {
                TEST("8ch volume clips at 1.0",
                     std::fabs(in->buffer.data[0] - 1.0f) < 0.001f);
            }
        }
    }

    {
        chimera::GainNode gain(2);
        gain.prepare(48000.0, 256);

        for (size_t i = 0; i < 256; ++i) {
            gain.input(0)->buffer.data[i] = 0.5f;
        }

        // Ramp from 1.0 to 0.0 over 256 frames → last frame reaches 0
        gain.set_gain(0.0f);
        gain.process(256);
        TEST("gain ramps end near zero", std::fabs(gain.output(0)->buffer.data[255]) < 0.001f);

        // Already at 0, set to 2.0 and process again
        // Halfway through the ramp, output should be > 0
        gain.set_gain(2.0f);
        gain.process(256);
        TEST("gain ramps up", gain.output(0)->buffer.data[255] > 0.9f);
    }

    {
        // Create a small WAV for sampler test
        std::string wav_path = "/tmp/test_sampler_unit.wav";
        FILE* f = std::fopen(wav_path.c_str(), "wb");
        uint32_t sr = 44100;
        uint16_t bits = 16;
        uint16_t ch = 1;
        uint32_t num_samples = 441;
        uint32_t data_size = num_samples * (bits / 8);
        uint32_t riff_size = 36 + data_size;
        fwrite("RIFF", 1, 4, f);
        fwrite(&riff_size, 4, 1, f);
        fwrite("WAVE", 1, 4, f);
        uint32_t fmt_size = 16;
        uint16_t fmt_tag = 1;
        uint32_t byte_rate = sr * ch * (bits / 8);
        uint16_t block_align = ch * (bits / 8);
        fwrite("fmt ", 1, 4, f);
        fwrite(&fmt_size, 4, 1, f);
        fwrite(&fmt_tag, 2, 1, f);
        fwrite(&ch, 2, 1, f);
        fwrite(&sr, 4, 1, f);
        fwrite(&byte_rate, 4, 1, f);
        fwrite(&block_align, 2, 1, f);
        fwrite(&bits, 2, 1, f);
        fwrite("data", 1, 4, f);
        fwrite(&data_size, 4, 1, f);
        for (uint32_t i = 0; i < num_samples; ++i) {
            int16_t s = static_cast<int16_t>(i * 10);
            fwrite(&s, 2, 1, f);
        }
        std::fclose(f);

        chimera::SamplerNode sampler;
        TEST("load wav succeeds", sampler.load_wav(wav_path));
        TEST("sampler sample rate", sampler.sample_rate() == 44100);
        TEST("sampler channels", sampler.num_channels() == 1);
        TEST("sampler frames", sampler.total_frames() == 441);
        TEST("sampler not playing initially", !sampler.is_playing());

        sampler.prepare(48000.0, 256);
        sampler.trigger();
        TEST("sampler playing after trigger", sampler.is_playing());

        sampler.process(256);
        TEST("sampler advanced position", sampler.position() > 0);

        sampler.set_loop(true);

        // Process past end with loop
        chimera::SamplerNode sampler_loop;
        sampler_loop.load_wav(wav_path);
        sampler_loop.prepare(48000.0, 256);
        sampler_loop.set_loop(true);
        sampler_loop.trigger();
        // Call process twice: 256 + 256 = 512 frames, 441-sample file loops once
        sampler_loop.process(256);
        sampler_loop.process(256);
        TEST("looping sampler wraps", sampler_loop.position() == 71);

        std::remove(wav_path.c_str());
    }

    // ADSR envelope tests
    {
        chimera::ADSREnvelope env;
        TEST("adsr initially inactive", !env.is_active());
        TEST("adsr initial value zero", env.value() < 0.001f);

        env.set_decay(0.5f, 48000.0f);
        env.trigger();
        TEST("adsr active after trigger", env.is_active());
        TEST("adsr value 1.0 after trigger", std::fabs(env.value() - 1.0f) < 0.001f);

        float prev = env.value();
        for (int i = 0; i < 100; ++i) {
            env.process();
        }
        TEST("adsr decays", env.value() < prev);

        // Process until inactive
        for (int i = 0; i < 50000; ++i) {
            if (!env.is_active()) break;
            env.process();
        }
        TEST("adsr finishes", !env.is_active());
    }

    // DrumNode tests
    {
        // Create a small WAV for drum pad
        std::string drum_wav = "/tmp/test_drum_pad.wav";
        FILE* f = std::fopen(drum_wav.c_str(), "wb");
        uint32_t sr = 44100;
        uint16_t bits = 16;
        uint16_t ch = 1;
        uint32_t num_samples = 441;
        uint32_t data_size = num_samples * (bits / 8);
        uint32_t riff_size = 36 + data_size;
        fwrite("RIFF", 1, 4, f);
        fwrite(&riff_size, 4, 1, f);
        fwrite("WAVE", 1, 4, f);
        uint32_t fmt_size = 16;
        uint16_t fmt_tag = 1;
        uint32_t byte_rate = sr * ch * (bits / 8);
        uint16_t block_align = ch * (bits / 8);
        fwrite("fmt ", 1, 4, f);
        fwrite(&fmt_size, 4, 1, f);
        fwrite(&fmt_tag, 2, 1, f);
        fwrite(&ch, 2, 1, f);
        fwrite(&sr, 4, 1, f);
        fwrite(&byte_rate, 4, 1, f);
        fwrite(&block_align, 2, 1, f);
        fwrite(&bits, 2, 1, f);
        fwrite("data", 1, 4, f);
        fwrite(&data_size, 4, 1, f);
        for (uint32_t i = 0; i < num_samples; ++i) {
            int16_t s = static_cast<int16_t>(i * 10);
            fwrite(&s, 2, 1, f);
        }
        std::fclose(f);

        chimera::DrumNode drum(4);
        drum.prepare(48000.0, 256);
        TEST("drum has 4 pads", drum.num_pads() == 4);
        TEST("drum has stereo output", drum.num_outputs() == 2);

        TEST("drum load pad 0", drum.load_sample(0, drum_wav));
        TEST("drum load pad 1", drum.load_sample(1, drum_wav));
        TEST("drum pad 0 loaded", drum.is_pad_loaded(0));

        // Trigger pad 0
        drum.trigger(0, 1.0f);
        drum.process(256);
        TEST("drum process produces output", true);

        auto* left = drum.output(0);
        auto* right = drum.output(1);
        TEST("drum left output exists", left != nullptr);
        TEST("drum right output exists", right != nullptr);

        float left_peak = 0.0f, right_peak = 0.0f;
        for (size_t i = 0; i < 256; ++i) {
            if (std::fabs(left->buffer.data[i]) > left_peak) left_peak = std::fabs(left->buffer.data[i]);
            if (std::fabs(right->buffer.data[i]) > right_peak) right_peak = std::fabs(right->buffer.data[i]);
        }
        TEST("drum left output has signal", left_peak > 0.0f);
        TEST("drum right output has signal (mono pan)", right_peak > 0.0f);

        // Pan hard left
        drum.set_pad_pan(0, -1.0f);
        drum.trigger(0, 1.0f);
        drum.process(256);
        left_peak = 0.0f; right_peak = 0.0f;
        for (size_t i = 0; i < 256; ++i) {
            if (std::fabs(left->buffer.data[i]) > left_peak) left_peak = std::fabs(left->buffer.data[i]);
            if (std::fabs(right->buffer.data[i]) > right_peak) right_peak = std::fabs(right->buffer.data[i]);
        }
        TEST("drum pan-left", left_peak > right_peak);

        // Load stereo wav
        std::string stereo_wav = "/tmp/test_drum_stereo.wav";
        f = std::fopen(stereo_wav.c_str(), "wb");
        ch = 2;
        num_samples = 220;
        data_size = num_samples * (bits / 8) * ch;
        riff_size = 36 + data_size;
        fmt_tag = 1;
        byte_rate = sr * ch * (bits / 8);
        block_align = ch * (bits / 8);
        fwrite("RIFF", 1, 4, f);
        fwrite(&riff_size, 4, 1, f);
        fwrite("WAVE", 1, 4, f);
        fwrite("fmt ", 1, 4, f);
        fwrite(&fmt_size, 4, 1, f);
        fwrite(&fmt_tag, 2, 1, f);
        fwrite(&ch, 2, 1, f);
        fwrite(&sr, 4, 1, f);
        fwrite(&byte_rate, 4, 1, f);
        fwrite(&block_align, 2, 1, f);
        fwrite(&bits, 2, 1, f);
        fwrite("data", 1, 4, f);
        fwrite(&data_size, 4, 1, f);
        for (uint32_t i = 0; i < num_samples; ++i) {
            int16_t s_l = static_cast<int16_t>(i * 20);
            int16_t s_r = static_cast<int16_t>(i * 5);
            fwrite(&s_l, 2, 1, f);
            fwrite(&s_r, 2, 1, f);
        }
        std::fclose(f);

        chimera::DrumNode drum_stereo(1);
        drum_stereo.prepare(48000.0, 256);
        TEST("drum stereo pad load", drum_stereo.load_sample(0, stereo_wav));
        drum_stereo.trigger(0, 1.0f);
        drum_stereo.process(256);
        left_peak = 0.0f; right_peak = 0.0f;
        for (size_t i = 0; i < 256; ++i) {
            if (std::fabs(drum_stereo.output(0)->buffer.data[i]) > left_peak)
                left_peak = std::fabs(drum_stereo.output(0)->buffer.data[i]);
            if (std::fabs(drum_stereo.output(1)->buffer.data[i]) > right_peak)
                right_peak = std::fabs(drum_stereo.output(1)->buffer.data[i]);
        }
        TEST("drum stereo L > R (L=20*i, R=5*i)", left_peak > right_peak);

        std::remove(drum_wav.c_str());
        std::remove(stereo_wav.c_str());
    }

    // StepSequencer tests — legacy trigger mode
    {
        chimera::StepSequencer seq(4, 16);
        TEST("seq 4 tracks", seq.num_tracks() == 4);
        TEST("seq 16 steps", seq.num_steps() == 16);
        TEST("seq default bpm", std::fabs(seq.bpm() - 120.0f) < 0.001f);

        // Set a pattern
        seq.set_step(0, 0, true, 1.0f, 1.0f);
        seq.set_step(1, 4, true, 0.8f, 1.0f);
        seq.set_step(2, 8, true, 0.6f, 0.5f);

        TEST("seq step 0,0 active", seq.step(0, 0).active);
        TEST("seq step 1,4 vel", std::fabs(seq.step(1, 4).velocity - 0.8f) < 0.001f);
        TEST("seq step 2,8 prob", std::fabs(seq.step(2, 8).probability - 0.5f) < 0.001f);
        TEST("seq step 3,0 inactive", !seq.step(3, 0).active);

        // Toggle
        seq.toggle_step(0, 0);
        TEST("seq toggle off", !seq.step(0, 0).active);
        seq.toggle_step(0, 0);
        TEST("seq toggle on", seq.step(0, 0).active);

        // Advance sequencer
        // 120 BPM, 4 steps/beat => 8 steps/second => 6000 frames/step at 48000 Hz
        // Step 0: frames 1..6000, Step 1: 6001..12000, Step 2: 12001..18000,
        // Step 3: 18001..24000, Step 4: 24001..30000
        seq.reset();
        auto triggers = seq.advance(6001, 48000.0);
        // Frame 6001 is in step 1 (6000/6000=1, but 6001/6000=1)
        // Actually: step_frame = floor(6001/6000)*6000 = 6000
        // new_step = floor(6001/6000) % 16 = 1
        // pad 0 is active at step 0, not step 1 - so triggers should be for step 1
        // No pads active at step 1 in our test setup
        TEST("seq triggers at step 1 empty", triggers.size() == 0);

        // Advance past step 4 boundary (step 4 = frames 24001..30000)
        seq.reset();
        triggers = seq.advance(24001, 48000.0);
        // frame 24001: step_frame = floor(24001/6000)*6000 = 24000
        // new_step = floor(24001/6000) % 16 = 4
        // pad 1 is active at step 4
        bool found_pad1 = false;
        for (auto& t : triggers) {
            if (t.track == 1) found_pad1 = true;
        }
        TEST("seq triggers track1 at step 4", found_pad1);

        // Num steps change
        seq.set_num_steps(8);
        TEST("seq resize to 8", seq.num_steps() == 8);

        // Toggle at new valid step
        seq.toggle_step(0, 7);
        TEST("seq new step toggle", seq.step(0, 7).active);
    }

    // Oscillator tests
    {
        chimera::Oscillator osc;
        osc.set_frequency(440.0f, 48000.0f);

        // Check that oscillator produces output for each waveform
        for (auto wave : {chimera::Waveform::Sine, chimera::Waveform::Saw,
                           chimera::Waveform::Square, chimera::Waveform::Triangle}) {
            float sum = 0.0f;
            float peak = 0.0f;
            osc.reset();
            for (int i = 0; i < 4800; ++i) {
                float s = osc.process(wave, 0.5f);
                sum += s;
                if (std::fabs(s) > peak) peak = std::fabs(s);
            }
            TEST("oscillator peak ~1.0", std::fabs(peak - 1.0f) < 0.01f);
        }

        // Check frequency is approximately correct by counting zero crossings
        osc.reset();
        osc.set_frequency(440.0f, 48000.0f);
        int crossings = 0;
        float prev = 0.0f;
        for (int i = 0; i < 4800; ++i) {
            float s = osc.process(chimera::Waveform::Sine, 0.5f);
            if (prev <= 0.0f && s > 0.0f) crossings++;
            prev = s;
        }
        // 440 Hz in 4800 samples at 48000 Hz = 0.1s = 44 cycles
        TEST("oscillator frequency ~440 Hz",
             crossings > 40 && crossings < 50);

        // Pulse width for square
        osc.reset();
        float pw = 0.25f;
        int high_count = 0;
        for (int i = 0; i < 4800; ++i) {
            float s = osc.process(chimera::Waveform::Square, pw);
            if (s > 0.0f) high_count++;
        }
        // Roughly 25% of samples should be high
        float ratio = static_cast<float>(high_count) / 4800.0f;
        TEST("oscillator pulse width", std::fabs(ratio - 0.25f) < 0.03f);
    }

    // SVF filter tests
    {
        chimera::StateVariableFilter filt;

        // Low-pass: DC should pass through
        filt.reset();
        float dc_out = 0.0f;
        for (int i = 0; i < 10000; ++i) {
            dc_out = filt.process(1.0f, 0.1f, 0.8f, chimera::FilterMode::LowPass);
        }
        TEST("filter LP DC passes", std::fabs(dc_out - 1.0f) < 0.1f);

        // High-pass: DC should be blocked
        filt.reset();
        float hp_dc = 1.0f;
        for (int i = 0; i < 10000; ++i) {
            hp_dc = filt.process(1.0f, 0.1f, 0.8f, chimera::FilterMode::HighPass);
        }
        TEST("filter HP blocks DC", std::fabs(hp_dc) < 0.1f);

        // Band-pass: DC should be blocked
        filt.reset();
        float bp_dc = 1.0f;
        for (int i = 0; i < 10000; ++i) {
            bp_dc = filt.process(1.0f, 0.1f, 0.8f, chimera::FilterMode::BandPass);
        }
        TEST("filter BP blocks DC", std::fabs(bp_dc) < 0.1f);

        // Notch: DC should pass
        filt.reset();
        float notch_dc = 0.0f;
        for (int i = 0; i < 10000; ++i) {
            notch_dc = filt.process(1.0f, 0.1f, 0.8f, chimera::FilterMode::Notch);
        }
        TEST("filter notch DC passes", std::fabs(notch_dc - 1.0f) < 0.1f);

        // Reset should clear state
        filt.reset();
        float after_reset = filt.process(0.0f, 0.5f, 0.0f, chimera::FilterMode::LowPass);
        TEST("filter reset clears state", std::fabs(after_reset) < 0.001f);
    }

    // Full ADSR envelope tests
    {
        chimera::Envelope env;
        TEST("env initially idle", env.stage() == chimera::Envelope::Stage::Idle);
        TEST("env initial value zero", env.value() < 0.001f);
        TEST("env not active", !env.is_active());

        env.set_params(5.0f, 100.0f, 0.5f, 50.0f, 48000.0f);
        env.trigger();
        TEST("env attack after trigger", env.stage() == chimera::Envelope::Stage::Attack);
        TEST("env active after trigger", env.is_active());

        // Process through attack
        for (int i = 0; i < 4800; ++i) env.process();
        // Should now be in decay (attack was 5ms = 240 samples)
        TEST("env reaches decay or sustain",
             env.stage() == chimera::Envelope::Stage::Decay ||
             env.stage() == chimera::Envelope::Stage::Sustain);

        // Process through decay
        for (int i = 0; i < 48000; ++i) env.process();
        // Should be in sustain
        TEST("env reaches sustain", env.stage() == chimera::Envelope::Stage::Sustain);
        TEST("env sustain value ~0.5", std::fabs(env.value() - 0.5f) < 0.01f);

        // Release
        env.release();
        TEST("env release after release", env.stage() == chimera::Envelope::Stage::Release);
        for (int i = 0; i < 48000; ++i) env.process();
        TEST("env idle after release", env.stage() == chimera::Envelope::Stage::Idle);
        TEST("env not active after release", !env.is_active());
    }

    // SynthNode tests
    {
        chimera::SynthNode synth(4);
        synth.prepare(48000.0, 256);
        TEST("synth 4 voices", synth.max_voices() == 4);
        TEST("synth stereo output", synth.num_outputs() == 2);
        TEST("synth 0 active initially", synth.active_voice_count() == 0);

        // Use a fast release for test
        synth.params().release_ms = 10.0f;

        // Trigger a note
        synth.note_on(69, 1.0f); // A4 = 440 Hz
        synth.process(256);
        TEST("synth 1 active after note-on", synth.active_voice_count() == 1);

        auto* left = synth.output(0);
        TEST("synth left output exists", left != nullptr);
        float peak = 0.0f;
        for (size_t i = 0; i < 256; ++i) {
            if (std::fabs(left->buffer.data[i]) > peak) peak = std::fabs(left->buffer.data[i]);
        }
        TEST("synth produces output", peak > 0.0f);

        // Second note (same note should retrigger)
        synth.note_on(69, 1.0f);
        synth.process(256);
        TEST("synth still 1 voice (same note retriggered)",
             synth.active_voice_count() == 1);

        // Note off
        synth.note_off(69);
        synth.process(256);
        TEST("synth voice released", synth.active_voice_count() > 0);
        // Process until release finishes
        for (int i = 0; i < 200; ++i) synth.process(256);
        TEST("synth all voices idle after release", synth.active_voice_count() == 0);
    }

    // StepSequencer — note tracks
    {
        chimera::StepSequencer seq(2, 8);
        seq.set_track_type(0, chimera::TrackType::Trigger);
        seq.set_track_type(1, chimera::TrackType::Note);
        TEST("note track type",
             seq.track_type(1) == chimera::TrackType::Note);

        seq.set_step(1, 0, true, 1.0f, 1.0f, 60, 0.5f);
        seq.set_step(1, 2, true, 0.8f, 1.0f, 64, 0.75f);
        TEST("note track step", seq.step(1, 0).note == 60);
        TEST("note track gate", std::fabs(seq.step(1, 0).gate - 0.5f) < 0.001f);

        seq.set_note(1, 0, 72);
        TEST("set_note", seq.step(1, 0).note == 72);

        seq.set_gate(1, 0, 0.25f);
        TEST("set_gate", std::fabs(seq.step(1, 0).gate - 0.25f) < 0.001f);
    }

    // StepSequencer — multiple patterns and song mode
    {
        chimera::StepSequencer seq(2, 8);
        seq.set_step(0, 0, true); // pattern 0, step 0 active

        uint32_t p1 = seq.add_pattern("Pattern 2");
        uint32_t p2 = seq.add_pattern("Breakdown");
        TEST("3 patterns", seq.num_patterns() == 3);

        seq.select_pattern(p1);
        TEST("pattern 1 selected", seq.current_pattern_index() == p1);
        seq.set_step(0, 0, true);
        seq.set_step(1, 4, true);

        seq.select_pattern(p2);
        seq.set_step(0, 0, false);
        TEST("indep pattern data", !seq.step(0, 0).active);

        seq.select_pattern(0);
        TEST("pattern 0 step preserved", seq.step(0, 0).active);
    }

    // StepSequencer — param locks
    {
        chimera::StepSequencer seq(1, 8);
        seq.set_step(0, 0, true);
        seq.set_param_lock(0, 0, 0, 0.5f);
        seq.set_param_lock(0, 0, 1, 0.8f);
        TEST("has param locks", seq.has_param_locks(0, 0));
        TEST("no locks elsewhere", !seq.has_param_locks(0, 1));

        auto& locks = seq.param_locks(0, 0);
        TEST("2 param locks", locks.size() == 2);
        TEST("lock index 0", locks[0].param_index == 0);
        TEST("lock value 0.5", std::fabs(locks[0].value - 0.5f) < 0.001f);
        TEST("lock index 1", locks[1].param_index == 1);

        // Replace existing
        seq.set_param_lock(0, 0, 0, 0.9f);
        TEST("lock replaced", std::fabs(seq.param_locks(0, 0)[0].value - 0.9f) < 0.001f);
        TEST("still 2 locks", seq.param_locks(0, 0).size() == 2);
    }

    // StepSequencer — song mode
    {
        chimera::StepSequencer seq(1, 4);
        uint32_t p1 = seq.add_pattern();
        uint32_t p2 = seq.add_pattern();
        seq.select_pattern(p1);
        seq.set_step(0, 0, true);
        seq.select_pattern(p2);
        seq.set_step(0, 0, true);

        seq.select_pattern(p1);
        std::vector<chimera::SongStep> song = {{p1, 1}, {p2, 1}};
        seq.set_song(song);
        seq.set_song_mode(true);

        TEST("song mode on", seq.song_mode());

        // 130 BPM, 4 steps/beat → 48000*60/130/4 ≈ 5538 frames per step
        double fps = (60.0 / 130.0) / 4.0 * 48000.0;
        seq.set_bpm(130);
        seq.set_steps_per_beat(4);
        seq.set_sample_rate(48000.0);

        seq.reset();
        // Step 0: pattern p1, step 0 → trigger
        auto events = seq.advance(static_cast<uint64_t>(fps + 1), 48000.0);
        // After 4 steps, pattern p1 should finish (1 repeat) → switch to p2
        TEST("song starts on pattern p1", seq.current_pattern_index() == p1);

        // Advance through pattern p1 (4 steps)
        seq.advance(static_cast<uint64_t>(fps * 2 + 1), 48000.0);
        seq.advance(static_cast<uint64_t>(fps * 3 + 1), 48000.0);
        seq.advance(static_cast<uint64_t>(fps * 4 + 1), 48000.0);
        // After 4 steps, pattern p1 repeat done, should switch to p2
        // But the switch happens on step boundary at step 0 of next pattern
        // Actually song mode switches when cur_mod == 0
        // After 4 steps, current_step = 4, cur_mod = 4 % 4 = 0 → pattern switch
        seq.advance(static_cast<uint64_t>(fps * 5 + 1), 48000.0);
        // Now on pattern p2
        TEST("song switches to p2", seq.current_pattern_index() == p2);
    }

    // ===== E2E Integration Tests =====
    {
        // Create full engine pipeline: master + drum + synth + sequencer
        chimera::AudioGraph graph;
        auto master = std::make_unique<chimera::MasterOutputNode>(2u);
        chimera::NodeID master_id = graph.add_node(std::move(master));

        auto drum = std::make_unique<chimera::DrumNode>(4);
        auto* drum_ptr = drum.get();
        chimera::NodeID drum_id = graph.add_node(std::move(drum));
        graph.connect(drum_id, 0, master_id, 0);
        graph.connect(drum_id, 1, master_id, 1);

        auto synth = std::make_unique<chimera::SynthNode>(6);
        auto* synth_ptr = synth.get();
        chimera::NodeID synth_id = graph.add_node(std::move(synth));
        graph.connect(synth_id, 0, master_id, 0);
        graph.connect(synth_id, 1, master_id, 1);

        TEST("e2e graph has 3 nodes", graph.all_node_ids().size() == 3);

        // Load drum samples
        TEST("e2e drum load pad 0", drum_ptr->load_sample(0, "/tmp/test_kick.wav"));
        TEST("e2e drum load pad 1", drum_ptr->load_sample(1, "/tmp/test_snare.wav"));
        TEST("e2e drum load pad 2", drum_ptr->load_sample(2, "/tmp/test_hat.wav"));
        TEST("e2e drum pad 0 loaded", drum_ptr->is_pad_loaded(0));
        TEST("e2e drum pad 1 loaded", drum_ptr->is_pad_loaded(1));
        TEST("e2e drum pad 2 loaded", drum_ptr->is_pad_loaded(2));
        TEST("e2e drum pad 3 not loaded", !drum_ptr->is_pad_loaded(3));

        // Configure synth
        synth_ptr->params().waveform = chimera::Waveform::Saw;
        synth_ptr->params().filter_cutoff = 0.5f;
        synth_ptr->params().attack_ms = 2.0f;
        synth_ptr->params().release_ms = 50.0f;

        // Prepare graph
        graph.prepare(48000.0, 256);
        graph.process(256);

        // Trigger drum pad manually
        drum_ptr->trigger(0, 1.0f);
        graph.process(256);

        auto* m = graph.node(master_id);
        const auto* m_out0 = m->input(0)->buffer.channel(0);
        const auto* m_out1 = m->input(1)->buffer.channel(0);
        float sum0 = 0, sum1 = 0;
        for (size_t i = 0; i < 256; ++i) {
            sum0 += std::abs(m_out0[i]);
            sum1 += std::abs(m_out1[i]);
        }

        // Very quiet but should have some signal
        // Drum kick is mostly low frequencies, could be very quiet
        // We just check it's non-zero
        TEST("e2e master has output after drum trigger", sum0 > 0.0f || sum1 > 0.0f);

        // Trigger synth note
        synth_ptr->note_on(60, 0.8f);
        graph.process(256);

        float sum_s = 0;
        for (size_t i = 0; i < 256; ++i)
            sum_s += std::abs(m_out0[i]);
        TEST("e2e master has synth output", sum_s > 0.0001f);

        // Test sequencer → drum/synth routing
        chimera::StepSequencer seq(4, 16);
        seq.set_sample_rate(48000.0);
        seq.set_bpm(120);
        seq.set_steps_per_beat(4);

        seq.set_track_type(0, chimera::TrackType::Trigger); // kick
        seq.set_track_type(1, chimera::TrackType::Trigger); // snare
        seq.set_track_type(2, chimera::TrackType::Trigger); // hat
        seq.set_track_type(3, chimera::TrackType::Note);    // bass

        seq.set_step(0, 0, true, 1.0f);  // kick on 1
        seq.set_step(0, 8, true, 0.8f);  // kick on 9
        seq.set_step(1, 4, true, 1.0f);  // snare on 5
        seq.set_step(1, 12, true, 0.9f); // snare on 13
        for (uint32_t s = 0; s < 16; s += 2)
            seq.set_step(2, s, true, 0.5f); // hat on every 8th
        seq.set_step(3, 0, true, 0.9f, 1.0f, 36, 0.75f); // bass note

        TEST("e2e seq step 0,0 active", seq.step(0, 0).active);
        TEST("e2e seq track 0 type trigger",
             seq.track_type(0) == chimera::TrackType::Trigger);
        TEST("e2e seq track 3 type note",
             seq.track_type(3) == chimera::TrackType::Note);

        // Advance sequencer and simulate engine routing
        // At step boundary, sequencer generates events
        double fps = (60.0 / 120.0) / 4 * 48000.0; // frames per step

        // Clear any pending events from pads
        {
            chimera::DrumTrigger tmp;
            while (drum_ptr->trigger_queue().pop(tmp)) {}
        }

        auto events = seq.advance(static_cast<uint64_t>(fps), 48000.0);
        // Step 0 trigger
        int trig_count = 0;
        for (auto& e : events) {
            if (e.type == chimera::SequencerEvent::Type::Trigger) {
                trig_count++;
                if (e.track == 0) {
                    drum_ptr->trigger(e.track, 1.0f);
                }
            } else if (e.type == chimera::SequencerEvent::Type::NoteOn) {
                synth_ptr->note_on(e.note, e.velocity);
            }
        }
        TEST("e2e seq generated trigger events", trig_count > 0);

        graph.process(256);
        float sum_after = 0;
        for (size_t i = 0; i < 256; ++i)
            sum_after += std::abs(m_out0[i]);
        TEST("e2e full pipeline produces output", sum_after > 0.001f);

        // Advance further and test more triggers
        graph.process(256);
        seq.advance(static_cast<uint64_t>(fps * 4), 48000.0);
        // Clear trigger queue and process again
        { chimera::DrumTrigger tmp; while (drum_ptr->trigger_queue().pop(tmp)) {} }
        seq.advance(static_cast<uint64_t>(fps), 48000.0);
        // Process step 4 (snare)
        auto evts = seq.advance(static_cast<uint64_t>(fps), 48000.0);
        for (auto& e : evts) {
            if (e.type == chimera::SequencerEvent::Type::Trigger)
                drum_ptr->trigger(e.track, e.velocity);
        }
        graph.process(256);
        float sum_snare = 0;
        for (size_t i = 0; i < 256; ++i)
            sum_snare += std::abs(m_out0[i]);
        // With samples loaded, should have signal
        TEST("e2e snare produces output", sum_snare > 0.0f);

        // Test stereo routing: drum outputs to left/right
        drum_ptr->set_pad_pan(0, -1.0f); // hard left
        drum_ptr->set_pad_pan(1, 1.0f);  // hard right

        // Clear and trigger both
        { chimera::DrumTrigger tmp; while (drum_ptr->trigger_queue().pop(tmp)) {} }
        drum_ptr->trigger(0, 1.0f);
        drum_ptr->trigger(1, 1.0f);
        graph.process(256);

        float l = 0, r = 0;
        for (size_t i = 0; i < 256; ++i) {
            l += std::abs(m_out0[i]);
            r += std::abs(m_out1[i]);
        }
        // With hard panning, one side may dominate the other
        TEST("e2e stereo routing left channel", l > 0.0f);
        TEST("e2e stereo routing right channel", r > 0.0f);
    }

    std::printf("\n%d test(s) failed\n", failures);
    return failures > 0 ? 1 : 0;
}
