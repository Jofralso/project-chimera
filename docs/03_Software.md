# Software Architecture

```
UI → Application Layer → Session Manager → Audio Engine → Plugin Host → Hardware Layer → Linux
```

## Key Systems

| System | Status | Description |
|---|---|---|
| Audio Engine | ✅ Built | Engine class, real-time thread, lock-free control queue |
| DSP Graph | ✅ Built | AudioGraph with topological sort, AudioNode base, Port types |
| Plugin Host | ✅ Built | PluginScanner, PluginLoader (dlopen), PluginNode wrapper |
| Plugin SDK | ✅ Built | Pure C ABI, versioned structs, vtable dispatch |
| Session Manager | ✅ Built | Binary format, 60s autosave, save/load roundtrip |
| Logger | ✅ Built | Thread-safe, level-based, file/stderr output |
| Audio I/O | 🚧 Stub | AudioInputNode / AudioOutputNode (silent passthrough) |
| ALSA Backend | ✅ Built | PCM playback, float/s16, auto-configure |
| JACK Backend | ⏳ Planned | JACK client with real process callback |
| Hardware Abstraction | ⏳ Planned | RP2040, ESP32, GPIO abstractions |

## Detailed Stack

### Audio Engine (`software/audio-engine/`)
- `Engine` — owns graph, audio thread, control message queue
- `AudioGraph` — directed graph of nodes, topological sort on change
- `AudioNode` — base class: inputs/outputs/ports, prepare/process/release lifecycle
- `Port` — typed port (Audio, Control, Event) with AudioBuffer
- `AudioBuffer` — 64-byte aligned, realtime-allocated float buffer
- `RingBuffer<T>` — lock-free SPSC queue for cross-thread messaging

### Plugin Host (`software/audio-engine/`)
- `PluginLoader` — dlopen wrapper, symbol lookup, info scanner
- `PluginNode` — AudioNode subclass hosting a loaded plugin instance
- `PluginHost` — directory scanner, instantiate/unload in graph

### Plugin SDK (`sdk/`)
- `plugin.h` — pure C API, versioned with CHIMERA_PLUGIN_API_VERSION
- Entry point: `chimera_plugin_get_vtable` → `ChimeraPluginVTable`
- VTable: create, destroy, process, set_param, get_param

### Session Manager (`software/audio-engine/`)
- Binary format with magic header (0x4348494D), versioned
- Serializes name, sample rate, block size, graph data
- Configurable autosave interval, callback on autosave

## Build System

CMake 3.25+, C++20, superbuild pattern.
- `chimera_audio_engine` — static library
- `chimera_gain_plugin` — example shared plugin
- Test targets — 6 passing tests
