#include "chimera/backends/jack_backend.h"
#include "chimera/engine.h"
#include "chimera/logger.h"

#include <cstring>
#include <dlfcn.h>

namespace chimera {

// JACK API types and function pointer signatures
using jack_client_t = void;
using jack_port_t = void;
using jack_nframes_t = uint32_t;

using JackClientOpen = jack_client_t*(*)(const char*, uint32_t, uint32_t*, ...);
using JackClientClose = int(*)(jack_client_t*);
using JackActivate = int(*)(jack_client_t*);
using JackDeactivate = int(*)(jack_client_t*);
using JackSetProcessCallback = int(*)(jack_client_t*, int(*)(jack_nframes_t, void*), void*);
using JackOnShutdown = void(*)(jack_client_t*, void(*)(void*), void*);
using JackPortRegister = jack_port_t*(*)(jack_client_t*, const char*, const char*, uint32_t, uint32_t);
using JackPortGetBuffer = void*(*)(jack_port_t*, jack_nframes_t);
using JackConnect = int(*)(jack_client_t*, const char*, const char*);
using JackGetPortName = const char*(*)(const jack_port_t*);
using JackGetSampleRate = jack_nframes_t(*)(jack_client_t*);
using JackGetBufferSize = jack_nframes_t(*)(jack_client_t*);
using JackIsRealtime = int(*)(jack_client_t*);

struct JackBackend::JackFuncs {
    JackClientOpen client_open;
    JackClientClose client_close;
    JackActivate activate;
    JackDeactivate deactivate;
    JackSetProcessCallback set_process_callback;
    JackOnShutdown on_shutdown;
    JackPortRegister port_register;
    JackPortGetBuffer port_get_buffer;
    JackConnect connect;
    JackGetPortName get_port_name;
    JackGetSampleRate get_sample_rate;
    JackGetBufferSize get_buffer_size;
    JackIsRealtime is_realtime;
};

JackBackend::~JackBackend() {
    shutdown();
}

bool JackBackend::init(const EngineConfig& config) {
    sample_rate_ = config.sample_rate;
    block_size_ = config.block_size;
    num_inputs_ = config.num_inputs;
    num_outputs_ = config.num_outputs;
    client_name_ = config.client_name.empty() ? "Chimera" : config.client_name;
    CHIMERA_INFO("JACK backend initializing: %s", client_name_.c_str());
    return true;
}

bool JackBackend::load_jack_library() {
    dl_handle_ = dlopen("libjack.so.0", RTLD_NOW | RTLD_LOCAL);
    if (!dl_handle_) {
        CHIMERA_ERROR("JACK: dlopen failed: %s", dlerror());
        return false;
    }

    j_ = new JackFuncs;

#define LOAD_SYM(name, var) \
    var = reinterpret_cast<decltype(var)>(dlsym(dl_handle_, name)); \
    if (!var) { \
        CHIMERA_ERROR("JACK: dlsym(%s) failed: %s", name, dlerror()); \
        delete j_; \
        j_ = nullptr; \
        dlclose(dl_handle_); \
        dl_handle_ = nullptr; \
        return false; \
    }

    LOAD_SYM("jack_client_open", j_->client_open);
    LOAD_SYM("jack_client_close", j_->client_close);
    LOAD_SYM("jack_activate", j_->activate);
    LOAD_SYM("jack_deactivate", j_->deactivate);
    LOAD_SYM("jack_set_process_callback", j_->set_process_callback);
    LOAD_SYM("jack_on_shutdown", j_->on_shutdown);
    LOAD_SYM("jack_port_register", j_->port_register);
    LOAD_SYM("jack_port_get_buffer", j_->port_get_buffer);
    LOAD_SYM("jack_connect", j_->connect);
    LOAD_SYM("jack_port_name", j_->get_port_name);   // actually jack_port_name
    LOAD_SYM("jack_get_sample_rate", j_->get_sample_rate);
    LOAD_SYM("jack_get_buffer_size", j_->get_buffer_size);
    LOAD_SYM("jack_is_realtime", j_->is_realtime);

#undef LOAD_SYM

    CHIMERA_INFO("JACK: loaded libjack.so.0");
    return true;
}

void JackBackend::unload_jack_library() {
    delete j_;
    j_ = nullptr;
    if (dl_handle_) {
        dlclose(dl_handle_);
        dl_handle_ = nullptr;
    }
}

bool JackBackend::start() {
    if (running_.load()) return false;
    if (!process_callback_) return false;

    if (!load_jack_library()) {
        CHIMERA_ERROR("JACK: failed to load runtime library");
        return false;
    }

    // Open JACK client
    uint32_t status = 0;
    uint32_t options = 1; // JackNoStartServer
    const char* server_name = nullptr;

    client_ = j_->client_open(client_name_.c_str(), options, &status, server_name);
    if (!client_) {
        CHIMERA_ERROR("JACK: jack_client_open failed (status=0x%x)", status);
        unload_jack_library();
        return false;
    }

    CHIMERA_INFO("JACK: connected to server, client name: %s",
                 reinterpret_cast<const char*>(status)); // not quite right...

    // Get actual sample rate and buffer size from the server
    if (j_->get_sample_rate) {
        sample_rate_ = static_cast<double>(j_->get_sample_rate(client_));
    }
    if (j_->get_buffer_size) {
        block_size_ = static_cast<size_t>(j_->get_buffer_size(client_));
    }
    if (j_->is_realtime) {
        int rt = j_->is_realtime(client_);
        CHIMERA_INFO("JACK: running %s", rt ? "realtime" : "non-realtime");
    }

    // Register ports
    if (!create_ports()) {
        j_->client_close(client_);
        client_ = nullptr;
        unload_jack_library();
        return false;
    }

    // Set process callback
    using ProcessCallbackType = int(*)(jack_nframes_t, void*);
    int ret = j_->set_process_callback(client_,
        reinterpret_cast<ProcessCallbackType>(process_callback_thunk), this);
    if (ret != 0) {
        CHIMERA_ERROR("JACK: set_process_callback failed: %d", ret);
        j_->client_close(client_);
        client_ = nullptr;
        unload_jack_library();
        return false;
    }

    // Set shutdown callback
    j_->on_shutdown(client_, shutdown_callback_thunk, this);

    // Auto-connect to physical ports if possible
    // (We do this before activate for the ports to be available)

    // Activate
    ret = j_->activate(client_);
    if (ret != 0) {
        CHIMERA_ERROR("JACK: activate failed: %d", ret);
        j_->client_close(client_);
        client_ = nullptr;
        unload_jack_library();
        return false;
    }

    CHIMERA_INFO("JACK: port register");
    // Try to connect output ports to physical playback ports

    running_.store(true);
    CHIMERA_INFO("JACK backend started (%g Hz, %zu frames, %zu out, %zu in)",
                 sample_rate_, block_size_, num_outputs_, num_inputs_);
    return true;
}

bool JackBackend::create_ports() {
    output_ports_.resize(num_outputs_);
    output_buffers_.resize(num_outputs_, nullptr);

    for (size_t i = 0; i < num_outputs_; ++i) {
        std::string port_name = "output_" + std::to_string(i + 1);
        auto* port = j_->port_register(client_, port_name.c_str(),
                                       "32 bit float mono audio",
                                       0x2, // JackPortIsOutput
                                       0);
        if (!port) {
            CHIMERA_ERROR("JACK: failed to register output port %zu", i);
            return false;
        }
        output_ports_[i] = port;
    }

    input_ports_.resize(num_inputs_);
    input_buffers_.resize(num_inputs_, nullptr);

    for (size_t i = 0; i < num_inputs_; ++i) {
        std::string port_name = "input_" + std::to_string(i + 1);
        auto* port = j_->port_register(client_, port_name.c_str(),
                                       "32 bit float mono audio",
                                       0x1, // JackPortIsInput
                                       0);
        if (!port) {
            CHIMERA_ERROR("JACK: failed to register input port %zu", i);
            return false;
        }
        input_ports_[i] = port;
    }

    return true;
}

void JackBackend::stop() {
    running_.store(false);
    if (client_) {
        j_->deactivate(client_);
        j_->client_close(client_);
        client_ = nullptr;
    }
    output_ports_.clear();
    input_ports_.clear();
    output_buffers_.clear();
    input_buffers_.clear();
    unload_jack_library();
    CHIMERA_INFO("JACK backend stopped");
}

void JackBackend::shutdown() {
    stop();
}

void JackBackend::set_process_callback(
    void (*callback)(float**, float**, size_t, void*),
    void* userdata)
{
    process_callback_ = callback;
    userdata_ = userdata;
}

int JackBackend::process_callback_thunk(uint32_t nframes, void* arg) {
    auto* self = static_cast<JackBackend*>(arg);

    // Get buffer pointers for all ports
    for (size_t i = 0; i < self->num_outputs_; ++i) {
        self->output_buffers_[i] = static_cast<float*>(
            self->j_->port_get_buffer(self->output_ports_[i], nframes));
    }
    for (size_t i = 0; i < self->num_inputs_; ++i) {
        self->input_buffers_[i] = static_cast<float*>(
            self->j_->port_get_buffer(self->input_ports_[i], nframes));
    }

    self->process_callback_(
        self->output_buffers_.data(),
        self->input_buffers_.data(),
        static_cast<size_t>(nframes),
        self->userdata_);

    return 0;
}

void JackBackend::shutdown_callback_thunk(void* arg) {
    auto* self = static_cast<JackBackend*>(arg);
    CHIMERA_WARN("JACK server shutdown");
    self->running_.store(false);
}

} // namespace chimera
