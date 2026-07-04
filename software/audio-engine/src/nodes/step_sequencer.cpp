#include "chimera/nodes/step_sequencer.h"
#include <cmath>
#include <algorithm>

namespace chimera {

StepSequencer::StepSequencer(uint32_t num_tracks, uint32_t num_steps)
    : num_tracks_(num_tracks)
{
    track_types_.resize(num_tracks_, TrackType::Trigger);
    patterns_.resize(1);
    patterns_[0].resize(num_tracks_, num_steps);
}

void StepSequencer::set_track_type(uint32_t track, TrackType type) {
    if (track < num_tracks_) track_types_[track] = type;
}

TrackType StepSequencer::track_type(uint32_t track) const {
    return track < num_tracks_ ? track_types_[track] : TrackType::Trigger;
}

void StepSequencer::set_num_steps(uint32_t num) {
    if (num < 1) num = 1;
    if (num > 64) num = 64;
    for (auto& p : patterns_) {
        p.num_steps = num;
        for (auto& row : p.steps) row.resize(num);
        for (auto& row : p.param_locks) row.resize(num);
    }
    if (current_step_ >= num) current_step_ = 0;
}

StepData& StepSequencer::step(uint32_t track, uint32_t step_idx) {
    return active_pattern().steps[track % num_tracks_][step_idx % active_pattern().num_steps];
}

const StepData& StepSequencer::step(uint32_t track, uint32_t step_idx) const {
    return active_pattern().steps[track % num_tracks_][step_idx % active_pattern().num_steps];
}

void StepSequencer::set_step(uint32_t track, uint32_t step_idx, bool active,
                              float velocity, float probability,
                              uint8_t note, float gate) {
    if (track < num_tracks_ && step_idx < active_pattern().num_steps) {
        auto& s = active_pattern().steps[track][step_idx];
        s.active = active;
        s.velocity = std::max(0.0f, std::min(1.0f, velocity));
        s.probability = std::max(0.0f, std::min(1.0f, probability));
        s.note = note;
        s.gate = std::max(0.0f, std::min(1.0f, gate));
    }
}

void StepSequencer::set_note(uint32_t track, uint32_t step_idx, uint8_t note) {
    if (track < num_tracks_ && step_idx < active_pattern().num_steps) {
        active_pattern().steps[track][step_idx].note = note;
    }
}

void StepSequencer::set_gate(uint32_t track, uint32_t step_idx, float gate) {
    if (track < num_tracks_ && step_idx < active_pattern().num_steps) {
        active_pattern().steps[track][step_idx].gate = std::max(0.0f, std::min(1.0f, gate));
    }
}

void StepSequencer::toggle_step(uint32_t track, uint32_t step_idx) {
    if (track < num_tracks_ && step_idx < active_pattern().num_steps) {
        active_pattern().steps[track][step_idx].active =
            !active_pattern().steps[track][step_idx].active;
    }
}

void StepSequencer::set_param_lock(uint32_t track, uint32_t step_idx,
                                    uint32_t param_index, float value) {
    if (track < num_tracks_ && step_idx < active_pattern().num_steps) {
        auto& locks = active_pattern().param_locks[track][step_idx];
        // Replace existing lock for same param or add new
        for (auto& l : locks) {
            if (l.param_index == param_index) {
                l.value = value;
                return;
            }
        }
        locks.push_back({param_index, value});
    }
}

bool StepSequencer::has_param_locks(uint32_t track, uint32_t step_idx) const {
    if (track >= num_tracks_ || step_idx >= active_pattern().num_steps) return false;
    return !active_pattern().param_locks[track][step_idx].empty();
}

const std::vector<ParamLock>& StepSequencer::param_locks(uint32_t track, uint32_t step_idx) const {
    static std::vector<ParamLock> empty;
    if (track >= num_tracks_ || step_idx >= active_pattern().num_steps) return empty;
    return active_pattern().param_locks[track][step_idx];
}

uint32_t StepSequencer::add_pattern(const std::string& name) {
    uint32_t idx = static_cast<uint32_t>(patterns_.size());
    patterns_.emplace_back();
    patterns_.back().name = name.empty() ? "Pattern " + std::to_string(idx + 1) : name;
    patterns_.back().resize(num_tracks_, patterns_[0].num_steps);
    return idx;
}

void StepSequencer::select_pattern(uint32_t index) {
    if (index < patterns_.size()) {
        current_pattern_ = index;
        current_step_ = 0;
        last_step_frame_ = 0;
        pending_note_offs_.clear();
    }
}

void StepSequencer::set_song(const std::vector<SongStep>& song) {
    song_ = song;
    song_pos_ = 0;
    song_repeat_count_ = 0;
}

void StepSequencer::reset() {
    last_step_frame_ = 0;
    current_step_ = 0;
    last_transport_ = 0;
    pending_note_offs_.clear();
    song_pos_ = 0;
    song_repeat_count_ = 0;
}

void StepSequencer::set_current_step(uint32_t s) {
    current_step_ = s;
    last_step_frame_ = 0;
}

std::vector<SequencerEvent> StepSequencer::advance(uint64_t transport_frames,
                                                    double sample_rate) {
    std::vector<SequencerEvent> events;
    sample_rate_ = sample_rate;

    if (!playing_ || bpm_ <= 0.0f || steps_per_beat_ == 0) return events;

    double fps = frames_per_step();
    if (fps <= 0.0) return events;

    uint64_t step_frame = static_cast<uint64_t>(
        (static_cast<double>(transport_frames) / fps)) * static_cast<uint64_t>(fps);

    bool step_changed = (step_frame != last_step_frame_) && transport_frames > 0;

    if (step_changed) {
        uint32_t prev_step = current_step_;
        current_step_ = static_cast<uint32_t>(
            static_cast<double>(transport_frames) / fps);

        uint32_t active_num_steps = active_pattern().num_steps;
        uint32_t prev_mod = prev_step % active_num_steps;
        uint32_t cur_mod = current_step_ % active_num_steps;

        // Note-off for previous active step notes
        for (uint32_t t = 0; t < num_tracks_; ++t) {
            if (track_types_[t] == TrackType::Note) {
                auto& prev_s = active_pattern().steps[t][prev_mod];
                if (prev_s.active) {
                    events.push_back({SequencerEvent::Type::NoteOff, t, prev_s.note, 0.0f});
                }
            }
        }

        // Trigger/Note-on for current step
        for (uint32_t t = 0; t < num_tracks_; ++t) {
            auto& s = active_pattern().steps[t][cur_mod];
            if (!s.active) continue;

            float prob_roll = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
            if (prob_roll > s.probability) continue;

            if (track_types_[t] == TrackType::Trigger) {
                events.push_back({SequencerEvent::Type::Trigger, t, 0, s.velocity});
            } else {
                events.push_back({SequencerEvent::Type::NoteOn, t, s.note, s.velocity});

                // Schedule note-off for gate end if gate < 1.0
                if (s.gate < 1.0f) {
                    uint64_t off_frame = step_frame +
                        static_cast<uint64_t>(fps * s.gate);
                    pending_note_offs_.push_back({t, s.note, off_frame});
                }
            }
        }

        // Handle song mode pattern switching
        if (song_mode_ && !song_.empty()) {
            if (cur_mod == 0 && prev_mod != cur_mod) {
                song_repeat_count_++;
                if (song_repeat_count_ >= song_[song_pos_].repeats) {
                    song_repeat_count_ = 0;
                    song_pos_ = (song_pos_ + 1) % song_.size();
                    select_pattern(song_[song_pos_].pattern_index);
                }
            }
        }

        last_step_frame_ = step_frame;
    }

    // Process pending note-offs based on gate
    if (!pending_note_offs_.empty()) {
        auto it = pending_note_offs_.begin();
        while (it != pending_note_offs_.end()) {
            if (transport_frames >= it->off_frame) {
                events.push_back({SequencerEvent::Type::NoteOff, it->track, it->note, 0.0f});
                it = pending_note_offs_.erase(it);
            } else {
                ++it;
            }
        }
    }

    last_transport_ = transport_frames;
    return events;
}

} // namespace chimera
