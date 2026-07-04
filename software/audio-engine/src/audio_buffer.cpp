#include "chimera/audio_buffer.h"

#include <cstdlib>
#include <cstring>

namespace chimera {

namespace {
    constexpr size_t ALIGNMENT = 64;

    void* rt_alloc(size_t bytes) {
        void* ptr = nullptr;
        if (posix_memalign(&ptr, ALIGNMENT, bytes) != 0) {
            return nullptr;
        }
        std::memset(ptr, 0, bytes);
        return ptr;
    }

    void rt_free(void* ptr) {
        free(ptr);
    }
}

void* AudioBuffer::allocate_realtime(size_t bytes) {
    return rt_alloc(bytes);
}

void AudioBuffer::free_realtime(void* ptr, size_t) {
    rt_free(ptr);
}

} // namespace chimera
