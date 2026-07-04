#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include <cstdlib>

namespace chimera {

enum class TrackType : uint8_t {
    Trigger,
    Note
};

struct StepData {
    bool active = false;
    float velocity = 1.0f;
    float probability = 1.0f;
    uint8_t note = 60;
    float gate = 0.5f;
};

struct ParamLock {
    uint32_t param_index;
    float value;
};

struct SequencerEvent {
    enum class Type : uint8_t {
        Trigger,
        NoteOn,
        NoteOff,
    };
    Type type;
    uint32_t track;
    uint8_t note;
    float velocity;
};

struct SongStep {
    uint32_t pattern_index = 0;
    uint32_t repeats = 1;
};

struct StepSequencerPattern {
    std::string name = "Pattern 1";
    uint32_t num_steps = 16;
    std::vector<std::vector<StepData>> steps;
    std::vector<std::vector<std::vector<ParamLock>>> param_locks;

    void resize(uint32_t num_tracks, uint32_t steps_count) {
        num_steps = steps_count;
        steps.resize(num_tracks, std::vector<StepData>(steps_count));
        param_locks.resize(num_tracks, std::vector<std::vector<ParamLock>>(steps_count));
    }
};

class StepSequencer {
public:
    StepSequencer(uint32_t num_tracks = 16, uint32_t num_steps = 16);

    // Track config
    void set_track_type(uint32_t track, TrackType type);
    TrackType track_type(uint32_t track) const;

    // BPM / timing
    void set_bpm(float bpm) { bpm_ = bpm; }
    float bpm() const { return bpm_; }
    void set_steps_per_beat(uint32_t spb) { steps_per_beat_ = spb; }
    uint32_t steps_per_beat() const { return steps_per_beat_; }
    void set_sample_rate(double sr) { sample_rate_ = sr; }

    // Steps
    uint32_t num_tracks() const { return num_tracks_; }
    uint32_t num_steps() const { return active_pattern().num_steps; }
    void set_num_steps(uint32_t num);

    StepData& step(uint32_t track, uint32_t step_idx);
    const StepData& step(uint32_t track, uint32_t step_idx) const;

    void set_step(uint32_t track, uint32_t step_idx, bool active,
                  float velocity = 1.0f, float probability = 1.0f,
                  uint8_t note = 60, float gate = 0.5f);
    void set_note(uint32_t track, uint32_t step_idx, uint8_t note);
    void set_gate(uint32_t track, uint32_t step_idx, float gate);
    void toggle_step(uint32_t track, uint32_t step_idx);

    // Parameter locks
    void set_param_lock(uint32_t track, uint32_t step_idx,
                        uint32_t param_index, float value);
    bool has_param_locks(uint32_t track, uint32_t step_idx) const;
    const std::vector<ParamLock>& param_locks(uint32_t track, uint32_t step_idx) const;

    // Patterns
    uint32_t add_pattern(const std::string& name = "");
    void select_pattern(uint32_t index);
    uint32_t current_pattern_index() const { return current_pattern_; }
    uint32_t num_patterns() const { return patterns_.size(); }
    StepSequencerPattern& pattern(uint32_t index) { return patterns_[index]; }
    const StepSequencerPattern& pattern(uint32_t index) const { return patterns_[index]; }

    // Song mode
    void set_song(const std::vector<SongStep>& song);
    const std::vector<SongStep>& song() const { return song_; }
    void set_song_mode(bool enabled) { song_mode_ = enabled; }
    bool song_mode() const { return song_mode_; }

    // State
    void reset();
    uint32_t current_step() const { return current_step_; }
    void set_current_step(uint32_t s);
    bool is_playing() const { return playing_; }
    void set_playing(bool p) { playing_ = p; }

    std::vector<SequencerEvent> advance(uint64_t transport_frames,
                                        double sample_rate);

private:
    uint32_t num_tracks_;
    float bpm_ = 120.0f;
    uint32_t steps_per_beat_ = 4;
    double sample_rate_ = 48000.0;
    bool playing_ = true;

    std::vector<TrackType> track_types_;
    std::vector<StepSequencerPattern> patterns_;
    uint32_t current_pattern_ = 0;

    bool song_mode_ = false;
    std::vector<SongStep> song_;
    uint32_t song_pos_ = 0;
    uint32_t song_repeat_count_ = 0;

    uint64_t last_step_frame_ = 0;
    uint32_t current_step_ = 0;
    bool last_step_active_ = true;

    // Pending note-offs for gate tracking
    struct PendingNoteOff {
        uint32_t track;
        uint8_t note;
        uint64_t off_frame;
    };
    std::vector<PendingNoteOff> pending_note_offs_;
    uint64_t last_transport_ = 0;

    StepSequencerPattern& active_pattern() { return patterns_[current_pattern_]; }
    const StepSequencerPattern& active_pattern() const { return patterns_[current_pattern_]; }

    double frames_per_step() const {
        if (bpm_ <= 0.0f || steps_per_beat_ == 0) return 1.0;
        return (60.0 / bpm_) / steps_per_beat_ * sample_rate_;
    }
};

} // namespace chimera
