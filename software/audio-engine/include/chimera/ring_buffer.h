#pragma once

#include <atomic>
#include <cstddef>
#include <cstring>
#include <type_traits>

namespace chimera {

template<typename T>
class RingBuffer {
    static_assert(std::is_trivially_copyable_v<T>, "RingBuffer requires trivially copyable types");

public:
    explicit RingBuffer(size_t capacity)
        : capacity_(capacity)
        , mask_(capacity - 1)
        , buffer_(new T[capacity])
    {
    }

    ~RingBuffer() {
        delete[] buffer_;
    }

    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;

    bool push(const T& item) {
        size_t current_write = write_pos_.load(std::memory_order_relaxed);
        size_t current_read = read_pos_.load(std::memory_order_acquire);
        size_t next_write = (current_write + 1) & mask_;

        if (next_write == current_read) {
            return false;
        }

        buffer_[current_write] = item;
        write_pos_.store(next_write, std::memory_order_release);
        return true;
    }

    bool pop(T& item) {
        size_t current_read = read_pos_.load(std::memory_order_relaxed);
        size_t current_write = write_pos_.load(std::memory_order_acquire);

        if (current_read == current_write) {
            return false;
        }

        item = buffer_[current_read];
        read_pos_.store((current_read + 1) & mask_, std::memory_order_release);
        return true;
    }

    size_t size() const {
        size_t w = write_pos_.load(std::memory_order_acquire);
        size_t r = read_pos_.load(std::memory_order_acquire);
        return (w - r) & mask_;
    }

    size_t capacity() const { return capacity_; }
    bool empty() const { return size() == 0; }
    bool full() const { return size() == capacity_ - 1; }

private:
    size_t capacity_;
    size_t mask_;
    T* buffer_;
    std::atomic<size_t> write_pos_{0};
    std::atomic<size_t> read_pos_{0};
};

} // namespace chimera
