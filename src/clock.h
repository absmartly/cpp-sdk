#pragma once
#include <cstdint>

namespace absmartly::internal {

// The SDK's only clock: attribute setAt, exposedAt, achievedAt, publishedAt and the switchback
// period are all read here, in epoch milliseconds.
int64_t now_millis();

// Test seam, not public API: replaces the clock for the whole process. nullptr restores the
// system clock.
using ClockFunction = int64_t (*)();
void set_clock_for_testing(ClockFunction clock);

}
