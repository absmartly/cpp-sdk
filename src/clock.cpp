#include "clock.h"

#include <atomic>
#include <chrono>

namespace absmartly::internal {

static std::atomic<ClockFunction> test_clock{nullptr};

int64_t now_millis() {
    if (const ClockFunction clock = test_clock.load(std::memory_order_relaxed)) {
        return clock();
    }
    const auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

void set_clock_for_testing(ClockFunction clock) {
    test_clock.store(clock, std::memory_order_relaxed);
}

}
