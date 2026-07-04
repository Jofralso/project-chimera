#include "chimera/nodes/synth_node.h"
#include <cstring>
#include <cmath>

namespace chimera {

// SynthVoice implementation
void SynthVoice::trigger(uint8_t n, float vel, float sample_rate) {
    note = n;
    velocity = vel;
    note_freq = SynthNode::midi_note_to_freq(n);
    osc.reset();
    filter.reset();
    amp_env.trigger();
    filter_env.trigger();
    active = true;
}

void SynthVoice::release() {
    amp_env.release();
    filter_env.release();
}

float SynthVoice::process(float sample_rate, Waveform wave, float pw,
                           FilterMode fm, float cutoff, float res,
                           float filter_env_amount, float filter_keytrack) {
    if (!active) return 0.0f;

    float env_amp = amp_env.process();
    float env_fil = filter_env.process();

    if (amp_env.stage() == Envelope::Stage::Idle) {
        active = false;
        return 0.0f;
    }

    float raw = osc.process(wave, pw);

    // Keytrack: add note to cutoff (normalized)
    float key_offset = filter_keytrack * (note - 69) / 69.0f;
    float mod_cutoff = cutoff + filter_env_amount * env_fil + key_offset;
    mod_cutoff = std::max(0.0f, std::min(0.999f, mod_cutoff));

    float filtered = filter.process(raw, mod_cutoff, res, fm);

    return filtered * env_amp * velocity;
}

// SynthNode implementation
SynthNode::SynthNode(uint8_t max_voices)
    : AudioNode("Synth", NodeType::Source)
    , max_voices_(max_voices)
{
    voices_.resize(max_voices_);
    add_output({"Left", PortDirection::Output, PortDataType::Audio});
    add_output({"Right", PortDirection::Output, PortDataType::Audio});
}

void SynthNode::prepare(double sample_rate, size_t block_size) {
    AudioNode::prepare(sample_rate, block_size);
    sample_rate_ = sample_rate;
}

void SynthNode::release() {
    for (auto& v : voices_) {
        v.active = false;
        v.amp_env.reset();
        v.filter_env.reset();
        v.osc.reset();
        v.filter.reset();
    }
    AudioNode::release();
}

void SynthNode::note_on(uint8_t note, float velocity) {
    note_queue_.push({NoteEvent::Type::NoteOn, note, velocity});
}

void SynthNode::note_off(uint8_t note) {
    note_queue_.push({NoteEvent::Type::NoteOff, note, 0.0f});
}

void SynthNode::all_notes_off() {
    for (auto& v : voices_) {
        v.release();
    }
}

uint8_t SynthNode::active_voice_count() const {
    uint8_t count = 0;
    for (auto& v : voices_) {
        if (v.active) count++;
    }
    return count;
}

int SynthNode::find_voice_for_note(uint8_t note) {
    for (uint8_t i = 0; i < max_voices_; ++i) {
        if (voices_[i].active && voices_[i].note == note) {
            return i;
        }
    }
    return -1;
}

int SynthNode::find_free_or_steal_voice() {
    for (uint8_t i = 0; i < max_voices_; ++i) {
        if (!voices_[i].active) {
            return i;
        }
    }
    return 0; // steal first voice
}

float SynthNode::midi_note_to_freq(uint8_t note) {
    return 440.0f * std::pow(2.0f, (static_cast<float>(note) - 69.0f) / 12.0f);
}

void SynthNode::process(size_t num_frames) {
    auto* left = output(0);
    auto* right = output(1);
    if (!left || !right) return;

    std::memset(left->buffer.data, 0, num_frames * sizeof(float));
    std::memset(right->buffer.data, 0, num_frames * sizeof(float));

    // Drain note events
    NoteEvent ev;
    while (note_queue_.pop(ev)) {
        if (ev.type == NoteEvent::Type::NoteOn) {
            int existing = find_voice_for_note(ev.note);
            if (existing >= 0) {
                voices_[existing].trigger(ev.note, ev.velocity, sample_rate_);
            } else {
                int idx = find_free_or_steal_voice();
                voices_[idx].trigger(ev.note, ev.velocity, sample_rate_);
            }
        } else {
            for (auto& v : voices_) {
                if (v.active && v.note == ev.note) {
                    v.release();
                }
            }
        }
    }

    // Update envelope params for all active voices from current synth params
    for (auto& v : voices_) {
        if (v.active) {
            v.osc.set_frequency(v.note_freq * std::pow(2.0f, params_.detune_cents / 1200.0f),
                                sample_rate_);
            v.amp_env.set_params(params_.attack_ms, params_.decay_ms,
                                 params_.sustain, params_.release_ms,
                                 sample_rate_);
            v.filter_env.set_params(params_.filter_attack_ms,
                                    params_.filter_decay_ms,
                                    params_.filter_sustain,
                                    params_.filter_release_ms,
                                    sample_rate_);
        }
    }

    // Process each voice, mix to output
    for (uint8_t i = 0; i < max_voices_; ++i) {
        auto& v = voices_[i];
        if (!v.active) continue;

        for (size_t f = 0; f < num_frames; ++f) {
            float s = v.process(sample_rate_, params_.waveform, params_.pulse_width,
                                params_.filter_mode, params_.filter_cutoff,
                                params_.filter_resonance, params_.filter_env_amount,
                                params_.filter_keytrack);
            s *= params_.volume;
            left->buffer.data[f] += s;
            right->buffer.data[f] += s;
        }
    }
}

} // namespace chimera
