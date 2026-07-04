#include "chimera/plugin.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>

static int failures = 0;

#define TEST(name, expr) do { \
    if (!(expr)) { \
        std::fprintf(stderr, "FAIL: %s (%s:%d)\n", name, __FILE__, __LINE__); \
        failures++; \
    } else { \
        std::printf("PASS: %s\n", name); \
    } \
} while(0)

int main() {
    TEST("plugin header API version", CHIMERA_PLUGIN_API_VERSION == 1);
    TEST("struct sizes are reasonable",
         sizeof(ChimeraPluginDescriptor) > 0 &&
         sizeof(ChimeraPluginVTable) > 0);

    ChimeraPluginDescriptor desc;
    std::memset(&desc, 0, sizeof(desc));

    TEST("descriptor has correct layout",
         offsetof(ChimeraPluginDescriptor, api_version) == 0);

    TEST("port struct is at least pointer+int", sizeof(ChimeraPort) >= sizeof(float*) + sizeof(uint32_t));

    ChimeraPluginVTable vtable;
    std::memset(&vtable, 0, sizeof(vtable));
    TEST("vtable has create field", sizeof(vtable.create) > 0);

    void* handle = dlopen("libchimera_gain_plugin.so", RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        std::printf("SKIP: gain plugin not built (%s)\n", dlerror());
    } else {
        auto get_vtable = (chimera_plugin_get_vtable_fn)dlsym(handle, "chimera_plugin_get_vtable");
        TEST("plugin symbol found", get_vtable != nullptr);

        if (get_vtable) {
            const auto* vt = get_vtable();
            TEST("vtable is valid", vt != nullptr);
            TEST("vtable has create function", vt->create != nullptr);

            auto* plugin = vt->create(48000.0, 256);
            TEST("plugin created", plugin != nullptr);

            if (plugin) {
                TEST("plugin descriptor matches", plugin->descriptor != nullptr);
                TEST("plugin name is Gain",
                     std::strcmp(plugin->descriptor->name, "Gain") == 0);
                TEST("plugin vendor is Chimera",
                     std::strcmp(plugin->descriptor->vendor, "Chimera") == 0);
                TEST("plugin has 1 audio input",
                     plugin->descriptor->num_audio_inputs == 1);
                TEST("plugin has 1 audio output",
                     plugin->descriptor->num_audio_outputs == 1);

                float val = vt->get_param(plugin, 0);
                TEST("default gain is 1.0", val == 1.0f);

                vt->set_param(plugin, 0, 0.5f);
                val = vt->get_param(plugin, 0);
                TEST("gain set to 0.5", val == 0.5f);

                float input_data[4] = {0.1f, 0.2f, 0.3f, 0.4f};
                float output_data[4] = {};

                ChimeraPort inputs[1] = {{input_data, 4}};
                ChimeraPort outputs[1] = {{output_data, 4}};

                vt->process(plugin, inputs, 1, outputs, 1, 4);

                TEST("process works", output_data[0] == 0.05f &&
                     output_data[1] == 0.10f &&
                     output_data[2] == 0.15f &&
                     output_data[3] == 0.20f);

                vt->destroy(plugin);
            }
        }

        dlclose(handle);
    }

    std::printf("\n%d test(s) failed\n", failures);
    return failures > 0 ? 1 : 0;
}
