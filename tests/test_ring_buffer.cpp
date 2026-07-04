#include "chimera/ring_buffer.h"
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

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
    chimera::RingBuffer<int> rb(256);

    TEST("buffer starts empty", rb.empty());
    TEST("buffer not full", !rb.full());
    TEST("size is 0", rb.size() == 0);

    TEST("push returns true", rb.push(42));
    TEST("buffer not empty", !rb.empty());
    TEST("size is 1", rb.size() == 1);

    int val = 0;
    TEST("pop returns true", rb.pop(val));
    TEST("popped correct value", val == 42);
    TEST("buffer empty again", rb.empty());

    for (int i = 0; i < 255; ++i) {
        rb.push(i);
    }
    TEST("buffer full", rb.full());
    TEST("push on full returns false", !rb.push(999));

    int last = -1;
    while (rb.pop(val)) {
        last = val;
    }
    TEST("last value correct", last == 254);

    TEST("buffer empty after drain", rb.empty());

    constexpr int N = 10000;
    chimera::RingBuffer<int> stress(8192);

    auto producer = std::thread([&]() {
        for (int i = 0; i < N; ++i) {
            while (!stress.push(i)) {
                std::this_thread::yield();
            }
        }
    });

    auto consumer = std::thread([&]() {
        int expected = 0;
        int val = 0;
        for (int i = 0; i < N; ++i) {
            while (!stress.pop(val)) {
                std::this_thread::yield();
            }
            if (val != expected) {
                std::fprintf(stderr, "FAIL: expected %d got %d\n", expected, val);
                failures++;
            }
            expected++;
        }
    });

    producer.join();
    consumer.join();

    TEST("stress test completed", stress.empty());

    std::printf("\n%d test(s) failed\n", failures);
    return failures > 0 ? 1 : 0;
}
