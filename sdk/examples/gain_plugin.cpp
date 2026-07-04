#include "chimera/plugin.h"
#include <cmath>
#include <cstring>

namespace {

struct GainState {
    float gain = 1.0f;
};

constexpr ChimeraPluginDescriptor descriptor = {
    CHIMERA_PLUGIN_API_VERSION,
    "Gain",
    "Chimera",
    "1.0.0",
    1,
    1,
    1
};

ChimeraPlugin* create_gain(double sample_rate, uint32_t block_size) {
    auto* state = new GainState();
    state->gain = 1.0f;
    auto* plugin = new ChimeraPlugin();
    plugin->descriptor = &descriptor;
    plugin->state = state;
    return plugin;
}

void destroy_gain(ChimeraPlugin* plugin) {
    if (plugin) {
        delete static_cast<GainState*>(plugin->state);
        delete plugin;
    }
}

void process_gain(ChimeraPlugin* plugin,
                  const ChimeraPort* inputs, uint32_t,
                  ChimeraPort* outputs, uint32_t,
                  uint32_t num_frames) {
    auto* state = static_cast<GainState*>(plugin->state);
    for (uint32_t i = 0; i < num_frames; ++i) {
        outputs[0].data[i] = inputs[0].data[i] * state->gain;
    }
}

void set_param_gain(ChimeraPlugin* plugin, uint32_t index, float value) {
    auto* state = static_cast<GainState*>(plugin->state);
    if (index == 0) {
        state->gain = std::fmax(0.0f, std::fmin(value, 2.0f));
    }
}

float get_param_gain(ChimeraPlugin* plugin, uint32_t index) {
    auto* state = static_cast<GainState*>(plugin->state);
    return (index == 0) ? state->gain : 0.0f;
}

constexpr ChimeraPluginVTable vtable = {
    create_gain,
    destroy_gain,
    process_gain,
    set_param_gain,
    get_param_gain
};

} // namespace

extern "C" CHIMERA_PLUGIN_EXPORT const ChimeraPluginVTable* chimera_plugin_get_vtable(void) {
    return &vtable;
}
