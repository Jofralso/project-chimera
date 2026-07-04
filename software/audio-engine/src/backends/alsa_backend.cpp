#include "chimera/backends/alsa_backend.h"
#include "chimera/engine.h"
#include "chimera/logger.h"

#include <cstring>
#include <cstring>

#include <alsa/asoundlib.h>

namespace chimera {

AlsaBackend::~AlsaBackend() {
    shutdown();
}

bool AlsaBackend::init(const EngineConfig& config) {
    sample_rate_ = config.sample_rate;
    block_size_ = config.block_size;
    num_inputs_ = config.num_inputs;
    num_outputs_ = config.num_outputs;

    if (!config.audio_device.empty()) {
        device_ = config.audio_device;
    }

    CHIMERA_INFO("ALSA backend initializing: %s, %g Hz, %zu frames",
                 device_.c_str(), sample_rate_, block_size_);
    return true;
}

bool AlsaBackend::start() {
    if (running_.load()) return false;
    if (!process_callback_) return false;

    if (!open_device()) {
        CHIMERA_ERROR("ALSA backend: failed to open PCM device");
        return false;
    }

    if (num_inputs_ > 0 && !open_capture_device()) {
        CHIMERA_WARN("ALSA backend: capture device not available, inputs will be silent");
    }

    running_.store(true);
    thread_ = std::make_unique<std::thread>([this]() { thread_func(); });
    CHIMERA_INFO("ALSA backend started on %s", device_.c_str());
    return true;
}

void AlsaBackend::stop() {
    running_.store(false);
    if (thread_ && thread_->joinable()) {
        thread_->join();
    }
    thread_.reset();
    close_device();
    close_capture_device();
    CHIMERA_INFO("ALSA backend stopped");
}

void AlsaBackend::shutdown() {
    stop();
}

void AlsaBackend::set_process_callback(
    void (*callback)(float**, float**, size_t, void*),
    void* userdata)
{
    process_callback_ = callback;
    userdata_ = userdata;
}

bool AlsaBackend::open_device() {
    snd_pcm_t* pcm = nullptr;
    int err;

    err = snd_pcm_open(&pcm, device_.c_str(), SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        CHIMERA_ERROR("ALSA: snd_pcm_open failed: %s", snd_strerror(err));
        return false;
    }

    snd_pcm_hw_params_t* hw_params = nullptr;
    snd_pcm_hw_params_alloca(&hw_params);

    err = snd_pcm_hw_params_any(pcm, hw_params);
    if (err < 0) {
        CHIMERA_ERROR("ALSA: snd_pcm_hw_params_any failed: %s", snd_strerror(err));
        snd_pcm_close(pcm);
        return false;
    }

    err = snd_pcm_hw_params_set_access(pcm, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0) {
        CHIMERA_ERROR("ALSA: snd_pcm_hw_params_set_access failed: %s", snd_strerror(err));
        snd_pcm_close(pcm);
        return false;
    }

    err = snd_pcm_hw_params_set_format(pcm, hw_params, SND_PCM_FORMAT_FLOAT);
    if (err < 0) {
        CHIMERA_WARN("ALSA: float format not supported, trying S16_LE");
        err = snd_pcm_hw_params_set_format(pcm, hw_params, SND_PCM_FORMAT_S16_LE);
        if (err < 0) {
            CHIMERA_ERROR("ALSA: snd_pcm_hw_params_set_format failed: %s", snd_strerror(err));
            snd_pcm_close(pcm);
            return false;
        }
    }

    unsigned int rate = static_cast<unsigned int>(sample_rate_);
    err = snd_pcm_hw_params_set_rate_near(pcm, hw_params, &rate, nullptr);
    if (err < 0) {
        CHIMERA_ERROR("ALSA: snd_pcm_hw_params_set_rate_near failed: %s", snd_strerror(err));
        snd_pcm_close(pcm);
        return false;
    }
    sample_rate_ = static_cast<double>(rate);

    unsigned int channels = static_cast<unsigned int>(num_outputs_);
    err = snd_pcm_hw_params_set_channels_near(pcm, hw_params, &channels);
    if (err < 0) {
        CHIMERA_ERROR("ALSA: snd_pcm_hw_params_set_channels failed: %s", snd_strerror(err));
        snd_pcm_close(pcm);
        return false;
    }
    num_outputs_ = channels;

    snd_pcm_uframes_t period = static_cast<snd_pcm_uframes_t>(block_size_);
    err = snd_pcm_hw_params_set_period_size_near(pcm, hw_params, &period, nullptr);
    if (err < 0) {
        CHIMERA_ERROR("ALSA: snd_pcm_hw_params_set_period_size failed: %s", snd_strerror(err));
        snd_pcm_close(pcm);
        return false;
    }
    block_size_ = static_cast<size_t>(period);

    err = snd_pcm_hw_params(pcm, hw_params);
    if (err < 0) {
        CHIMERA_ERROR("ALSA: snd_pcm_hw_params failed: %s", snd_strerror(err));
        snd_pcm_close(pcm);
        return false;
    }

    pcm_handle_ = pcm;
    CHIMERA_INFO("ALSA: opened %s (%u Hz, %u ch, %zu frames)",
                 device_.c_str(), rate, channels, block_size_);
    return true;
}

void AlsaBackend::close_device() {
    if (pcm_handle_) {
        snd_pcm_drain(static_cast<snd_pcm_t*>(pcm_handle_));
        snd_pcm_close(static_cast<snd_pcm_t*>(pcm_handle_));
        pcm_handle_ = nullptr;
    }
}

bool AlsaBackend::open_capture_device() {
    snd_pcm_t* capture = nullptr;
    int err = snd_pcm_open(&capture, device_.c_str(), SND_PCM_STREAM_CAPTURE, 0);
    if (err < 0) {
        CHIMERA_WARN("ALSA: snd_pcm_open (capture) failed: %s", snd_strerror(err));
        return false;
    }

    snd_pcm_hw_params_t* params = nullptr;
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(capture, params);
    snd_pcm_hw_params_set_access(capture, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(capture, params, SND_PCM_FORMAT_FLOAT);

    unsigned int rate = static_cast<unsigned int>(sample_rate_);
    snd_pcm_hw_params_set_rate_near(capture, params, &rate, nullptr);

    unsigned int channels = static_cast<unsigned int>(num_inputs_);
    snd_pcm_hw_params_set_channels_near(capture, params, &channels);

    snd_pcm_uframes_t period = static_cast<snd_pcm_uframes_t>(block_size_);
    snd_pcm_hw_params_set_period_size_near(capture, params, &period, nullptr);

    err = snd_pcm_hw_params(capture, params);
    if (err < 0) {
        CHIMERA_ERROR("ALSA: capture hw_params failed: %s", snd_strerror(err));
        snd_pcm_close(capture);
        return false;
    }

    capture_handle_ = capture;
    CHIMERA_INFO("ALSA: capture opened (%u Hz, %u ch)", rate, channels);
    return true;
}

void AlsaBackend::close_capture_device() {
    if (capture_handle_) {
        snd_pcm_drain(static_cast<snd_pcm_t*>(capture_handle_));
        snd_pcm_close(static_cast<snd_pcm_t*>(capture_handle_));
        capture_handle_ = nullptr;
    }
}

void AlsaBackend::thread_func() {
    auto* pcm = static_cast<snd_pcm_t*>(pcm_handle_);
    auto num_ch = num_outputs_;

    auto** outputs = new float*[num_ch];
    auto** inputs = new float*[num_inputs_];
    auto* interleaved = new float[block_size_ * num_ch];

    for (size_t i = 0; i < num_ch; ++i) {
        outputs[i] = new float[block_size_]();
    }
    for (size_t i = 0; i < num_inputs_; ++i) {
        inputs[i] = new float[block_size_]();
    }

    auto* capture = static_cast<snd_pcm_t*>(capture_handle_);

    while (running_.load()) {
        // Read capture data
        if (capture && num_inputs_ > 0) {
            auto* cap_interleaved = new float[block_size_ * num_inputs_];
            snd_pcm_sframes_t cap_frames = snd_pcm_readi(capture, cap_interleaved, block_size_);
            if (cap_frames < 0) {
                snd_pcm_recover(capture, static_cast<int>(cap_frames), 0);
                for (size_t i = 0; i < num_inputs_; ++i)
                    std::memset(inputs[i], 0, block_size_ * sizeof(float));
            } else {
                for (size_t f = 0; f < static_cast<size_t>(cap_frames); ++f) {
                    for (size_t ch = 0; ch < num_inputs_; ++ch) {
                        inputs[ch][f] = cap_interleaved[f * num_inputs_ + ch];
                    }
                }
            }
            delete[] cap_interleaved;
        } else {
            for (size_t i = 0; i < num_inputs_; ++i) {
                std::memset(inputs[i], 0, block_size_ * sizeof(float));
            }
        }

        process_callback_(outputs, inputs, block_size_, userdata_);

        for (size_t f = 0; f < block_size_; ++f) {
            for (size_t ch = 0; ch < num_ch; ++ch) {
                interleaved[f * num_ch + ch] = outputs[ch][f];
            }
        }

        snd_pcm_sframes_t frames = snd_pcm_writei(pcm, interleaved, block_size_);
        if (frames < 0) {
            frames = snd_pcm_recover(pcm, static_cast<int>(frames), 0);
            if (frames < 0) {
                CHIMERA_ERROR("ALSA: write error: %s", snd_strerror(static_cast<int>(frames)));
                break;
            }
        }
    }

    for (size_t i = 0; i < num_ch; ++i) delete[] outputs[i];
    for (size_t i = 0; i < num_inputs_; ++i) delete[] inputs[i];
    delete[] outputs;
    delete[] inputs;
    delete[] interleaved;
}

} // namespace chimera
