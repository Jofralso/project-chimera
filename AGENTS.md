# Project Status - Project Chimera

## Goal
- Complete Project Chimera M0-M7: audio core, drum machine, subtractive synth, sequencer extensions, JACK backend, OP-1-style GUI with SDL2+canvas, and MIDI input via ALSA sequencer.

## Constraints & Preferences
- Linux first, C++20, CMake 3.25+, real‑time safe (no allocs in audio thread, lock‑free control queue).
- All features modular and removable.
- Zero external build dependencies (ALSA and JACK loaded via dlopen at runtime; SDL2 headers extracted from .deb, linked against system libSDL2-2.0.so.0).

## Progress
### Done
- M0‑M6: Platform, Audio Engine, I/O/Effects/Sampler, Drum Machine, Subtractive Synth, Sequencer (note tracks, gate, patterns, song, param locks), JACK Backend (runtime dlopen, no compile‑time dep).
- Engine routes `SequencerEvent` (Trigger/NoteOn/NoteOff) to graph nodes by `node_class()`.
- `chimera-play`: `-d`, `-f`, `-c`, `--alsa`, `--jack`, `--dummy`, `--wav`, `--session`, `--capture`, `--device`, `--drum`, `--drum-bpm`, `--drum-sample`, `--synth`, `--seq` flags.
- `chimera-ui` library (SDL2+canvas, OP-1 LCD aesthetic):
  - `Canvas` — 320×640 bitmap framebuffer, 5×7 pixel font, circles, waveforms, level meters, rects.
  - `Theme` — green-on-black color scheme, layout/metrics constants.
  - `Screen` base — contextual knobs, keyboard/mouse handling, `set_engine()` bridge.
  - `SynthScreen` — waveform preview, ADSR visual, filter params; writes waveform/cutoff/resonance/env-amount to engine `SynthNode`.
  - `DrumScreen` — 4‑pad grid, per‑pad volume/decay/tune/pan; writes to engine `DrumNode`.
  - `SequencerScreen` — 16×4 step grid, cursor navigation (arrows+enter), toggle steps; writes to engine `StepSequencer`.
  - `MixerScreen` — 4 channel strips with level meters, master section.
   - `Display` — SDL2 window, screen stack with 1‑5/Tab switching, dot indicator bar.
   - `BrowserScreen` — file browser with sample loading (navigate dirs, load .wav into drum pads) and session management (save/load .chimera, F5 to save).
- `MidiHandler` — ALSA sequencer input thread, lock‑free ring buffer, supports NoteOn/NoteOff/CC/PitchBend/Clock/Start/Stop/Continue.
- `chimera-desktop` app: creates synth+drum+sequencer+master graph, starts sequencer playing, opens MIDI input, runs SDL2 UI loop, 5 screens (Synth/Drum/Seq/Mixer/Browser).
- ALSA capture passthrough (`--capture`) verified working with dummy and real ALSA devices (2ch and 8ch).
- Session factory creates `builtin.drum` and `builtin.synth`.
- 92+ tests, 7 test suites, all green.

### In Progress
- (none)

### Blocked
- PipeWire backend — `libpipewire-0.3.so.0` runtime lib is present but no `.so` symlink for compile‑time linking. Could use `dlopen` approach like JACK (runtime load, no dev package).

## Key Decisions
- **Backend abstraction**: `AudioBackend` interface allows ALSA/JACK/PipeWire without Engine changes. JACK uses runtime `dlopen` to avoid compile‑time dependency.
- **Graph mutations**: control‑queue messages for connect/disconnect/remove (lock‑free); `add_node` uses `std::mutex`.
- **Serialisation**: binary v2 with node factory for deserialisation.
- **ALSA duplex**: single‑thread (read capture → process → write playback), capture failure non‑fatal (inputs zeroed).
- **DrumNode/StepSequencer**: separate classes — `StepSequencer` tracks transport position and generates events; engine routes events to `DrumNode` via lock‑free `RingBuffer<DrumTrigger>` / `SynthNode` via `RingBuffer<NoteEvent>`.
- **Sequencer extensions**: `SequencerEvent` unification — `Trigger`, `NoteOn`, `NoteOff` types dispatched in audio callback by matching `node_class()`.
- **Envelope coeffs**: use `pow(kThreshold, 1/(time*sr))` so decay/release reach threshold in specified time.
- **SVF resonance**: parameter clamped to 0–0.999; with zero damping the filter rings (not a DC blocker — test uses 0.8 for settled response).

## Next Steps
1. ALSA capture demo verification.
2. PipeWire backend (when libpipewire available).
3. GUI or MIDI control surface integration.

## Critical Context
- **ALSA backend compiles/links** — 2 and 8 channel duplex confirmed.
- **JACK backend** — runtime dlopen of `libjack.so.0`, graceful error if jackd not running.
- **Session format v2** — v1 files still load.
- **Plugin host uses `std::filesystem`** (C++17).
- **Tests run in `build/tests/`** with `LD_LIBRARY_PATH` for plugin `.so` loading.
- **RTTI disabled** — use `reinterpret_cast` or class‑name lookup instead of `dynamic_cast`.
- **ADSREnvelope** (decay only, in `dsp/adsr.h`) is separate from **Envelope** (full ADSR in `dsp/envelope.h`).
- **Display navigation**: 1-5 switches to screen index, Tab/Shift+Tab cycles screens, mouse wheel maps to knob 0.
- **SynthScreen/DrumScreen/SequencerScreen** write parameter changes directly to engine in `on_knob()` via `apply_to_engine()`.
- Git remote is `https://github.com/Jofralso/project-chimera.git`, branch `master`, HTTPS pushes need token.

## Relevant Files
- `software/chimera-ui/`: GUI library (SDL2+canvas, OP-1 LCD aesthetic).
  - `include/chimera/ui/canvas.h` / `src/display.cpp`: Pixel‑based 320×640 rendering, SDL2 window, screen stack with 1‑4/Tab navigation.
  - `include/chimera/ui/theme.h`: Color scheme, layout constants.
  - `include/chimera/ui/screen.h` / `src/screen.cpp`: Screen base with 4 contextual knobs, `set_engine()` bridge.
  - `include/chimera/ui/screens/synth_screen.h` / `src/screens/synth_screen.cpp`: Synth param editing + waveform preview; applies to `SynthNode` via engine.
  - `include/chimera/ui/screens/drum_screen.h` / `src/screens/drum_screen.cpp`: 4‑pad grid with volume/decay/tune/pan per pad; applies to `DrumNode`.
  - `include/chimera/ui/screens/sequencer_screen.h` / `src/screens/sequencer_screen.cpp`: 16×4 step grid, cursor navigation; applies to `StepSequencer`.
  - `include/chimera/ui/screens/mixer_screen.h` / `src/screens/mixer_screen.cpp`: 4 channel strips, level meters, master.
  - `include/chimera/ui/midi_handler.h` / `src/midi_handler.cpp`: ALSA sequencer input, lock‑free ring buffer.
  - `CMakeLists.txt`: Builds `chimera_ui` static lib, finds SDL2 headers/library.
- `software/apps/chimera-desktop.cpp`: Desktop app wiring engine + UI + MIDI.
- `software/audio-engine/`: Core engine, nodes, backends, DSP.
  - `include/chimera/engine.h` / `engine.cpp`: `StepSequencer` routing (triggers→DrumNode, notes→SynthNode).
  - `include/chimera/backends/jack_backend.h` / `src/backends/jack_backend.cpp`: JACK backend via runtime dlopen.
- `software/apps/chimera-play.cpp`: CLI app, all flags.
- `tests/test_nodes.cpp`: 92+ tests covering all nodes, sequencer, oscillator, filter, envelope, drum, synth.
- `cmake/FindJack.cmake`: JACK CMake module.
- `docs/02_ARCHITECTURE.md`, `docs/03_Interfaces.md`, `docs/04_UX_UI.md`.
