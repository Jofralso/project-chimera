#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CHIMERA_PLUGIN_API_VERSION 1
#define CHIMERA_PLUGIN_MAX_NAME 64

#if defined(_WIN32)
  #define CHIMERA_PLUGIN_EXPORT __declspec(dllexport)
#else
  #define CHIMERA_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

typedef struct ChimeraPort {
    float* data;
    uint32_t num_frames;
} ChimeraPort;

typedef struct ChimeraPluginDescriptor {
    uint32_t api_version;
    char name[CHIMERA_PLUGIN_MAX_NAME];
    char vendor[CHIMERA_PLUGIN_MAX_NAME];
    char version[CHIMERA_PLUGIN_MAX_NAME];
    uint32_t num_audio_inputs;
    uint32_t num_audio_outputs;
    uint32_t num_params;
} ChimeraPluginDescriptor;

typedef struct ChimeraPlugin {
    const ChimeraPluginDescriptor* descriptor;
    void* state;
} ChimeraPlugin;

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

typedef const ChimeraPluginVTable* (*chimera_plugin_get_vtable_fn)(void);

#ifdef __cplusplus
}
#endif
