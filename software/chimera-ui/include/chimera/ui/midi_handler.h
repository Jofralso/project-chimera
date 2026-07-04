#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

namespace chimera::ui {

struct MidiEvent {
    enum class Type {
        NoteOn,
        NoteOff,
        ControlChange,
        PitchBend,
        Clock,
        Start,
        Stop,
        Continue
    };

    Type type;
    uint8_t channel{0};
    uint8_t note{0};
    uint8_t velocity{0};
    uint8_t controller{0};
    uint8_t value{0};
    uint16_t pitch{0};
};

class MidiHandler {
public:
    MidiHandler();
    ~MidiHandler();

    MidiHandler(const MidiHandler&) = delete;
    MidiHandler& operator=(const MidiHandler&) = delete;

    bool init();
    void shutdown();
    bool running() const { return running_.load(); }

    using EventCallback = std::function<void(const MidiEvent&)>;
    void set_callback(EventCallback cb) { callback_ = cb; }

    // Poll for pending events (call from main thread)
    bool poll_event(MidiEvent& ev);

private:
    std::atomic<bool> running_{false};
    std::thread thread_;
    EventCallback callback_;

    // ALSA sequencer handles (opaque)
    void* seq_handle_{nullptr};
    int port_id_{-1};

    // Lock-free event queue (simple fixed ring buffer via std::vector)
    static constexpr size_t QUEUE_SIZE = 256;
    MidiEvent queue_[QUEUE_SIZE];
    std::atomic<size_t> write_pos_{0};
    std::atomic<size_t> read_pos_{0};

    void thread_func();
    bool push_event(const MidiEvent& ev);
};

} // namespace chimera::ui
