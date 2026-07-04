#pragma once

#include "../audio_node.h"
#include "../ring_buffer.h"
#include "../dsp/oscillator.h"
#include "../dsp/svf_filter.h"
#include "../dsp/envelope.h"
#include <atomic>
#include <vector>

namespace chimera {

struct NoteEvent {
    enum class Type : uint8_t { NoteOn, NoteOff };
    Type type;
    uint8_t note;
    float velocity;
};

struct SynthVoice {
    bool active = false;
    uint8_t note = 69;
    float velocity = 1.0f;
    float note_freq = 440.0f;

    Oscillator osc;
    StateVariableFilter filter;
    Envelope amp_env;
    Envelope filter_env;

    void trigger(uint8_t n, float vel, float sample_rate);
    void release();
    float process(float sample_rate, Waveform wave, float pw,
                  FilterMode fm, float cutoff, float res, float filter_env_amount,
                  float filter_keytrack);
};

struct SynthParams {
    float volume = 0.8f;

    Waveform waveform = Waveform::Saw;
    float pulse_width = 0.5f;
    float detune_cents = 0.0f;

    FilterMode filter_mode = FilterMode::LowPass;
    float filter_cutoff = 0.8f;
    float filter_resonance = 0.0f;
    float filter_env_amount = 0.3f;
    float filter_keytrack = 0.0f;

    float attack_ms = 10.0f;
    float decay_ms = 300.0f;
    float sustain = 0.7f;
    float release_ms = 500.0f;

    float filter_attack_ms = 10.0f;
    float filter_decay_ms = 300.0f;
    float filter_sustain = 0.5f;
    float filter_release_ms = 500.0f;
};

class SynthNode : public AudioNode {
public:
    explicit SynthNode(uint8_t max_voices = 8);

    void process(size_t num_frames) override;
    void prepare(double sample_rate, size_t block_size) override;
    void release() override;
    std::string node_class() const override { return "builtin.synth"; }

    void note_on(uint8_t note, float velocity = 1.0f);
    void note_off(uint8_t note);
    void all_notes_off();

    SynthParams& params() { return params_; }
    const SynthParams& params() const { return params_; }

    RingBuffer<NoteEvent>& note_queue() { return note_queue_; }

    uint8_t max_voices() const { return max_voices_; }
    uint8_t active_voice_count() const;

    static float midi_note_to_freq(uint8_t note);

private:
    uint8_t max_voices_;
    std::vector<SynthVoice> voices_;
    SynthParams params_;
    RingBuffer<NoteEvent> note_queue_{256};
    double sample_rate_{48000.0};

    int find_voice_for_note(uint8_t note);
    int find_free_or_steal_voice();
};

} // namespace chimera
