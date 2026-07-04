# Plugin SDK

## Status
Implemented — see `sdk/include/chimera/plugin.h`.

## ABI Design

The plugin ABI is pure C (`extern "C"`, `__attribute__((visibility("default")))`).
No RTTI, no exceptions, no C++ name mangling across the boundary.

### Lifecycle

1. Host calls `chimera_plugin_get_vtable()` to obtain `ChimeraPluginVTable`
2. Host calls `vtable->create(sample_rate, block_size)` → returns `ChimeraPlugin*`
3. Each audio cycle: `vtable->process(plugin, inputs, outputs, num_frames)`
4. Parameter changes: `vtable->set_param(plugin, index, value)`
5. Shutdown: `vtable->destroy(plugin)`

### Plugin Descriptor

```c
typedef struct ChimeraPluginDescriptor {
    uint32_t api_version;            // CHIMERA_PLUGIN_API_VERSION
    char name[64];                   // Human-readable name
    char vendor[64];                 // Vendor name
    char version[64];                // Version string
    uint32_t num_audio_inputs;
    uint32_t num_audio_outputs;
    uint32_t num_params;
} ChimeraPluginDescriptor;
```

### VTable

```c
typedef struct ChimeraPluginVTable {
    ChimeraPlugin* (*create)(double sample_rate, uint32_t block_size);
    void (*destroy)(ChimeraPlugin* plugin);
    void (*process)(ChimeraPlugin* plugin,
                    const ChimeraPort* inputs, uint32_t num_inputs,
                    ChimeraPort* outputs, uint32_t num_outputs,
                    uint32_t num_frames);
    void (*set_param)(ChimeraPlugin* plugin, uint32_t index, float value);
    float (*get_param)(ChimeraPlugin* plugin, uint32_t index);
} ChimeraPluginVTable;
```

### Export Macro

```c
// Non-Windows
#define CHIMERA_PLUGIN_EXPORT __attribute__((visibility("default")))

extern "C" CHIMERA_PLUGIN_EXPORT const ChimeraPluginVTable* chimera_plugin_get_vtable(void);
```

## Plugin Host Integration

The `PluginNode` class wraps a loaded plugin as an `AudioNode` in the graph:
- Ports created dynamically from descriptor (num_audio_inputs/outputs)
- `prepare()` instantiates the plugin via vtable->create
- `process()` bridges AudioBuffers to ChimeraPort arrays
- `set_param()` / `get_param()` delegate to plugin vtable

## Provided Example

`sdk/examples/gain_plugin.cpp` — stereo gain with one parameter:
- 1 audio input, 1 audio output
- Gain parameter (0.0 – 2.0, default 1.0)
- Demonstrates full lifecycle and ABI compliance

## Constraints

- No plugin may block the audio thread
- Plugins must not allocate memory in process()
- Plugins run in isolated graph nodes (no shared state)
- Future: sandboxing via separate process or WASM
