#include "chimera/ui/midi_handler.h"
#include <cstdio>
#include <cstring>

// ALSA sequencer headers
#include <alsa/asoundlib.h>

namespace chimera::ui {

MidiHandler::MidiHandler() = default;

MidiHandler::~MidiHandler() {
    shutdown();
}

bool MidiHandler::init() {
    snd_seq_t* seq = nullptr;
    int err = snd_seq_open(&seq, "default", SND_SEQ_OPEN_INPUT, 0);
    if (err < 0) {
        std::fprintf(stderr, "MIDI: snd_seq_open failed: %s\n", snd_strerror(err));
        return false;
    }

    snd_seq_set_client_name(seq, "Chimera");

    int port = snd_seq_create_simple_port(seq, "input",
                                           SND_SEQ_PORT_CAP_WRITE |
                                           SND_SEQ_PORT_CAP_SUBS_WRITE,
                                           SND_SEQ_PORT_TYPE_APPLICATION);
    if (port < 0) {
        std::fprintf(stderr, "MIDI: create port failed\n");
        snd_seq_close(seq);
        return false;
    }

    seq_handle_ = seq;
    port_id_ = port;

    // Subscribe to all sequencer clients (optional)
    snd_seq_port_subscribe_t* sub;
    snd_seq_port_subscribe_alloca(&sub);

    running_.store(true);
    thread_ = std::thread([this]() { thread_func(); });

    std::printf("MIDI: handler initialized\n");
    return true;
}

void MidiHandler::shutdown() {
    running_.store(false);
    if (thread_.joinable()) {
        thread_.join();
    }
    if (seq_handle_) {
        snd_seq_close(static_cast<snd_seq_t*>(seq_handle_));
        seq_handle_ = nullptr;
    }
}

bool MidiHandler::push_event(const MidiEvent& ev) {
    size_t w = write_pos_.load(std::memory_order_relaxed);
    size_t r = read_pos_.load(std::memory_order_acquire);
    size_t next = (w + 1) % QUEUE_SIZE;
    if (next == r) return false; // full
    queue_[w] = ev;
    write_pos_.store(next, std::memory_order_release);
    return true;
}

bool MidiHandler::poll_event(MidiEvent& ev) {
    size_t r = read_pos_.load(std::memory_order_relaxed);
    size_t w = write_pos_.load(std::memory_order_acquire);
    if (r == w) return false;
    ev = queue_[r];
    read_pos_.store((r + 1) % QUEUE_SIZE, std::memory_order_release);
    return true;
}

void MidiHandler::thread_func() {
    auto* seq = static_cast<snd_seq_t*>(seq_handle_);
    int npfd = snd_seq_poll_descriptors_count(seq, POLLIN);
    struct pollfd* pfd = new struct pollfd[npfd];
    snd_seq_poll_descriptors(seq, pfd, npfd, POLLIN);

    while (running_.load()) {
        int err = poll(pfd, npfd, 50); // 50ms timeout
        if (err < 0) break;
        if (err == 0) continue;

        do {
            snd_seq_event_t* ev = nullptr;
            err = snd_seq_event_input(seq, &ev);
            if (err < 0) break;
            if (!ev) continue;

            MidiEvent mev{};
            switch (ev->type) {
                case SND_SEQ_EVENT_NOTEON:
                    mev.type = MidiEvent::Type::NoteOn;
                    mev.channel = ev->data.note.channel;
                    mev.note = ev->data.note.note;
                    mev.velocity = ev->data.note.velocity;
                    break;
                case SND_SEQ_EVENT_NOTEOFF:
                    mev.type = MidiEvent::Type::NoteOff;
                    mev.channel = ev->data.note.channel;
                    mev.note = ev->data.note.note;
                    mev.velocity = ev->data.note.velocity;
                    break;
                case SND_SEQ_EVENT_CONTROLLER:
                    mev.type = MidiEvent::Type::ControlChange;
                    mev.channel = ev->data.control.channel;
                    mev.controller = ev->data.control.param;
                    mev.value = ev->data.control.value;
                    break;
                case SND_SEQ_EVENT_PITCHBEND:
                    mev.type = MidiEvent::Type::PitchBend;
                    mev.channel = ev->data.control.channel;
                    mev.pitch = ev->data.control.value + 8192;
                    break;
                case SND_SEQ_EVENT_CLOCK:
                    mev.type = MidiEvent::Type::Clock;
                    break;
                case SND_SEQ_EVENT_START:
                    mev.type = MidiEvent::Type::Start;
                    break;
                case SND_SEQ_EVENT_STOP:
                    mev.type = MidiEvent::Type::Stop;
                    break;
                case SND_SEQ_EVENT_CONTINUE:
                    mev.type = MidiEvent::Type::Continue;
                    break;
                default:
                    break;
            }

            if (mev.type != MidiEvent::Type::NoteOn || mev.type != MidiEvent::Type::NoteOff || mev.type != MidiEvent::Type::ControlChange) {
                // Actually we want to process all events, not filter
            }

            push_event(mev);

            // Also call callback directly if set (for real-time)
            if (callback_) {
                callback_(mev);
            }

            snd_seq_free_event(ev);
        } while (err > 0);
    }

    delete[] pfd;
}

} // namespace chimera::ui
