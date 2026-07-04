# Architecture

## Hardware Topology

```
┌─────────────────────────────────────────────────────────┐
│  Raspberry Pi 5 (main compute)                          │
│  ┌────────────────────────────────────────────────────┐ │
│  │  Audio Engine ─── Plugin Host ─── Plugin .so       │ │
│  │  Session Mgr ──── Audio Graph ──── AudioNode(s)    │ │
│  │  Logger ───────── Application Layer ──── UI        │ │
│  └────────────────────────────────────────────────────┘ │
│                        │                                │
│         USB Audio ─────┤                                │
│         7" TFT ────────┤                                │
│                        │                                │
├── RP2040 ──────────────┤  (buttons, encoders, LEDs)     │
├── ESP32 ───────────────┤  (Wi-Fi, BLE MIDI)             │
├── Pi Zero + eInk ──────┤  (status display)              │
└─────────────────────────────────────────────────────────┘
```

## Software Stack

```
┌──────────────────────────────────────────────┐
│  Application Layer (future)                   │
├──────────────────────────────────────────────┤
│  Session Manager                              │
│  ─ Binary serialization, autosave, crash rec  │
├──────────────────────────────────────────────┤
│  Audio Engine                                 │
│  ─ Engine, AudioGraph, AudioNode             │
│  ─ PluginHost (dlopen, PluginNode wrapper)   │
├──────────────────────────────────────────────┤
│  Plugin SDK / ABI                             │
│  ─ plugin.h: C vtable, versioned, dlopen-safe │
├──────────────────────────────────────────────┤
│  Hardware Abstraction Layer (future)          │
├──────────────────────────────────────────────┤
│  Linux (ALSA, JACK, PipeWire, GPIO, SPI, I2C) │
└──────────────────────────────────────────────┘
```

## Data Flow (Audio Cycle)

```
TestToneNode → AudioGraph.process()
    ↓
PluginNode (Gain)
    ↓
MasterOutputNode (volume + clip)
    ↓
AudioOutputNode → system playback
```

1. Engine wakes audio thread (timer or JACK callback)
2. Control messages drained from lock-free RingBuffer
3. AudioGraph::process() walks nodes in topological order
4. Each node reads inputs, processes, writes outputs
5. Master output buffers copied to system audio output

## Key Design Decisions

- **No allocations in audio thread** — all buffers pre-allocated in prepare()
- **Lock-free control** — RingBuffer<EngineMessage> for thread-safe commands
- **Topological sort** — graph re-orders on connection change (dirty flag)
- **Plugin sandboxing** — plugins are AudioNode subclasses, not raw callbacks
- **Binary-safe ABI** — pure C, no name mangling, no exceptions across boundary
